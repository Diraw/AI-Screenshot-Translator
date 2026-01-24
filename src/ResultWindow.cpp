#include "ResultWindow.h"
#include "ThemeUtils.h"
#include "EmbedWebView.h"
#include "TagDialog.h"
#include "HistoryManager.h"
#include "TranslationManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFile>
#include <QCoreApplication>
#include <QDir>
#include <QDebug>
#include <QRegularExpression>
#include <QSizeGrip>
#include <QResizeEvent>
#include <QCloseEvent>
#include <QShortcut>
#include <QKeySequence>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QByteArray>
#include <QUuid>
#include <QBuffer>
#include <string>
#include <functional>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

ResultWindow::ResultWindow(QWidget *parent) : QWidget(parent)
{
  qDebug() << "[RW] ctor this=" << (void *)this;
  setWindowTitle(TranslationManager::instance().tr("result_window_title"));
  resize(400, 450);
  setAttribute(Qt::WA_DeleteOnClose);

  m_webContainer = new QWidget(this);
  m_webView = std::make_unique<EmbedWebView>(m_webContainer);

  // DevTools shortcut for WebView debugging
  QShortcut *devToolsShortcut = new QShortcut(QKeySequence(Qt::Key_F12), this);
  devToolsShortcut->setContext(Qt::ApplicationShortcut);
  connect(devToolsShortcut, &QShortcut::activated, this, [this]()
          {
        if (m_webView) m_webView->openDevTools(); });

  m_toolBar = new QToolBar(this);
  m_toolBar->setMovable(false);
  m_toolBar->setFloatable(false);

  // 1. Lock (Left)
  m_lockAction = m_toolBar->addAction("🔓");
  m_lockAction->setCheckable(true);
  m_lockAction->setToolTip(tr("Lock Window"));
  connect(m_lockAction, &QAction::triggered, this, &ResultWindow::toggleLock);

  QWidget *leftSpacer = new QWidget();
  leftSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  m_toolBar->addWidget(leftSpacer);

  // 2. Paging (Center)
  m_prevAction = m_toolBar->addAction("<");
  connect(m_prevAction, &QAction::triggered, this, &ResultWindow::showPrevious);

  m_pageLabel = new QLabel(" 1 / 1 ", this);
  m_pageLabel->setAlignment(Qt::AlignCenter);
  m_toolBar->addWidget(m_pageLabel);

  m_nextAction = m_toolBar->addAction(">");
  connect(m_nextAction, &QAction::triggered, this, &ResultWindow::showNext);

  QWidget *rightSpacer = new QWidget();
  rightSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  m_toolBar->addWidget(rightSpacer);

  // 3. Mode (Right)
  m_statusLabel = new QLabel("MODE: VIEW", this);
  m_toolBar->addWidget(m_statusLabel);

  // --- JS BINDINGS ---
  m_webView->bind("log", [this](std::string seq, std::string req, void *arg)
                  {
         QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(req));
         if (doc.isArray()) {
             qInfo() << "WebView Log:" << doc.array().at(1).toString();
         } });

  m_webView->bind("js_error", [this](std::string seq, std::string req, void *arg)
                  {
         QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(req));
         if (doc.isArray()) {
             QJsonArray arr = doc.array();
             qWarning() << "JS ERROR:" << arr.at(1).toString() << "Line:" << (arr.count() > 2 ? arr.at(2).toInt() : 0);
         } });

  m_webView->bind("cmd_showScreenshot", [this](std::string seq, std::string req, void *arg)
                  {
         if (m_currentIndex >= 0 && m_currentIndex < m_history.size()) {
             emit screenshotRequested(m_history[m_currentIndex].id, m_history[m_currentIndex].originalBase64);
         } else if (!m_originalBase64.isEmpty()) {
             emit screenshotRequested(m_entryId, m_originalBase64);
         } });

  m_webView->bind("cmd_updateStatus", [this](std::string seq, std::string req, void *arg)
                  {
        QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(req));
        QString mode = doc.isArray() ? doc.array().at(0).toString() : QString::fromUtf8(req.c_str());
        QString labelText = "MODE: " + mode.toUpper();
        QMetaObject::invokeMethod(this, [this, labelText, mode](){ 
            if (m_statusLabel) m_statusLabel->setText(labelText);
            if (mode == "view" || mode == "view_done") {
              requestFocusToWeb(false);
            }
        }, Qt::QueuedConnection); });

  m_webView->bind("cmd_updateContent", [this](std::string seq, std::string req, void *arg)
                  {
         QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(req));
         QString newMd = doc.isArray() ? doc.array().at(0).toString() : QString::fromUtf8(req.c_str());
         m_currentMarkdown = newMd; 
         emit contentUpdated(newMd);
         emit contentUpdatedWithId(m_entryId, newMd); });

  m_webView->bind("cmd_showPrevious", [this](std::string seq, std::string req, void *arg)
                  { QMetaObject::invokeMethod(this, "showPrevious", Qt::QueuedConnection); });
  m_webView->bind("cmd_showNext", [this](std::string seq, std::string req, void *arg)
                  { QMetaObject::invokeMethod(this, "showNext", Qt::QueuedConnection); });
  m_webView->bind("cmd_openTagDialog", [this](std::string seq, std::string req, void *arg)
                  { QMetaObject::invokeMethod(this, "openTagDialog", Qt::QueuedConnection); });
  m_webView->bind("cmd_openDevTools", [this](std::string, std::string, void *)
                  {
        if (m_webView) {
            qDebug() << "[DevTools] Request from JS in ResultWindow";
            m_webView->openDevTools();
        } });

  connect(m_webView.get(), &EmbedWebView::ready, this, [this]()
          {
        setUpdatesEnabled(true);
        m_webView->setSize(width(), height());
        // Apply initial theme
    updateTheme(ThemeUtils::isSystemDark());
    requestFocusToWeb(false); });

  updateTheme(ThemeUtils::isSystemDark());
}

void ResultWindow::toggleLock()
{
  m_isLocked = m_lockAction->isChecked();
  m_lockAction->setText(m_isLocked ? "🔒" : "🔓");
#ifdef _WIN32
  SetWindowPos((HWND)this->winId(), m_isLocked ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
#else
  setWindowFlags(m_isLocked ? (windowFlags() | Qt::WindowStaysOnTopHint) : (windowFlags() & ~Qt::WindowStaysOnTopHint));
  show();
#endif
}

void ResultWindow::setContent(const QString &markdown, const QString &originalBase64, const QString &prompt, const QString &entryId)
{
  bool sameContent = m_htmlLoaded && (entryId == m_entryId) && (markdown == m_currentMarkdown);

  m_originalBase64 = originalBase64;
  m_originalPrompt = prompt;
  m_entryId = entryId;
  m_currentMarkdown = markdown;

  if (sameContent)
  {
    // Avoid reloading identical content; still ensure theme is in sync
    bool isDark = ThemeUtils::isSystemDark();
    QString toggleJs = QString("document.documentElement.classList.toggle('dark-mode', %1); document.body.classList.toggle('dark-mode', %1);").arg(isDark ? "true" : "false");
    if (m_webView)
      m_webView->eval(toggleJs.toStdString());
    return;
  }

  if (m_history.isEmpty() && !entryId.isEmpty())
  {
    TranslationEntry e;
    e.id = entryId;
    e.translatedMarkdown = markdown;
    e.originalBase64 = originalBase64;
    e.prompt = prompt;
    m_history.append(e);
    m_currentIndex = 0;
    updateNavigation();
  }

  QJsonObject initData;
  initData["raw_md"] = markdown;
  initData["key_view"] = m_viewToggleKey;
  initData["key_edit"] = m_editToggleKey;
  initData["key_screenshot"] = m_screenshotToggleKey;
  initData["key_prev"] = m_prevKey;
  initData["key_next"] = m_nextKey;
  initData["key_tag"] = m_tagKey.isEmpty() ? "t" : m_tagKey;
  initData["key_bold"] = m_boldKey;
  initData["key_underline"] = m_underlineKey;
  initData["key_highlight"] = m_highlightKey;
  initData["is_dark"] = ThemeUtils::isSystemDark();
  initData["win_id"] = QString::number(reinterpret_cast<quintptr>(this), 16);

  QString pJson = QString::fromUtf8(QJsonDocument(initData).toJson(QJsonDocument::Compact));
  pJson.replace("</script>", "<\\/script>", Qt::CaseInsensitive);

  qInfo() << "ResultWindow: setContent - Hotkey mapping:"
          << "view=" << m_viewToggleKey
          << "edit=" << m_editToggleKey
          << "screenshot=" << m_screenshotToggleKey;

  auto loadAsset = [](const QString &name)
  {
    QFile f(QCoreApplication::applicationDirPath() + "/assets/libs/" + name);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
  };
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
  auto toUri = [](const QByteArray &c, const char *t)
  { return QString("data:%1;charset=utf-8;base64,%2").arg(t).arg(QString(c.toBase64())); };

  QString html = R"RAW_HTML(
<!DOCTYPE html>
<html>
<head>
<meta charset='UTF-8'>
<link rel='stylesheet' href='%1'>
<link rel='stylesheet' href='%4'>
<style>
html, body {
  background: #ffffff;
  color: #111111;
}

body {
  font-family: "Segoe UI", sans-serif;
  font-size: %7px;
  padding: 20px;
  padding-top: 60px;
  overflow-y: auto;
}

pre {
  background: #f4f4f4;
  padding: 12px;
  border-radius: 4px;
  overflow-x: auto;
  color: #111111;
}

#content { line-height: 1.6; }

#raw_view,
#edit_view {
  display: none;
  white-space: pre-wrap;
  font-family: monospace;
  background: #f8f9fa;
  padding: 15px;
  border: 1px solid #ddd;
  border-radius: 4px;
  margin-top: 10px;
  color: #111111;
}

#edit_view {
  width: 100%;
  min-height: 250px;
  box-sizing: border-box;
  background: #fff;
  border: 2px solid #0078d4;
  outline: none;
  font-size: 14px;
  resize: none;
  overflow: hidden;
}

mark { background: #ffeb3b; color: #000; }

html.dark-mode,
body.dark-mode {
  background: #1e1e1e !important;
  color: #e0e0e0 !important;
}

body.dark-mode pre,
body.dark-mode code,
body.dark-mode #raw_view,
body.dark-mode #edit_view {
  background: #2d2d2d !important;
  color: #e0e0e0 !important;
  border-color: #444 !important;
}

body.dark-mode mark { background: #d4af37; color: #000; }

body.dark-mode .hljs,
body.dark-mode pre code,
body.dark-mode pre code.hljs {
  color: #e0e0e0 !important;
  background: #2d2d2d !important;
}

body.dark-mode .hljs * { color: inherit !important; }

body.dark-mode .katex,
body.dark-mode .katex * {
  color: #e0e0e0 !important;
}
</style>
<script src='%2'></script>
<script src='%3'></script>
<script src='%5'></script>
<script src='%6'></script>
</head>
<body>
<div id='content'><h3>Loading...</h3></div>
<textarea id='edit_view'></textarea>
<div id='raw_view'></div>
<script id="init-data" type="application/json">__INIT_JSON__</script>
<script>
if (typeof marked !== 'undefined') {
  marked.setOptions({ breaks: true, gfm: true });
}
var INIT_DATA = {}; var CUR_MD = '';
try { INIT_DATA = JSON.parse(document.getElementById('init-data').textContent || '{}'); CUR_MD = INIT_DATA.raw_md || ''; } catch(e) {}
const DARK_MODE = !!INIT_DATA.is_dark;
document.documentElement.classList.toggle('dark-mode', DARK_MODE);
document.body.classList.toggle('dark-mode', DARK_MODE);
function log(m) { if (window.chrome&&window.chrome.webview) window.chrome.webview.postMessage(JSON.stringify(["log", m])); }
window.onerror = function(m,u,l,c,e) { log("JS ERR: "+m+" @ "+l+":"+c); };
var EDIT = false;
function focusSink() {
  try {
    document.body.tabIndex = -1;
    document.body.focus({ preventScroll: true });
  } catch(e) {}
}
function render(md) {
  if (typeof marked==='undefined') return;
  var prot = (function(t){
    var b=[], c=0, r=/(\$\$[\s\S]*?\$\$)|(\\\[[\s\S]*?\\\])|(\\\([\s\S]*?\\\))|(\$[^\$\n]+\$)/g;
    return {text: t.replace(r, function(m){ b.push(m); return 'MATHPH'+(c++); }), blocks: b};
  })(md||'');
  var h = marked.parse(prot.text);
  prot.blocks.forEach(function(b, i){ h = h.split('MATHPH'+i).join(b); });
  var d = document.getElementById('content');
  if (d) { d.innerHTML = h; if(window.hljs) hljs.highlightAll(); if(window.renderMathInElement) renderMathInElement(d, {delimiters:[{left:'$$',right:'$$',display:true},{left:'$',right:'$',display:false}],throwOnError:false}); }
}
window.updateContentFromNative = function(m) { CUR_MD = m; if(!EDIT) render(m); };
window.toggleSource = function() {
  if (EDIT) return;
  var c = document.getElementById('content'), r = document.getElementById('raw_view');
  if (r.style.display==='none') { r.innerText = CUR_MD; r.style.display='block'; c.style.display='none'; if(window.cmd_updateStatus) window.cmd_updateStatus('raw'); }
  else { r.style.display='none'; c.style.display='block'; if(window.cmd_updateStatus) window.cmd_updateStatus('view'); focusSink(); }
};
window.toggleEdit = function() {
  var c = document.getElementById('content'), e = document.getElementById('edit_view'), r = document.getElementById('raw_view');
  if (!EDIT) { EDIT=true; e.value=CUR_MD; e.style.display='block'; c.style.display='none'; r.style.display='none'; fitEditHeight(); e.focus(); if(window.cmd_updateStatus) window.cmd_updateStatus('edit'); }
  else {
    EDIT=false;
    CUR_MD=e.value;
    if(window.cmd_updateContent) window.cmd_updateContent(CUR_MD);
    render(CUR_MD);
    c.style.display='block';
    setTimeout(function(){
      e.style.display='none';
      focusSink();
      if(window.cmd_updateStatus) window.cmd_updateStatus('view');
    }, 0);
  }
};
function applyFormat(t) {
  var e = document.getElementById('edit_view'); if (!EDIT || !e) return;
  var start = e.selectionStart, end = e.selectionEnd, val = e.value;
  var tag = (t==='bold'?'b':(t==='underline'?'u':'mark')), txt = val.substring(start, end);
  var ins = '<'+tag+'>'+txt+'</'+tag+'>';
  e.value = val.substring(0, start) + ins + val.substring(end);
  e.selectionStart = start; e.selectionEnd = start + ins.length; e.focus();
  CUR_MD = e.value; if(window.cmd_updateContent) window.cmd_updateContent(CUR_MD);
}
window.toggleScreenshot = function() { if(window.cmd_showScreenshot) window.cmd_showScreenshot(); };
function parseHK(hk) {
  if(!hk) return null; var p = hk.toLowerCase().split('+').map(x=>x.trim()).filter(x=>x.length>0), k = p.pop();
  return {key:k, ctrl:p.includes('ctrl')||p.includes('control'), alt:p.includes('alt'), shift:p.includes('shift')};
}
function matchHK(e, h) { return h && !!e.ctrlKey==!!h.ctrl && !!e.altKey==!!h.alt && !!e.shiftKey==!!h.shift && e.key.toLowerCase()==h.key; }
const HK_V=parseHK(INIT_DATA.key_view), HK_E=parseHK(INIT_DATA.key_edit), HK_S=parseHK(INIT_DATA.key_screenshot), HK_P=parseHK(INIT_DATA.key_prev), HK_N=parseHK(INIT_DATA.key_next), HK_T=parseHK(INIT_DATA.key_tag), HK_B=parseHK(INIT_DATA.key_bold), HK_U=parseHK(INIT_DATA.key_underline), HK_H=parseHK(INIT_DATA.key_highlight);
window.addEventListener('keydown', function(e) {
  log("Keydown: " + e.key + " (code: " + e.code + ")");
  var k = (e.key || '').toLowerCase();
  if (k === 'f12' || (e.ctrlKey && e.shiftKey && k === 'i')) {
    if (window.cmd_openDevTools) {
      log('Trigger openDevTools from JS');
      window.cmd_openDevTools();
    } else {
      log('cmd_openDevTools not bound');
    }
    e.preventDefault();
    return;
  }
  var raw = document.getElementById('raw_view');
  var isRawVisible = raw && raw.style.display !== 'none';
  if (!EDIT && isRawVisible && e.key === 'Escape') {
    e.preventDefault();
    toggleSource();
    setTimeout(function() { focusSink(); }, 0);
    return;
  }
  if (EDIT) {
    if (matchHK(e, HK_E) || e.key === 'Escape') {
      e.preventDefault(); toggleEdit(); return;
    }
    if (matchHK(e, HK_B)) { e.preventDefault(); applyFormat('bold'); return; }
    if (matchHK(e, HK_U)) { e.preventDefault(); applyFormat('underline'); return; }
    if (matchHK(e, HK_H)) { e.preventDefault(); applyFormat('highlight'); return; }
    return;
  }
  if (matchHK(e, HK_V)){ log("Matched View"); e.preventDefault(); toggleSource(); return; }
  if (matchHK(e, HK_E)){ log("Matched Edit"); e.preventDefault(); toggleEdit(); return; }
  if (matchHK(e, HK_S)){ log("Matched Screenshot"); e.preventDefault(); toggleScreenshot(); return; }
  if (matchHK(e, HK_P)){ e.preventDefault(); if(window.cmd_showPrevious) window.cmd_showPrevious(); return; }
  if (matchHK(e, HK_N)){ e.preventDefault(); if(window.cmd_showNext) window.cmd_showNext(); return; }
  if (matchHK(e, HK_T)){ e.preventDefault(); if(window.cmd_openTagDialog) window.cmd_openTagDialog(); return; }
  if (EDIT) {
    if (matchHK(e, HK_B)){ e.preventDefault(); applyFormat('bold'); return; }
    if (matchHK(e, HK_U)){ e.preventDefault(); applyFormat('underline'); return; }
    if (matchHK(e, HK_H)){ e.preventDefault(); applyFormat('highlight'); return; }
  }
}, true);
document.addEventListener('DOMContentLoaded', () => { render(CUR_MD); focusSink(); log("WIN=" + (INIT_DATA.win_id||"?") + " DOMContentLoaded; KEY_VIEW=" + (INIT_DATA.key_view||"") + " KEY_SHOT=" + (INIT_DATA.key_screenshot||"") + " KEY_EDIT=" + (INIT_DATA.key_edit||"")); });

document.addEventListener('mouseup', () => {
  if (!EDIT) focusSink();
}, true);

function fitEditHeight() {
  var e = document.getElementById('edit_view');
  if (!e) return;
  e.style.height = 'auto';
  var target = Math.max(200, e.scrollHeight);
  e.style.height = target + 'px';
}
window.addEventListener('resize', fitEditHeight);
window.addEventListener('input', function(e){ if(e && e.target && e.target.id==='edit_view') fitEditHeight(); }, true);
</script></body></html>
)RAW_HTML";

  QString out = html
                    .arg(toUri(loadAsset("highlight.default.min.css"), "text/css"))
                    .arg(toUri(loadAsset("marked.min.js"), "text/javascript"))
                    .arg(toUri(loadAsset("highlight.min.js"), "text/javascript"))
                    .arg(toUri(embedFonts(QString::fromUtf8(loadAsset("katex.min.css"))).toUtf8(), "text/css"))
                    .arg(toUri(loadAsset("katex.min.js"), "text/javascript"))
                    .arg(toUri(loadAsset("auto-render.min.js"), "text/javascript"))
                    .arg(m_config.initialFontSize);
  out.replace("__RAW_MD__", markdown.toHtmlEscaped());
  out.replace("__INIT_JSON__", pJson);

  if (m_webView)
  {
    if (!m_htmlLoaded)
    {
      m_webView->setHtml(out.toStdString());
      m_htmlLoaded = true;
    }
    else
    {
      externalContentUpdate(markdown);
    }
    bool isDark = ThemeUtils::isSystemDark();
    QString toggleJs = QString("document.documentElement.classList.toggle('dark-mode', %1); document.body.classList.toggle('dark-mode', %1);").arg(isDark ? "true" : "false");
    m_webView->eval(toggleJs.toStdString());
  }
}

void ResultWindow::configureHotkeys(const QString &v, const QString &e, const QString &s, const QString &b, const QString &u, const QString &h, const QString &p, const QString &n, const QString &t)
{
  m_viewToggleKey = v;
  m_editToggleKey = e;
  m_screenshotToggleKey = s;
  m_boldKey = b;
  m_underlineKey = u;
  m_highlightKey = h;
  m_prevKey = p;
  m_nextKey = n;
  m_tagKey = t;
  qDeleteAll(m_navShortcuts);
  m_navShortcuts.clear();
  auto add = [this](const QString &k, std::function<void()> f, Qt::ShortcutContext c = Qt::WindowShortcut)
  {
    if (k.isEmpty())
      return;
    QShortcut *sc = new QShortcut(QKeySequence(k), this);
    sc->setContext(c);
    connect(sc, &QShortcut::activated, this, f);
    m_navShortcuts.append(sc);
  };
  add(m_viewToggleKey, [this]()
      { if(m_webView) m_webView->eval("toggleSource();"); }, Qt::WindowShortcut);
  add(m_editToggleKey, [this]()
      { if(m_webView) m_webView->eval("toggleEdit();"); }, Qt::WindowShortcut);
  add(m_screenshotToggleKey, [this]()
      { if(m_webView) m_webView->eval("toggleScreenshot();"); }, Qt::WindowShortcut);
  add(m_boldKey, [this]()
      { if(m_webView) m_webView->eval("applyFormat('bold');"); });
  add(m_underlineKey, [this]()
      { if(m_webView) m_webView->eval("applyFormat('underline');"); });
  add(m_highlightKey, [this]()
      { if(m_webView) m_webView->eval("applyFormat('highlight');"); });
  add(m_prevKey, [this]()
      { showPrevious(); });
  add(m_nextKey, [this]()
      { showNext(); });
  add(m_tagKey, [this]()
      { openTagDialog(); });
}

void ResultWindow::addEntry(const TranslationEntry &entry)
{
  m_history.append(entry);
  m_currentIndex = m_history.size() - 1;
  updateNavigation();
  setContent(entry.translatedMarkdown, entry.originalBase64, entry.prompt, entry.id);
}
void ResultWindow::showPrevious()
{
  if (m_currentIndex > 0)
  {
    m_currentIndex--;
    TranslationEntry e = m_history[m_currentIndex];
    // Refresh from history manager to avoid stale cached content (e.g. "Processing...").
    if (m_historyManager && !e.id.isEmpty())
    {
      TranslationEntry fresh = m_historyManager->getEntryById(e.id);
      if (!fresh.id.isEmpty())
      {
        m_history[m_currentIndex] = fresh;
        e = fresh;
      }
    }
    setContent(e.translatedMarkdown, e.originalBase64, e.prompt, e.id);
    updateNavigation();
  }
}
void ResultWindow::showNext()
{
  if (m_currentIndex < m_history.size() - 1)
  {
    m_currentIndex++;
    TranslationEntry e = m_history[m_currentIndex];
    // Refresh from history manager to avoid stale cached content (e.g. "Processing...").
    if (m_historyManager && !e.id.isEmpty())
    {
      TranslationEntry fresh = m_historyManager->getEntryById(e.id);
      if (!fresh.id.isEmpty())
      {
        m_history[m_currentIndex] = fresh;
        e = fresh;
      }
    }
    setContent(e.translatedMarkdown, e.originalBase64, e.prompt, e.id);
    updateNavigation();
  }
}
void ResultWindow::updateNavigation()
{
  m_prevAction->setEnabled(m_currentIndex > 0);
  m_nextAction->setEnabled(m_currentIndex < m_history.size() - 1);
  m_pageLabel->setText(QString(" %1 / %2 ").arg(m_currentIndex + 1).arg(m_history.size()));
}
void ResultWindow::updateTheme(bool isDark)
{
  ThemeUtils::applyThemeToWindow(this, isDark);
  if (m_webView)
  {
    QColor bg = isDark ? QColor(30, 30, 30) : QColor(255, 255, 255);
    m_webView->setBackgroundColor(bg.red(), bg.green(), bg.blue(), 255);
    QString toggleJs = QString("document.documentElement.classList.toggle('dark-mode', %1); document.body.classList.toggle('dark-mode', %1);").arg(isDark ? "true" : "false");
    m_webView->eval(toggleJs.toStdString());
  }
  if (m_toolBar)
    m_toolBar->setStyleSheet("");
  if (m_statusLabel)
    m_statusLabel->setStyleSheet("");
  if (m_pageLabel)
    m_pageLabel->setStyleSheet("");
}
void ResultWindow::resizeEvent(QResizeEvent *event)
{
  if (m_webContainer)
    m_webContainer->setGeometry(0, 0, width(), height());
  if (m_webView)
    m_webView->setSize(width(), height());
  if (m_toolBar)
  {
    m_toolBar->setGeometry(0, 0, width(), 46);
    m_toolBar->raise();
  }
  QSizeGrip *grip = findChild<QSizeGrip *>();
  if (grip)
  {
    grip->move(width() - grip->width(), height() - grip->height());
    grip->raise();
  }
  QWidget::resizeEvent(event);
}
void ResultWindow::showEvent(QShowEvent *event)
{
  QWidget::showEvent(event);
  if (m_isFirstLoad)
  {
    m_isFirstLoad = false;
    requestFocusToWeb(false);
  }
}
void ResultWindow::closeEvent(QCloseEvent *event)
{
  emit closed();
  event->accept();
}
bool ResultWindow::event(QEvent *event)
{
  if (event->type() == QEvent::WindowActivate)
  {
    requestFocusToWeb(false);
  }
  return QWidget::event(event);
}
void ResultWindow::setHistoryManager(HistoryManager *h) { m_historyManager = h; }
void ResultWindow::requestFocusToWeb(bool allowActivate)
{
  if (m_focusPending)
    return;

  m_focusPending = true;

  QTimer::singleShot(0, this, [this, allowActivate]()
                     {
                       m_focusPending = false;

                       if (!isVisible())
                         return;

                       if (!isActiveWindow())
                       {
                         if (allowActivate)
                         {
                           activateWindow();
                           raise();
                         }
                         else
                         {
                           return;
                         }
                       }

                       if (m_lastFocus.isValid() && m_lastFocus.elapsed() < 80)
                         return;

                       m_lastFocus.restart();

                       // 避免 Qt 先 setFocus/clearFocus 导致 WebView 页面触发 blur。
                       // 这里直接把焦点给到 WebView 即可。
                       focusEditor();

#ifdef _WIN32
                       HWND f = GetFocus();
                       wchar_t cls[256] = {0};
                       if (f)
                         GetClassNameW(f, cls, 255);
                       qDebug() << "[Focus] GetFocus=" << (void *)f << " class=" << QString::fromWCharArray(cls);
#endif
                     });
}
void ResultWindow::focusEditor()
{
  if (m_webView)
  {
    m_webView->focusNative();
    QTimer::singleShot(0, this, [this]()
                       {
        if (m_webView)
          m_webView->eval("(()=>{try{document.body.tabIndex=-1;document.body.focus({preventScroll:true});}catch(e){}})();"); });
  }
}
void ResultWindow::setConfig(const AppConfig &config)
{
  m_config = config;

  // Apply default lock state from config (used when creating new result windows).
  if (m_lockAction)
  {
    const bool wantLocked = m_config.defaultResultWindowLocked;
    if (m_lockAction->isChecked() != wantLocked)
    {
      m_lockAction->setChecked(wantLocked);
      toggleLock();
    }
    else if (m_isLocked != wantLocked)
    {
      // Keep internal state consistent with the action state.
      toggleLock();
    }
  }
}

ResultWindow::ProtectedContent ResultWindow::protectMath(const QString &m)
{
  ProtectedContent p;
  p.text = m;
  return p;
}
void ResultWindow::showLoading() {}
void ResultWindow::showError(const QString &m) {}
void ResultWindow::externalContentUpdate(const QString &m)
{
  m_currentMarkdown = m;

  // Keep cached history entry in sync so paging won't resurrect stale content.
  if (!m_entryId.isEmpty())
  {
    for (int i = 0; i < m_history.size(); ++i)
    {
      if (m_history[i].id == m_entryId)
      {
        m_history[i].translatedMarkdown = m;
        break;
      }
    }
  }

  if (m_webView)
  {
    QString js = m;
    js.replace("\\", "\\\\").replace("\"", "\\\"").replace("\n", "\\n").replace("\r", "");
    m_webView->eval(QString("updateContentFromNative(\"%1\");").arg(js).toStdString());
  }
}
ResultWindow::~ResultWindow()
{
  qDebug() << "[RW] dtor this=" << (void *)this;
}

void ResultWindow::openTagDialog()
{
  if (!m_historyManager || m_entryId.isEmpty())
    return;
  QStringList allTags = m_historyManager->getAllTags();
  TagDialog *dialog = new TagDialog(allTags, m_currentTags, this);
  connect(dialog, &TagDialog::tagsUpdated, this, [this](const QStringList &tags)
          {
        m_currentTags = tags;
        emit tagsUpdated(m_entryId, tags); });
  dialog->exec();
  dialog->deleteLater();
}

void ResultWindow::updateShortcuts() {}
