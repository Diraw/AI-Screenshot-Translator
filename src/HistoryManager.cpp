#include "HistoryManager.h"

HistoryManager::HistoryManager(QObject *parent) : QObject(parent)
{
    m_watcher = new QFileSystemWatcher(this);
    connect(m_watcher, &QFileSystemWatcher::fileChanged, this, &HistoryManager::onFileChanged);

    // Default
    setStoragePath("");
}
