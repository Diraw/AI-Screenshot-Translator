#include "App.h"

#include "ThemeUtils.h"

#include <QApplication>
#include <QDebug>
#include <QGuiApplication>
#include <QScreen>
#include <QSettings>
#include <QTimer>
#include <QUuid>
#include <QDateTime>

#include "HintPopup.h"

#ifdef _WIN32
#include "WinKeyForwarder.h"
#endif

void App::showSummary()
{
    // Track summary window open
    if (m_analytics)
        m_analytics->trackSummaryWindowOpened();

    if (m_summaryWindow)
    {
        if (m_summaryWindow->isVisible())
        {
            if (m_summaryWindow->isActiveWindow())
            {
                // Save geometry before hiding
                AppConfig cfg = m_configManager.getConfig();
                cfg.summaryWindowGeometry = m_summaryWindow->saveGeometry();
                m_configManager.setConfig(cfg);
                m_configManager.saveConfig();
                m_summaryWindow->captureScrollPosition();
                QSettings settings(ConfigManager::settingsIniPath(), QSettings::IniFormat);
                settings.setValue("summaryWindow/scrollY", m_summaryWindow->getLastScrollY());
                m_summaryWindow->hide();
            }
            else
            {
                m_summaryWindow->setWindowState((m_summaryWindow->windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
                m_summaryWindow->raise();
                m_summaryWindow->activateWindow();
            }
        }
        else
        {
            // Update history before showing
            m_summaryWindow->setInitialHistory(m_historyManager.loadEntries());

            // Fix white flash: Disable updates during show/restore
            m_summaryWindow->setUpdatesEnabled(false);

            // Restore geometry before showing to prevent flicker
            AppConfig cfg = m_configManager.getConfig();
            if (!cfg.summaryWindowGeometry.isEmpty())
            {
                m_summaryWindow->restoreGeometry(cfg.summaryWindowGeometry);
            }
            m_summaryWindow->show();
            m_summaryWindow->setWindowState((m_summaryWindow->windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
            m_summaryWindow->raise();
            m_summaryWindow->activateWindow();

            // Show archive-window hints unless user opted out.
            HintPopup::maybeShow(HintPopup::Kind::ArchiveWindow, m_summaryWindow, m_configManager.getConfig());

            // Re-enable updates in next event loop
            QTimer::singleShot(0, m_summaryWindow, [w = m_summaryWindow]()
                               {
                if (!w) return;
                w->setUpdatesEnabled(true);
                w->update(); });
        }
    }
}

void App::showResult(const QString &entryId)
{
    TranslationEntry entry = m_historyManager.getEntryById(entryId);
    if (entry.id.isEmpty())
        return;

    AppConfig cfg = m_configManager.getConfig();

    // If there are locked ResultWindows, reuse them instead of creating a new one.
    // Multiple locked windows should stay in sync by receiving the same new entry.
    QList<ResultWindow *> lockedWindows;
    for (auto w : m_activeWindows)
    {
        ResultWindow *rw = qobject_cast<ResultWindow *>(w.data());
        if (rw && rw->isLocked())
        {
            lockedWindows.append(rw);
        }
    }

    if (!lockedWindows.isEmpty())
    {
        for (ResultWindow *rw : lockedWindows)
        {
            if (!rw)
                continue;

            // Ensure screenshot toggle ("s") works for reused locked windows as well.
            connect(rw, &ResultWindow::screenshotRequested,
                    this, &App::onResultWindowScreenshotRequested,
                    Qt::UniqueConnection);

            // Connect retranslate signal for locked windows
            connect(rw, &ResultWindow::retranslateRequested,
                    this, &App::onRetranslateRequested,
                    Qt::UniqueConnection);

            AppConfig cfgForLocked = cfg;
            cfgForLocked.defaultResultWindowLocked = rw->isLocked();
            rw->setConfig(cfgForLocked);
            rw->configureHotkeys(cfg.viewToggleHotkey, cfg.editHotkey, cfg.screenshotToggleHotkey,
                                 cfg.boldHotkey, cfg.underlineHotkey, cfg.highlightHotkey,
                                 cfg.prevResultShortcut, cfg.nextResultShortcut,
                                 cfg.tagHotkey, cfg.retranslateHotkey);
            rw->setHistoryManager(&m_historyManager);
            rw->addEntry(entry);
        }
        return;
    }

    ResultWindow *window = new ResultWindow();

    // Determine initial lock state for newly created ResultWindow
    cfg.defaultResultWindowLocked = m_preferredLockState;

    window->setConfig(cfg);
    window->configureHotkeys(cfg.viewToggleHotkey, cfg.editHotkey, cfg.screenshotToggleHotkey,
                             cfg.boldHotkey, cfg.underlineHotkey, cfg.highlightHotkey,
                             cfg.prevResultShortcut, cfg.nextResultShortcut,
                             cfg.tagHotkey, cfg.retranslateHotkey);
    window->setHistoryManager(&m_historyManager);

    // Connect Screenshot Request (Fixes 's' hotkey)
    connect(window, &ResultWindow::screenshotRequested,
            this, &App::onResultWindowScreenshotRequested,
            Qt::UniqueConnection);

    // Connect Retranslate Request (Ctrl+R)
    connect(window, &ResultWindow::retranslateRequested,
            this, &App::onRetranslateRequested,
            Qt::UniqueConnection);

    window->setContent(entry.translatedMarkdown, entry.originalBase64, entry.prompt, entry.id);
    window->show();

    // Show result-window hints unless user opted out.
    HintPopup::maybeShow(HintPopup::Kind::ResultWindow, window, cfg);

    m_activeWindows.append(window);

    connect(window, &ResultWindow::closed, this, [this, window]()
            {
         m_activeWindows.removeAll(window);
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
         } });

    connect(window, &ResultWindow::contentUpdatedWithId, this, [this](const QString &id, const QString &newMarkdown)
            { m_historyManager.updateEntryContent(id, newMarkdown); });

    connect(window, &ResultWindow::tagsUpdated, this, [this](const QString &id, const QStringList &tags)
            { m_historyManager.updateEntryTags(id, tags); });

    connect(window, &ResultWindow::lockStateChanged, this, [this](bool locked)
            {
        if (m_analytics)
            m_analytics->trackResultWindowLocked(locked); });
}

void App::onResultWindowScreenshotRequested(const QString &entryId, const QString &base64)
{
#ifdef _WIN32
    {
        QString msg = QString("[APP] got screenshotRequested id=%1 b64=%2")
                          .arg(entryId)
                          .arg(base64.size());
        QByteArray line = msg.toUtf8();
        WinKeyForwarder::trace(line.constData());
    }
#endif
    Q_UNUSED(base64);
    restorePreview(entryId);
}

void App::restorePreview(const QString &entryId)
{
#ifdef _WIN32
    {
        QString msg = QString("[APP] restorePreview entryId=%1").arg(entryId);
        QByteArray line = msg.toUtf8();
        WinKeyForwarder::trace(line.constData());
    }
#endif

    if (entryId.isEmpty())
    {
#ifdef _WIN32
        WinKeyForwarder::trace("[APP] restorePreview abort: empty entryId");
#endif
        return;
    }

    // Toggle behavior: if already open for this entry, close it.
    if (m_previewCards.contains(entryId) && m_previewCards[entryId])
    {
#ifdef _WIN32
        WinKeyForwarder::trace("[APP] restorePreview toggle: closing existing PreviewCard");
#endif
        m_previewCards[entryId]->close();
        // QPointer will null it out automatically because WA_DeleteOnClose is set
        return;
    }

    TranslationEntry entry = m_historyManager.getEntryById(entryId);
    if (entry.id.isEmpty())
    {
#ifdef _WIN32
        WinKeyForwarder::trace("[APP] restorePreview abort: entry not found in history");
#endif
        return;
    }

#ifdef _WIN32
    {
        QString msg = QString("[APP] entry found: b64=%1").arg(entry.originalBase64.size());
        QByteArray line = msg.toUtf8();
        WinKeyForwarder::trace(line.constData());
    }
#endif

    AppConfig cfg = m_configManager.getConfig();

    QPixmap pixmap;
    if (!entry.originalBase64.isEmpty())
    {
        QByteArray bytes = QByteArray::fromBase64(entry.originalBase64.toLatin1());
#ifdef _WIN32
        {
            QString msg = QString("[APP] decoded bytes=%1").arg(bytes.size());
            QByteArray line = msg.toUtf8();
            WinKeyForwarder::trace(line.constData());
        }
#endif
        pixmap.loadFromData(bytes);
    }

    if (entry.originalBase64.isEmpty())
    {
#ifdef _WIN32
        WinKeyForwarder::trace("[APP] restorePreview abort: entry.originalBase64 empty");
#endif
    }

    if (pixmap.isNull())
    {
#ifdef _WIN32
        WinKeyForwarder::trace("[APP] restorePreview abort: pixmap.isNull (decode failed)");
#endif
        return;
    }

#ifdef _WIN32
    {
        const qreal rawDpr = pixmap.devicePixelRatio();
        const QSize rawPx = pixmap.size();
        const QSizeF rawDip = pixmap.deviceIndependentSize();
        QString msg = QString("[APP] pixmap OK rawPx=%1x%2 rawDpr=%3 rawDip=%4x%5")
                          .arg(rawPx.width())
                          .arg(rawPx.height())
                          .arg(rawDpr, 0, 'f', 3)
                          .arg(rawDip.width(), 0, 'f', 1)
                          .arg(rawDip.height(), 0, 'f', 1);
        QByteArray line = msg.toUtf8();
        WinKeyForwarder::trace(line.constData());
    }
#endif

    // If the screenshot came from a HiDPI monitor, the captured image is usually in physical pixels,
    // but decoding from PNG loses the original DPR metadata. Restore an appropriate DPR so the
    // PreviewCard (which sizes itself by deviceIndependentSize) doesn't jump larger after toggling.
    {
        qreal desiredDpr = 1.0;
        const QList<QScreen *> screens = QGuiApplication::screens();
        const int idx = cfg.targetScreenIndex;
        if (idx >= 0 && idx < screens.size() && screens[idx])
            desiredDpr = screens[idx]->devicePixelRatio();
        else if (QGuiApplication::primaryScreen())
            desiredDpr = QGuiApplication::primaryScreen()->devicePixelRatio();

        if (desiredDpr <= 0.0)
            desiredDpr = 1.0;

        if (qAbs(pixmap.devicePixelRatio() - desiredDpr) > 0.001)
        {
            pixmap.setDevicePixelRatio(desiredDpr);
#ifdef _WIN32
            {
                const QSize px = pixmap.size();
                const QSizeF dip = pixmap.deviceIndependentSize();
                QString msg = QString("[APP] pixmap DPR adjusted to %1 -> dip=%2x%3 (px=%4x%5)")
                                  .arg(desiredDpr, 0, 'f', 3)
                                  .arg(dip.width(), 0, 'f', 1)
                                  .arg(dip.height(), 0, 'f', 1)
                                  .arg(px.width())
                                  .arg(px.height());
                QByteArray line = msg.toUtf8();
                WinKeyForwarder::trace(line.constData());
            }
#endif
        }
    }

    PreviewCard *card = new PreviewCard(pixmap);
    card->setZoomSensitivity(cfg.zoomSensitivity);
    card->setBorderColor(cfg.cardBorderColor);
    card->setUseBorder(cfg.useCardBorder);

    // Restore geometry if available
    if (m_previewGeometries.contains(entryId))
    {
        QRect geom = m_previewGeometries[entryId];
        card->resize(geom.size());
        card->move(geom.topLeft());
    }

    card->show();
    m_activeWindows.append(card);
    m_previewCards[entryId] = card;

    connect(card, &PreviewCard::closedWithGeometry, this, [this, entryId](QPoint pos, QSize size)
            { m_previewGeometries[entryId] = QRect(pos, size); });

    connect(card, &PreviewCard::closed, [this, card, entryId]()
            {
                m_activeWindows.removeAll(card);
                // m_previewCards[entryId] will become null due to QPointer
            });
}

void App::onRetranslateRequested(const QString &base64Image)
{
    // Track retranslation
    if (m_analytics)
        m_analytics->trackRetranslation();

    if (base64Image.isEmpty())
    {
        qWarning() << "[App] onRetranslateRequested: empty base64 image";
        return;
    }

    AppConfig cfg = m_configManager.getConfig();

    if (!cfg.useAdvancedApiMode && cfg.apiKey.isEmpty())
    {
        qWarning() << "[App] onRetranslateRequested: no API key configured";
        return;
    }

    // Generate new entryId for this retranslation
    QString entryId = QUuid::createUuid().toString(QUuid::WithoutBraces);

    qDebug() << "[App] Retranslating image, new entryId:" << entryId;

    // Create entry in history
    TranslationEntry entry;
    entry.id = entryId;
    entry.timestamp = QDateTime::currentDateTime();
    entry.prompt = cfg.promptText;
    entry.translatedMarkdown = "Processing...";
    entry.originalBase64 = base64Image;

    m_historyManager.saveEntry(entry);

    // Show result window
    if (cfg.showResultWindow)
    {
        showResult(entryId);
    }

    // Convert QString apiProvider to enum
    ApiProvider provider = ApiProvider::OpenAI; // default
    QString providerStr = cfg.apiProvider.trimmed().toLower();
    if (providerStr == "gemini")
        provider = ApiProvider::Gemini;
    else if (providerStr == "claude")
        provider = ApiProvider::Claude;

    m_apiClient->configure(cfg.apiKey, cfg.baseUrl, cfg.modelName, provider, cfg.useProxy,
                           cfg.proxyUrl, cfg.endpointPath, cfg.useAdvancedApiMode, cfg.advancedApiTemplate);

    // Track translation started (retranslation)
    if (m_analytics)
        m_analytics->trackTranslationStarted(cfg.apiProvider, cfg.useAdvancedApiMode);

    // Store entryId in heap to pass as context
    QByteArray *contextData = new QByteArray(entryId.toUtf8());

    // Convert base64 string back to QByteArray for API call
    QByteArray base64Bytes = base64Image.toLatin1();

    m_apiClient->processImage(base64Bytes, cfg.promptText, (void *)contextData);

    qDebug() << "[App] Retranslation request sent for entry:" << entryId;
}
