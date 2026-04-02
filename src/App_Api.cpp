#include "App.h"

#include <QtGlobal>

void App::onApiSuccess(const QString &result, const QString &originalBase64, const QString &originalPrompt,
                       const QString &requestId, qint64 elapsedMs)
{
    Q_UNUSED(originalBase64);
    Q_UNUSED(originalPrompt);

    const QString entryId = requestId;
    const int durationMs = static_cast<int>(qBound<qint64>(0, elapsedMs, 2147483647LL));

    // Track translation completed (success)
    AppConfig cfg = m_configManager.getConfig();
    if (m_analytics)
        m_analytics->trackTranslationCompleted(cfg.apiProvider, true, durationMs);

    if (m_summaryWindow)
    {
        m_summaryWindow->updateEntry(entryId, result);
    }

    m_historyManager.updateEntryContent(entryId, result);
}

void App::onApiError(const QString &error, const QString &requestId, qint64 elapsedMs)
{
    const QString entryId = requestId;
    const int durationMs = static_cast<int>(qBound<qint64>(0, elapsedMs, 2147483647LL));

    // Track translation completed (failure)
    AppConfig cfg = m_configManager.getConfig();
    if (m_analytics)
        m_analytics->trackTranslationCompleted(cfg.apiProvider, false, durationMs);

    const QString errorText = "Error: " + error;
    if (m_summaryWindow)
    {
        m_summaryWindow->updateEntry(entryId, errorText);
    }
    // Persist the failure so history/result windows stay in sync
    m_historyManager.updateEntryContent(entryId, errorText);
}
