#include "App.h"

#include <QBuffer>
#include <QClipboard>
#include <QDateTime>
#include <QDebug>
#include <QFuture>
#include <QFutureWatcher>
#include <QGuiApplication>
#include <QImage>
#include <QtConcurrent>
#include <QUuid>

#include "TranslationManager.h"

void App::onScreenshotRequested()
{
    // Track screenshot trigger
    if (m_analytics)
        m_analytics->trackScreenshotTriggered();

    // If screenshot tool is active, close it (cancel)
    if (m_activeScreenshotTool)
    {
        m_activeScreenshotTool->close();
        m_activeScreenshotTool = nullptr;
        return;
    }

    // Otherwise, create new screenshot tool
    AppConfig cfg = m_configManager.getConfig();
    ScreenshotTool *sw = new ScreenshotTool(cfg.targetScreenIndex,
                                            !m_pendingBatchCaptures.isEmpty(),
                                            m_pendingBatchCaptures.size(),
                                            cfg.batchScreenshotToggleHotkey);
    m_activeScreenshotTool = sw;
    connect(sw, &ScreenshotTool::screenshotTaken, this, &App::onScreenshotCaptured);
    connect(sw, &ScreenshotTool::screenshotTaken, this, [this, sw](const QPixmap &, const QRect &, bool, bool)
            {
        if (m_activeScreenshotTool == sw)
        {
            m_activeScreenshotTool = nullptr;
        }
        sw->deleteLater(); });
    connect(sw, &ScreenshotTool::cancelled, this, &App::onScreenshotCancelled);
    connect(sw, &ScreenshotTool::cancelled, this, [this, sw]()
            {
        if (m_activeScreenshotTool == sw)
        {
            m_activeScreenshotTool->deleteLater();
            m_activeScreenshotTool = nullptr;
        } }, Qt::QueuedConnection);
    connect(sw, &ScreenshotTool::destroyed, this, [this]()
            { m_activeScreenshotTool = nullptr; });

    sw->show();
}

void App::onScreenshotCaptured(const QPixmap &pixmap, const QRect &rect, bool batchMode, bool finalizeBatch)
{
    if (QClipboard *clipboard = QGuiApplication::clipboard())
    {
        clipboard->setPixmap(pixmap);
    }

    if (batchMode)
    {
        m_pendingBatchCaptures.append({pixmap, rect});
        if (!finalizeBatch)
            return;

        const QList<PendingBatchCapture> captures = m_pendingBatchCaptures;
        clearPendingBatchCaptures();
        submitCapturedImages(captures);
        return;
    }

    submitCapturedImages(QList<PendingBatchCapture>{{pixmap, rect}});
}

void App::submitCapturedImages(const QList<PendingBatchCapture> &captures)
{
    if (captures.isEmpty())
        return;

    AppConfig cfg = m_configManager.getConfig();
    const QString entryId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QList<QPixmap> previewPixmaps;
    QList<QImage> images;
    previewPixmaps.reserve(captures.size());
    images.reserve(captures.size());
    for (const PendingBatchCapture &capture : captures)
    {
        previewPixmaps.append(capture.pixmap);
        images.append(capture.pixmap.toImage());
    }

    if (cfg.showPreviewCard)
    {
        PreviewCard *card = new PreviewCard(previewPixmaps);
        card->setZoomSensitivity(cfg.zoomSensitivity);
        card->setBorderColor(cfg.cardBorderColor);
        card->setUseBorder(cfg.useCardBorder);
        card->setNavigationHotkeys(cfg.prevResultShortcut, cfg.nextResultShortcut);
        card->move(captures.first().rect.topLeft());
        card->show();
        trackActiveWindow(card);
        m_activePreviewCard = card;
        m_previewImageCache[entryId] = previewPixmaps;
        m_previewCards[entryId] = card;

        connect(card, &PreviewCard::closedWithGeometry, this, [this, entryId](QPoint pos, QSize size)
                {
            m_previewGeometries[entryId] = QRect(pos, size);
            m_lastPreviewGeometry = QRect(pos, size); });

        connect(card, &PreviewCard::closed, [this, card, entryId]()
                {
                    m_activeWindows.removeAll(card);
                    m_previewCards.remove(entryId);
                });
    }

    if (!cfg.useAdvancedApiMode && cfg.apiKey.isEmpty())
        return;

    auto *watcher = new QFutureWatcher<QList<QByteArray>>(this);
    connect(watcher, &QFutureWatcher<QList<QByteArray>>::finished, this, [this, watcher, cfg, entryId]()
            {
        const QList<QByteArray> base64Images = watcher->result();
        watcher->deleteLater();

        if (base64Images.isEmpty()) {
            qWarning() << "Failed to encode images to base64";
            return;
        }

        TranslationEntry entry;
        entry.id = entryId;
        entry.timestamp = QDateTime::currentDateTime();
        entry.prompt = cfg.promptText;
        entry.translatedMarkdown = "Processing...";
        entry.originalBase64 = QString::fromLatin1(base64Images.first());
        for (const QByteArray &base64Image : base64Images)
            entry.originalBase64List.append(QString::fromLatin1(base64Image));

        m_historyManager.saveEntry(entry);

        if (cfg.showResultWindow)
            showResult(entryId);

        ApiProvider provider = ApiProvider::OpenAI;
        QString providerStr = cfg.apiProvider.trimmed().toLower();
        if (providerStr == "gemini")
            provider = ApiProvider::Gemini;
        else if (providerStr == "claude")
            provider = ApiProvider::Claude;

        m_apiClient->configure(cfg.apiKey, cfg.baseUrl, cfg.modelName, provider, cfg.useProxy,
                               cfg.proxyUrl, cfg.endpointPath, cfg.useAdvancedApiMode, cfg.advancedApiTemplate);

        if (m_analytics)
            m_analytics->trackTranslationStarted(cfg.apiProvider, cfg.useAdvancedApiMode);

        m_apiClient->processImages(base64Images, cfg.promptText, entryId);
    });

    QFuture<QList<QByteArray>> future = QtConcurrent::run([images]()
                                                          {
        QList<QByteArray> out;
        out.reserve(images.size());
        for (const QImage &image : images)
        {
            QByteArray bytes;
            QBuffer buffer(&bytes);
            buffer.open(QIODevice::WriteOnly);
            image.save(&buffer, "PNG");
            out.append(bytes.toBase64());
        }
        return out; });

    watcher->setFuture(future);
}

void App::clearPendingBatchCaptures()
{
    m_pendingBatchCaptures.clear();
}

void App::onScreenshotCancelled(bool clearPendingBatch)
{
    if (clearPendingBatch)
        clearPendingBatchCaptures();
}
