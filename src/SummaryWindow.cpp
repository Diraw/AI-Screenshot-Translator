#include "SummaryWindow.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QCloseEvent>
#include <QUuid>

#include <QIcon>
#include <QMenu>
#include <QAction>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QTimer>
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QRegularExpression>
#include <QJsonArray>
#include <QJsonDocument>

#include "TranslationManager.h"
#include "HistoryManager.h"

#include "EmbedWebView.h"
#include "TagDialog.h"
#include <QMessageBox>


SummaryWindow::SummaryWindow(QWidget *parent) : QWidget(parent) {
    setWindowTitle(TranslationManager::instance().tr("summary_title"));
    setWindowIcon(QIcon(":/assets/icon.ico"));
    resize(1000, 750); // Landscape default as requested
    
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    // Create container for webview
    m_webContainer = new QWidget(this);
    m_webContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(m_webContainer);

    // EmbedWebView handles native window attachment
    m_webView = std::make_unique<EmbedWebView>(m_webContainer);

    // Bind 'cmd_restore'
    m_webView->bind("cmd_restore", [this](std::string seq, std::string req, void* arg) {
        QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(req));
        if (doc.isArray() && !doc.array().isEmpty()) {
            QString id = doc.array().at(0).toString();
            emit restorePreviewRequested(id);
        }
    });

    // Bind 'cmd_delete'
    m_webView->bind("cmd_delete", [this](std::string seq, std::string req, void* arg) {
        QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(req));
        if (doc.isArray() && !doc.array().isEmpty()) {
            QString id = doc.array().at(0).toString();
            // Remove from entries list
            for (int i = 0; i < m_entries.size(); ++i) {
                if (m_entries[i].id == id) {
                    m_entries.removeAt(i);
                    break;
                }
            }
            emit requestDeleteEntry(id);
        }
    });
    
    // Bind 'cmd_updateEntry'
    m_webView->bind("cmd_updateEntry", [this](std::string seq, std::string req, void* arg) {
        // req is JSON array [id, content]
        QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(req));
        if (doc.isArray()) {
            QJsonArray arr = doc.array();
            if (arr.size() >= 2) {
                QString id = arr.at(0).toString();
                QString content = arr.at(1).toString();
                emit entryEdited(id, content);
            }
        }
    });
    
    auto parseIdsFromReq = [](const std::string& req) -> QStringList {
        QStringList ids;
        QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(req));
        if (!doc.isArray()) return ids;

        QJsonArray arr = doc.array();
        // Case A: ["id1", "id2"] - multiple args passed from JSspread
        // Case B: ["[\"id1\", \"id2\"]"] - user's reported "double encoded" case
        
        for (const auto& v : arr) {
            QString s = v.toString();
            if (s.startsWith("[") && s.endsWith("]")) {
                // Try to parse inner array
                QJsonDocument innerDoc = QJsonDocument::fromJson(s.toUtf8());
                if (innerDoc.isArray()) {
                    for (const auto& iv : innerDoc.array()) ids << iv.toString();
                    continue; 
                }
            }
            if (v.isString()) ids << s;
            else if (v.isArray()) {
                for (const auto& iv : v.toArray()) ids << iv.toString();
            }
        }
        return ids;
    };

    // Bind batch operations
    m_webView->bind("cmd_batchDelete", [this, parseIdsFromReq](std::string seq, std::string req, void* arg) {
        qDebug() << "cmd_batchDelete triggered. Raw request:" << QString::fromStdString(req);
        QStringList ids = parseIdsFromReq(req);
        qDebug() << "Extracted IDs count:" << ids.size() << "IDs:" << ids;
        if (ids.isEmpty()) return;
        
        auto reply = QMessageBox::question(this, tr("Delete"), tr("Are you sure you want to delete %1 selected entries?").arg(ids.size()));
        if (reply == QMessageBox::Yes) {
            if (m_historyManager && m_historyManager->deleteEntries(ids)) {
                qDebug() << "Batch delete successful. Updating local list.";
                QSet<QString> idSet = QSet<QString>(ids.begin(), ids.end());
                for (int i = m_entries.size() - 1; i >= 0; --i) {
                    if (idSet.contains(m_entries[i].id)) m_entries.removeAt(i);
                }
                applyFilters(); // Refresh UI
            } else {
                qDebug() << "Batch delete failed in HistoryManager.";
            }
        }
    });

    m_webView->bind("cmd_batchAddTags", [this, parseIdsFromReq](std::string seq, std::string req, void* arg) {
        qDebug() << "cmd_batchAddTags triggered. Raw request:" << QString::fromStdString(req);
        QStringList ids = parseIdsFromReq(req);
        qDebug() << "Extracted IDs count:" << ids.size() << "IDs:" << ids;
        if (ids.isEmpty() || !m_historyManager) return;

        QStringList allTags = m_historyManager->getAllTags();
        TagDialog *dialog = new TagDialog(allTags, QStringList(), this);
        connect(dialog, &TagDialog::tagsUpdated, this, [this, ids](const QStringList& tags) {
            qDebug() << "Batch add tags - user selected tags:" << tags;
            if (m_historyManager->addTagsToEntries(ids, tags)) {
                qDebug() << "Batch add tags successful. Reloading entries.";
                m_entries = m_historyManager->loadEntries();
                applyFilters();
                loadAvailableTags();
            } else {
                qDebug() << "Batch add tags returned false (maybe no changes needed or failed).";
            }
        });
        dialog->exec();
        dialog->deleteLater();
    });

    m_webView->bind("cmd_batchRemoveTags", [this, parseIdsFromReq](std::string seq, std::string req, void* arg) {
        qDebug() << "cmd_batchRemoveTags triggered. Raw request:" << QString::fromStdString(req);
        QStringList ids = parseIdsFromReq(req);
        qDebug() << "Extracted IDs count:" << ids.size() << "IDs:" << ids;
        if (ids.isEmpty() || !m_historyManager) return;

        QStringList allTags = m_historyManager->getAllTags();
        TagDialog *dialog = new TagDialog(allTags, QStringList(), this);
        connect(dialog, &TagDialog::tagsUpdated, this, [this, ids](const QStringList& tags) {
            qDebug() << "Batch remove tags - user selected tags:" << tags;
            if (m_historyManager->removeTagsFromEntries(ids, tags)) {
                qDebug() << "Batch remove tags successful. Reloading entries.";
                m_entries = m_historyManager->loadEntries();
                applyFilters();
                loadAvailableTags();
            } else {
                qDebug() << "Batch remove tags returned false (maybe no changes needed or failed).";
            }
        });
        dialog->exec();
        dialog->deleteLater();
    });

    // We need to initialize HTML after webview is ready.
    QTimer::singleShot(0, this, [this](){
        initHtml();
        // Force an initial resize to ensure WebView matches container
        QTimer::singleShot(100, this, [this](){
            if (m_webView && m_webContainer) {
                qDebug() << "Forcing initial WebView size:" << m_webContainer->width() << "x" << m_webContainer->height();
                m_webView->setSize(m_webContainer->width(), m_webContainer->height());
            }
        });
    });
    
    // Setup filter toolbar
    setupFilterUI();
    
    // Restore window state from previous session
    restoreState();
    
    // Initial Theme Update
    updateTheme(false);
}

SummaryWindow::~SummaryWindow() {
    // unique_ptr handles cleanup
}

void SummaryWindow::resizeEvent(QResizeEvent *event) {
    qDebug() << "SummaryWindow::resizeEvent - Window:" << width() << "x" << height() 
             << "Container:" << (m_webContainer ? m_webContainer->size() : QSize(0,0));
    QWidget::resizeEvent(event);
}

// closeEvent restored
void SummaryWindow::closeEvent(QCloseEvent *event) {
    saveState();
    emit closed();
    hide(); // Just hide instead of close
    event->ignore(); // Don't actually close, just hide
}

void SummaryWindow::setInitialHistory(const QList<TranslationEntry>& history) {
    m_entries = history;
    refreshHtml();
}

void SummaryWindow::addEntry(const TranslationEntry& entry) {
    m_entries.append(entry);
    appendEntryHtml(entry);
}

void SummaryWindow::clearEntries() {
    m_entries.clear();
    refreshHtml();
}

const TranslationEntry* SummaryWindow::getEntry(const QString& id) const {
    for (const auto& entry : m_entries) {
        if (entry.id == id) return &entry;
    }
    return nullptr;
}

void SummaryWindow::updateEntryGeometry(const QString& id, const QPoint& pos, const QSize& size) {
    for (auto& entry : m_entries) {
        if (entry.id == id) {
            entry.lastPosition = pos;
            entry.lastSize = size;
            entry.hasLastPosition = true;
            break;
        }
    }
}

void SummaryWindow::updateEntry(const QString& id, const QString& markdown) {
    // Alias for updateEntryContent to match the API calls from App.cpp
    updateEntryContent(id, markdown);
}

void SummaryWindow::updateEntryContent(const QString& id, const QString& markdown) {
    for (auto& entry : m_entries) {
        if (entry.id == id) {
            entry.translatedMarkdown = markdown;
            break;
        }
    }
    if (m_webView) {
        // Simple unescape for JS injection
        QString safeMd = markdown;
        safeMd.replace("\\", "\\\\").replace("'", "\\'").replace("\n", "\\n").replace("\r", "");
        QString js = QString("updateEntryInDom('%1', '%2')").arg(id, safeMd);
        m_webView->eval(js.toStdString());
    }
}

void SummaryWindow::setZoomFactor(qreal factor) {
    m_currentZoom = factor;
    if (m_webView) {
         m_webView->eval("document.body.style.zoom = '" + std::to_string(factor) + "'");
    }
}

qreal SummaryWindow::getZoomFactor() const {
    return m_currentZoom;
}

// Duplicate closeEvent removed

// Helper struct for math protection
struct ProtectedContent {
    QString text;
    QStringList mathBlocks;
};

static ProtectedContent protectMath(const QString &markdown) {
    ProtectedContent result;
    result.text = markdown;
    
    QRegularExpression finalRegex(R"((\$\$[\s\S]*?\$\$)|(\\\[[\s\S]*?\\\])|(\\\([\s\S]*?\\\))|(\$[^\$\n]+\$))");

    int counter = 0;
    QString newText;
    int lastPos = 0;
    
    QRegularExpressionMatchIterator i = finalRegex.globalMatch(result.text);
    while (i.hasNext()) {
        QRegularExpressionMatch match = i.next();
        newText.append(result.text.mid(lastPos, match.capturedStart() - lastPos));
        
        QString matchStr = match.captured();
        result.mathBlocks.append(matchStr);
        
        QString placeholder = QString("MATHBLOCKPH%1").arg(counter++);
        newText.append(placeholder);
        
        lastPos = match.capturedEnd();
    }
    newText.append(result.text.mid(lastPos));
    result.text = newText;
    return result;
}

void SummaryWindow::saveState() {
    QSettings settings("YourCompany", "AIScreenshotTranslator");
    settings.setValue("summaryWindow/geometry", saveGeometry());
    settings.setValue("summaryWindow/zoom", m_currentZoom);
    
    // Save scroll position via JavaScript
    if (m_webView) {
        QString jsGetScroll = "window.scrollY;";
        // Note: eval doesn't return values directly, would need binding for this
        // For now just save geometry
    }
}

void SummaryWindow::restoreState() {
    QSettings settings("YourCompany", "AIScreenshotTranslator");
    
    // Restore geometry
    QByteArray geom = settings.value("summaryWindow/geometry").toByteArray();
    if (!geom.isEmpty()) {
        restoreGeometry(geom);
    }
    
    // Restore zoom
    qreal zoom = settings.value("summaryWindow/zoom", 1.0).toReal();
    setZoomFactor(zoom);
}

void SummaryWindow::configureHotkeys(const QString& editKey, const QString& viewKey, const QString& screenshotKey,
                                     const QString& boldKey, const QString& underlineKey, const QString& highlightKey) {
    m_editKey = editKey.toLower();
    m_viewKey = viewKey.toLower();
    m_screenshotKey = screenshotKey.toLower();
    m_boldKey = boldKey.toLower();
    m_underlineKey = underlineKey.toLower();
    m_highlightKey = highlightKey.toLower();
    refreshHtml();
}

void SummaryWindow::refreshHtml() {
    initHtml();
}

QString SummaryWindow::getAddEntryJs(const TranslationEntry& entry) {
    QString originalMarkdown = entry.translatedMarkdown;
    ProtectedContent protectedData = protectMath(originalMarkdown);
    
    QString safeContent = protectedData.text;
    safeContent.replace("\\", "\\\\").replace("\"", "\\\"").replace("\n", "\\n").replace("\r", "");
    
    QString mathBlocksJs = "[";
    for (int i=0; i<protectedData.mathBlocks.size(); ++i) {
         QString b = protectedData.mathBlocks[i];
         b.replace("\\", "\\\\").replace("\"", "\\\"").replace("\n", "\\n").replace("\r", "");
         if (i>0) mathBlocksJs += ",";
         mathBlocksJs += "\"" + b + "\"";
    }
    mathBlocksJs += "]";
    
    QString originalSafe = originalMarkdown;
    originalSafe.replace("\\", "\\\\").replace("\"", "\\\"").replace("\n", "\\n").replace("\r", "");
    
    QString tagsJs = "[";
    for (int i=0; i<entry.tags.size(); ++i) {
        if (i>0) tagsJs += ",";
        QString t = entry.tags[i];
        t.replace("\\", "\\\\").replace("\"", "\\\"");
        tagsJs += "\"" + t + "\"";
    }
    tagsJs += "]";
    
    return QString("addEntryToDom('%1', '%2', \"%3\", %4, \"%5\", %6, %7);")
        .arg(entry.id)
        .arg(entry.timestamp.toString("yyyy-MM-dd HH:mm:ss"))
        .arg(safeContent)
        .arg(mathBlocksJs)
        .arg(originalSafe)
        .arg(m_selectionMode ? "true" : "false")
        .arg(tagsJs);
}

void SummaryWindow::initHtml() {
    if (!m_webView) return;

    QString html = R"RAW_HTML(<!DOCTYPE html><html><head><meta charset='UTF-8'>
<meta name='viewport' content='width=device-width, initial-scale=1.0'>
<style>
body { font-family: sans-serif; padding: 8px; }
.entry { margin-bottom: 12px; padding: 8px; }
.entry.mode-edit { outline: 1px solid #777; }
.entry-header { display: flex; align-items: center; gap: 6px; margin-bottom: 6px; }
.selection-checkbox { margin-right: 6px; }
.raw-text { width: 100%; min-height: 80px; white-space: pre-wrap; }
#status-indicator { margin-bottom: 8px; font-size: 12px; }
</style>
<script src='%1'></script>
<link rel='stylesheet' href='%2'>
<script src='%3'></script>
<script src='%4'></script>
<script>
)RAW_HTML";

    auto loadAsset = [](const QString& name) -> QString {
        QString path = QCoreApplication::applicationDirPath() + "/assets/libs/" + name;
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return "";
        }
        return QString::fromUtf8(file.readAll());
    };

    QString jsMarked = loadAsset("marked.min.js");
    QString cssKatex = loadAsset("katex.min.css");
    QString jsKatex = loadAsset("katex.min.js");
    QString jsKatexAuto = loadAsset("auto-render.min.js");

    auto embedFonts = [](QString css) -> QString {
        QString fontDir = QCoreApplication::applicationDirPath() + "/assets/libs/fonts/";
        QRegularExpression regex("url\\(fonts/([^)]+)\\)");
        QRegularExpressionMatchIterator it = regex.globalMatch(css);
        QString result = css;
        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            QString filename = match.captured(1);
            QString extension = QFileInfo(filename).suffix().toLower();
            if (extension != "woff2") {
                result.replace(match.captured(0), "url(data:application/x-font-placeholder;base64,AAA=)");
                continue;
            }
            QString filePath = fontDir + filename;
            QFile fontFile(filePath);
            if (fontFile.open(QIODevice::ReadOnly)) {
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

    html += QString("var KEY_EDIT = '%1';\n").arg(m_editKey);
    html += QString("var KEY_VIEW = '%1';\n").arg(m_viewKey);
    html += QString("var KEY_SHOT = '%1';\n").arg(m_screenshotKey);
    html += QString("var KEY_BOLD = '%1';\n").arg(m_boldKey);
    html += QString("var KEY_UNDERLINE = '%1';\n").arg(m_underlineKey);
    html += QString("var KEY_HIGHLIGHT = '%1';\n").arg(m_highlightKey);

    // Static JS Logic
    html += R"JSCODE(
function updateStatus(mode) {
   var ind = document.getElementById('status-indicator');
   if (ind) {
       if (mode === 'edit') ind.innerText = 'MODE: EDIT';
       else if (mode === 'raw') ind.innerText = 'MODE: RAW';
       else ind.innerText = 'MODE: VIEW';
   }
}

function protectMathJs(text) {
   var blocks = [];
   var counter = 0;
   var regex = /(\$\$[\s\S]*?\$\$)|(\\\[[\s\S]*?\\\])|(\\\([\s\S]*?\\\))|(\$[^\$\n]+\$)/g;
   var protectedText = text.replace(regex, function(match) {
       var ph = 'MATHBLOCKPH' + counter++;
       blocks.push(match);
       return ph;
   });
   return {text: protectedText, blocks: blocks};
}

function renderContent(id) {
   var raw = document.getElementById('raw_' + id);
   var rendered = document.getElementById('rendered_' + id);
   var markdown = raw.innerText;
   var p = protectMathJs(markdown);
   var html = marked.parse(p.text);
   p.blocks.forEach(function(block, index) {
       html = html.replace('MATHBLOCKPH' + index, block);
   });
   rendered.innerHTML = html;
   requestAnimationFrame(function() {
     renderMathInElement(rendered, {delimiters: [{left: '$$', right: '$$', display: true}, {left: '$', right: '$', display: false}, {left: '\\(', right: '\\)', display: false}, {left: '\\[', right: '\\]', display: true}], throwOnError : false});
   });
}

function insertMarkdown(startTag, endTag) {
   var sel = window.getSelection();
   if (!sel.rangeCount) return;
   var range = sel.getRangeAt(0);
   var text = range.toString();
   document.execCommand('insertText', false, startTag + text + endTag);
}

function matchHotkey(e, hotkeyStr) {
   var parts = hotkeyStr.split('+');
   var key = parts[parts.length-1].toLowerCase();
   var ctrl = parts.includes('ctrl');
   var alt = parts.includes('alt');
   var shift = parts.includes('shift');
   return (e.key.toLowerCase() === key && e.ctrlKey === ctrl && e.altKey === alt && e.shiftKey === shift);
}

function toggleView(id) {
  var raw = document.getElementById('raw_' + id);
  var rendered = document.getElementById('rendered_' + id);
  if (raw.style.display === 'none') {
    raw.style.display = 'block';
    rendered.style.display = 'none';
    updateStatus('raw');
  } else {
    raw.style.display = 'none';
    rendered.style.display = 'block';
    updateStatus('view');
  }
}

function toggleEdit(entry) {
   var id = entry.getAttribute('data-id');
   var rendered = document.getElementById('rendered_' + id);
   var raw = document.getElementById('raw_' + id);
   
   if (entry.classList.contains('mode-edit')) {
       entry.classList.remove('mode-edit');
       entry.classList.add('mode-view');
       updateStatus('view');
       raw.contentEditable = 'false';
       renderContent(id);
       raw.style.display = 'none';
       rendered.style.display = 'block';
       
       // Notify C++ of edit
       // Pass as multiple arguments to the bridge
       window.cmd_updateEntry(id, raw.innerText);

       entry.focus();
   } else {
       entry.classList.remove('mode-view');
       entry.classList.add('mode-edit');
       updateStatus('edit');
       rendered.style.display = 'none';
       raw.style.display = 'block';
       raw.contentEditable = 'true';
       raw.focus();
   }
}

document.addEventListener('focusin', function(e) {
   var entry = e.target.closest('.entry');
   if (entry) {
       if (entry.classList.contains('mode-edit')) updateStatus('edit');
       else updateStatus('view');
   } else { updateStatus('view'); }
});

document.addEventListener('keydown', function(e) {
   var active = document.activeElement;
   if (!active) return;
   var entry = active.closest('.entry');
   if (!entry) return;
   
   var isEditing = entry.classList.contains('mode-edit');
   var k = e.key.toLowerCase();

   if (matchHotkey(e, KEY_EDIT) && !isEditing) {
       e.preventDefault(); toggleEdit(entry); return; 
   }
   
   if (isEditing) {
      if (e.key === 'Escape') {
          e.preventDefault(); toggleEdit(entry); return;
      }
      if (matchHotkey(e, KEY_BOLD)) {
          e.preventDefault(); insertMarkdown('**', '**'); return;
      }
      if (matchHotkey(e, KEY_UNDERLINE)) {
          e.preventDefault(); insertMarkdown('<u>', '</u>'); return;
      }
      if (matchHotkey(e, KEY_HIGHLIGHT)) {
          e.preventDefault(); insertMarkdown('<mark>', '</mark>'); return;
      }
      return; 
    }
    if (!isEditing) {
        if (k === KEY_VIEW) { toggleView(entry.getAttribute('data-id')); e.preventDefault(); return; }
        if (k === KEY_SHOT) { 
            // Call native webview binding
            window.cmd_restore(entry.getAttribute('data-id'));
            e.preventDefault(); return; 
        }
        
        // Handle 'dd' for deletion
        if (k === 'd') {
            e.preventDefault();
            var now = Date.now();
            if (entry.lastDTime && (now - entry.lastDTime < 500)) {
                 // Double press detected
                 var id = entry.getAttribute('data-id');
                 // No confirm dialog as requested
                 window.cmd_delete(id);
                 entry.remove();
                 entry.lastDTime = 0;
            } else {
                 entry.lastDTime = now;
            }
            return;
        }
    }
});

function updateEntryInDom(id, newMarkdown) {
   var raw = document.getElementById('raw_' + id);
   if (raw) {
       raw.innerText = newMarkdown;
       var entry = raw.closest('.entry');
       if (entry && !entry.classList.contains('mode-edit')) {
           renderContent(id);
       }
   }
}

function addEntryToDom(id, time, markdown, mathBlocks, originalRaw, isSelectionMode, tags) {
  var div = document.createElement('div');
  div.className = 'entry mode-view';
  div.id = 'entry_' + id;
  div.setAttribute('data-id', id);
  div.tabIndex = 0;

  var checkboxDisplay = isSelectionMode ? 'block' : 'none';

  var tagText = tags.length ? `Tags: ${tags.join(', ')}` : '';

  div.innerHTML = `
    <div class='entry-header'>
        <input type='checkbox' class='selection-checkbox' style='display: ${checkboxDisplay}' data-id='${id}'>
        <div class='entry-info'>
            <div>${time}</div>
            ${tagText ? `<div>${tagText}</div>` : ''}
        </div>
    </div>
    <div class='content-area'>
        <div id='rendered_${id}' class='rendered-html'></div>
        <div id='raw_${id}' class='raw-text' style='display:none;' spellcheck='false'></div>
    </div>
  `;
  document.body.appendChild(div);
  
  var rawContainer = document.getElementById('raw_' + id);
  if (rawContainer) {
      rawContainer.innerText = originalRaw; 
      renderContent(id);
  }
}

function toggleSelectionMode(show) {
    var checkboxes = document.querySelectorAll('.selection-checkbox');
    checkboxes.forEach(cb => {
        cb.style.display = show ? 'block' : 'none';
        if (!show) cb.checked = false;
    });
}

function getSelectedIds() {
    var checkboxes = document.querySelectorAll('.selection-checkbox:checked');
    return Array.from(checkboxes).map(cb => cb.getAttribute('data-id'));
}

function selectAllEntries(select) {
    var checkboxes = document.querySelectorAll('.selection-checkbox');
    checkboxes.forEach(cb => {
        cb.checked = select;
    });
}
</script>
)JSCODE";

    html += "</head><body>";
    html += "<div id='status-indicator'>MODE: VIEW</div>";
    html += "<script>";
    QList<TranslationEntry> filteredEntries = getFilteredEntries();
    for (const auto& entry : filteredEntries) {
        html += getAddEntryJs(entry);
    }
    // Apply zoom
    if (m_currentZoom != 1.0) {
        html += QString("document.body.style.zoom = '%1';").arg(m_currentZoom);
    }
    html += "</script>";

    html += "</body></html>";
    
    m_webView->setHtml(html.toStdString());
}

void SummaryWindow::appendEntryHtml(const TranslationEntry& entry) {
    if (!m_webView) return;
    QString js = getAddEntryJs(entry);
    m_webView->eval(js.toStdString());
}

void SummaryWindow::setupFilterUI() {
    m_filterToolbar = new QToolBar(this);
    m_filterToolbar->setAttribute(Qt::WA_StyledBackground, true);
    m_filterToolbar->setAutoFillBackground(true);
    // Dark style to match application and prevent white flash
    // Style will be set by updateTheme
    
    m_filterToolbar->addWidget(new QLabel(TranslationManager::instance().tr("filter_from_date") + ": ", this));
    m_fromDateEdit = new QDateEdit(this);
    m_fromDateEdit->setCalendarPopup(true);
    m_fromDateEdit->setDate(QDate::currentDate().addMonths(-1));
    m_fromDateEdit->setDisplayFormat("yyyy-MM-dd");
    m_filterToolbar->addWidget(m_fromDateEdit);
    
    m_filterToolbar->addSeparator();
    
    m_filterToolbar->addWidget(new QLabel(TranslationManager::instance().tr("filter_to_date") + ": ", this));
    m_toDateEdit = new QDateEdit(this);
    m_toDateEdit->setCalendarPopup(true);
    m_toDateEdit->setDate(QDate::currentDate());
    m_toDateEdit->setDisplayFormat("yyyy-MM-dd");
    m_filterToolbar->addWidget(m_toDateEdit);
    
    m_filterToolbar->addSeparator();
    
    m_filterToolbar->addWidget(new QLabel(TranslationManager::instance().tr("filter_tags") + ": ", this));
    m_tagFilterCombo = new QComboBox(this);
    m_tagFilterCombo->setEditable(false);
    m_tagFilterCombo->addItem(TranslationManager::instance().tr("filter_all_tags"), "");
    m_filterToolbar->addWidget(m_tagFilterCombo);
    
    m_filterToolbar->addSeparator();
    
    m_clearFilterBtn = new QPushButton(TranslationManager::instance().tr("filter_clear"), this);
    m_filterToolbar->addWidget(m_clearFilterBtn);
    
    m_filterToolbar->addSeparator();

    m_selectionModeBtn = new QPushButton(TranslationManager::instance().tr("btn_selection_mode"), this);
    m_selectionModeBtn->setCheckable(true);
    // Fix dark mode text visibility issues
    // Style will be set by updateTheme
    
    connect(m_selectionModeBtn, &QPushButton::toggled, this, &SummaryWindow::toggleSelectionMode);
    m_filterToolbar->addWidget(m_selectionModeBtn);

    m_selectAllBtn = new QPushButton(TranslationManager::instance().tr("btn_select_all"), this);
    connect(m_selectAllBtn, &QPushButton::clicked, this, &SummaryWindow::onBatchSelectAll);
    m_batchSelectAllAction = m_filterToolbar->addWidget(m_selectAllBtn);
    m_batchSelectAllAction->setVisible(false);

    m_batchDeleteBtn = new QPushButton(TranslationManager::instance().tr("btn_batch_delete"), this);
    // Red color for delete - special case, handled in updateTheme or kept here if constant?
    // Let's move to updateTheme to ensure contrast.
    connect(m_batchDeleteBtn, &QPushButton::clicked, this, &SummaryWindow::onBatchDelete);
    m_batchDeleteAction = m_filterToolbar->addWidget(m_batchDeleteBtn);
    m_batchDeleteAction->setVisible(false);

    m_batchAddTagBtn = new QPushButton(TranslationManager::instance().tr("btn_batch_add_tag"), this);
    connect(m_batchAddTagBtn, &QPushButton::clicked, this, &SummaryWindow::onBatchAddTags);
    m_batchAddTagAction = m_filterToolbar->addWidget(m_batchAddTagBtn);
    m_batchAddTagAction->setVisible(false);

    m_batchRemoveTagBtn = new QPushButton(TranslationManager::instance().tr("btn_batch_remove_tag"), this);
    connect(m_batchRemoveTagBtn, &QPushButton::clicked, this, &SummaryWindow::onBatchRemoveTags);
    m_batchRemoveTagAction = m_filterToolbar->addWidget(m_batchRemoveTagBtn);
    m_batchRemoveTagAction->setVisible(false);
    
    QVBoxLayout* mainLayout = qobject_cast<QVBoxLayout*>(layout());
    if (mainLayout) {
        mainLayout->insertWidget(0, m_filterToolbar);
    }
    
    connect(m_fromDateEdit, &QDateEdit::dateChanged, this, &SummaryWindow::applyFilters);
    connect(m_toDateEdit, &QDateEdit::dateChanged, this, &SummaryWindow::applyFilters);
    connect(m_tagFilterCombo, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &SummaryWindow::applyFilters);
    connect(m_clearFilterBtn, &QPushButton::clicked, this, [this]() {
        m_fromDateEdit->setDate(QDate::currentDate().addMonths(-1));
        m_toDateEdit->setDate(QDate::currentDate());
        m_tagFilterCombo->setCurrentIndex(0);
        applyFilters();
    });
}

void SummaryWindow::setHistoryManager(HistoryManager* historyManager) {
    m_historyManager = historyManager;
    loadAvailableTags();
}

void SummaryWindow::loadAvailableTags() {
    if (!m_historyManager || !m_tagFilterCombo) return;
    
    QStringList allTags = m_historyManager->getAllTags();
    
    while (m_tagFilterCombo->count() > 1) {
        m_tagFilterCombo->removeItem(1);
    }
    
    for (const QString& tag : allTags) {
        m_tagFilterCombo->addItem(tag, tag);
    }
}

void SummaryWindow::applyFilters() {
    refreshHtml();
}

QList<TranslationEntry> SummaryWindow::getFilteredEntries() const {
    QList<TranslationEntry> filtered;
    
    QDate fromDate = m_fromDateEdit ? m_fromDateEdit->date() : QDate();
    QDate toDate = m_toDateEdit ? m_toDateEdit->date() : QDate();
    QString selectedTag = m_tagFilterCombo ? m_tagFilterCombo->currentData().toString() : "";
    
    for (const TranslationEntry& entry : m_entries) {
        QDate entryDate = entry.timestamp.date();
        if (m_fromDateEdit && entryDate < fromDate) continue;
        if (m_toDateEdit && entryDate > toDate) continue;
        
        if (!selectedTag.isEmpty() && !entry.tags.contains(selectedTag)) continue;
        
        filtered.append(entry);
    }
    
    return filtered;
}

// Toggle Selection Mode Logic
void SummaryWindow::toggleSelectionMode() {
    m_selectionMode = m_selectionModeBtn->isChecked();
    if (m_batchDeleteAction) m_batchDeleteAction->setVisible(m_selectionMode);
    if (m_batchAddTagAction) m_batchAddTagAction->setVisible(m_selectionMode);
    if (m_batchRemoveTagAction) m_batchRemoveTagAction->setVisible(m_selectionMode);
    if (m_batchSelectAllAction) m_batchSelectAllAction->setVisible(m_selectionMode);
    
    // Reset Select All state when toggling mode
    m_allSelected = false;
    if (m_selectAllBtn) {
        m_selectAllBtn->setText(TranslationManager::instance().tr("btn_select_all"));
    }

    // Notify JS to show/hide checkboxes
    m_webView->eval(QString("if(window.toggleSelectionMode) window.toggleSelectionMode(%1);").arg(m_selectionMode ? "true" : "false").toStdString());
}

void SummaryWindow::onBatchSelectAll() {
    m_allSelected = !m_allSelected;
    
    // Update button text
    if (m_selectAllBtn) {
        QString key = m_allSelected ? "btn_deselect_all" : "btn_select_all";
        m_selectAllBtn->setText(TranslationManager::instance().tr(key));
    }

    // execute JS
    QString js = QString("if(window.selectAllEntries) window.selectAllEntries(%1);").arg(m_allSelected ? "true" : "false");
    m_webView->eval(js.toStdString());
}

void SummaryWindow::onBatchDelete() {
    m_webView->eval("if(window.getSelectedIds) { let ids = window.getSelectedIds(); if(ids.length > 0) window.cmd_batchDelete(...ids); }");
}

void SummaryWindow::onBatchAddTags() {
    m_webView->eval("if(window.getSelectedIds) { let ids = window.getSelectedIds(); if(ids.length > 0) window.cmd_batchAddTags(...ids); }");
}

void SummaryWindow::onBatchRemoveTags() {
    m_webView->eval("if(window.getSelectedIds) { let ids = window.getSelectedIds(); if(ids.length > 0) window.cmd_batchRemoveTags(...ids); }");
}
void SummaryWindow::updateTheme(bool isDark) {
    Q_UNUSED(isDark);
    setStyleSheet("");
    if (m_filterToolbar) m_filterToolbar->setStyleSheet("");
    if (m_selectionModeBtn) m_selectionModeBtn->setStyleSheet("");
    if (m_batchDeleteBtn) m_batchDeleteBtn->setStyleSheet("");
}
