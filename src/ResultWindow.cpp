
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
#include <QToolButton>
#include <QFontMetrics>

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

  m_toolBar = new QWidget(this);
  m_toolBar->setObjectName("resultToolBar");
  m_toolBar->setAttribute(Qt::WA_StyledBackground, true);

  // --- Toolbar layout: fixed Left / centered Paging / fixed Right ---
  // Goal: < 1/1 > position must never shift with MODE text width changes.
  auto makeToolButton = [](QAction *a) -> QToolButton *
  {
    QToolButton *b = new QToolButton();
    b->setDefaultAction(a);
    b->setAutoRaise(true);
    b->setFocusPolicy(Qt::NoFocus);
    return b;
  };

  // Actions (kept as QAction so other code can enable/disable them)
  m_lockAction = new QAction("🔓", this);
  m_lockAction->setCheckable(true);
  m_lockAction->setToolTip(tr("Lock Window"));
  connect(m_lockAction, &QAction::triggered, this, &ResultWindow::toggleLock);

  m_prevAction = new QAction("<", this);
  connect(m_prevAction, &QAction::triggered, this, &ResultWindow::showPrevious);

  m_nextAction = new QAction(">", this);
  connect(m_nextAction, &QAction::triggered, this, &ResultWindow::showNext);

  // Create widgets
  m_lockBtn = makeToolButton(m_lockAction);
  m_prevBtn = makeToolButton(m_prevAction);
  m_nextBtn = makeToolButton(m_nextAction);

  m_pageLabel = new QLabel(" 1 / 1 ", this);
  m_pageLabel->setAlignment(Qt::AlignCenter);
  {
    const QFontMetrics fm(m_pageLabel->font());
    m_pageMinW = fm.horizontalAdvance(" 1 / 1 ");
    m_pageCompactW = fm.horizontalAdvance("1/1");
    m_pageMaxW = fm.horizontalAdvance(" 888 / 888 ");
    m_pageLabel->setMinimumWidth(0);
    m_pageLabel->setMaximumWidth(m_pageMaxW);
    m_pageLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
  }

  m_statusLabel = new QLabel("MODE: VIEW", this);
  m_statusLabel->setObjectName("statusIndicator");
  m_statusLabel->setAlignment(Qt::AlignCenter);
  m_statusLabel->setFixedHeight(26);
  {
    const QFontMetrics fm(m_statusLabel->font());
    m_statusMinW = fm.horizontalAdvance("EDIT") + 18;
    const QStringList candidates = {"MODE: VIEW", "MODE: EDIT", "MODE: RAW", "MODE: VIEW_DONE", "MODE: SOURCE"};
    int maxW = 0;
    for (const QString &s : candidates)
      maxW = qMax(maxW, fm.horizontalAdvance(s));
    m_statusMaxW = maxW + 18;
    m_statusLabel->setMinimumWidth(0);
    m_statusLabel->setMaximumWidth(m_statusMaxW);
    m_statusLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
  }

  m_statusMode = "view";

  m_leftBlock = new QWidget(this);
  {
    QHBoxLayout *l = new QHBoxLayout(m_leftBlock);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(0);
    l->addStretch();
    l->addWidget(m_lockBtn);
    l->addStretch();
    m_leftBlock->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
  }

  m_pagingGroup = new QWidget(this);
  m_pagingGroup->setObjectName("pagingGroup");
  {
    QHBoxLayout *p = new QHBoxLayout(m_pagingGroup);
    p->setContentsMargins(0, 0, 0, 0);
    p->setSpacing(6);
    p->addWidget(m_prevBtn);
    p->addWidget(m_pageLabel);
    p->addWidget(m_nextBtn);
    m_pagingGroup->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
  }

  m_centerBlock = new QWidget(this);
  {
    QHBoxLayout *c = new QHBoxLayout(m_centerBlock);
    c->setContentsMargins(0, 0, 0, 0);
    c->setSpacing(0);
    c->addStretch();
    c->addWidget(m_pagingGroup);
    c->addStretch();
    m_centerBlock->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  }

  m_rightBlock = new QWidget(this);
  {
    QHBoxLayout *r = new QHBoxLayout(m_rightBlock);
    r->setContentsMargins(0, 0, 0, 0);
    r->setSpacing(0);
    r->addWidget(m_statusLabel, 0, Qt::AlignCenter);
    m_rightBlock->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
  }

  {
    QHBoxLayout *bar = new QHBoxLayout(m_toolBar);
    bar->setContentsMargins(0, 0, 0, 0);
    bar->setSpacing(0);
    bar->addWidget(m_leftBlock);
    bar->addWidget(m_centerBlock);
    bar->addWidget(m_rightBlock);
  }

  updateToolbarResponsive();

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
        QMetaObject::invokeMethod(this, [this, mode](){ 
            m_statusMode = mode;
            applyStatusLabelText();
            updateToolbarResponsive();
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

void ResultWindow::updateToolbarBalance()
{
  // No-op: toolbar layout is stable via fixed Left/Center/Right blocks.
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
    updateToolbarResponsive();
  }
  QSizeGrip *grip = findChild<QSizeGrip *>();
  if (grip)
  {
    grip->move(width() - grip->width(), height() - grip->height());
    grip->raise();
  }
  QWidget::resizeEvent(event);
}

void ResultWindow::applyStatusLabelText()
{
  if (!m_statusLabel)
    return;
  const QString mode = m_statusMode.isEmpty() ? QStringLiteral("view") : m_statusMode;
  const QString up = mode.toUpper();

  const QFontMetrics fm(m_statusLabel->font());
  const int avail = qMax(0, m_statusLabel->maximumWidth());
  const QString full = QStringLiteral("MODE: ") + up;
  const QString shortText = up;
  const QString tiny = up.isEmpty() ? QString() : up.left(1);

  auto fits = [&](const QString &s) -> bool
  {
    // Leave some padding for the pill style.
    return (fm.horizontalAdvance(s) + 12) <= avail;
  };

  if (fits(full))
    m_statusLabel->setText(full);
  else if (fits(shortText))
    m_statusLabel->setText(shortText);
  else
    m_statusLabel->setText(tiny);
}

void ResultWindow::updateToolbarResponsive()
{
  if (!m_toolBar || !m_leftBlock || !m_rightBlock || !m_pagingGroup || !m_statusLabel || !m_lockBtn || !m_prevBtn || !m_nextBtn)
    return;

  const int toolbarW = m_toolBar->width();
  if (toolbarW <= 0)
    return;

  // Minimum usable widths
  const int lockW = m_lockBtn->sizeHint().width();
  const int minSideW = qMax(lockW + 10, 28);

  const int prevW = m_prevBtn->sizeHint().width();
  const int nextW = m_nextBtn->sizeHint().width();
  const int spacing = 6;

  // Decide page label display mode: full / compact / hidden.
  const int minCenterFull = prevW + nextW + m_pageMinW + spacing * 2;
  const int minCenterCompact = prevW + nextW + m_pageCompactW + spacing * 2;
  const int minCenterNoLabel = prevW + nextW + spacing;

  enum PageMode
  {
    PageHidden,
    PageCompact,
    PageFull
  };

  PageMode pageMode = PageHidden;
  if (toolbarW >= (minSideW * 2 + minCenterFull + 6))
    pageMode = PageFull;
  else if (toolbarW >= (minSideW * 2 + minCenterCompact + 6))
    pageMode = PageCompact;

  if (m_pageLabel)
  {
    m_pageLabel->setVisible(pageMode != PageHidden);
    if (pageMode == PageFull)
      m_pageLabel->setMaximumWidth(m_pageMaxW);
    else if (pageMode == PageCompact)
      m_pageLabel->setMaximumWidth(m_pageCompactW + 6);
  }

  // Keep text in sync with current navigation state.
  if (m_pageLabel && pageMode != PageHidden)
  {
    const int cur = qMax(0, m_currentIndex) + 1;
    const int total = qMax(1, m_history.size());
    if (pageMode == PageFull)
      m_pageLabel->setText(QString(" %1 / %2 ").arg(cur).arg(total));
    else
      m_pageLabel->setText(QString("%1/%2").arg(cur).arg(total));
  }

  const int centerMin = (pageMode == PageFull) ? minCenterFull : ((pageMode == PageCompact) ? minCenterCompact : minCenterNoLabel);
  const int sideMaxBySpace = (toolbarW - centerMin) / 2;

  // Cap side width by our preferred max (MODE max), but allow shrinking when narrow.
  const int preferredSideW = qMax(minSideW, m_statusMaxW);
  const int sideW = qMax(minSideW, qMin(preferredSideW, qMax(0, sideMaxBySpace)));

  m_leftBlock->setFixedWidth(sideW);
  m_rightBlock->setFixedWidth(sideW);
  m_statusLabel->setMaximumWidth(sideW);
  m_statusLabel->setMinimumWidth(qMin(sideW, m_statusMinW));

  // Re-apply text after width changes.
  applyStatusLabelText();
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
