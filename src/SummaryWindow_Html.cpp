#include "SummaryWindow.h"

#include "ColorUtils.h"
#include "EmbedWebView.h"
#include "HistoryManager.h"
#include "ThemeUtils.h"
#include "TranslationManager.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QRegularExpressionMatchIterator>
#include <QStringList>
#include <QtGlobal>

static QString makeHtmlSafeJson(const QJsonDocument &doc)
{
    QString json = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
    // Keep JSON valid while preventing inline HTML parsing from breaking out.
    json.replace("<", "\\u003C");
    json.replace(">", "\\u003E");
    json.replace("&", "\\u0026");
    json.replace(QChar(0x2028), "\\u2028");
    json.replace(QChar(0x2029), "\\u2029");
    return json;
}

static QJsonObject makeEntryPayload(const TranslationEntry &entry)
{
    QJsonObject payload;
    payload["id"] = entry.id;
    payload["time"] = entry.timestamp.toString("yyyy-MM-dd HH:mm:ss");
    payload["originalRaw"] = entry.translatedMarkdown;
    payload["tags"] = QJsonArray::fromStringList(entry.tags);
    return payload;
}

void SummaryWindow::refreshHtml(bool preserveScroll)
{
    if (preserveScroll)
        captureScrollPosition();
    else
        m_lastScrollY = 0.0;

    initHtml();
}

QList<TranslationEntry> SummaryWindow::applyPagination(const QList<TranslationEntry> &entries)
{
    m_filteredEntryCount = entries.size();
    if (!m_archiveUsePagination)
    {
        m_currentPage = 1;
        m_totalPages = 1;
        updatePaginationUi();
        return entries;
    }

    const int pageSize = qMax(1, m_archivePageSize);
    m_totalPages = qMax(1, (m_filteredEntryCount + pageSize - 1) / pageSize);
    m_currentPage = qBound(1, m_currentPage, m_totalPages);

    const int start = (m_currentPage - 1) * pageSize;
    updatePaginationUi();
    return entries.mid(start, pageSize);
}

QList<TranslationEntry> SummaryWindow::getEntriesForCurrentView()
{
    if (m_historyManager)
    {
        const QDate fromDate = m_fromDateEdit ? m_fromDateEdit->date() : QDate();
        const QDate toDate = m_toDateEdit ? m_toDateEdit->date() : QDate();
        const QString searchText = m_searchEdit ? m_searchEdit->text().trimmed() : QString();

        if (!m_archiveUsePagination)
        {
            int totalCount = 0;
            QList<TranslationEntry> rows =
                m_historyManager->queryEntries(fromDate, toDate, m_selectedTags, searchText, 0, 0, &totalCount);
            m_filteredEntryCount = totalCount;
            m_currentPage = 1;
            m_totalPages = 1;
            updatePaginationUi();
            return rows;
        }

        const int pageSize = qMax(1, m_archivePageSize);
        m_currentPage = qMax(1, m_currentPage);

        int totalCount = 0;
        int offset = (m_currentPage - 1) * pageSize;
        QList<TranslationEntry> rows =
            m_historyManager->queryEntries(fromDate, toDate, m_selectedTags, searchText, pageSize, offset, &totalCount);

        m_filteredEntryCount = totalCount;
        m_totalPages = qMax(1, (m_filteredEntryCount + pageSize - 1) / pageSize);
        if (m_currentPage > m_totalPages)
        {
            m_currentPage = m_totalPages;
            offset = (m_currentPage - 1) * pageSize;
            rows = m_historyManager->queryEntries(fromDate, toDate, m_selectedTags, searchText, pageSize, offset, nullptr);
        }
        updatePaginationUi();
        return rows;
    }

    return applyPagination(getFilteredEntries());
}

void SummaryWindow::updatePaginationUi()
{
    if (!m_paginationGroup)
        return;

    m_paginationGroup->setVisible(m_archiveUsePagination);
    if (!m_archiveUsePagination)
        return;

    if (m_prevPageBtn)
        m_prevPageBtn->setEnabled(m_currentPage > 1);
    if (m_nextPageBtn)
        m_nextPageBtn->setEnabled(m_currentPage < m_totalPages);
    if (m_pageInfoLabel)
    {
        const QString pageText = TranslationManager::instance().tr("archive_page_indicator")
                                     .arg(m_currentPage)
                                     .arg(m_totalPages)
                                     .arg(m_filteredEntryCount);
        m_pageInfoLabel->setText(pageText);
    }
}

QString SummaryWindow::getAddEntryJs(const TranslationEntry &entry)
{
    const QString entryJson = makeHtmlSafeJson(QJsonDocument(makeEntryPayload(entry)));
    QString js = "addEntryFromNative(";
    js += entryJson;
    js += ",";
    js += (m_selectionMode ? "true" : "false");
    js += ");";
    return js;
}

void SummaryWindow::initHtml()
{
    if (!m_webView)
        return;

    bool isDark = ThemeUtils::isSystemDark();
    const bool showAdvancedDebug = m_config.useAdvancedApiMode && m_config.showAdvancedDebugInArchiveWindow;

    // If the HTML shell has already been loaded once, avoid calling setHtml() again (it is intentionally guarded
    // in EmbedWebView to prevent reload-induced focus issues). Instead, update entries via JS.
    if (m_htmlLoaded)
    {
        QList<TranslationEntry> filteredEntries = getEntriesForCurrentView();

        QString js;
        js += "(()=>{";
        js += QString("try{applyDarkMode(%1);}catch(e){};").arg(isDark ? "true" : "false");
        js += QString("try{SHOW_ADV_DEBUG_ARCHIVE=%1;}catch(e){};").arg(showAdvancedDebug ? "true" : "false");
        js += QString("try{SELECTION_MODE=%1;}catch(e){};").arg(m_selectionMode ? "true" : "false");
        js += "try{document.querySelectorAll('.entry').forEach(function(n){n.remove();});}catch(e){};";
        for (const auto &entry : filteredEntries)
        {
            js += getAddEntryJs(entry) + ";";
        }
        if (m_currentZoom != 1.0)
        {
            js += QString("try{document.body.style.zoom='%1';}catch(e){};").arg(m_currentZoom);
        }
        js += "})();";

        m_webView->eval(js.toStdString());
        if (m_lastScrollY > 0.0)
            m_webView->eval(QString("window.scrollTo(0,%1);").arg(m_lastScrollY).toStdString());
        return;
    }

    QString html = R"RAW_HTML(<!DOCTYPE html>
<html class="__HTML_CLASS__">
<head>
<meta charset='UTF-8'>
<meta name='viewport' content='width=device-width, initial-scale=1.0'>
<script src='%1'></script>
<link rel='stylesheet' href='%2'>
<script src='%3'></script>
<script src='%4'></script>
<style>
html, body {
  background: #ffffff;
  color: #111111;
}

/* Avoid horizontal overflow artifacts (RAW/EDIT right edge) */
html, body { overflow-x: hidden; }
*, *::before, *::after { box-sizing: border-box; }

:root {
            --mark-bg: __MARK_BG__;
            --mark-bg-dark: __MARK_BG_DARK__;
    --mark-fg: #000;

    --link-color: #0645ad;
    --link-color-visited: #0b0080;
    --link-color-dark: #78b7e6;
    --link-color-dark-visited: #78b7e6;
}

a { color: var(--link-color); }
a:visited { color: var(--link-color-visited); }
a:hover { text-decoration: underline; }

mark { background: var(--mark-bg); color: var(--mark-fg); }
body.dark-mode mark { background: var(--mark-bg-dark); color: var(--mark-fg); }
body { font-family: sans-serif; padding: 8px; }
__STATUS_INDICATOR_CSS__
.entry { margin-bottom: 12px; padding: 8px; background: #f7f7f7; color: #111111; border: 1px solid #ddd; border-radius: 4px; }
.entry.mode-edit { outline: 1px solid #777; }
.entry-header { display: flex; align-items: center; gap: 6px; margin-bottom: 6px; }
.selection-checkbox { margin-right: 6px; }
.raw-text { width: 100%; max-width: 100%; min-height: 80px; white-space: pre-wrap; background: #ffffff; color: #111111; border: 1px solid #ccc; border-radius: 4px; padding: 8px; overflow-wrap: anywhere; }
.rendered-html { color: #111111; }
html.dark-mode, body.dark-mode { background: #1e1e1e !important; color: #e0e0e0 !important; }
body.dark-mode .entry { background: #2a2a2a !important; color: #e0e0e0 !important; border-color: #444 !important; }
body.dark-mode .entry-header { color: #e0e0e0 !important; }
body.dark-mode .raw-text { background: #1f1f1f !important; color: #e0e0e0 !important; border-color: #555 !important; }
body.dark-mode .rendered-html { color: #e0e0e0 !important; }
body.dark-mode .katex,
body.dark-mode .katex * { color: #e0e0e0 !important; }

body.dark-mode a,
body.dark-mode a:visited {
    color: var(--link-color-dark) !important;
}

body.dark-mode a:hover {
    color: var(--link-color-dark) !important;
    text-decoration: underline;
}
.entry.selected { outline: 2px solid #3d8bfd; }
.selection-rect {
  position: fixed;
  border: 1px dashed #3d8bfd;
  background: rgba(61,139,253,0.1);
  pointer-events: none;
  z-index: 9998;
}
</style>
<script>
const IS_DARK = __IS_DARK__;
function applyDarkMode(d) {
  document.documentElement.classList.toggle('dark-mode', d);
  if (document.body) {
    document.body.classList.toggle('dark-mode', d);
  } else {
    document.addEventListener('DOMContentLoaded', () => applyDarkMode(d), { once: true });
  }
}
applyDarkMode(IS_DARK);
try {
        document.documentElement.style.setProperty('--mark-bg', '__MARK_BG__');
        document.documentElement.style.setProperty('--mark-bg-dark', '__MARK_BG_DARK__');
} catch(e) {}
function log(msg){
  try { if (window.cmd_log) window.cmd_log(JSON.stringify([msg])); }
  catch(e){}
}
let SELECTION_MODE = false;
let lastScrollReport = 0;
window.addEventListener('scroll', function() {
  if (!window.cmd_scroll) return;
  const now = Date.now();
  if (now - lastScrollReport < 50) return;
  lastScrollReport = now;
  window.cmd_scroll(window.scrollY);
}, { passive: true });

// When the user clicks inside the content area, clear focus from the Qt toolbar (if any).
// This is needed because the WebView is a native window and Qt won't receive those mouse events.
document.addEventListener('mousedown', function() {
    try { if (window.cmd_clearToolbarFocus) window.cmd_clearToolbarFocus(); } catch(e) {}
}, true);
</script>
)RAW_HTML";

    auto loadAsset = [](const QString &relativePath) -> QString
    {
        QString path = QCoreApplication::applicationDirPath() + "/assets/" + relativePath;
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            return "";
        }
        return QString::fromUtf8(file.readAll());
    };

    QString jsMarked = loadAsset("libs/marked.min.js");
    QString cssKatex = loadAsset("libs/katex.min.css");
    QString jsKatex = loadAsset("libs/katex.min.js");
    QString jsKatexAuto = loadAsset("libs/auto-render.min.js");
    QString statusIndicatorCss = loadAsset("templates/status_indicator.css");

    auto embedFonts = [](QString css) -> QString
    {
        QString fontDir = QCoreApplication::applicationDirPath() + "/assets/libs/fonts/";
        QRegularExpression regex("url\\(fonts/([^)]+)\\)");
        QRegularExpressionMatchIterator it = regex.globalMatch(css);
        QString result = css;
        while (it.hasNext())
        {
            QRegularExpressionMatch match = it.next();
            QString filename = match.captured(1);
            QString extension = QFileInfo(filename).suffix().toLower();
            if (extension != "woff2")
            {
                result.replace(match.captured(0), "url(data:application/x-font-placeholder;base64,AAA=)");
                continue;
            }
            QString filePath = fontDir + filename;
            QFile fontFile(filePath);
            if (fontFile.open(QIODevice::ReadOnly))
            {
                QString base64 = fontFile.readAll().toBase64();
                QString dataUri = QString("url(data:font/woff2;base64,%1)").arg(base64);
                result.replace(match.captured(0), dataUri);
            }
        }
        return result;
    };

    cssKatex = embedFonts(cssKatex);

    html = html.replace("<script src='%1'></script>", "<script>\n" + jsMarked + "\n</script>");
    html = html.replace("<link rel='stylesheet' href='%2'>", "<style>\n" + cssKatex + "\n</style>");
    html = html.replace("<script src='%3'></script>", "<script>\n" + jsKatex + "\n</script>");
    html = html.replace("<script src='%4'></script>", "<script>\n" + jsKatexAuto + "\n</script>");
    html = html.replace("__IS_DARK__", isDark ? "true" : "false");
    html = html.replace("__HTML_CLASS__", isDark ? "dark-mode" : "");
    html = html.replace("__STATUS_INDICATOR_CSS__", statusIndicatorCss.isEmpty() ? "" : statusIndicatorCss);

    const QString mark = ColorUtils::normalizeCssColor(m_config.highlightMarkColor, "#ffeb3b");
    const QString markDark = ColorUtils::normalizeCssColor(m_config.highlightMarkColorDark, "#d4af37");
    html = html.replace("__MARK_BG__", mark);
    html = html.replace("__MARK_BG_DARK__", markDark);

    // Static JS Logic
    html += "<script>";
    html += "if (typeof marked !== 'undefined') { marked.setOptions({ breaks: true, gfm: true }); }\n";
    html += QString("var KEY_EDIT = '%1';\n").arg(m_editKey);
    html += QString("var KEY_VIEW = '%1';\n").arg(m_viewKey);
    html += QString("var KEY_SHOT = '%1';\n").arg(m_screenshotKey);
    html += QString("var KEY_SELECT = '%1';\n").arg(m_selectionToggleKey);
    html += QString("var KEY_BOLD = '%1';\n").arg(m_boldKey);
    html += QString("var KEY_UNDERLINE = '%1';\n").arg(m_underlineKey);
    html += QString("var KEY_HIGHLIGHT = '%1';\n").arg(m_highlightKey);
    html += QString("var RESTORE_SCROLL = %1;\n").arg(m_lastScrollY);
    html += QString("var SHOW_ADV_DEBUG_ARCHIVE = %1;\n").arg(showAdvancedDebug ? "true" : "false");

    QString summaryLogic = loadAsset("templates/summary_logic.js");
    if (summaryLogic.isEmpty())
    {
        summaryLogic = "/* missing assets/templates/summary_logic.js */";
    }
    html += summaryLogic;
    html += "\n</script>";

    html += "</head><body class=\"__BODY_CLASS__\">";
    html = html.replace("__BODY_CLASS__", isDark ? "dark-mode" : "");
    QList<TranslationEntry> filteredEntries = getEntriesForCurrentView();
    QJsonArray initialEntries;
    for (const auto &entry : filteredEntries)
    {
        initialEntries.append(makeEntryPayload(entry));
    }
    html += "<div id='status-indicator'>MODE: VIEW</div>";
    html += "<script id='initial-entry-data' type='application/json'>";
    html += makeHtmlSafeJson(QJsonDocument(initialEntries));
    html += "</script>";
    html += "<script>";
    html += "try { bootstrapEntriesFromNative('initial-entry-data', ";
    html += (m_selectionMode ? "true" : "false");
    html += "); } catch(e) { console.error('bootstrapEntriesFromNative failed', e); }";
    // Apply zoom
    if (m_currentZoom != 1.0)
    {
        html += QString("document.body.style.zoom = '%1';").arg(m_currentZoom);
    }
    html += "</script>";

    html += "</body></html>";

    m_webView->setHtml(html.toStdString());
    m_htmlLoaded = true;
    m_webView->focusNative();
    QString toggleJs = QString("applyDarkMode(%1);").arg(isDark ? "true" : "false");
    m_webView->eval(toggleJs.toStdString());
    if (m_lastScrollY > 0.0)
    {
        m_webView->eval(QString("window.scrollTo(0,%1);").arg(m_lastScrollY).toStdString());
    }
}

void SummaryWindow::appendEntryHtml(const TranslationEntry &entry)
{
    if (!m_webView)
        return;
    QString js = getAddEntryJs(entry);
    m_webView->eval(js.toStdString());
}
