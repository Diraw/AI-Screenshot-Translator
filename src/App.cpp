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

#include "TranslationManager.h"

App::App(QObject *parent) 
    : QObject(parent), m_configManager(),
      m_screenshotHotkey(100, this), 
      m_summaryHotkey(101, this),
      m_settingsHotkey(102, this)
{
    AppConfig cfg = m_configManager.getConfig();
    g_enableLogging = cfg.debugMode;
    TranslationManager::instance().setLanguage(cfg.language);
    
    m_preferredLockState = cfg.defaultResultWindowLocked;
    
    m_apiClient = new ApiClient(this);
    connect(m_apiClient, &ApiClient::success, this, &App::onApiSuccess);
    connect(m_apiClient, &ApiClient::error, this, &App::onApiError);

    m_summaryWindow = new SummaryWindow();
    m_summaryWindow->setConfig(cfg);
    connect(m_summaryWindow, &SummaryWindow::restorePreviewRequested, this, [this](const QString& id){
        restorePreview(id);
    });
    
    connect(m_summaryWindow, &SummaryWindow::requestDeleteEntry, this, [this](const QString& id){
        if (m_historyManager.deleteEntry(id)) {
            qDebug() << "Deleted entry:" << id;
        }
    });

    connect(&m_historyManager, &HistoryManager::entryMarkdownChanged, this, [this](const QString& id, const QString& content){
        if (m_summaryWindow) {
            m_summaryWindow->updateEntryContent(id, content);
        }
        for (auto w : m_activeWindows) {
            if (auto res = qobject_cast<ResultWindow*>(w.data())) {
                if (res->entryId() == id) {
                    res->externalContentUpdate(content);
                }
            }
        }
    });
    
    setupTray(); 
    m_historyManager.setStoragePath(cfg.storagePath);
    
    QList<TranslationEntry> history = m_historyManager.loadEntries();
    m_summaryWindow->setInitialHistory(history);

    reloadHotkeys(); 
    
    connect(&m_screenshotHotkey, &GlobalHotkey::activated, this, &App::onScreenshotRequested);
    connect(&m_summaryHotkey, &GlobalHotkey::activated, this, &App::showSummary);
    connect(&m_settingsHotkey, &GlobalHotkey::activated, this, &App::showConfig);
    
    if (m_configManager.getConfig().apiKey.isEmpty()) {
        QTimer::singleShot(100, this, &App::showConfig);
    }
    
    // Theme Init
    m_lastTopBarDark = ThemeUtils::isSystemDark();
    connect(&m_themeTimer, &QTimer::timeout, this, &App::checkForThemeChange);
    m_themeTimer.start(2000);
}

App::~App() {
    if (m_summaryWindow) delete m_summaryWindow;
}

void App::setupTray() {
    if (m_trayIcon) delete m_trayIcon; // Re-creating or creating first time
    if (m_trayMenu) delete m_trayMenu;

    m_trayIcon = new QSystemTrayIcon(this);
    QIcon icon(":/assets/icon.ico"); 
    if (icon.isNull()) {
        icon = QIcon("assets/icon.ico"); 
    }
    m_trayIcon->setIcon(icon);
    
    m_trayMenu = new QMenu();
    TranslationManager &tm = TranslationManager::instance();
    
    m_trayMenu->addAction(tm.tr("tray_screenshot"), this, &App::onScreenshotRequested);
    m_trayMenu->addAction(tm.tr("tray_summary"), this, &App::showSummary);
    m_trayMenu->addAction(tm.tr("tray_settings"), this, &App::showConfig);
    m_trayMenu->addSeparator();
    m_trayMenu->addAction(tm.tr("tray_quit"), qApp, &QCoreApplication::quit);
    
    m_trayIcon->setContextMenu(m_trayMenu);
    m_trayIcon->show();
    
    connect(m_trayIcon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason){
        if (reason == QSystemTrayIcon::Trigger) {
            onScreenshotRequested();
        } else if (reason == QSystemTrayIcon::DoubleClick) {
            showSummary();
        }
    });
}

void App::onScreenshotRequested() {
    // If screenshot tool is active, close it (cancel)
    if (m_activeScreenshotTool) {
        m_activeScreenshotTool->close();
        m_activeScreenshotTool = nullptr;
        return;
    }
    
    // Otherwise, create new screenshot tool
    AppConfig cfg = m_configManager.getConfig();
    ScreenshotTool *sw = new ScreenshotTool(cfg.targetScreenIndex);
    connect(sw, &ScreenshotTool::screenshotTaken, this, &App::onScreenshotCaptured);
    connect(sw, &ScreenshotTool::destroyed, this, [this]() {
        m_activeScreenshotTool = nullptr;
    });
    
    sw->show();
}

void App::onScreenshotCaptured(const QPixmap &pixmap, const QRect &rect) {
    Q_UNUSED(rect);
    AppConfig cfg = m_configManager.getConfig();

    // 0. Generate entryId early so PreviewCard and API can share it
    QString entryId = QUuid::createUuid().toString(QUuid::WithoutBraces);

    // 1. Show Preview Card (If enabled)
    if (cfg.showPreviewCard) {
        PreviewCard *card = new PreviewCard(pixmap);
        card->setZoomSensitivity(cfg.zoomSensitivity);
        card->setBorderColor(cfg.cardBorderColor);
        card->setUseBorder(cfg.useCardBorder);
        card->move(rect.topLeft()); 
        card->show();
        m_activeWindows.append(card);
        m_activePreviewCard = card;
        
        // Track card by entryId
        m_previewCards[entryId] = card;

        connect(card, &PreviewCard::closedWithGeometry, this, [this, entryId](QPoint pos, QSize size){
            m_previewGeometries[entryId] = QRect(pos, size);
            m_lastPreviewGeometry = QRect(pos, size);
        });

        connect(card, &PreviewCard::closed, [this, card, entryId](){
            m_activeWindows.removeAll(card);
        });
    }


    // 2. Call API (If enabled)
    if (cfg.apiKey.isEmpty()) return;

    qDebug() << "App: Starting async screenshot processing needed for API";

    // Start Async Processing to prevent UI freeze
    // We need to convert QPixmap to QImage because QPixmap is NOT thread-safe
    QImage image = pixmap.toImage();
    
    auto *watcher = new QFutureWatcher<QByteArray>(this);
    
    // Connect finished signal
    connect(watcher, &QFutureWatcher<QByteArray>::finished, this, [this, watcher, cfg, entryId](){
        QByteArray base64Bytes = watcher->result();
        
        qDebug() << "App: Async encoding finished. Bytes:" << base64Bytes.size();
        
        // Clean up watcher
        watcher->deleteLater();
        
        if (base64Bytes.isEmpty()) {
            qWarning() << "Failed to encode image to base64";
            return;
        }

        // Create entry using pre-generated entryId
        TranslationEntry entry;
        entry.id = entryId;

        entry.timestamp = QDateTime::currentDateTime();
        entry.prompt = cfg.promptText;
        entry.translatedMarkdown = "Processing...";
        QString base64Str = QString::fromLatin1(base64Bytes);
        entry.originalBase64 = base64Str;
        
        m_historyManager.saveEntry(entry);
        
        // Show partial result window
        if (cfg.showResultWindow) {
            showResult(entryId);
        }

        // Convert QString apiProvider to enum
        ApiProvider provider = ApiProvider::OpenAI;  // default
        QString providerStr = cfg.apiProvider.trimmed().toLower();
        if (providerStr == "gemini") provider = ApiProvider::Gemini;
        else if (providerStr == "claude") provider = ApiProvider::Claude;
        
        m_apiClient->configure(cfg.apiKey, cfg.baseUrl, cfg.modelName, provider, cfg.useProxy, cfg.proxyUrl);
        
        // Store entryId in heap to pass as context
        QByteArray *contextData = new QByteArray(entryId.toUtf8());
        
        m_apiClient->processImage(base64Bytes, cfg.promptText, (void*)contextData);
        
        qDebug() << "Async image processing completed for entry:" << entryId;
    });

    // Run heavy encoding in thread pool
    QFuture<QByteArray> future = QtConcurrent::run([image]() {
        QByteArray bytes;
        QBuffer buffer(&bytes);
        buffer.open(QIODevice::WriteOnly);
        // Save as PNG (expensive operation)
        image.save(&buffer, "PNG");
        return bytes.toBase64();
    });
    
    watcher->setFuture(future);
}

void App::onApiSuccess(const QString &result, const QString &originalBase64, const QString &originalPrompt, void* context) {
    QByteArray *contextData = static_cast<QByteArray*>(context);
    QString entryId = QString::fromUtf8(*contextData);
    delete contextData; // Free the allocated memory
    
    if (m_summaryWindow) {
        m_summaryWindow->updateEntry(entryId, result);
    }
    
    m_historyManager.updateEntryContent(entryId, result);
}

void App::onApiError(const QString &error, void* context) {
    QByteArray *contextData = static_cast<QByteArray*>(context);
    QString entryId = QString::fromUtf8(*contextData);
    delete contextData; // Free the allocated memory
    
    if (m_summaryWindow) {
        m_summaryWindow->updateEntry(entryId, "Error: " + error);
    }
}

void App::showSummary() {
    if (m_summaryWindow) {
        if (m_summaryWindow->isVisible()) {
            // Save geometry before hiding
            AppConfig cfg = m_configManager.getConfig();
            cfg.summaryWindowGeometry = m_summaryWindow->saveGeometry();
            m_configManager.setConfig(cfg);
            m_configManager.saveConfig();
            m_summaryWindow->hide();
        } else {
            // Update history before showing
            m_summaryWindow->setInitialHistory(m_historyManager.loadEntries());
            
            // Fix white flash: Disable updates during show/restore
            m_summaryWindow->setUpdatesEnabled(false);

            // Restore geometry before showing to prevent flicker
            AppConfig cfg = m_configManager.getConfig();
            if (!cfg.summaryWindowGeometry.isEmpty()) {
                m_summaryWindow->restoreGeometry(cfg.summaryWindowGeometry);
            }
            m_summaryWindow->show();
            m_summaryWindow->raise();
            m_summaryWindow->activateWindow();

            // Re-enable updates in next event loop
            QTimer::singleShot(0, m_summaryWindow, [w = m_summaryWindow]() {
                if (!w) return;
                w->setUpdatesEnabled(true);
                w->update();
            });
        }
    }
}

void App::showResult(const QString &entryId) {
    TranslationEntry entry = m_historyManager.getEntryById(entryId);
    if (entry.id.isEmpty()) return;

    AppConfig cfg = m_configManager.getConfig();
    
    ResultWindow *window = new ResultWindow();
    
    // Determine initial lock state
    if (m_preferredLockState) {
        cfg.defaultResultWindowLocked = true;
    } else {
        cfg.defaultResultWindowLocked = false;
    }
    
    window->setConfig(cfg);
    window->configureHotkeys(cfg.viewToggleHotkey, cfg.editHotkey, cfg.screenshotToggleHotkey,
                            cfg.boldHotkey, cfg.underlineHotkey, cfg.highlightHotkey,
                            cfg.prevResultShortcut, cfg.nextResultShortcut,
                            cfg.tagHotkey);
    window->setHistoryManager(&m_historyManager);
    
    // Connect Screenshot Request (Fixes 's' hotkey)
    connect(window, &ResultWindow::screenshotRequested, this, [this](const QString& id, const QString& b64){
        // Reuse existing preview restoration or trigger new screenshot tool?
        // Usually 's' is "Show Screenshot" of current entry
        restorePreview(id);
    });

    window->setContent(entry.translatedMarkdown, entry.originalBase64, entry.prompt, entry.id); 
    window->show();
    m_activeWindows.append(window);
    
    connect(window, &ResultWindow::closed, this, [this, window](){
         bool wasLocked = window->isLocked();
         bool otherLocked = false;
         for(auto w : m_activeWindows) {
             if (auto rw = qobject_cast<ResultWindow*>(w.data())) {
                 if (rw != window && rw->isVisible() && rw->isLocked()) {
                     otherLocked = true;
                     break;
                 }
             }
         }
         
         if (!otherLocked) {
             AppConfig c = m_configManager.getConfig();
             if (c.lockBehavior == 0) {
                 m_preferredLockState = false;
             } else {
                 m_preferredLockState = wasLocked;
             }
         }
    });
    
    connect(window, &ResultWindow::contentUpdatedWithId, this, [this](const QString &id, const QString &newMarkdown){
        m_historyManager.updateEntryContent(id, newMarkdown);
    });

    connect(window, &ResultWindow::tagsUpdated, this, [this](const QString &id, const QStringList &tags){
        m_historyManager.updateEntryTags(id, tags);
    });
}

void App::restorePreview(const QString &entryId) {
    if (entryId.isEmpty()) return;

    // Toggle behavior: if already open for this entry, close it.
    if (m_previewCards.contains(entryId) && m_previewCards[entryId]) {
        m_previewCards[entryId]->close();
        // QPointer will null it out automatically because WA_DeleteOnClose is set
        return;
    }

    TranslationEntry entry = m_historyManager.getEntryById(entryId);
    if (entry.id.isEmpty()) return;

    AppConfig cfg = m_configManager.getConfig();
    
    QPixmap pixmap;
    if (!entry.originalBase64.isEmpty()) {
        QByteArray bytes = QByteArray::fromBase64(entry.originalBase64.toLatin1());
        pixmap.loadFromData(bytes);
    }
    
    if (pixmap.isNull()) return;

    PreviewCard *card = new PreviewCard(pixmap);
    card->setZoomSensitivity(cfg.zoomSensitivity);
    card->setBorderColor(cfg.cardBorderColor);
    card->setUseBorder(cfg.useCardBorder);
    
    // Restore geometry if available
    if (m_previewGeometries.contains(entryId)) {
        QRect geom = m_previewGeometries[entryId];
        card->resize(geom.size());
        card->move(geom.topLeft());
    }

    card->show();
    m_activeWindows.append(card);
    m_previewCards[entryId] = card;

    connect(card, &PreviewCard::closedWithGeometry, this, [this, entryId](QPoint pos, QSize size){
        m_previewGeometries[entryId] = QRect(pos, size);
    });

    connect(card, &PreviewCard::closed, [this, card, entryId](){
        m_activeWindows.removeAll(card);
        // m_previewCards[entryId] will become null due to QPointer
    });
}

QString App::reloadHotkeys() {
    AppConfig cfg = m_configManager.getConfig();
    m_screenshotHotkey.registerHotkey(cfg.screenshotHotkey);
    m_summaryHotkey.registerHotkey(cfg.summaryHotkey);
    m_settingsHotkey.registerHotkey(cfg.settingsHotkey);
    return ""; // Return empty string on success
}

QString App::updateConfig(const AppConfig &cfg) {
    QString errorMsg = "";
    
    m_historyManager.setStoragePath(cfg.storagePath);
    g_enableLogging = cfg.debugMode;
    TranslationManager::instance().setLanguage(cfg.language);
    setupTray();

    reloadHotkeys();

    if (m_summaryWindow) {
        m_summaryWindow->configureHotkeys(cfg.editHotkey, cfg.viewToggleHotkey, cfg.screenshotToggleHotkey,
                                          cfg.boldHotkey, cfg.underlineHotkey, cfg.highlightHotkey);
    }
    
    // Sync to active PreviewCards
    for (auto w : m_activeWindows) {
        if (!w) continue;
        PreviewCard *pc = qobject_cast<PreviewCard*>(w.data());
        if (pc) {
            pc->setZoomSensitivity(cfg.zoomSensitivity);
            pc->setBorderColor(cfg.cardBorderColor);
            pc->setUseBorder(cfg.useCardBorder);
        }
    }

    return errorMsg;
}

void App::showConfig() {
    if (m_activeConfigDialog) {
        // Toggle: if visible, hide; if hidden, show
        if (m_activeConfigDialog->isVisible()) {
            // Save geometry before hiding
            AppConfig cfg = m_configManager.getConfig();
            cfg.configWindowGeometry = m_activeConfigDialog->saveGeometry();
            m_configManager.setConfig(cfg);
            m_configManager.saveConfig();
            m_activeConfigDialog->hide();
        } else {
            // Fix white flash: Disable updates during show/restore
            m_activeConfigDialog->setUpdatesEnabled(false);

            // Restore geometry before showing to prevent flicker
            AppConfig cfg = m_configManager.getConfig();
            if (!cfg.configWindowGeometry.isEmpty()) {
                m_activeConfigDialog->restoreGeometry(cfg.configWindowGeometry);
            }
            m_activeConfigDialog->show();
            m_activeConfigDialog->raise();
            m_activeConfigDialog->activateWindow();

            // Re-enable updates in next event loop
            QTimer::singleShot(0, m_activeConfigDialog, [dlg = m_activeConfigDialog]() {
                if (!dlg) return;
                dlg->setUpdatesEnabled(true);
                dlg->update();
            });
        }
        return;
    }

    // Create new dialog
    ConfigDialog *dlg = new ConfigDialog(&m_configManager);
    dlg->setAttribute(Qt::WA_DeleteOnClose, false); // Don't delete on close, allow toggle
    m_activeConfigDialog = dlg;
    
    // Restore saved geometry
    AppConfig cfg = m_configManager.getConfig();
    if (!cfg.configWindowGeometry.isEmpty()) {
        dlg->restoreGeometry(cfg.configWindowGeometry);
    }
    
    connect(dlg, &ConfigDialog::saved, this, [this](){
        AppConfig cfg = m_configManager.getConfig();
        updateConfig(cfg);
    });
    
    // Save geometry when dialog closes naturally (X button)
    connect(dlg, &QDialog::finished, this, [this, dlg](){
        AppConfig cfg = m_configManager.getConfig();
        cfg.configWindowGeometry = dlg->saveGeometry();
        m_configManager.setConfig(cfg);
        m_configManager.saveConfig();
    });
    
    dlg->show();
}

void App::onScreenshotCancelled() {
    // Screenshot was cancelled, no action needed
}

void App::quitApp() {
    qApp->quit();
}

void App::checkForThemeChange() {
    bool currentDark = ThemeUtils::isSystemDark();
    if (currentDark != m_lastTopBarDark) {
        qDebug() << "Theme change detected: " << (currentDark ? "Dark" : "Light");
        m_lastTopBarDark = currentDark;
        updateAllWindowThemes(currentDark);
    }
}

void App::updateAllWindowThemes(bool isDark) {
    if (m_summaryWindow) {
        m_summaryWindow->updateTheme(isDark);
    }
    
    if (m_activeConfigDialog) {
       m_activeConfigDialog->updateTheme(isDark);
    }

    for (auto w : m_activeWindows) {
        if (!w) continue;
        if (ResultWindow* rw = qobject_cast<ResultWindow*>(w.data())) {
            rw->updateTheme(isDark);
        }
    }
}
