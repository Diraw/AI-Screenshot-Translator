
#include "ResultWindow.h"

#include "ThemeUtils.h"
#include "EmbedWebView.h"
#include "TagDialog.h"
#include "HistoryManager.h"
#include "TranslationManager.h"
#include "ColorUtils.h"

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

#ifdef _WIN32
#include "WinKeyForwarder.h"
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

#ifdef _WIN32
  // Register this window for native key forwarding (WebView2 often consumes WM_KEYDOWN).
  WinKeyForwarder::instance().registerResultWindow(this);
#endif
}

void ResultWindow::triggerScreenshotFromNative()
{
#ifdef _WIN32
  {
    QString msg = QString("[RW] triggerScreenshotFromNative entryId=%1 idx=%2 hist=%3 origB64=%4")
                      .arg(m_entryId)
                      .arg(m_currentIndex)
                      .arg(m_history.size())
                      .arg(m_originalBase64.size());
    QByteArray line = msg.toUtf8();
    WinKeyForwarder::trace(line.constData());
  }
#endif
  // Native hotkey path: do not rely on JS keydown or Qt shortcuts.
  if (m_currentIndex >= 0 && m_currentIndex < m_history.size())
  {
#ifdef _WIN32
    {
      const auto &e = m_history[m_currentIndex];
      QString msg = QString("[RW] emit screenshotRequested from history id=%1 b64=%2")
                        .arg(e.id)
                        .arg(e.originalBase64.size());
      QByteArray line = msg.toUtf8();
      WinKeyForwarder::trace(line.constData());
    }
#endif
    emit screenshotRequested(m_history[m_currentIndex].id, m_history[m_currentIndex].originalBase64);
    return;
  }
  if (!m_originalBase64.isEmpty())
  {
#ifdef _WIN32
    {
      QString msg = QString("[RW] emit screenshotRequested from current entryId=%1 b64=%2")
                        .arg(m_entryId)
                        .arg(m_originalBase64.size());
      QByteArray line = msg.toUtf8();
      WinKeyForwarder::trace(line.constData());
    }
#endif
    emit screenshotRequested(m_entryId, m_originalBase64);
    return;
  }

#ifdef _WIN32
  WinKeyForwarder::trace("[RW] NO EMIT (no valid index and originalBase64 empty)");
#endif
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
          m_webView->eval(R"JS((()=>{try{
  const kt = document.getElementById('keytrap');
  if (kt && kt.focus) { kt.focus({preventScroll:true}); return; }
  document.body.tabIndex = 0;
  document.body.focus({preventScroll:true});
}catch(e){}})();
)JS"); });
  }
}
void ResultWindow::showLoading() {}
void ResultWindow::showError(const QString &m) {}
ResultWindow::~ResultWindow()
{
#ifdef _WIN32
  WinKeyForwarder::instance().unregisterResultWindow(this);
#endif
  qDebug() << "[RW] dtor this=" << (void *)this;
}
