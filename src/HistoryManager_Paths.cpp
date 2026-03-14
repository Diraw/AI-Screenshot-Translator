#include "HistoryManager.h"
#include "ConfigManager.h"

#include <QDateTime>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLibraryInfo>
#include <QScopedValueRollback>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace
{
QStringList parseStringListJson(const QString &json)
{
    if (json.trimmed().isEmpty())
        return QStringList();

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isArray())
        return QStringList();

    QStringList out;
    for (const QJsonValue &v : doc.array())
    {
        if (v.isString())
            out.append(v.toString());
    }
    return out;
}

QStringList parseTagsJsonString(const QString &tagsJson)
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
} // namespace

QString HistoryManager::getJsonPath() const
{
    return m_basePath + "/history.json";
}

QString HistoryManager::getDbPath() const
{
    return m_basePath + "/history.db";
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

void HistoryManager::closeDatabase()
{
    if (!m_db.isValid())
        return;

    const QString connectionName = m_db.connectionName();
    if (m_db.isOpen())
        m_db.close();
    m_db = QSqlDatabase();

    if (QSqlDatabase::contains(connectionName))
        QSqlDatabase::removeDatabase(connectionName);
}

bool HistoryManager::ensureDatabaseReady()
{
    ensureDirectories();

    const QString pluginsPath = QLibraryInfo::path(QLibraryInfo::PluginsPath);
    if (!pluginsPath.isEmpty() && !QCoreApplication::libraryPaths().contains(pluginsPath))
    {
        QCoreApplication::addLibraryPath(pluginsPath);
    }

    if (!QSqlDatabase::isDriverAvailable("QSQLITE"))
    {
        qWarning() << "QSQLITE driver unavailable. libraryPaths=" << QCoreApplication::libraryPaths()
                   << "drivers=" << QSqlDatabase::drivers();
        return false;
    }

    const QString dbPath = getDbPath();
    if (!m_db.isValid())
    {
        if (QSqlDatabase::contains(m_dbConnectionName))
            m_db = QSqlDatabase::database(m_dbConnectionName);
        else
            m_db = QSqlDatabase::addDatabase("QSQLITE", m_dbConnectionName);
    }

    if (m_db.databaseName() != dbPath)
        m_db.setDatabaseName(dbPath);

    if (!m_db.isOpen() && !m_db.open())
    {
        qWarning() << "HistoryManager failed to open sqlite database:" << dbPath
                   << "error:" << m_db.lastError().text()
                   << "libraryPaths=" << QCoreApplication::libraryPaths()
                   << "drivers=" << QSqlDatabase::drivers();
        return false;
    }

    QSqlQuery query(m_db);
    if (!query.exec(
            "CREATE TABLE IF NOT EXISTS entries ("
            "id TEXT PRIMARY KEY,"
            "timestamp TEXT NOT NULL,"
            "image_file TEXT NOT NULL,"
            "image_files_json TEXT NOT NULL DEFAULT '[]',"
            "prompt TEXT,"
            "markdown TEXT,"
            "tags_json TEXT NOT NULL DEFAULT '[]'"
            ")"))
    {
        qWarning() << "HistoryManager failed to create entries table:" << query.lastError().text();
        return false;
    }

    QStringList columns;
    if (query.exec("PRAGMA table_info(entries)"))
    {
        while (query.next())
            columns << query.value(1).toString();
    }
    if (!columns.contains("image_files_json"))
    {
        if (!query.exec("ALTER TABLE entries ADD COLUMN image_files_json TEXT NOT NULL DEFAULT '[]'"))
        {
            qWarning() << "HistoryManager failed to add image_files_json column:" << query.lastError().text();
            return false;
        }
    }

    if (!query.exec("CREATE INDEX IF NOT EXISTS idx_entries_timestamp ON entries(timestamp DESC)"))
    {
        qWarning() << "HistoryManager failed to create timestamp index:" << query.lastError().text();
        return false;
    }

    return true;
}

bool HistoryManager::importLegacyJsonInternal(const QString &jsonPath, int *importedCount, QString *errorMessage)
{
    if (importedCount)
        *importedCount = 0;
    if (errorMessage)
        *errorMessage = QString();

    if (!ensureDatabaseReady())
    {
        if (errorMessage)
            *errorMessage = QString("Failed to open SQLite database.\nDriver available: %1\nDrivers: %2\nStorage path: %3\nDB path: %4")
                                .arg(QSqlDatabase::isDriverAvailable("QSQLITE") ? "yes" : "no")
                                .arg(QSqlDatabase::drivers().join(", "))
                                .arg(QDir::toNativeSeparators(m_basePath))
                                .arg(QDir::toNativeSeparators(getDbPath()));
        return false;
    }

    QFile file(jsonPath);
    if (!file.exists())
    {
        if (errorMessage)
            *errorMessage = QString("JSON file does not exist: %1").arg(QDir::toNativeSeparators(jsonPath));
        return false;
    }

    if (!file.open(QIODevice::ReadOnly))
    {
        if (errorMessage)
            *errorMessage = QString("Failed to open JSON file: %1").arg(QDir::toNativeSeparators(jsonPath));
        return false;
    }

    const QByteArray jsonBytes = file.readAll();
    file.close();

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(jsonBytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isArray())
    {
        if (errorMessage)
            *errorMessage = QString("Invalid JSON format: %1").arg(parseError.errorString());
        return false;
    }

    const QJsonArray array = doc.array();
    if (array.isEmpty())
        return true;

    const QString sourceDir = QFileInfo(jsonPath).absolutePath();

    QSqlQuery tx(m_db);
    if (!tx.exec("BEGIN IMMEDIATE TRANSACTION"))
    {
        if (errorMessage)
            *errorMessage = QString("Failed to begin SQLite transaction: %1").arg(tx.lastError().text());
        return false;
    }

    QSqlQuery upsert(m_db);
    upsert.prepare(
        "INSERT OR REPLACE INTO entries (id, timestamp, image_file, image_files_json, prompt, markdown, tags_json) "
        "VALUES (?, ?, ?, ?, ?, ?, ?)");

    int inserted = 0;
    for (const QJsonValue &value : array)
    {
        if (!value.isObject())
            continue;

        const QJsonObject obj = value.toObject();
        const QString id = obj.value("id").toString().trimmed();
        if (id.isEmpty())
            continue;

        QDateTime timestamp = QDateTime::fromString(obj.value("timestamp").toString(), Qt::ISODate);
        if (!timestamp.isValid())
            timestamp = QDateTime::currentDateTime();

        QStringList sourceImagePaths;
        if (obj.contains("image_files") && obj.value("image_files").isArray())
        {
            const QJsonArray imageFiles = obj.value("image_files").toArray();
            for (const QJsonValue &imageFileValue : imageFiles)
            {
                if (imageFileValue.isString())
                    sourceImagePaths << imageFileValue.toString().trimmed();
            }
        }
        if (sourceImagePaths.isEmpty())
            sourceImagePaths << obj.value("image_file").toString().trimmed();
        sourceImagePaths.removeAll(QString());
        if (sourceImagePaths.isEmpty())
            continue;

        QStringList relPaths;
        bool copyFailed = false;
        for (int imageIndex = 0; imageIndex < sourceImagePaths.size(); ++imageIndex)
        {
            QString sourceImagePath = sourceImagePaths[imageIndex];
            if (!QDir::isAbsolutePath(sourceImagePath))
                sourceImagePath = QDir(sourceDir).filePath(sourceImagePath);

            const QFileInfo sourceInfo(sourceImagePath);
            if (!sourceInfo.exists() || !sourceInfo.isFile())
            {
                copyFailed = true;
                break;
            }

            QString fileName = sourceInfo.fileName();
            if (fileName.isEmpty())
                fileName = sourceImagePaths.size() == 1 ? (id + ".png") : QString("%1_%2.png").arg(id).arg(imageIndex + 1);

            QString relPath = "images/" + fileName;
            QString destPath = QDir(m_basePath).filePath(relPath);

            const QString srcNorm = QDir::toNativeSeparators(QDir::cleanPath(sourceInfo.absoluteFilePath()));
            const QString dstNorm = QDir::toNativeSeparators(QDir::cleanPath(destPath));
            const bool samePath = (srcNorm.compare(dstNorm, Qt::CaseInsensitive) == 0);
            if (!samePath)
            {
                if (QFile::exists(destPath))
                {
                    const QString base = QFileInfo(fileName).completeBaseName();
                    const QString suffix = QFileInfo(fileName).suffix();
                    QString candidate;
                    int n = 1;
                    do
                    {
                        candidate = suffix.isEmpty()
                                        ? QString("%1_%2").arg(base, QString::number(n))
                                        : QString("%1_%2.%3").arg(base, QString::number(n), suffix);
                        relPath = "images/" + candidate;
                        destPath = QDir(m_basePath).filePath(relPath);
                        ++n;
                    } while (QFile::exists(destPath));
                }

                if (!QFile::copy(sourceInfo.absoluteFilePath(), destPath))
                {
                    qWarning() << "HistoryManager migration skipped image copy failure:" << sourceInfo.absoluteFilePath();
                    copyFailed = true;
                    break;
                }
            }

            relPaths << relPath;
        }
        if (copyFailed || relPaths.isEmpty())
            continue;

        QStringList tags;
        if (obj.contains("tags") && obj.value("tags").isArray())
            tags = obj.value("tags").toVariant().toStringList();
        const QString tagsJson = QString::fromUtf8(QJsonDocument(QJsonArray::fromStringList(tags)).toJson(QJsonDocument::Compact));

        const QString markdown = HistoryManager::normalizeMarkdown(obj.value("markdown").toString());
        const QString prompt = obj.value("prompt").toString();

        upsert.bindValue(0, id);
        upsert.bindValue(1, timestamp.toString(Qt::ISODate));
        upsert.bindValue(2, relPaths.value(0));
        upsert.bindValue(3, QString::fromUtf8(QJsonDocument(QJsonArray::fromStringList(relPaths)).toJson(QJsonDocument::Compact)));
        upsert.bindValue(4, prompt);
        upsert.bindValue(5, markdown);
        upsert.bindValue(6, tagsJson);
        if (!upsert.exec())
        {
            qWarning() << "HistoryManager migration upsert failed for id" << id << ":" << upsert.lastError().text();
            continue;
        }

        ++inserted;
    }

    QSqlQuery commit(m_db);
    if (!commit.exec("COMMIT"))
    {
        QSqlQuery rollback(m_db);
        rollback.exec("ROLLBACK");
        if (errorMessage)
            *errorMessage = QString("Failed to commit SQLite transaction: %1").arg(commit.lastError().text());
        return false;
    }

    if (importedCount)
        *importedCount = inserted;
    return true;
}

bool HistoryManager::importLegacyJson(const QString &jsonPath, int *importedCount, QString *errorMessage)
{
    const QString effectiveJsonPath = jsonPath.trimmed().isEmpty() ? getJsonPath() : QDir::fromNativeSeparators(jsonPath.trimmed());
    int localImported = 0;
    int *countPtr = importedCount ? importedCount : &localImported;

    QScopedValueRollback<bool> guard(m_ignoreNextChange, true);
    bool ok = importLegacyJsonInternal(effectiveJsonPath, countPtr, errorMessage);
    if (!ok)
        return false;

    if (*countPtr > 0)
    {
        loadEntries();
        emit historyFileChanged();
    }
    return true;
}

bool HistoryManager::exportHistoryJson(const QString &jsonPath, int *exportedCount, QString *errorMessage)
{
    if (exportedCount)
        *exportedCount = 0;
    if (errorMessage)
        *errorMessage = QString();

    if (!ensureDatabaseReady())
    {
        if (errorMessage)
            *errorMessage = "Failed to open SQLite database.";
        return false;
    }

    const QString normalizedPath = QDir::fromNativeSeparators(jsonPath.trimmed());
    if (normalizedPath.isEmpty())
    {
        if (errorMessage)
            *errorMessage = "Export path is empty.";
        return false;
    }

    const QFileInfo outInfo(normalizedPath);
    const QString outDirPath = outInfo.absolutePath();
    QDir outDir(outDirPath);
    if (!outDir.exists() && !outDir.mkpath("."))
    {
        if (errorMessage)
            *errorMessage = QString("Failed to create export directory: %1").arg(QDir::toNativeSeparators(outDirPath));
        return false;
    }

    const QString imagesDirPath = outDir.filePath("images");
    QDir imagesDir(imagesDirPath);
    if (!imagesDir.exists() && !imagesDir.mkpath("."))
    {
        if (errorMessage)
            *errorMessage = QString("Failed to create export images directory: %1").arg(QDir::toNativeSeparators(imagesDirPath));
        return false;
    }

    QSqlQuery query(m_db);
    if (!query.exec("SELECT id, timestamp, image_file, image_files_json, prompt, markdown, tags_json FROM entries ORDER BY timestamp ASC"))
    {
        if (errorMessage)
            *errorMessage = QString("Failed to read history from SQLite: %1").arg(query.lastError().text());
        return false;
    }

    QJsonArray array;
    int count = 0;
    while (query.next())
    {
        const QString id = query.value(0).toString();
        const QString timestampIso = query.value(1).toString();
        const QString prompt = query.value(4).toString();
        const QString markdown = query.value(5).toString();
        const QString tagsJson = query.value(6).toString();
        const QStringList sourceImages = parseStringListJson(query.value(3).toString()).isEmpty()
                                             ? QStringList{query.value(2).toString()}
                                             : parseStringListJson(query.value(3).toString());

        QStringList exportedRelPaths;
        bool copyFailed = false;
        for (int imageIndex = 0; imageIndex < sourceImages.size(); ++imageIndex)
        {
            QString imagePath = sourceImages[imageIndex];
            if (!QDir::isAbsolutePath(imagePath))
                imagePath = QDir(m_basePath).filePath(imagePath);
            QFileInfo srcImageInfo(imagePath);
            if (!srcImageInfo.exists() || !srcImageInfo.isFile())
            {
                copyFailed = true;
                break;
            }

            QString fileName = srcImageInfo.fileName();
            if (fileName.isEmpty())
                fileName = sourceImages.size() == 1 ? (id + ".png") : QString("%1_%2.png").arg(id).arg(imageIndex + 1);

            QString outRelPath = "images/" + fileName;
            QString outImagePath = outDir.filePath(outRelPath);
            if (QFile::exists(outImagePath))
            {
                const QString srcNorm = QDir::cleanPath(srcImageInfo.absoluteFilePath());
                const QString dstNorm = QDir::cleanPath(outImagePath);
                if (srcNorm.compare(dstNorm, Qt::CaseInsensitive) != 0)
                {
                    const QString base = QFileInfo(fileName).completeBaseName();
                    const QString suffix = QFileInfo(fileName).suffix();
                    QString candidate;
                    int n = 1;
                    do
                    {
                        candidate = suffix.isEmpty()
                                        ? QString("%1_%2").arg(base, QString::number(n))
                                        : QString("%1_%2.%3").arg(base, QString::number(n), suffix);
                        outRelPath = "images/" + candidate;
                        outImagePath = outDir.filePath(outRelPath);
                        ++n;
                    } while (QFile::exists(outImagePath));
                }
            }

            const QString srcNorm = QDir::cleanPath(srcImageInfo.absoluteFilePath());
            const QString dstNorm = QDir::cleanPath(outImagePath);
            if (srcNorm.compare(dstNorm, Qt::CaseInsensitive) != 0)
            {
                if (!QFile::copy(srcImageInfo.absoluteFilePath(), outImagePath))
                {
                    qWarning() << "History export skipped image copy failure:" << srcImageInfo.absoluteFilePath();
                    copyFailed = true;
                    break;
                }
            }

            exportedRelPaths << outRelPath;
        }
        if (copyFailed || exportedRelPaths.isEmpty())
            continue;

        QJsonObject obj;
        obj["id"] = id;
        obj["timestamp"] = timestampIso;
        obj["image_file"] = exportedRelPaths.value(0);
        obj["image_files"] = QJsonArray::fromStringList(exportedRelPaths);
        obj["prompt"] = prompt;
        obj["markdown"] = markdown;
        obj["tags"] = QJsonArray::fromStringList(parseTagsJsonString(tagsJson));
        array.append(obj);
        ++count;
    }

    QFile outFile(normalizedPath);
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        if (errorMessage)
            *errorMessage = QString("Failed to open export file for write: %1").arg(QDir::toNativeSeparators(normalizedPath));
        return false;
    }
    outFile.write(QJsonDocument(array).toJson(QJsonDocument::Indented));
    outFile.close();

    if (exportedCount)
        *exportedCount = count;
    return true;
}

bool HistoryManager::maybeAutoImportLegacyJson()
{
    QFileInfo legacyJson(getJsonPath());
    if (!legacyJson.exists() || !legacyJson.isFile() || legacyJson.size() <= 2)
        return false;

    if (!ensureDatabaseReady())
        return false;

    QSqlQuery count(m_db);
    if (!count.exec("SELECT COUNT(*) FROM entries"))
        return false;
    if (count.next() && count.value(0).toLongLong() > 0)
        return false;

    int imported = 0;
    QString error;
    QScopedValueRollback<bool> guard(m_ignoreNextChange, true);
    const bool ok = importLegacyJsonInternal(legacyJson.absoluteFilePath(), &imported, &error);
    if (!ok)
    {
        qWarning() << "HistoryManager auto migration failed:" << error;
        return false;
    }

    if (imported > 0)
    {
        qInfo() << "HistoryManager auto-migrated legacy history.json entries:" << imported;
    }
    return imported > 0;
}

void HistoryManager::setStoragePath(const QString &path)
{
    const QString resolvedPath = ConfigManager::resolveStoragePath(path);
    const QString previousDbPath = m_watchedDbPath;
    const bool pathChanged = (m_basePath != resolvedPath);

    if (pathChanged)
    {
        closeDatabase();
        m_basePath = resolvedPath;
    }

    if (!ensureDatabaseReady())
        return;

    const QString dbPath = getDbPath();
    if (m_watcher)
    {
        if (!previousDbPath.isEmpty() && previousDbPath != dbPath && m_watcher->files().contains(previousDbPath))
            m_watcher->removePath(previousDbPath);
        if (!m_watcher->files().contains(dbPath))
            m_watcher->addPath(dbPath);
    }
    m_watchedDbPath = dbPath;

    if (pathChanged)
    {
        m_entryCache.clear();
        m_markdownCache.clear();
        m_allTagsCache.clear();
        m_tagsCacheDirty = true;
        maybeAutoImportLegacyJson();
    }
}

QString HistoryManager::getStoragePath() const
{
    return m_basePath;
}
