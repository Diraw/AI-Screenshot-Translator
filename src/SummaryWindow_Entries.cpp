#include "SummaryWindow.h"

#include "EmbedWebView.h"

#include <string>

void SummaryWindow::setInitialHistory(const QList<TranslationEntry> &history)
{
    captureScrollPosition();
    m_entries = history;
    refreshHtml();
}

void SummaryWindow::reloadFromStorage(bool preserveScroll)
{
    refreshHtml(preserveScroll);
}

void SummaryWindow::addEntry(const TranslationEntry &entry)
{
    m_entries.append(entry);
    if (m_archiveUsePagination)
    {
        refreshHtml(false);
        return;
    }
    appendEntryHtml(entry);
}

void SummaryWindow::clearEntries()
{
    captureScrollPosition();
    m_entries.clear();
    refreshHtml();
}

const TranslationEntry *SummaryWindow::getEntry(const QString &id) const
{
    for (const auto &entry : m_entries)
    {
        if (entry.id == id)
            return &entry;
    }
    return nullptr;
}

void SummaryWindow::updateEntryGeometry(const QString &id, const QPoint &pos, const QSize &size)
{
    for (auto &entry : m_entries)
    {
        if (entry.id == id)
        {
            entry.lastPosition = pos;
            entry.lastSize = size;
            entry.hasLastPosition = true;
            break;
        }
    }
}

void SummaryWindow::updateEntry(const QString &id, const QString &markdown)
{
    // Alias for updateEntryContent to match the API calls from App.cpp
    updateEntryContent(id, markdown);
}

void SummaryWindow::updateEntryContent(const QString &id, const QString &markdown)
{
    for (auto &entry : m_entries)
    {
        if (entry.id == id)
        {
            entry.translatedMarkdown = markdown;
            break;
        }
    }
    if (m_webView)
    {
        // Simple unescape for JS injection
        QString safeMd = markdown;
        safeMd.replace("\\", "\\\\").replace("'", "\\'").replace("\n", "\\n").replace("\r", "");
        QString js = QString("updateEntryInDom('%1', '%2')").arg(id, safeMd);
        m_webView->eval(js.toStdString());
    }
}

void SummaryWindow::setZoomFactor(qreal factor)
{
    m_currentZoom = factor;
    if (m_webView)
    {
        m_webView->eval("document.body.style.zoom = '" + std::to_string(factor) + "'");
    }
}

qreal SummaryWindow::getZoomFactor() const
{
    return m_currentZoom;
}

void SummaryWindow::captureScrollPosition()
{
    if (m_webView)
    {
        m_webView->eval("if(window.cmd_scroll){window.cmd_scroll(window.scrollY);}");
    }
}
