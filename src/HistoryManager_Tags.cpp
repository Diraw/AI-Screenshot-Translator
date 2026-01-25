#include "HistoryManager.h"

#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

bool HistoryManager::updateEntryTags(const QString &id, const QStringList &tags)
{
    m_ignoreNextChange = true;

    QFile file(getJsonPath());
    if (!file.exists() || !file.open(QIODevice::ReadOnly))
    {
        m_ignoreNextChange = false;
        return false;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isArray())
    {
        m_ignoreNextChange = false;
        return false;
    }

    QJsonArray array = doc.array();
    bool found = false;

    for (int i = 0; i < array.size(); ++i)
    {
        QJsonObject obj = array[i].toObject();
        if (obj["id"].toString() == id)
        {
            obj["tags"] = QJsonArray::fromStringList(tags);
            array[i] = obj;
            found = true;
            break;
        }
    }

    if (found)
    {
        if (file.open(QIODevice::WriteOnly))
        {
            QJsonDocument outDoc(array);
            file.write(outDoc.toJson());
            file.close();
            if (m_entryCache.contains(id))
            {
                TranslationEntry cached = m_entryCache.value(id);
                cached.tags = tags;
                m_entryCache[id] = cached;
            }
            m_ignoreNextChange = false;
            return true;
        }
    }

    m_ignoreNextChange = false;
    return false;
}

bool HistoryManager::addTagsToEntries(const QStringList &ids, const QStringList &tags)
{
    qDebug() << "HistoryManager::addTagsToEntries called. IDs:" << ids << "Tags to add:" << tags;
    if (ids.isEmpty() || tags.isEmpty())
        return true;

    m_ignoreNextChange = true;

    QFile file(getJsonPath());
    if (!file.exists() || !file.open(QIODevice::ReadOnly))
    {
        qDebug() << "Failed to open history.json for reading";
        m_ignoreNextChange = false;
        return false;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isArray())
    {
        qDebug() << "history.json is not an array";
        m_ignoreNextChange = false;
        return false;
    }

    QJsonArray array = doc.array();
    bool modified = false;
    QSet<QString> idSet = QSet<QString>(ids.begin(), ids.end());

    for (int i = 0; i < array.size(); ++i)
    {
        QJsonObject obj = array[i].toObject();
        QString currentId = obj["id"].toString();
        if (idSet.contains(currentId))
        {
            QStringList currentTags = obj["tags"].toVariant().toStringList();
            QSet<QString> tagSet = QSet<QString>(currentTags.begin(), currentTags.end());
            bool changed = false;
            for (const QString &tag : tags)
            {
                if (!tagSet.contains(tag))
                {
                    tagSet.insert(tag);
                    changed = true;
                }
            }
            if (changed)
            {
                qDebug() << "Tags modified for entry:" << currentId << "New tags:" << tagSet.values();
                obj["tags"] = QJsonArray::fromStringList(tagSet.values());
                array[i] = obj;
                if (m_entryCache.contains(currentId))
                {
                    TranslationEntry cached = m_entryCache.value(currentId);
                    cached.tags = tagSet.values();
                    m_entryCache[currentId] = cached;
                }
                modified = true;
            }
            else
            {
                qDebug() << "No new tags to add for entry:" << currentId;
            }
        }
    }

    if (modified)
    {
        if (file.open(QIODevice::WriteOnly))
        {
            QJsonDocument outDoc(array);
            file.write(outDoc.toJson());
            file.close();
            qDebug() << "Successfully saved updated history.json after adding tags";
            m_ignoreNextChange = false;
            return true;
        }
        else
        {
            qDebug() << "Failed to open history.json for writing";
        }
    }
    else
    {
        qDebug() << "No entries were modified (tags already existed or IDs didn't match)";
    }

    m_ignoreNextChange = false;
    return modified;
}

bool HistoryManager::removeTagsFromEntries(const QStringList &ids, const QStringList &tags)
{
    qDebug() << "HistoryManager::removeTagsFromEntries called. IDs:" << ids << "Tags to remove:" << tags;
    if (ids.isEmpty() || tags.isEmpty())
        return true;

    m_ignoreNextChange = true;

    QFile file(getJsonPath());
    if (!file.exists() || !file.open(QIODevice::ReadOnly))
    {
        qDebug() << "Failed to open history.json for reading";
        m_ignoreNextChange = false;
        return false;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isArray())
    {
        qDebug() << "history.json is not an array";
        m_ignoreNextChange = false;
        return false;
    }

    QJsonArray array = doc.array();
    bool modified = false;
    QSet<QString> idSet = QSet<QString>(ids.begin(), ids.end());

    for (int i = 0; i < array.size(); ++i)
    {
        QJsonObject obj = array[i].toObject();
        QString currentId = obj["id"].toString();
        if (idSet.contains(currentId))
        {
            QStringList currentTags = obj["tags"].toVariant().toStringList();
            QSet<QString> tagSet = QSet<QString>(currentTags.begin(), currentTags.end());
            bool changed = false;
            for (const QString &tag : tags)
            {
                if (tagSet.contains(tag))
                {
                    tagSet.remove(tag);
                    changed = true;
                }
            }
            if (changed)
            {
                qDebug() << "Tags modified for entry:" << currentId << "New tags:" << tagSet.values();
                obj["tags"] = QJsonArray::fromStringList(tagSet.values());
                array[i] = obj;
                if (m_entryCache.contains(currentId))
                {
                    TranslationEntry cached = m_entryCache.value(currentId);
                    cached.tags = tagSet.values();
                    m_entryCache[currentId] = cached;
                }
                modified = true;
            }
            else
            {
                qDebug() << "No tags to remove for entry:" << currentId;
            }
        }
    }

    if (modified)
    {
        if (file.open(QIODevice::WriteOnly))
        {
            QJsonDocument outDoc(array);
            file.write(outDoc.toJson());
            file.close();
            qDebug() << "Successfully saved updated history.json after removing tags";
            m_ignoreNextChange = false;
            return true;
        }
        else
        {
            qDebug() << "Failed to open history.json for writing";
        }
    }
    else
    {
        qDebug() << "No entries were modified (tags didn't exist or IDs didn't match)";
    }

    m_ignoreNextChange = false;
    return modified;
}

QStringList HistoryManager::getAllTags()
{
    QSet<QString> uniqueTags;

    QFile file(getJsonPath());
    if (!file.exists() || !file.open(QIODevice::ReadOnly))
    {
        return QStringList();
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isArray())
        return QStringList();

    QJsonArray array = doc.array();
    for (const auto &val : array)
    {
        if (!val.isObject())
            continue;
        QJsonObject obj = val.toObject();

        if (obj.contains("tags") && obj["tags"].isArray())
        {
            QStringList entryTags = obj["tags"].toVariant().toStringList();
            for (const QString &tag : entryTags)
            {
                uniqueTags.insert(tag);
            }
        }
    }

    return uniqueTags.values();
}
