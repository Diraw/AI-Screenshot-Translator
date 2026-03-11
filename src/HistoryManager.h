#ifndef HISTORYMANAGER_H
#define HISTORYMANAGER_H

#include <QObject>
#include <QString>
#include <QList>
#include <QFileSystemWatcher>
#include <QMap>
#include <QSqlDatabase>
#include "TranslationEntry.h"

class HistoryManager : public QObject
{
    Q_OBJECT
public:
    HistoryManager(QObject *parent = nullptr);

    static QString normalizeMarkdown(const QString &raw);

    // Set the base storage path. Handles relative/absolute paths.
    void setStoragePath(const QString &path);
    QString getStoragePath() const;

    // Save a new entry to disk (image + metadata)
    void saveEntry(const TranslationEntry &entry);

    // Load all valid entries from disk
    QList<TranslationEntry> loadEntries(); // Non-const now because it updates internal cache

    // Delete an entry from disk
    bool deleteEntry(const QString &id);
    bool deleteEntries(const QStringList &ids);

    // Update the content of an entry
    bool updateEntryContent(const QString &id, const QString &newMarkdown);

    // Update the tags of an entry
    bool updateEntryTags(const QString &id, const QStringList &tags);
    bool addTagsToEntries(const QStringList &ids, const QStringList &tags);
    bool removeTagsFromEntries(const QStringList &ids, const QStringList &tags);

    // Get all unique tags across all entries
    QStringList getAllTags();

    // Get a single entry by ID (helper for App.cpp)
    TranslationEntry getEntryById(const QString &id);

    // Import legacy history.json into SQLite storage.
    // If jsonPath is empty, defaults to "<storage>/history.json".
    bool importLegacyJson(const QString &jsonPath = QString(), int *importedCount = nullptr, QString *errorMessage = nullptr);
    bool exportHistoryJson(const QString &jsonPath, int *exportedCount = nullptr, QString *errorMessage = nullptr);

signals:
    void entryMarkdownChanged(const QString &id, const QString &newMarkdown);
    void historyFileChanged();

private slots:
    void onFileChanged(const QString &path);

private:
    QString m_basePath;
    QFileSystemWatcher *m_watcher;
    QString m_watchedDbPath;
    QString m_dbConnectionName;
    QSqlDatabase m_db;
    QMap<QString, QString> m_markdownCache;
    QMap<QString, TranslationEntry> m_entryCache;
    bool m_ignoreNextChange = false;

    QString getImagesPath() const;
    QString getJsonPath() const;
    QString getDbPath() const;
    void ensureDirectories() const;
    bool ensureDatabaseReady();
    void closeDatabase();
    bool maybeAutoImportLegacyJson();
    bool importLegacyJsonInternal(const QString &jsonPath, int *importedCount, QString *errorMessage);
};

#endif // HISTORYMANAGER_H
