#include "HistoryManager.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QCoreApplication>
#include <QFileInfo>
#include <QDateTime>
#include <QImage>
#include <QBuffer>
#include <QDebug>

HistoryManager::HistoryManager(QObject *parent) : QObject(parent)
{
    m_watcher = new QFileSystemWatcher(this);
    connect(m_watcher, &QFileSystemWatcher::fileChanged, this, &HistoryManager::onFileChanged);

    // Default
    setStoragePath("");
}

QString HistoryManager::normalizeMarkdown(const QString &raw)
{
    QString md = raw.trimmed();
    if (md.startsWith('['))
    {
        QJsonParseError err;
        auto doc = QJsonDocument::fromJson(md.toUtf8(), &err);
        if (err.error == QJsonParseError::NoError && doc.isArray())
        {
            QStringList parts;
            for (auto v : doc.array())
            {
                if (v.isString())
                    parts << v.toString();
            }
            if (!parts.isEmpty())
                return parts.join("\n");
        }
    }
    return md;
}

QString HistoryManager::getJsonPath() const
{
    return m_basePath + "/history.json";
}

void HistoryManager::setStoragePath(const QString &path)
{
    const QString previousJson = m_watchedJsonPath;

    if (path.isEmpty())
    {
        m_basePath = QCoreApplication::applicationDirPath() + "/storage";
    }
    else
    {
        QFileInfo info(path);
        if (info.isRelative())
        {
            m_basePath = QCoreApplication::applicationDirPath() + "/" + path;
        }
        else
        {
            m_basePath = path;
        }
    }

    ensureJsonFileExists();

    QString jsonPath = getJsonPath();
    if (m_watcher)
    {
        if (!previousJson.isEmpty() && previousJson != jsonPath && m_watcher->files().contains(previousJson))
        {
            m_watcher->removePath(previousJson);
        }
        if (!m_watcher->files().contains(jsonPath))
        {
            m_watcher->addPath(jsonPath);
        }
    }
    m_watchedJsonPath = jsonPath;

    // Reset caches to match the new storage location
    m_entryCache.clear();
    m_markdownCache.clear();
    loadEntries();
}

QString HistoryManager::getStoragePath() const
{
    return m_basePath;
}

QString HistoryManager::getImagesPath() const
{
    return m_basePath + "/images";
}

void HistoryManager::ensureDirectories() const
{
    QDir dir(m_basePath);
    if (!dir.exists())
        dir.mkpath(".");

    QDir imgDir(getImagesPath());
    if (!imgDir.exists())
        imgDir.mkpath(".");
}

void HistoryManager::ensureJsonFileExists() const
{
    ensureDirectories();
    QFile file(getJsonPath());
    if (!file.exists())
    {
        if (file.open(QIODevice::WriteOnly))
        {
            QJsonArray arr;
            QJsonDocument doc(arr);
            file.write(doc.toJson());
            file.close();
        }
    }
}

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
    // Ensure uniqueness if multiple in same millisecond? Unlikely but safe to append ID part or random
    // But user wants to sort by name to sort by time.
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
    obj["image_file"] = "images/" + imgFilename; // Relative to history.json
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
    QJsonArray validArray; // Rebuild array with only valid entries
    bool modified = false;

    // Clear old cache keys that might be gone? Or just update.
    // m_markdownCache.clear(); // Safe but maybe inefficient.

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
            // User Request: "If I delete the picture, the json data is also deleted"
            modified = true;
            continue; // Skip this entry (effectively deleting it from loaded list)
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

            // Update caches (must include image/base64 for screenshot toggling)
            m_markdownCache[entry.id] = entry.translatedMarkdown;
            m_entryCache[entry.id] = entry;
        }
        else
        {
            // Should not happen due to check above, but if load fails:
            continue;
        }

        entries.append(entry);
        validArray.append(obj); // Keep compliant logic
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

    return TranslationEntry(); // Return empty entry if not found
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

void HistoryManager::onFileChanged(const QString &path)
{
    if (m_ignoreNextChange)
        return;
    Q_UNUSED(path);

    QMap<QString, QString> previousMarkdown = m_markdownCache;
    loadEntries(); // Rebuild caches from disk

    for (auto it = m_markdownCache.constBegin(); it != m_markdownCache.constEnd(); ++it)
    {
        const QString id = it.key();
        const QString normMd = it.value();
        if (!previousMarkdown.contains(id) || previousMarkdown.value(id) != normMd)
        {
            emit entryMarkdownChanged(id, normMd);
        }
    }

    // Re-arm watcher if the file was recreated by external edits
    if (m_watcher && !m_watchedJsonPath.isEmpty() && !m_watcher->files().contains(m_watchedJsonPath))
    {
        m_watcher->addPath(m_watchedJsonPath);
    }

    emit historyFileChanged();
}
