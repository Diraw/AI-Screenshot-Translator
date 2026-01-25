#include "HistoryManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>

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
