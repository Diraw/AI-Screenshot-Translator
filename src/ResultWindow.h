#ifndef RESULTWINDOW_H
#define RESULTWINDOW_H

#include <QWidget>
#include <QString>
#include <QStringList>
#include <memory>
#include <QJsonArray>
#include <QToolButton>
#include <QAction>
#include <QLabel>
#include <QList>
#include <QElapsedTimer>
#include <QWidget>
#include "TranslationManager.h" // For TranslationEntry struct if defined there, otherwise minimal struct needed here or forward decl
// Actually TranslationEntry is in HistoryManager.h usually, let's check or define a local struct/use shared.
// Based on App.cpp, TranslationManager seems to handle strings. HistoryManager handles persistence.
// TranslationEntry is used in App.cpp line 73. It's likely in HistoryManager.h or a common header.
// Let's assume it's available or we can use a struct here.
// Checking App.cpp includes: HistoryManager.h.
#include "HistoryManager.h"
#include "ConfigManager.h"

class EmbedWebView;

class ResultWindow : public QWidget
{
    Q_OBJECT
public:
    explicit ResultWindow(QWidget *parent = nullptr);
    ~ResultWindow();

    void setContent(const QString &markdown, const QString &originalBase64, const QString &prompt, const QString &entryId = "");
    void externalContentUpdate(const QString &markdown);

    // Locked Window Methods
    bool isLocked() const { return m_isLocked; }
    void addEntry(const TranslationEntry &entry);
    void updateNavigation();

    QString entryId() const { return m_entryId; }
    void showLoading();
    void showError(const QString &msg);

    void configureHotkeys(const QString &viewToggle, const QString &editToggle,
                          const QString &screenshotToggle, const QString &boldKey,
                          const QString &underlineKey, const QString &highlightKey,
                          const QString &prevKey, const QString &nextKey,
                          const QString &tagKey);

    void setHistoryManager(class HistoryManager *historyManager);

    void setConfig(const AppConfig &config);
    void focusEditor(); // Restore focus to WebView
    void updateTheme(bool isDark);

signals:
    void closed();
    void retranslateRequested();
    void screenshotRequested(const QString &entryId, const QString &base64);  // New signal
    void contentUpdated(const QString &newMarkdown);                          // Legacy Signal
    void contentUpdatedWithId(const QString &id, const QString &newMarkdown); // Signal for persistence with ID
    void tagsUpdated(const QString &id, const QStringList &tags);             // Signal for tag updates

protected:
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    bool event(QEvent *event) override;
public slots:
    void triggerScreenshotFromNative();
private slots:
    void toggleLock();
    void showPrevious();
    void showNext();
    void openTagDialog();

private:
    void updateToolbarBalance();
    void updateToolbarResponsive();
    void applyStatusLabelText();
    std::unique_ptr<EmbedWebView> m_webView;
    QWidget *m_webContainer;

    // UI Elements
    // UI Elements
    QWidget *m_toolBar = nullptr;
    QAction *m_lockAction = nullptr;
    QAction *m_prevAction = nullptr;
    QAction *m_nextAction = nullptr;
    QToolButton *m_lockBtn = nullptr;
    QToolButton *m_prevBtn = nullptr;
    QToolButton *m_nextBtn = nullptr;
    QWidget *m_leftBlock = nullptr;
    QWidget *m_centerBlock = nullptr;
    QWidget *m_rightBlock = nullptr;
    QWidget *m_pagingGroup = nullptr;
    QLabel *m_pageLabel = nullptr;
    QLabel *m_statusLabel = nullptr; // New status label

    int m_pageMinW = 0;
    int m_pageCompactW = 0;
    int m_pageMaxW = 0;
    int m_statusMinW = 0;
    int m_statusMaxW = 0;
    QString m_statusMode; // e.g. "view", "edit", "raw"

    // State
    bool m_isLocked = false;
    QList<TranslationEntry> m_history;
    int m_currentIndex = -1;

    QString m_originalBase64;
    QString m_originalPrompt;
    QString m_entryId;
    AppConfig m_config;

    // Hotkey configuration
    // State for async retrieval
    QString m_currentMarkdown;
    QString m_currentProtectedText;
    QJsonArray m_currentMathBlocks;

    QString m_viewToggleKey;
    QString m_editToggleKey;
    QString m_screenshotToggleKey;
    QString m_boldKey;
    QString m_underlineKey;
    QString m_highlightKey;
    QString m_prevKey;
    QString m_nextKey;
    QString m_tagKey;

    // Tag management
    class HistoryManager *m_historyManager = nullptr;
    QStringList m_currentTags;

    QString loadTemplate();

    struct ProtectedContent
    {
        QString text;
        QStringList mathBlocks;
    };

    ProtectedContent protectMath(const QString &markdown);

    // New Members
    QList<QShortcut *> m_navShortcuts;
    void updateShortcuts();
    bool m_isFirstLoad = true;
    bool m_focusPending = false;
    QElapsedTimer m_lastFocus;
    void requestFocusToWeb(bool allowActivate = false);
    bool m_htmlLoaded = false;
};

#endif // RESULTWINDOW_H
