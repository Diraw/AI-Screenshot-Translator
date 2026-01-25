#include "HistoryManager.h"

#include <QMap>

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
