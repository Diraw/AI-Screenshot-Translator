#include "App.h"

#include <QDialog>
#include <QTimer>

#include "TranslationManager.h"

QString App::reloadHotkeys()
{
    AppConfig cfg = m_configManager.getConfig();
    m_screenshotHotkey.registerHotkey(cfg.screenshotHotkey);
    m_summaryHotkey.registerHotkey(cfg.summaryHotkey);
    m_settingsHotkey.registerHotkey(cfg.settingsHotkey);

    if (cfg.enableQuitHotkey && !cfg.quitHotkey.trimmed().isEmpty())
    {
        m_quitHotkey.registerHotkey(cfg.quitHotkey);
    }
    else
    {
        m_quitHotkey.unregisterHotkey();
    }
    return ""; // Return empty string on success
}

QString App::updateConfig(const AppConfig &cfg)
{
    QString errorMsg = "";

    m_historyManager.setStoragePath(cfg.storagePath);
    g_enableLogging = cfg.debugMode;
    TranslationManager::instance().setLanguage(cfg.language);
    setupTray();

    // Apply lock-related preferences immediately for future ResultWindow creation.
    m_preferredLockState = cfg.defaultResultWindowLocked;

    reloadHotkeys();

    if (m_summaryWindow)
    {
        m_summaryWindow->setConfig(cfg);
        m_summaryWindow->configureHotkeys(cfg.editHotkey, cfg.viewToggleHotkey, cfg.screenshotToggleHotkey,
                                          cfg.boldHotkey, cfg.underlineHotkey, cfg.highlightHotkey);
    }

    // Sync to active ResultWindows so changes (e.g. <mark> highlight colors) apply immediately.
    for (auto w : m_activeWindows)
    {
        if (!w)
            continue;

        if (ResultWindow *rw = qobject_cast<ResultWindow *>(w.data()))
        {
            AppConfig cfgForWindow = cfg;
            cfgForWindow.defaultResultWindowLocked = rw->isLocked();
            rw->setConfig(cfgForWindow);
        }
    }

    // Sync to active PreviewCards
    for (auto w : m_activeWindows)
    {
        if (!w)
            continue;
        PreviewCard *pc = qobject_cast<PreviewCard *>(w.data());
        if (pc)
        {
            pc->setZoomSensitivity(cfg.zoomSensitivity);
            pc->setBorderColor(cfg.cardBorderColor);
            pc->setUseBorder(cfg.useCardBorder);
        }
    }

    return errorMsg;
}

void App::showConfig()
{
    auto forceToFront = [](QWidget *w)
    {
        if (!w)
            return;
        w->show();
        w->setWindowState((w->windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
        w->raise();
        w->activateWindow();

        // Some Windows configurations ignore immediate activateWindow() due to focus stealing prevention.
        // A queued retry usually succeeds.
        QTimer::singleShot(0, w, [w]()
                           {
            if (!w) return;
            w->raise();
            w->activateWindow(); });
        QTimer::singleShot(150, w, [w]()
                           {
            if (!w) return;
            w->raise();
            w->activateWindow(); });
    };

    if (m_activeConfigDialog)
    {
        // Toggle: if visible, hide; if hidden, show
        if (m_activeConfigDialog->isVisible())
        {
            if (m_activeConfigDialog->isActiveWindow())
            {
                // Save geometry before hiding
                AppConfig cfg = m_configManager.getConfig();
                cfg.configWindowGeometry = m_activeConfigDialog->saveGeometry();
                m_configManager.setConfig(cfg);
                m_configManager.saveConfig();
                m_activeConfigDialog->hide();
            }
            else
            {
                m_activeConfigDialog->setWindowState((m_activeConfigDialog->windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
                m_activeConfigDialog->raise();
                m_activeConfigDialog->activateWindow();
            }
        }
        else
        {
            // Fix white flash: Disable updates during show/restore
            m_activeConfigDialog->setUpdatesEnabled(false);

            // Restore geometry before showing to prevent flicker
            AppConfig cfg = m_configManager.getConfig();
            if (!cfg.configWindowGeometry.isEmpty())
            {
                m_activeConfigDialog->restoreGeometry(cfg.configWindowGeometry);
            }
            forceToFront(m_activeConfigDialog);

            // Re-enable updates in next event loop
            QTimer::singleShot(0, m_activeConfigDialog, [dlg = m_activeConfigDialog]()
                               {
                if (!dlg) return;
                dlg->setUpdatesEnabled(true);
                dlg->update(); });
        }
        return;
    }

    // Create new dialog
    ConfigDialog *dlg = new ConfigDialog(&m_configManager);
    dlg->setAttribute(Qt::WA_DeleteOnClose, false); // Don't delete on close, allow toggle
    m_activeConfigDialog = dlg;

    // If this is the first-run auto-open, temporarily force it on top so it's not hidden behind other windows.
    const bool forceOnce = m_forceConfigDialogForegroundOnce;
    m_forceConfigDialogForegroundOnce = false;
    if (forceOnce)
    {
        dlg->setWindowFlag(Qt::WindowStaysOnTopHint, true);
    }

    // Restore saved geometry
    AppConfig cfg = m_configManager.getConfig();
    if (!cfg.configWindowGeometry.isEmpty())
    {
        dlg->restoreGeometry(cfg.configWindowGeometry);
    }

    connect(dlg, &ConfigDialog::saved, this, [this]()
            {
        AppConfig cfg = m_configManager.getConfig();
        updateConfig(cfg); });

    // Save geometry when dialog closes naturally (X button)
    connect(dlg, &QDialog::finished, this, [this, dlg]()
            {
        AppConfig cfg = m_configManager.getConfig();
        cfg.configWindowGeometry = dlg->saveGeometry();
        m_configManager.setConfig(cfg);
        m_configManager.saveConfig(); });

    forceToFront(dlg);

    if (forceOnce)
    {
        // Remove the always-on-top flag after the user sees it, to avoid interfering with normal workflow.
        QTimer::singleShot(1200, dlg, [dlg]()
                           {
            if (!dlg) return;
            dlg->setWindowFlag(Qt::WindowStaysOnTopHint, false);
            dlg->show();
            dlg->raise();
            dlg->activateWindow(); });
    }
}
