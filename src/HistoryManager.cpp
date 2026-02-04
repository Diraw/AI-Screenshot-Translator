#include "HistoryManager.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QStringList>

HistoryManager::HistoryManager(QObject *parent) : QObject(parent)
{
    m_watcher = new QFileSystemWatcher(this);
    connect(m_watcher, &QFileSystemWatcher::fileChanged, this, &HistoryManager::onFileChanged);

    // Default
    setStoragePath("");
}

// Normalize Markdown content saved to history.
// Keep raw content intact; only normalize newlines and legacy JSON-array payloads.
QString HistoryManager::normalizeMarkdown(const QString &raw)
{
    QString text = raw.trimmed();

    // Legacy: some providers returned JSON array of strings.
    if (text.startsWith('['))
    {
        QJsonParseError err;
        auto doc = QJsonDocument::fromJson(text.toUtf8(), &err);
        if (err.error == QJsonParseError::NoError && doc.isArray())
        {
            QStringList parts;
            for (const auto &v : doc.array())
            {
                if (v.isString())
                    parts << v.toString();
            }
            if (!parts.isEmpty())
                text = parts.join("\n");
        }
    }

    text.replace("\r\n", "\n");
    return text;
}