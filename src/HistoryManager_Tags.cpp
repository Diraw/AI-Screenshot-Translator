#include "HistoryManager.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QScopedValueRollback>
#include <QSet>
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

    QStringList out;
    for (const QJsonValue &v : doc.array())
    {
        if (!v.isString())
            continue;
        const QString t = v.toString();
        if (!out.contains(t))
            out.append(t);
    }
    return out;
}

QString stringifyTags(const QStringList &tags)
{
    return QString::fromUtf8(QJsonDocument(QJsonArray::fromStringList(tags)).toJson(QJsonDocument::Compact));
}
} // namespace

bool HistoryManager::updateEntryTags(const QString &id, const QStringList &tags)
{
    if (!ensureDatabaseReady())
        return false;

    QScopedValueRollback<bool> guard(m_ignoreNextChange, true);
    QSqlQuery update(m_db);
    update.prepare("UPDATE entries SET tags_json = ? WHERE id = ?");
    update.addBindValue(stringifyTags(tags));
    update.addBindValue(id);
    if (!update.exec())
    {
        qWarning() << "HistoryManager::updateEntryTags failed:" << update.lastError().text();
        return false;
    }
    if (update.numRowsAffected() <= 0)
        return false;

    if (m_entryCache.contains(id))
    {
        TranslationEntry cached = m_entryCache.value(id);
        cached.tags = tags;
        m_entryCache[id] = cached;
    }
    return true;
}

bool HistoryManager::addTagsToEntries(const QStringList &ids, const QStringList &tags)
{
    if (ids.isEmpty() || tags.isEmpty())
        return true;
    if (!ensureDatabaseReady())
        return false;

    QScopedValueRollback<bool> guard(m_ignoreNextChange, true);
    QSqlQuery tx(m_db);
    if (!tx.exec("BEGIN IMMEDIATE TRANSACTION"))
        return false;

    bool modified = false;
    QSet<QString> tagsToAdd(tags.begin(), tags.end());

    QSqlQuery select(m_db);
    QSqlQuery update(m_db);
    select.prepare("SELECT tags_json FROM entries WHERE id = ?");
    update.prepare("UPDATE entries SET tags_json = ? WHERE id = ?");

    for (const QString &id : ids)
    {
        select.bindValue(0, id);
        if (!select.exec() || !select.next())
        {
            select.finish();
            continue;
        }

        const QStringList existingTags = parseTagsJson(select.value(0).toString());
        QSet<QString> merged(existingTags.begin(), existingTags.end());
        select.finish();

        const int before = merged.size();
        merged.unite(tagsToAdd);
        if (merged.size() == before)
            continue;

        const QStringList updatedTags = merged.values();
        update.bindValue(0, stringifyTags(updatedTags));
        update.bindValue(1, id);
        if (!update.exec())
        {
            update.finish();
            continue;
        }
        update.finish();

        modified = true;
        if (m_entryCache.contains(id))
        {
            TranslationEntry cached = m_entryCache.value(id);
            cached.tags = updatedTags;
            m_entryCache[id] = cached;
        }
    }

    QSqlQuery done(m_db);
    if (!done.exec("COMMIT"))
    {
        QSqlQuery rollback(m_db);
        rollback.exec("ROLLBACK");
        return false;
    }

    return modified;
}

bool HistoryManager::removeTagsFromEntries(const QStringList &ids, const QStringList &tags)
{
    if (ids.isEmpty() || tags.isEmpty())
        return true;
    if (!ensureDatabaseReady())
        return false;

    QScopedValueRollback<bool> guard(m_ignoreNextChange, true);
    QSqlQuery tx(m_db);
    if (!tx.exec("BEGIN IMMEDIATE TRANSACTION"))
        return false;

    bool modified = false;
    QSet<QString> tagsToRemove(tags.begin(), tags.end());

    QSqlQuery select(m_db);
    QSqlQuery update(m_db);
    select.prepare("SELECT tags_json FROM entries WHERE id = ?");
    update.prepare("UPDATE entries SET tags_json = ? WHERE id = ?");

    for (const QString &id : ids)
    {
        select.bindValue(0, id);
        if (!select.exec() || !select.next())
        {
            select.finish();
            continue;
        }

        const QStringList existingTags = parseTagsJson(select.value(0).toString());
        QSet<QString> current(existingTags.begin(), existingTags.end());
        select.finish();

        const int before = current.size();
        for (const QString &tag : tagsToRemove)
            current.remove(tag);
        if (current.size() == before)
            continue;

        const QStringList updatedTags = current.values();
        update.bindValue(0, stringifyTags(updatedTags));
        update.bindValue(1, id);
        if (!update.exec())
        {
            update.finish();
            continue;
        }
        update.finish();

        modified = true;
        if (m_entryCache.contains(id))
        {
            TranslationEntry cached = m_entryCache.value(id);
            cached.tags = updatedTags;
            m_entryCache[id] = cached;
        }
    }

    QSqlQuery done(m_db);
    if (!done.exec("COMMIT"))
    {
        QSqlQuery rollback(m_db);
        rollback.exec("ROLLBACK");
        return false;
    }

    return modified;
}

QStringList HistoryManager::getAllTags()
{
    if (!ensureDatabaseReady())
        return QStringList();

    QSet<QString> uniqueTags;
    QSqlQuery query(m_db);
    if (!query.exec("SELECT tags_json FROM entries"))
        return QStringList();

    while (query.next())
    {
        for (const QString &tag : parseTagsJson(query.value(0).toString()))
            uniqueTags.insert(tag);
    }

    return uniqueTags.values();
}
