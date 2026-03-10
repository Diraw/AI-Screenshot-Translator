#include "App.h"

#include <QDialog>
#include <QDebug>
#include <QDir>
#include <QMessageBox>
#include <QSettings>
#include <QStringList>
#include <QTimer>
#include <QCoreApplication>

#include "TranslationManager.h"

extern QString g_logDirectoryPath;

namespace
{

QString cleanedHotkeyLabel(const QString &text)
{
    QString label = text.trimmed();
    while (label.endsWith(':') || label.endsWith(QChar(0xff1a)))
        label.chop(1);
    return label.trimmed();
}

} // namespace

QString App::syncLaunchAtStartup(bool enabled)
{
#ifdef _WIN32
    static const QString kRunKey =
        QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run");
    static const QString kValueName = QStringLiteral("AI Screenshot Translator");

    QSettings settings(kRunKey, QSettings::NativeFormat);
    if (enabled)
    {
        const QString exePath = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
        settings.setValue(kValueName, QString("\"%1\"").arg(exePath));
    }
    else
    {
        settings.remove(kValueName);
    }

    settings.sync();
    if (settings.status() != QSettings::NoError)
    {
        return enabled ? QStringLiteral("Failed to enable launch at startup.")
                       : QStringLiteral("Failed to disable launch at startup.");
    }
#else
    Q_UNUSED(enabled);
#endif
    return QString();
}

QString App::reloadHotkeys()
{
    AppConfig cfg = m_configManager.getConfig();
    TranslationManager &tm = TranslationManager::instance();
    QStringList conflicts;
    m_lastConflictingGlobalHotkeyKeys.clear();

    auto registerTrackedHotkey = [&](GlobalHotkey &hotkey, const QString &keySequence, const QString &labelKey)
    {
        const QString normalizedKey = keySequence.trimmed();
        if (normalizedKey.isEmpty())
        {
            hotkey.unregisterHotkey();
            return;
        }

        if (!hotkey.registerHotkey(normalizedKey))
        {
            const QString label = cleanedHotkeyLabel(tm.tr(labelKey));
            conflicts << tm.tr("msg_hotkey_conflict_item").arg(label, normalizedKey.toUpper());
            m_lastConflictingGlobalHotkeyKeys << labelKey;
        }
    };

    registerTrackedHotkey(m_screenshotHotkey, cfg.screenshotHotkey, "lbl_shot_hotkey");
    registerTrackedHotkey(m_summaryHotkey, cfg.summaryHotkey, "lbl_sum_hotkey");
    registerTrackedHotkey(m_settingsHotkey, cfg.settingsHotkey, "lbl_set_hotkey");

    if (cfg.enableQuitHotkey && !cfg.quitHotkey.trimmed().isEmpty())
    {
        registerTrackedHotkey(m_quitHotkey, cfg.quitHotkey, "lbl_quit_hotkey");
    }
    else
    {
        m_quitHotkey.unregisterHotkey();
    }

    if (conflicts.isEmpty())
    {
        if (m_activeConfigDialog)
            m_activeConfigDialog->setGlobalHotkeyConflictKeys(QStringList(), false);
        return QString();
    }

    if (m_activeConfigDialog)
        m_activeConfigDialog->setGlobalHotkeyConflictKeys(m_lastConflictingGlobalHotkeyKeys, false);

    return tm.tr("msg_hotkey_conflict_body").arg(conflicts.join("\n"));
}

QString App::updateConfig(const AppConfig &cfg)
{
    QString errorMsg = "";

    bool storageFallbackUsed = false;
    QString storageWriteError;
    const QString configuredStoragePath = ConfigManager::resolveStoragePath(cfg.storagePath);
    const QString effectiveStoragePath =
        ConfigManager::resolveWritableStoragePath(cfg.storagePath, &storageFallbackUsed, &storageWriteError);
    const bool loggingWasEnabled = g_enableLogging;
    const QString previousLogDirectory = g_logDirectoryPath;

    m_historyManager.setStoragePath(effectiveStoragePath);
    g_enableLogging = cfg.debugMode;
    g_logDirectoryPath = effectiveStoragePath;
    TranslationManager::instance().setLanguage(cfg.language);
    setupTray();
    const QString autoStartError = syncLaunchAtStartup(cfg.launchAtStartup);

    if (g_enableLogging && (!loggingWasEnabled || previousLogDirectory != g_logDirectoryPath))
    {
        qInfo() << "[Logging] Debug logging enabled. File:"
                << QDir::toNativeSeparators(QDir(g_logDirectoryPath).filePath("debug.log"));
    }

    if (storageFallbackUsed)
    {
        errorMsg = QString("The selected storage directory is not writable:\n%1\n\nReason: %2\n\n"
                           "The app has temporarily switched to:\n%3")
                       .arg(QDir::toNativeSeparators(configuredStoragePath),
                            storageWriteError.isEmpty() ? QString("Write access denied.") : storageWriteError,
                            QDir::toNativeSeparators(effectiveStoragePath));
        QMessageBox::warning(nullptr, "Storage Path Not Writable", errorMsg);
    }
    if (!autoStartError.isEmpty())
    {
        QMessageBox::warning(nullptr, "Launch at Startup", autoStartError);
    }

    if (m_analytics)
    {
        const bool wasEnabled = m_analytics->isUserEnabled();
        m_analytics->setEnabled(cfg.enableUmamiAnalytics);
        if (cfg.enableUmamiAnalytics && !wasEnabled)
            m_analytics->startDelayed(1000);
    }

    // Apply lock-related preferences immediately for future ResultWindow creation.
    m_preferredLockState = cfg.defaultResultWindowLocked;

    const QString hotkeyConflictMessage = reloadHotkeys();
    m_lastHotkeyConflictMessage = hotkeyConflictMessage;
    if (!hotkeyConflictMessage.isEmpty())
        notifyHotkeyConflicts(hotkeyConflictMessage, true);

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

void App::notifyHotkeyConflicts(const QString &message, bool interactive)
{
    if (message.isEmpty())
        return;

    TranslationManager &tm = TranslationManager::instance();
    const QString title = tm.tr("hotkey_conflict_title");

    if (!interactive && m_trayIcon && QSystemTrayIcon::supportsMessages())
    {
        m_trayIcon->showMessage(title, message, QSystemTrayIcon::Warning, 10000);
        return;
    }

    QWidget *parent = nullptr;
    if (m_activeConfigDialog && m_activeConfigDialog->isVisible())
        parent = m_activeConfigDialog;

    QMessageBox box(parent);
    box.setIcon(QMessageBox::Warning);
    box.setWindowTitle(title);
    box.setText(message);
    box.setStandardButtons(QMessageBox::Ok);
    box.setDefaultButton(QMessageBox::Ok);
    box.setWindowModality(Qt::ApplicationModal);
    box.setWindowFlag(Qt::WindowStaysOnTopHint, true);
    box.show();
    box.raise();
    box.activateWindow();
    QTimer::singleShot(0, &box, [&box]()
                       {
        box.raise();
        box.activateWindow(); });
    box.exec();

    if (interactive && !m_lastConflictingGlobalHotkeyKeys.isEmpty())
    {
        QTimer::singleShot(0, this, [this]()
                           { presentConfigDialog(false, true, true); });
    }
}

void App::checkHotkeyRegistrationHealth()
{
    const QString hotkeyConflictMessage = reloadHotkeys();
    if (hotkeyConflictMessage == m_lastHotkeyConflictMessage)
        return;

    m_lastHotkeyConflictMessage = hotkeyConflictMessage;
    if (!hotkeyConflictMessage.isEmpty())
        notifyHotkeyConflicts(hotkeyConflictMessage, false);
}

void App::showConfig()
{
    // Track config dialog open
    if (m_analytics)
        m_analytics->trackConfigDialogOpened();

    presentConfigDialog(true, false, false);
}

void App::presentConfigDialog(bool allowToggle, bool focusGlobalHotkeys, bool forceForeground)
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

    auto applyTemporaryTopMost = [](QWidget *w)
    {
        if (!w)
            return;
        w->setWindowFlag(Qt::WindowStaysOnTopHint, true);
        w->show();
        w->raise();
        w->activateWindow();
        QTimer::singleShot(1200, w, [w]()
                           {
            if (!w)
                return;
            w->setWindowFlag(Qt::WindowStaysOnTopHint, false);
            w->show();
            w->raise();
            w->activateWindow(); });
    };

    if (m_activeConfigDialog)
    {
        if (m_activeConfigDialog->isVisible() && allowToggle)
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
                forceToFront(m_activeConfigDialog);
                if (focusGlobalHotkeys)
                    m_activeConfigDialog->setGlobalHotkeyConflictKeys(m_lastConflictingGlobalHotkeyKeys, true);
                if (forceForeground)
                    applyTemporaryTopMost(m_activeConfigDialog);
            }
            return;
        }

        if (!m_activeConfigDialog->isVisible())
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
        else
        {
            forceToFront(m_activeConfigDialog);
        }

        if (focusGlobalHotkeys)
            m_activeConfigDialog->setGlobalHotkeyConflictKeys(m_lastConflictingGlobalHotkeyKeys, true);
        else
            m_activeConfigDialog->setGlobalHotkeyConflictKeys(m_lastConflictingGlobalHotkeyKeys, false);

        if (forceForeground)
            applyTemporaryTopMost(m_activeConfigDialog);

        return;
    }

    // Create new dialog
    ConfigDialog *dlg = new ConfigDialog(&m_configManager);
    dlg->setAttribute(Qt::WA_DeleteOnClose, false); // Don't delete on close, allow toggle
    m_activeConfigDialog = dlg;

    // If this is the first-run auto-open, temporarily force it on top so it's not hidden behind other windows.
    const bool forceOnce = m_forceConfigDialogForegroundOnce || forceForeground;
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

    connect(dlg, &ConfigDialog::languageChanged, this, &App::onLanguageChanged);

    // Save geometry when dialog closes naturally (X button)
    connect(dlg, &QDialog::finished, this, [this, dlg]()
            {
        AppConfig cfg = m_configManager.getConfig();
        cfg.configWindowGeometry = dlg->saveGeometry();
        m_configManager.setConfig(cfg);
        m_configManager.saveConfig(); });

    forceToFront(dlg);
    dlg->setGlobalHotkeyConflictKeys(m_lastConflictingGlobalHotkeyKeys, focusGlobalHotkeys);

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

void App::onLanguageChanged(const QString &lang)
{
    // Track language change
    if (m_analytics)
        m_analytics->trackLanguageChanged(lang);

    // Update SummaryWindow language
    if (m_summaryWindow)
    {
        m_summaryWindow->updateLanguage();
    }
    
    // Update all ResultWindows
    for (auto w : m_activeWindows)
    {
        if (auto rw = qobject_cast<ResultWindow*>(w.data()))
        {
            rw->updateLanguage();
        }
        else if (auto pc = qobject_cast<PreviewCard*>(w.data()))
        {
            pc->updateLanguage();
        }
    }
    
    // Update tray menu
    if (m_trayMenu)
    {
        setupTray();
    }
}
