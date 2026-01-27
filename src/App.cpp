#include "App.h"
#include "ThemeUtils.h"
#include <QApplication>
#include <QIcon>
#include <QBuffer>
#include <QDebug>
#include <QTimer>
#include <QMessageBox>
#include <QUuid>
#include <QDateTime>
#include <QtConcurrent>
#include <QFuture>
#include <QFutureWatcher>
#include <QScreen>

#include "TranslationManager.h"

#include "StartupWindow.h"

#ifdef _WIN32
#include "WinKeyForwarder.h"
#endif

App::App(QObject *parent)
    : QObject(parent), m_configManager(),
      m_screenshotHotkey(100, this),
      m_summaryHotkey(101, this),
      m_settingsHotkey(102, this),
      m_quitHotkey(103, this)
{
#ifdef _WIN32
    // WebView2 often consumes WM_KEYDOWN in its child HWND, bypassing Qt shortcuts.
    // Use a low-level keyboard hook as a reliable path.
    static bool s_hookInstalled = false;
    if (!s_hookInstalled)
    {
        WinKeyForwarder::trace("[WKF] installing forwarder");
        WinKeyForwarder::instance().install();
        s_hookInstalled = true;
    }
#endif

    AppConfig cfg = m_configManager.getConfig();
    g_enableLogging = cfg.debugMode;
    TranslationManager::instance().setLanguage(cfg.language);

    // Startup window (shows on every launch)
    {
        StartupWindow w(cfg);
        w.exec();
    }

    m_preferredLockState = cfg.defaultResultWindowLocked;

    m_apiClient = new ApiClient(this);
    connect(m_apiClient, &ApiClient::success, this, &App::onApiSuccess);
    connect(m_apiClient, &ApiClient::error, this, &App::onApiError);

    m_summaryWindow = new SummaryWindow();
    m_summaryWindow->setConfig(cfg);
    m_summaryWindow->setHistoryManager(&m_historyManager);
    connect(m_summaryWindow, &SummaryWindow::restorePreviewRequested, this, [this](const QString &id)
            { restorePreview(id); });

    connect(m_summaryWindow, &SummaryWindow::requestDeleteEntry, this, [this](const QString &id)
            {
        if (m_historyManager.deleteEntry(id)) {
            qDebug() << "Deleted entry:" << id;
        } });

    // Persist edits from SummaryWindow back to history.json
    connect(m_summaryWindow, &SummaryWindow::entryEdited, this, [this](const QString &id, const QString &content)
            {
        qDebug() << "[SummaryWindow] entryEdited -> persist to history.json for id" << id;
        m_historyManager.updateEntryContent(id, content); });

    connect(&m_historyManager, &HistoryManager::entryMarkdownChanged, this, [this](const QString &id, const QString &content)
            {
        if (m_summaryWindow) {
            m_summaryWindow->updateEntryContent(id, content);
        }
        for (auto w : m_activeWindows) {
            if (auto res = qobject_cast<ResultWindow*>(w.data())) {
                if (res->entryId() == id) {
                    res->externalContentUpdate(content);
                }
            }
        } });

    // Reload summary list when history.json changes externally
    connect(&m_historyManager, &HistoryManager::historyFileChanged, this, [this]()
            {
        QList<TranslationEntry> history = m_historyManager.loadEntries();
        if (m_summaryWindow) {
            m_summaryWindow->setInitialHistory(history);
            // Reload tags list for filters/batch tag ops
            m_summaryWindow->setHistoryManager(&m_historyManager);
        } });

    setupTray();
    m_historyManager.setStoragePath(cfg.storagePath);

    QList<TranslationEntry> history = m_historyManager.loadEntries();
    m_summaryWindow->setInitialHistory(history);

    // Initialize summary hotkeys (view/edit/screenshot/bold/underline/highlight)
    m_summaryWindow->configureHotkeys(cfg.editHotkey, cfg.viewToggleHotkey, cfg.screenshotToggleHotkey,
                                      cfg.boldHotkey, cfg.underlineHotkey, cfg.highlightHotkey);

    reloadHotkeys();

    connect(&m_screenshotHotkey, &GlobalHotkey::activated, this, &App::onScreenshotRequested);
    connect(&m_summaryHotkey, &GlobalHotkey::activated, this, &App::showSummary);
    connect(&m_settingsHotkey, &GlobalHotkey::activated, this, &App::showConfig);
    connect(&m_quitHotkey, &GlobalHotkey::activated, this, &App::quitApp);

    if (m_configManager.getConfig().apiKey.isEmpty())
    {
        m_forceConfigDialogForegroundOnce = true;
        QTimer::singleShot(100, this, &App::showConfig);
    }

    // Theme Init
    m_lastTopBarDark = ThemeUtils::isSystemDark();
    connect(&m_themeTimer, &QTimer::timeout, this, &App::checkForThemeChange);
    m_themeTimer.start(2000);

    // Analytics (Umami): start 5s after launch to avoid startup stalls
    m_analytics = new AnalyticsManager(this);
    m_analytics->startDelayed(5000);
    connect(qApp, &QCoreApplication::aboutToQuit, this, [this]()
            {
        if (m_analytics)
            m_analytics->stop(); });
}

App::~App()
{
#ifdef _WIN32
    WinKeyForwarder::trace("[WKF] uninstall forwarder");
    WinKeyForwarder::instance().uninstall();
#endif
    if (m_summaryWindow)
        delete m_summaryWindow;
}
