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
#include <QSet>
#include <QTime>
#include <QVariant>
#include <algorithm>
#include <limits>

namespace
{
QStringList parseStringListJson(const QString &json)
{
    if (json.trimmed().isEmpty())
        return {};

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isArray())
        return {};

    QStringList out;
    for (const QJsonValue &v : doc.array())
    {
        if (v.isString())
            out.append(v.toString());
    }
    return out;
}

QStringList parseTagsJson(const QString &tagsJson)
{
    QStringList tags = parseStringListJson(tagsJson);
    tags.removeDuplicates();
    return tags;
}

QString stringifyStringList(const QStringList &values)
{
    return QString::fromUtf8(QJsonDocument(QJsonArray::fromStringList(values)).toJson(QJsonDocument::Compact));
}

QStringList normalizedImagePaths(const QString &basePath, const QString &firstPath, const QString &pathsJson)
{
    QStringList paths = parseStringListJson(pathsJson);
    if (paths.isEmpty() && !firstPath.trimmed().isEmpty())
        paths << firstPath.trimmed();

    for (QString &path : paths)
    {
        if (!QDir::isAbsolutePath(path))
            path = QDir(basePath).filePath(path);
    }
    return paths;
}

QStringList normalizedBase64List(const TranslationEntry &entry)
{
    QStringList base64List = entry.originalBase64List;
    if (base64List.isEmpty() && !entry.originalBase64.isEmpty())
        base64List << entry.originalBase64;
    return base64List;
}
} // namespace

void HistoryManager::saveEntry(const TranslationEntry &entry)
{
    QScopedValueRollback<bool> guard(m_ignoreNextChange, true);
    ensureDirectories();
    if (!ensureDatabaseReady())
        return;

    const QStringList base64Images = normalizedBase64List(entry);
    if (base64Images.isEmpty())
    {
        qWarning() << "HistoryManager::saveEntry skipped: empty image payload for entry" << entry.id;
        return;
    }

    QStringList imageRelPaths;
    QStringList fullImagePaths;
    imageRelPaths.reserve(base64Images.size());
    fullImagePaths.reserve(base64Images.size());

    const QString timeStr = entry.timestamp.toString("yyyyMMdd_HHmmss_zzz");
    for (int i = 0; i < base64Images.size(); ++i)
    {
        const QByteArray bytes = QByteArray::fromBase64(base64Images[i].toLatin1());
        QImage img;
        if (!img.loadFromData(bytes) || img.isNull())
        {
            qWarning() << "HistoryManager::saveEntry skipped invalid image payload for entry" << entry.id << "index" << i;
            for (const QString &path : fullImagePaths)
                QFile::remove(path);
            return;
        }

        QString imgFilename = (base64Images.size() == 1)
                                  ? QString("%1.png").arg(timeStr)
                                  : QString("%1_%2.png").arg(timeStr).arg(i + 1);
        QString fullImgPath = getImagesPath() + "/" + imgFilename;
        if (QFile::exists(fullImgPath))
        {
            imgFilename = QString("%1_%2_%3.png").arg(timeStr).arg(entry.id.left(4)).arg(i + 1);
            fullImgPath = getImagesPath() + "/" + imgFilename;
        }

        if (!img.save(fullImgPath, "PNG"))
        {
            qWarning() << "HistoryManager::saveEntry failed to write image:" << fullImgPath;
            for (const QString &path : fullImagePaths)
                QFile::remove(path);
            return;
        }

        fullImagePaths << fullImgPath;
        imageRelPaths << ("images/" + imgFilename);
    }

    QStringList previousImageRelPaths;
    {
        QSqlQuery existing(m_db);
        existing.prepare("SELECT image_file, image_files_json FROM entries WHERE id = ?");
        existing.addBindValue(entry.id);
        if (existing.exec() && existing.next())
            previousImageRelPaths = normalizedImagePaths(m_basePath, existing.value(0).toString(), existing.value(1).toString());
    }

    QSqlQuery upsert(m_db);
    upsert.prepare(
        "INSERT OR REPLACE INTO entries (id, timestamp, image_file, image_files_json, prompt, markdown, tags_json) "
        "VALUES (?, ?, ?, ?, ?, ?, ?)");
    upsert.addBindValue(entry.id);
    upsert.addBindValue(entry.timestamp.toString(Qt::ISODate));
    upsert.addBindValue(imageRelPaths.value(0));
    upsert.addBindValue(stringifyStringList(imageRelPaths));
    upsert.addBindValue(entry.prompt);
    upsert.addBindValue(HistoryManager::normalizeMarkdown(entry.translatedMarkdown));
    upsert.addBindValue(stringifyStringList(entry.tags));
    if (!upsert.exec())
    {
        qWarning() << "HistoryManager::saveEntry sqlite upsert failed:" << upsert.lastError().text();
        for (const QString &path : fullImagePaths)
            QFile::remove(path);
        return;
    }

    for (const QString &previousPath : previousImageRelPaths)
    {
        QString normalized = previousPath;
        if (!QDir::isAbsolutePath(normalized))
            normalized = QDir(m_basePath).filePath(normalized);
        if (!fullImagePaths.contains(normalized))
            QFile::remove(normalized);
    }

    TranslationEntry cachedEntry = entry;
    cachedEntry.translatedMarkdown = HistoryManager::normalizeMarkdown(entry.translatedMarkdown);
    cachedEntry.localImagePath = fullImagePaths.value(0);
    cachedEntry.localImagePaths = fullImagePaths;
    cachedEntry.originalBase64.clear();
    cachedEntry.originalBase64List.clear();
    m_markdownCache[entry.id] = cachedEntry.translatedMarkdown;
    m_entryCache[entry.id] = cachedEntry;
    m_tagsCacheDirty = true;
}

QList<TranslationEntry> HistoryManager::loadEntries()
{
    QList<TranslationEntry> entries;
    if (!ensureDatabaseReady())
        return entries;

    m_entryCache.clear();
    m_markdownCache.clear();

    QSqlQuery query(m_db);
    if (!query.exec("SELECT id, timestamp, image_file, image_files_json, prompt, markdown, tags_json FROM entries"))
    {
        qWarning() << "HistoryManager::loadEntries sqlite query failed:" << query.lastError().text();
        return entries;
    }

    QSet<QString> uniqueTags;
    QStringList staleIds;
    while (query.next())
    {
        TranslationEntry entry;
        entry.id = query.value(0).toString();
        entry.timestamp = QDateTime::fromString(query.value(1).toString(), Qt::ISODate);
        if (!entry.timestamp.isValid())
            entry.timestamp = QDateTime::currentDateTime();

        entry.localImagePaths = normalizedImagePaths(m_basePath, query.value(2).toString(), query.value(3).toString());
        if (entry.localImagePaths.isEmpty())
        {
            staleIds << entry.id;
            continue;
        }

        bool missingImage = false;
        for (const QString &imagePath : entry.localImagePaths)
        {
            if (!QFile::exists(imagePath))
            {
                missingImage = true;
                break;
            }
        }
        if (missingImage)
        {
            staleIds << entry.id;
            continue;
        }

        entry.localImagePath = entry.localImagePaths.value(0);
        entry.prompt = query.value(4).toString();
        entry.translatedMarkdown = HistoryManager::normalizeMarkdown(query.value(5).toString());
        entry.tags = parseTagsJson(query.value(6).toString());
        for (const QString &tag : entry.tags)
            uniqueTags.insert(tag);

        entry.originalBase64.clear();
        entry.originalBase64List.clear();

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

    m_allTagsCache = uniqueTags.values();
    std::sort(m_allTagsCache.begin(), m_allTagsCache.end(), [](const QString &a, const QString &b)
              { return QString::localeAwareCompare(a, b) < 0; });
    m_tagsCacheDirty = false;

    return entries;
}

QList<TranslationEntry> HistoryManager::queryEntries(const QDate &fromDate,
                                                     const QDate &toDate,
                                                     const QStringList &tags,
                                                     const QString &searchText,
                                                     int limit,
                                                     int offset,
                                                     int *totalCount)
{
    QList<TranslationEntry> entries;
    if (totalCount)
        *totalCount = 0;
    if (!ensureDatabaseReady())
        return entries;

    auto escapeLike = [](QString s)
    {
        s.replace("\\", "\\\\");
        s.replace("%", "\\%");
        s.replace("_", "\\_");
        return s;
    };

    QStringList whereClauses;
    QVariantList bindValues;

    if (fromDate.isValid())
    {
        const QString fromIso = QDateTime(fromDate, QTime(0, 0, 0, 0)).toString(Qt::ISODate);
        whereClauses << "timestamp >= ?";
        bindValues << fromIso;
    }
    if (toDate.isValid())
    {
        const QString toIso = QDateTime(toDate, QTime(23, 59, 59, 999)).toString(Qt::ISODate);
        whereClauses << "timestamp <= ?";
        bindValues << toIso;
    }
    if (!tags.isEmpty())
    {
        QStringList tagPredicates;
        for (const QString &tag : tags)
        {
            if (tag.trimmed().isEmpty())
                continue;
            tagPredicates << "tags_json LIKE ? ESCAPE '\\'";
            bindValues << QString("%\"%1\"%").arg(escapeLike(tag));
        }
        if (!tagPredicates.isEmpty())
            whereClauses << QString("(%1)").arg(tagPredicates.join(" OR "));
    }
    const QString normalizedSearch = searchText.trimmed().toLower();
    if (!normalizedSearch.isEmpty())
    {
        whereClauses << "LOWER(markdown) LIKE ? ESCAPE '\\'";
        bindValues << QString("%" + escapeLike(normalizedSearch) + "%");
    }

    QString whereSql;
    if (!whereClauses.isEmpty())
        whereSql = " WHERE " + whereClauses.join(" AND ");

    const int effectiveOffset = qMax(0, offset);
    const int effectiveLimit = (limit > 0) ? qMax(1, limit) : std::numeric_limits<int>::max();
    QStringList staleIds;

    if (totalCount)
    {
        QSqlQuery countQuery(m_db);
        if (countQuery.prepare("SELECT COUNT(*) FROM entries" + whereSql))
        {
            for (const QVariant &v : bindValues)
                countQuery.addBindValue(v);
            if (countQuery.exec() && countQuery.next())
                *totalCount = countQuery.value(0).toInt();
        }
    }

    int dbOffset = effectiveOffset;
    const int batchSize = (limit > 0) ? qMax(effectiveLimit, 100) : 200;

    while (entries.size() < effectiveLimit)
    {
        QString selectSql =
            "SELECT id, timestamp, image_file, image_files_json, prompt, markdown, tags_json FROM entries" + whereSql +
            " ORDER BY timestamp DESC";
        if (limit > 0)
            selectSql += " LIMIT ? OFFSET ?";

        QSqlQuery query(m_db);
        if (!query.prepare(selectSql))
            break;
        for (const QVariant &v : bindValues)
            query.addBindValue(v);
        if (limit > 0)
        {
            query.addBindValue(batchSize);
            query.addBindValue(dbOffset);
        }
        if (!query.exec())
            break;

        bool sawRows = false;
        while (query.next())
        {
            sawRows = true;

            TranslationEntry entry;
            entry.id = query.value(0).toString();
            entry.timestamp = QDateTime::fromString(query.value(1).toString(), Qt::ISODate);
            if (!entry.timestamp.isValid())
                entry.timestamp = QDateTime::currentDateTime();

            entry.localImagePaths = normalizedImagePaths(m_basePath, query.value(2).toString(), query.value(3).toString());
            if (entry.localImagePaths.isEmpty())
            {
                staleIds.append(entry.id);
                continue;
            }

            bool missingImage = false;
            for (const QString &imagePath : entry.localImagePaths)
            {
                if (!QFile::exists(imagePath))
                {
                    missingImage = true;
                    break;
                }
            }
            if (missingImage)
            {
                staleIds.append(entry.id);
                continue;
            }

            entry.localImagePath = entry.localImagePaths.value(0);
            entry.prompt = query.value(4).toString();
            entry.translatedMarkdown = HistoryManager::normalizeMarkdown(query.value(5).toString());
            entry.tags = parseTagsJson(query.value(6).toString());
            entry.originalBase64.clear();
            entry.originalBase64List.clear();

            m_markdownCache[entry.id] = entry.translatedMarkdown;
            m_entryCache[entry.id] = entry;
            entries.append(entry);
            if (entries.size() >= effectiveLimit)
                break;
        }

        if (limit <= 0 || !sawRows)
            break;
        dbOffset += batchSize;
    }

    staleIds.removeDuplicates();
    if (!staleIds.isEmpty())
    {
        QScopedValueRollback<bool> guard(m_ignoreNextChange, true);
        QSqlQuery tx(m_db);
        if (tx.exec("BEGIN IMMEDIATE TRANSACTION"))
        {
            QSqlQuery del(m_db);
            del.prepare("DELETE FROM entries WHERE id = ?");
            for (const QString &id : staleIds)
            {
                del.bindValue(0, id);
                del.exec();
                del.finish();
            }
            QSqlQuery commit(m_db);
            if (!commit.exec("COMMIT"))
            {
                QSqlQuery rollback(m_db);
                rollback.exec("ROLLBACK");
            }
        }
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
    select.prepare("SELECT image_file, image_files_json FROM entries WHERE id = ?");
    del.prepare("DELETE FROM entries WHERE id = ?");

    for (const QString &id : ids)
    {
        QStringList imagePaths;
        select.bindValue(0, id);
        if (select.exec() && select.next())
            imagePaths = normalizedImagePaths(m_basePath, select.value(0).toString(), select.value(1).toString());
        select.finish();

        del.bindValue(0, id);
        if (del.exec() && del.numRowsAffected() > 0)
        {
            modified = true;
            m_markdownCache.remove(id);
            m_entryCache.remove(id);
            for (const QString &fullPath : imagePaths)
                QFile::remove(fullPath);
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

    if (modified)
        m_tagsCacheDirty = true;
    return modified;
}

TranslationEntry HistoryManager::getEntryById(const QString &id)
{
    TranslationEntry entry;
    if (m_entryCache.contains(id))
        entry = m_entryCache.value(id);

    if (!ensureDatabaseReady())
        return TranslationEntry();

    if (entry.id.isEmpty())
    {
        QSqlQuery query(m_db);
        query.prepare("SELECT id, timestamp, image_file, image_files_json, prompt, markdown, tags_json FROM entries WHERE id = ?");
        query.addBindValue(id);
        if (!query.exec() || !query.next())
            return TranslationEntry();

        entry.id = query.value(0).toString();
        entry.timestamp = QDateTime::fromString(query.value(1).toString(), Qt::ISODate);
        if (!entry.timestamp.isValid())
            entry.timestamp = QDateTime::currentDateTime();

        entry.localImagePaths = normalizedImagePaths(m_basePath, query.value(2).toString(), query.value(3).toString());
        if (entry.localImagePaths.isEmpty())
            return TranslationEntry();

        bool missingImage = false;
        for (const QString &imagePath : entry.localImagePaths)
        {
            if (!QFile::exists(imagePath))
            {
                missingImage = true;
                break;
            }
        }
        if (missingImage)
            return TranslationEntry();

        entry.localImagePath = entry.localImagePaths.value(0);
        entry.prompt = query.value(4).toString();
        entry.translatedMarkdown = HistoryManager::normalizeMarkdown(query.value(5).toString());
        entry.tags = parseTagsJson(query.value(6).toString());

        TranslationEntry metadataOnly = entry;
        metadataOnly.originalBase64.clear();
        metadataOnly.originalBase64List.clear();
        m_markdownCache[entry.id] = metadataOnly.translatedMarkdown;
        m_entryCache[entry.id] = metadataOnly;
    }

    if (entry.localImagePaths.isEmpty())
        return TranslationEntry();

    QStringList originalBase64List;
    for (const QString &imagePath : entry.localImagePaths)
    {
        if (!QFile::exists(imagePath))
            return TranslationEntry();

        QImage img(imagePath);
        if (img.isNull())
            return TranslationEntry();

        QByteArray ba;
        QBuffer buffer(&ba);
        buffer.open(QIODevice::WriteOnly);
        img.save(&buffer, "PNG");
        originalBase64List.append(QString::fromLatin1(ba.toBase64()));
    }

    entry.originalBase64List = originalBase64List;
    entry.originalBase64 = originalBase64List.value(0);
    entry.localImagePath = entry.localImagePaths.value(0);

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
