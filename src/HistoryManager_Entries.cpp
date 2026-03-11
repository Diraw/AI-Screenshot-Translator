#include "HistoryManager.h"

#include <QBuffer>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QScopedValueRollback>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace
{
QStringList parseTagsJson(const QString &tagsJson)
{
    if (tagsJson.trimmed().isEmpty())
        return QStringList();

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(tagsJson.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isArray())
        return QStringList();
    QStringList tags;
    const QJsonArray arr = doc.array();
    for (const QJsonValue &v : arr)
    {
        if (!v.isString())
            continue;
        const QString tag = v.toString();
        if (!tags.contains(tag))
            tags.append(tag);
    }
    return tags;
}

QString stringifyTags(const QStringList &tags)
{
    return QString::fromUtf8(QJsonDocument(QJsonArray::fromStringList(tags)).toJson(QJsonDocument::Compact));
}
} // namespace

void HistoryManager::saveEntry(const TranslationEntry &entry)
{
    QScopedValueRollback<bool> guard(m_ignoreNextChange, true);
    ensureDirectories();
    if (!ensureDatabaseReady())
        return;

    QByteArray bytes = QByteArray::fromBase64(entry.originalBase64.toLatin1());
    QImage img;
    if (!img.loadFromData(bytes) || img.isNull())
    {
        qWarning() << "HistoryManager::saveEntry skipped: invalid image payload for entry" << entry.id;
        return;
    }

    QString timeStr = entry.timestamp.toString("yyyyMMdd_HHmmss_zzz");
    QString imgFilename = timeStr + ".png";
    QString fullImgPath = getImagesPath() + "/" + imgFilename;
    if (QFile::exists(fullImgPath))
    {
        imgFilename = timeStr + "_" + entry.id.left(4) + ".png";
        fullImgPath = getImagesPath() + "/" + imgFilename;
    }

    if (!img.save(fullImgPath, "PNG"))
    {
        qWarning() << "HistoryManager::saveEntry failed to write image:" << fullImgPath;
        return;
    }

    QString previousImageRelPath;
    {
        QSqlQuery existing(m_db);
        existing.prepare("SELECT image_file FROM entries WHERE id = ?");
        existing.addBindValue(entry.id);
        if (existing.exec() && existing.next())
            previousImageRelPath = existing.value(0).toString();
    }

    const QString imageRelPath = "images/" + imgFilename;
    QSqlQuery upsert(m_db);
    upsert.prepare(
        "INSERT OR REPLACE INTO entries (id, timestamp, image_file, prompt, markdown, tags_json) "
        "VALUES (?, ?, ?, ?, ?, ?)");
    upsert.addBindValue(entry.id);
    upsert.addBindValue(entry.timestamp.toString(Qt::ISODate));
    upsert.addBindValue(imageRelPath);
    upsert.addBindValue(entry.prompt);
    upsert.addBindValue(HistoryManager::normalizeMarkdown(entry.translatedMarkdown));
    upsert.addBindValue(stringifyTags(entry.tags));
    if (!upsert.exec())
    {
        qWarning() << "HistoryManager::saveEntry sqlite upsert failed:" << upsert.lastError().text();
        QFile::remove(fullImgPath);
        return;
    }

    if (!previousImageRelPath.isEmpty() && previousImageRelPath != imageRelPath)
    {
        QFile::remove(QDir(m_basePath).filePath(previousImageRelPath));
    }

    TranslationEntry cachedEntry = entry;
    cachedEntry.translatedMarkdown = HistoryManager::normalizeMarkdown(entry.translatedMarkdown);
    cachedEntry.localImagePath = fullImgPath;
    m_markdownCache[entry.id] = cachedEntry.translatedMarkdown;
    m_entryCache[entry.id] = cachedEntry;
}

QList<TranslationEntry> HistoryManager::loadEntries()
{
    QList<TranslationEntry> entries;
    if (!ensureDatabaseReady())
        return entries;

    m_entryCache.clear();
    m_markdownCache.clear();

    QSqlQuery query(m_db);
    if (!query.exec("SELECT id, timestamp, image_file, prompt, markdown, tags_json FROM entries"))
    {
        qWarning() << "HistoryManager::loadEntries sqlite query failed:" << query.lastError().text();
        return entries;
    }

    QStringList staleIds;
    while (query.next())
    {
        TranslationEntry entry;
        entry.id = query.value(0).toString();
        entry.timestamp = QDateTime::fromString(query.value(1).toString(), Qt::ISODate);
        if (!entry.timestamp.isValid())
            entry.timestamp = QDateTime::currentDateTime();

        QString imagePath = query.value(2).toString();
        if (imagePath.isEmpty())
        {
            staleIds << entry.id;
            continue;
        }

        if (!QDir::isAbsolutePath(imagePath))
            imagePath = QDir(m_basePath).filePath(imagePath);
        if (!QFile::exists(imagePath))
        {
            staleIds << entry.id;
            continue;
        }

        QImage img(imagePath);
        if (img.isNull())
        {
            staleIds << entry.id;
            continue;
        }

        entry.prompt = query.value(3).toString();
        entry.translatedMarkdown = HistoryManager::normalizeMarkdown(query.value(4).toString());
        entry.tags = parseTagsJson(query.value(5).toString());
        entry.localImagePath = imagePath;

        QByteArray ba;
        QBuffer buffer(&ba);
        buffer.open(QIODevice::WriteOnly);
        img.save(&buffer, "PNG");
        entry.originalBase64 = ba.toBase64();

        m_markdownCache[entry.id] = entry.translatedMarkdown;
        m_entryCache[entry.id] = entry;
        entries.append(entry);
    }

    if (!staleIds.isEmpty())
    {
        QScopedValueRollback<bool> guard(m_ignoreNextChange, true);
        QSqlQuery tx(m_db);
        tx.exec("BEGIN IMMEDIATE TRANSACTION");
        QSqlQuery del(m_db);
        del.prepare("DELETE FROM entries WHERE id = ?");
        for (const QString &id : staleIds)
        {
            del.bindValue(0, id);
            del.exec();
        }
        tx.exec("COMMIT");
    }

    return entries;
}

bool HistoryManager::deleteEntry(const QString &id)
{
    if (id.trimmed().isEmpty())
        return false;
    return deleteEntries(QStringList{id});
}

bool HistoryManager::deleteEntries(const QStringList &ids)
{
    if (ids.isEmpty())
        return true;
    if (!ensureDatabaseReady())
        return false;

    QScopedValueRollback<bool> guard(m_ignoreNextChange, true);
    QSqlQuery tx(m_db);
    if (!tx.exec("BEGIN IMMEDIATE TRANSACTION"))
    {
        qWarning() << "HistoryManager::deleteEntries failed to begin transaction:" << tx.lastError().text();
        return false;
    }

    bool modified = false;
    QSqlQuery select(m_db);
    QSqlQuery del(m_db);
    select.prepare("SELECT image_file FROM entries WHERE id = ?");
    del.prepare("DELETE FROM entries WHERE id = ?");

    for (const QString &id : ids)
    {
        QString imageRelPath;
        select.bindValue(0, id);
        if (select.exec() && select.next())
        {
            imageRelPath = select.value(0).toString();
        }
        select.finish();

        del.bindValue(0, id);
        if (del.exec() && del.numRowsAffected() > 0)
        {
            modified = true;
            m_markdownCache.remove(id);
            m_entryCache.remove(id);
            if (!imageRelPath.isEmpty())
            {
                const QString fullPath = QDir(m_basePath).filePath(imageRelPath);
                QFile::remove(fullPath);
            }
        }
        del.finish();
    }

    QSqlQuery commit(m_db);
    if (!commit.exec("COMMIT"))
    {
        QSqlQuery rollback(m_db);
        rollback.exec("ROLLBACK");
        qWarning() << "HistoryManager::deleteEntries failed to commit transaction:" << commit.lastError().text();
        return false;
    }

    return modified;
}

TranslationEntry HistoryManager::getEntryById(const QString &id)
{
    if (m_entryCache.contains(id))
        return m_entryCache.value(id);
    if (!ensureDatabaseReady())
        return TranslationEntry();

    QSqlQuery query(m_db);
    query.prepare("SELECT id, timestamp, image_file, prompt, markdown, tags_json FROM entries WHERE id = ?");
    query.addBindValue(id);
    if (!query.exec() || !query.next())
        return TranslationEntry();

    TranslationEntry entry;
    entry.id = query.value(0).toString();
    entry.timestamp = QDateTime::fromString(query.value(1).toString(), Qt::ISODate);
    if (!entry.timestamp.isValid())
        entry.timestamp = QDateTime::currentDateTime();

    QString imagePath = query.value(2).toString();
    if (!QDir::isAbsolutePath(imagePath))
        imagePath = QDir(m_basePath).filePath(imagePath);
    if (!QFile::exists(imagePath))
        return TranslationEntry();

    entry.prompt = query.value(3).toString();
    entry.translatedMarkdown = HistoryManager::normalizeMarkdown(query.value(4).toString());
    entry.tags = parseTagsJson(query.value(5).toString());
    entry.localImagePath = imagePath;

    QImage img(imagePath);
    if (img.isNull())
        return TranslationEntry();
    QByteArray ba;
    QBuffer buffer(&ba);
    buffer.open(QIODevice::WriteOnly);
    img.save(&buffer, "PNG");
    entry.originalBase64 = ba.toBase64();

    m_markdownCache[entry.id] = entry.translatedMarkdown;
    m_entryCache[entry.id] = entry;
    return entry;
}

bool HistoryManager::updateEntryContent(const QString &id, const QString &newMarkdown)
{
    if (!ensureDatabaseReady())
        return false;

    const QString normalized = HistoryManager::normalizeMarkdown(newMarkdown);
    QScopedValueRollback<bool> guard(m_ignoreNextChange, true);

    QSqlQuery query(m_db);
    query.prepare("UPDATE entries SET markdown = ? WHERE id = ?");
    query.addBindValue(normalized);
    query.addBindValue(id);
    if (!query.exec())
    {
        qWarning() << "HistoryManager::updateEntryContent sqlite update failed:" << query.lastError().text();
        return false;
    }
    if (query.numRowsAffected() <= 0)
        return false;

    m_markdownCache[id] = normalized;
    if (m_entryCache.contains(id))
    {
        TranslationEntry cached = m_entryCache.value(id);
        cached.translatedMarkdown = normalized;
        m_entryCache[id] = cached;
    }
    emit entryMarkdownChanged(id, normalized);
    return true;
}
