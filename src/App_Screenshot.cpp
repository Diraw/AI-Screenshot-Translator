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
    // If screenshot tool is active, close it (cancel)
    if (m_activeScreenshotTool)
    {
        m_activeScreenshotTool->close();
        m_activeScreenshotTool = nullptr;
        return;
    }

    // Otherwise, create new screenshot tool
    AppConfig cfg = m_configManager.getConfig();
    ScreenshotTool *sw = new ScreenshotTool(cfg.targetScreenIndex);
    m_activeScreenshotTool = sw;
    connect(sw, &ScreenshotTool::screenshotTaken, this, &App::onScreenshotCaptured);
    connect(sw, &ScreenshotTool::screenshotTaken, this, [this, sw](const QPixmap &, const QRect &)
            {
        if (m_activeScreenshotTool == sw)
        {
            m_activeScreenshotTool = nullptr;
        }
        sw->deleteLater(); });
    connect(sw, &ScreenshotTool::cancelled, this, [this]()
            {
        if (m_activeScreenshotTool)
        {
            m_activeScreenshotTool->deleteLater();
            m_activeScreenshotTool = nullptr;
        } });
    connect(sw, &ScreenshotTool::destroyed, this, [this]()
            { m_activeScreenshotTool = nullptr; });

    sw->show();
}

void App::onScreenshotCaptured(const QPixmap &pixmap, const QRect &rect)
{
    Q_UNUSED(rect);
    AppConfig cfg = m_configManager.getConfig();

    if (QClipboard *clipboard = QGuiApplication::clipboard())
    {
        clipboard->setPixmap(pixmap);
    }

    // 0. Generate entryId early so PreviewCard and API can share it
    QString entryId = QUuid::createUuid().toString(QUuid::WithoutBraces);

    // 1. Show Preview Card (If enabled)
    if (cfg.showPreviewCard)
    {
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

        connect(card, &PreviewCard::closedWithGeometry, this, [this, entryId](QPoint pos, QSize size)
                {
            m_previewGeometries[entryId] = QRect(pos, size);
            m_lastPreviewGeometry = QRect(pos, size); });

        connect(card, &PreviewCard::closed, [this, card, entryId]()
                { m_activeWindows.removeAll(card); });
    }

    // 2. Call API (If enabled)
    if (!cfg.useAdvancedApiMode && cfg.apiKey.isEmpty())
        return;

    qDebug() << "App: Starting async screenshot processing needed for API";

    // Start Async Processing to prevent UI freeze
    // We need to convert QPixmap to QImage because QPixmap is NOT thread-safe
    QImage image = pixmap.toImage();

    auto *watcher = new QFutureWatcher<QByteArray>(this);

    // Connect finished signal
    connect(watcher, &QFutureWatcher<QByteArray>::finished, this, [this, watcher, cfg, entryId]()
            {
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

        m_apiClient->configure(cfg.apiKey, cfg.baseUrl, cfg.modelName, provider, cfg.useProxy,
                       cfg.proxyUrl, cfg.endpointPath, cfg.useAdvancedApiMode, cfg.advancedApiTemplate);

        // Store entryId in heap to pass as context
        QByteArray *contextData = new QByteArray(entryId.toUtf8());

        m_apiClient->processImage(base64Bytes, cfg.promptText, (void*)contextData);

        qDebug() << "Async image processing completed for entry:" << entryId; });

    // Run heavy encoding in thread pool
    QFuture<QByteArray> future = QtConcurrent::run([image]()
                                                   {
        QByteArray bytes;
        QBuffer buffer(&bytes);
        buffer.open(QIODevice::WriteOnly);
        // Save as PNG (expensive operation)
        image.save(&buffer, "PNG");
        return bytes.toBase64(); });

    watcher->setFuture(future);
}

void App::onScreenshotCancelled()
{
    // Screenshot was cancelled, no action needed
}
