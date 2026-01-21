#ifndef HISTORYMANAGER_H
#define HISTORYMANAGER_H

#include <QObject>
#include <QString>
#include <QList>
#include <QFileSystemWatcher>
#include <QMap>
#include "TranslationEntry.h"

class HistoryManager : public QObject {
    Q_OBJECT
public:
    HistoryManager(QObject* parent = nullptr);
    
    static QString normalizeMarkdown(const QString& raw);

    // Set the base storage path. Handles relative/absolute paths.
    void setStoragePath(const QString& path);
    QString getStoragePath() const;

    // Save a new entry to disk (image + metadata)
    void saveEntry(const TranslationEntry& entry);
    
    // Load all valid entries from disk
    QList<TranslationEntry> loadEntries(); // Non-const now because it updates internal cache

    // Delete an entry from disk
    bool deleteEntry(const QString& id);
    bool deleteEntries(const QStringList& ids);

    // Update the content of an entry
    bool updateEntryContent(const QString& id, const QString& newMarkdown);

    // Update the tags of an entry
    bool updateEntryTags(const QString& id, const QStringList& tags);
    bool addTagsToEntries(const QStringList& ids, const QStringList& tags);
    bool removeTagsFromEntries(const QStringList& ids, const QStringList& tags);

    // Get all unique tags across all entries
    QStringList getAllTags();
    
    // Get a single entry by ID (helper for App.cpp)
    TranslationEntry getEntryById(const QString& id);

signals:
    void entryMarkdownChanged(const QString& id, const QString& newMarkdown);

private slots:
    void onFileChanged(const QString& path);

private:
    QString m_basePath;
    QFileSystemWatcher* m_watcher;
    QMap<QString, QString> m_markdownCache;
    bool m_ignoreNextChange = false;
    
    QString getImagesPath() const;
    QString getJsonPath() const;
    void ensureDirectories() const;
};

#endif // HISTORYMANAGER_H
