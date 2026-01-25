#include "HistoryManager.h"

#include <QBuffer>
#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

void HistoryManager::saveEntry(const TranslationEntry &entry)
{
    m_ignoreNextChange = true;
    ensureDirectories();
    ensureJsonFileExists();

    // 1. Save Image
    // Entry has originalBase64. Convert to Image and save.
    QByteArray bytes = QByteArray::fromBase64(entry.originalBase64.toLatin1());
    QImage img;
    img.loadFromData(bytes);

    // User Request: Use timestamp for filename to allow sorting/deletion
    QString timeStr = entry.timestamp.toString("yyyyMMdd_HHmmss_zzz");
    QString imgFilename = timeStr + ".png";
    QString fullImgPath = getImagesPath() + "/" + imgFilename;

    // Handle potential collision (extremely rare)
    if (QFile::exists(fullImgPath))
    {
        imgFilename = timeStr + "_" + entry.id.left(4) + ".png";
        fullImgPath = getImagesPath() + "/" + imgFilename;
    }

    img.save(fullImgPath, "PNG");

    // 2. Load existing JSON
    QFile file(getJsonPath());
    QJsonArray array;
    if (file.exists() && file.open(QIODevice::ReadOnly))
    {
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        if (doc.isArray())
            array = doc.array();
        file.close();
    }

    // 3. Append new entry
    QJsonObject obj;
    obj["id"] = entry.id;
    obj["timestamp"] = entry.timestamp.toString(Qt::ISODate);
    obj["image_file"] = "images/" + imgFilename;
    obj["prompt"] = entry.prompt;
    obj["markdown"] = entry.translatedMarkdown;
    obj["tags"] = QJsonArray::fromStringList(entry.tags);

    array.append(obj);

    // 4. Write back
    if (file.open(QIODevice::WriteOnly))
    {
        QJsonDocument doc(array);
        file.write(doc.toJson());
        file.close();

        // Update caches
        TranslationEntry cachedEntry = entry;
        cachedEntry.translatedMarkdown = HistoryManager::normalizeMarkdown(entry.translatedMarkdown);
        cachedEntry.localImagePath = fullImgPath;
        m_markdownCache[entry.id] = cachedEntry.translatedMarkdown;
        m_entryCache[entry.id] = cachedEntry;
    }
    m_ignoreNextChange = false;
}

QList<TranslationEntry> HistoryManager::loadEntries()
{
    QList<TranslationEntry> entries;
    ensureJsonFileExists();
    QFile file(getJsonPath());

    if (!file.exists() || !file.open(QIODevice::ReadOnly))
    {
        return entries;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isArray())
        return entries;

    m_entryCache.clear();
    m_markdownCache.clear();

    QJsonArray array = doc.array();
    QJsonArray validArray;
    bool modified = false;

    for (const auto &val : array)
    {
        if (!val.isObject())
            continue;
        QJsonObject obj = val.toObject();

        // Load Image
        QString relPath = obj["image_file"].toString();
        QString fullPath = m_basePath + "/" + relPath;

        // Validation: Check if image exists
        if (!QFile::exists(fullPath))
        {
            modified = true;
            continue;
        }

        TranslationEntry entry;
        entry.id = obj["id"].toString();

        // Parse timestamp
        entry.timestamp = QDateTime::fromString(obj["timestamp"].toString(), Qt::ISODate);
        if (!entry.timestamp.isValid())
        {
            entry.timestamp = QDateTime::currentDateTime();
        }

        entry.translatedMarkdown = HistoryManager::normalizeMarkdown(obj["markdown"].toString());
        entry.prompt = obj["prompt"].toString();

        // Load tags
        if (obj.contains("tags") && obj["tags"].isArray())
        {
            entry.tags = obj["tags"].toVariant().toStringList();
        }

        QImage img(fullPath);
        if (!img.isNull())
        {
            QByteArray ba;
            QBuffer buffer(&ba);
            buffer.open(QIODevice::WriteOnly);
            img.save(&buffer, "PNG");
            entry.originalBase64 = ba.toBase64();
            entry.localImagePath = fullPath;

            // Update caches
            m_markdownCache[entry.id] = entry.translatedMarkdown;
            m_entryCache[entry.id] = entry;
        }
        else
        {
            continue;
        }

        entries.append(entry);
        validArray.append(obj);
    }

    // If we removed items, update the JSON file on disk
    if (modified)
    {
        m_ignoreNextChange = true;
        QFile outFile(getJsonPath());
        if (outFile.open(QIODevice::WriteOnly))
        {
            QJsonDocument outDoc(validArray);
            outFile.write(outDoc.toJson());
            outFile.close();
        }
        m_ignoreNextChange = false;
    }

    return entries;
}

bool HistoryManager::deleteEntry(const QString &id)
{
    QFile file(getJsonPath());
    if (!file.exists() || !file.open(QIODevice::ReadOnly))
    {
        return false;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isArray())
        return false;

    QJsonArray array = doc.array();
    bool found = false;

    for (int i = 0; i < array.size(); ++i)
    {
        QJsonObject obj = array[i].toObject();
        if (obj["id"].toString() == id)
        {
            // Found it. Delete image file.
            QString relPath = obj["image_file"].toString();
            QString fullPath = m_basePath + "/" + relPath;
            QFile::remove(fullPath);

            // Remove from array
            array.removeAt(i);
            found = true;
            break;
        }
    }

    if (found)
    {
        // Save back to JSON
        if (file.open(QIODevice::WriteOnly))
        {
            QJsonDocument outDoc(array);
            file.write(outDoc.toJson());
            file.close();
            m_entryCache.remove(id);
            m_markdownCache.remove(id);
            return true;
        }
    }

    return false;
}

bool HistoryManager::deleteEntries(const QStringList &ids)
{
    qDebug() << "HistoryManager::deleteEntries called with IDs:" << ids;
    if (ids.isEmpty())
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

    qDebug() << "Current history array size:" << array.size();
    for (int i = array.size() - 1; i >= 0; --i)
    {
        QJsonObject obj = array[i].toObject();
        QString currentId = obj["id"].toString();
        if (idSet.contains(currentId))
        {
            qDebug() << "Matched ID for deletion:" << currentId;
            QString relPath = obj["image_file"].toString();
            QString fullPath = m_basePath + "/" + relPath;
            QFile::remove(fullPath);
            array.removeAt(i);
            m_markdownCache.remove(currentId);
            m_entryCache.remove(currentId);
            modified = true;
        }
    }

    if (modified)
    {
        if (file.open(QIODevice::WriteOnly))
        {
            QJsonDocument outDoc(array);
            file.write(outDoc.toJson());
            file.close();
            qDebug() << "Successfully saved updated history.json after deletion";
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
        qDebug() << "No IDs matched for deletion";
    }
    m_ignoreNextChange = false;
    return modified;
}

TranslationEntry HistoryManager::getEntryById(const QString &id)
{
    if (m_entryCache.contains(id))
    {
        return m_entryCache.value(id);
    }

    loadEntries();
    if (m_entryCache.contains(id))
    {
        return m_entryCache.value(id);
    }

    return TranslationEntry();
}

bool HistoryManager::updateEntryContent(const QString &id, const QString &newMarkdown)
{
    m_ignoreNextChange = true;
    QString normalized = HistoryManager::normalizeMarkdown(newMarkdown);

    QFile file(getJsonPath());
    if (!file.exists() || !file.open(QIODevice::ReadOnly))
    {
        return false;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isArray())
        return false;

    QJsonArray array = doc.array();
    bool found = false;

    for (int i = 0; i < array.size(); ++i)
    {
        QJsonObject obj = array[i].toObject();
        if (obj["id"].toString() == id)
        {
            obj["markdown"] = normalized;
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
            m_markdownCache[id] = normalized;
            if (m_entryCache.contains(id))
            {
                TranslationEntry cached = m_entryCache.value(id);
                cached.translatedMarkdown = normalized;
                m_entryCache[id] = cached;
            }
            emit entryMarkdownChanged(id, normalized);
            m_ignoreNextChange = false;
            return true;
        }
    }

    m_ignoreNextChange = false;
    return false;
}
