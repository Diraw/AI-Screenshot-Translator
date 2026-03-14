#include "App.h"

void App::onApiSuccess(const QString &result, const QString &originalBase64, const QString &originalPrompt, const QString &requestId)
{
    Q_UNUSED(originalBase64);
    Q_UNUSED(originalPrompt);

    const QString entryId = requestId;

    // Track translation completed (success)
    AppConfig cfg = m_configManager.getConfig();
    if (m_analytics)
        m_analytics->trackTranslationCompleted(cfg.apiProvider, true, 0);

    if (m_summaryWindow)
    {
        m_summaryWindow->updateEntry(entryId, result);
    }

    m_historyManager.updateEntryContent(entryId, result);
}

void App::onApiError(const QString &error, const QString &requestId)
{
    const QString entryId = requestId;

    // Track translation completed (failure)
    AppConfig cfg = m_configManager.getConfig();
    if (m_analytics)
        m_analytics->trackTranslationCompleted(cfg.apiProvider, false, 0);

    const QString errorText = "Error: " + error;
    if (m_summaryWindow)
    {
        m_summaryWindow->updateEntry(entryId, errorText);
    }
    // Persist the failure so history/result windows stay in sync
    m_historyManager.updateEntryContent(entryId, errorText);
}
