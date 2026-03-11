#ifndef SUMMARYWINDOW_H
#define SUMMARYWINDOW_H

#include <QWidget>
#include <QList>
#include "ConfigManager.h"
#include <QDateTime>
#include <memory>
#include <QToolBar>
#include <QDateEdit>
#include <QPushButton>
#include <QShortcut>
#include <QLineEdit>
#include <QLabel>

#include "TranslationEntry.h"

class EmbedWebView;
class HistoryManager;

class SummaryWindow : public QWidget
{
    Q_OBJECT
public:
    explicit SummaryWindow(QWidget *parent = nullptr);
    ~SummaryWindow();

    void setInitialHistory(const QList<TranslationEntry> &history);
    void addEntry(const TranslationEntry &entry);
    void clearEntries();
    const TranslationEntry *getEntry(const QString &id) const;

    void setConfig(const AppConfig &config);
    void updateEntryGeometry(const QString &id, const QPoint &pos, const QSize &size);
    void updateEntryContent(const QString &id, const QString &markdown);
    void updateEntry(const QString &id, const QString &markdown); // Alias for updateEntryContent

    void setZoomFactor(qreal factor);
    qreal getZoomFactor() const;
    void captureScrollPosition();
    qreal getLastScrollY() const { return m_lastScrollY; }

    void configureHotkeys(const QString &editKey, const QString &viewKey, const QString &screenshotKey,
                          const QString &boldKey, const QString &underlineKey, const QString &highlightKey);

    void setHistoryManager(HistoryManager *historyManager);
    void updateTheme(bool isDark);
    void updateLanguage();

signals:
    void restorePreviewRequested(const QString &entryId);
    void requestDeleteEntry(const QString &entryId); // New signal for physical deletion
    void entryEdited(const QString &id, const QString &content);
    void closed();

protected:
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
public:
    void saveState();

private:
    void restoreState();
    std::unique_ptr<EmbedWebView> m_webView;
    QWidget *m_webContainer;
    bool m_htmlLoaded = false;
    QList<TranslationEntry> m_entries;
    AppConfig m_config;
    qreal m_currentZoom = 1.0;
    qreal m_lastScrollY = 0.0;

    // Filter UI
    QToolBar *m_filterToolbar = nullptr;
    QWidget *m_filtersGroup = nullptr;
    QWidget *m_paginationGroup = nullptr;
    QWidget *m_actionsGroup = nullptr;
    QDateEdit *m_fromDateEdit = nullptr;
    QDateEdit *m_toDateEdit = nullptr;
    QPushButton *m_tagFilterBtn = nullptr;
    QPushButton *m_clearFilterBtn = nullptr;
    QPushButton *m_prevPageBtn = nullptr;
    QPushButton *m_nextPageBtn = nullptr;
    QLineEdit *m_searchEdit = nullptr; // Search by translation content
    QLabel *m_fromLabel = nullptr;
    QLabel *m_toLabel = nullptr;
    QLabel *m_searchLabel = nullptr;
    QLabel *m_pageInfoLabel = nullptr;
    QStringList m_selectedTags;
    HistoryManager *m_historyManager = nullptr;
    bool m_archiveUsePagination = false;
    int m_archivePageSize = 50;
    int m_currentPage = 1;
    int m_totalPages = 1;
    int m_filteredEntryCount = 0;

    void setupFilterUI();
    void applyFilters();
    QList<TranslationEntry> getFilteredEntries() const;
    QList<TranslationEntry> applyPagination(const QList<TranslationEntry> &entries);
    void loadAvailableTags();
    void updatePaginationUi();

    void refreshHtml(bool preserveScroll = true);

    QString m_editKey = "e";
    QString m_viewKey = "r";
    QString m_screenshotKey = "s";
    QString m_selectionToggleKey = "ctrl+s";

    QString m_boldKey = "ctrl+b";
    QString m_underlineKey = "ctrl+u";
    QString m_highlightKey = "ctrl+h";
    QList<QShortcut *> m_shortcuts;

    QShortcut *m_selectionToggleShortcut = nullptr;

    void initHtml();

    void appendEntryHtml(const TranslationEntry &entry);
    QString getAddEntryJs(const TranslationEntry &entry);

    // Batch UI
    QPushButton *m_selectionModeBtn = nullptr;
    QPushButton *m_selectAllBtn = nullptr; // New Select All button
    QPushButton *m_batchDeleteBtn = nullptr;
    QPushButton *m_batchAddTagBtn = nullptr;
    QPushButton *m_batchRemoveTagBtn = nullptr;
    QAction *m_batchDeleteAction = nullptr;
    QAction *m_batchSelectAllAction = nullptr; // Action for toolbar insertion
    QAction *m_batchAddTagAction = nullptr;
    QAction *m_batchRemoveTagAction = nullptr;
    bool m_selectionMode = false;
    bool m_allSelected = false;

private slots:
    void toggleSelectionMode();
    void onBatchSelectAll();
    void onBatchDelete();
    void onBatchAddTags();
    void onBatchRemoveTags();
    // void onCustomContextMenu(const QPoint &pos); // Native context menu handling might differ
};

#endif // SUMMARYWINDOW_H
