#include "HistoryManager.h"
#include "ConfigManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>

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
    m_basePath = ConfigManager::resolveStoragePath(path);

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
