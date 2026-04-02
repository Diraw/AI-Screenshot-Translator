#ifndef APP_H
#define APP_H

#include <QObject>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QList>
#include <QPointer>
#include <QTimer>
#include <QPixmap>
#include <QStringList>

extern bool g_enableLogging;

#include "ConfigManager.h"
#include "GlobalHotkey.h"
#include "ApiClient.h"
#include "ScreenshotTool.h"
#include "PreviewCard.h"
#include "ResultWindow.h"
#include "ConfigDialog.h"
#include "SummaryWindow.h"
#include "HistoryManager.h"
#include "AnalyticsManager.h"

class App : public QObject
{
    Q_OBJECT
public:
    explicit App(QObject *parent = nullptr);
    ~App();

private slots:
    void onScreenshotRequested();
    void onScreenshotCaptured(const QPixmap &pixmap, const QRect &rect, bool batchMode, bool finalizeBatch);
    void onBatchFinalizeRequested();
    void onScreenshotCancelled(bool clearPendingBatch);

    void onResultWindowScreenshotRequested(const QString &entryId, const QString &base64);
    void onRetranslateRequested(const QStringList &base64Images);

    void onApiSuccess(const QString &text, const QString &originalBase64, const QString &originalPrompt,
                      const QString &requestId, qint64 elapsedMs);
    void onApiError(const QString &errorMsg, const QString &requestId, qint64 elapsedMs);

    void showConfig();
    void showSummary();
    void restorePreview(const QString &entryId);
    void onLanguageChanged(const QString &lang);

    // Theme Management
    void checkForThemeChange();
    void checkHotkeyRegistrationHealth();
    void updateAllWindowThemes(bool isDark);

public slots:
    void quitApp();

private:
    ConfigManager m_configManager;
    HistoryManager m_historyManager;
    GlobalHotkey m_screenshotHotkey;
    GlobalHotkey m_summaryHotkey;
    GlobalHotkey m_settingsHotkey;
    GlobalHotkey m_quitHotkey;
    ApiClient *m_apiClient;
    SummaryWindow *m_summaryWindow;
    QPointer<ConfigDialog> m_activeConfigDialog; // Track active settings dialog for toggle // New member

    QSystemTrayIcon *m_trayIcon = nullptr;
    QMenu *m_trayMenu = nullptr;

    // Windows tray quirk: right-click/context menu can sometimes emit Trigger.
    // Use these flags to avoid starting screenshot selection while the menu is opening/visible.
    bool m_trayMenuOpen = false;
    bool m_pendingTrayScreenshot = false;

    // Track active windows
    QList<QPointer<QWidget>> m_activeWindows;
    QMap<QString, QPointer<PreviewCard>> m_previewCards; // ID -> Card (Restored for history)
    QMap<QString, QRect> m_previewGeometries;            // ID -> Last known geometry
    QMap<QString, QList<QPixmap>> m_previewImageCache;   // ID -> Recently decoded preview images
    QMap<QString, QPointer<QTimer>> m_previewReleaseTimers; // ID -> Delayed release timers
    QPointer<PreviewCard> m_activePreviewCard;           // The main/initial screenshot card
    QRect m_lastPreviewGeometry;                         // Persistence for screenshot card

    QPointer<ScreenshotTool> m_activeScreenshotTool;

    struct PendingBatchCapture
    {
        QPixmap pixmap;
        QRect rect;
    };
    QList<PendingBatchCapture> m_pendingBatchCaptures;
    QList<QPixmap> m_lastSubmittedCapturePixmaps;

    void setupTray();
    void notifyHotkeyConflicts(const QString &message, bool interactive);
    void presentConfigDialog(bool allowToggle, bool focusGlobalHotkeys, bool forceForeground);
    QString reloadHotkeys();
    QString syncLaunchAtStartup(bool enabled);
    void showResult(const QString &entryId);
    QString updateConfig(const AppConfig &cfg);
    void trackActiveWindow(QWidget *window);
    void pruneActiveWindows();
    void schedulePreviewImageRelease(const QString &entryId);
    void cancelPreviewImageRelease(const QString &entryId);
    void clearPendingBatchCaptures();
    void submitCapturedImages(const QList<PendingBatchCapture> &captures);
    // New hotkey configuration

    // Dynamic State
    bool m_preferredLockState = true;
    bool m_forceConfigDialogForegroundOnce = false;

    QTimer m_themeTimer;
    QTimer m_hotkeyHealthTimer;
    bool m_lastTopBarDark = false;
    QString m_lastHotkeyConflictMessage;
    QStringList m_lastConflictingGlobalHotkeyKeys;

    AnalyticsManager *m_analytics = nullptr;
};

#endif // APP_H
