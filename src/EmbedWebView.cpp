
#include "EmbedWebView_Impl.h"

#include <QEvent>
#include <QGuiApplication>
#include <QMetaObject>
#include <QPointer>
#include <QResizeEvent>
#include <QScreen>
#include <QTimer>
#include <QWidget>

EmbedWebView::EmbedWebView(QWidget *parent) : QObject(parent)
{
    try
    {
        m_parentWidget = parent;
        if (!parent->testAttribute(Qt::WA_NativeWindow))
        {
            parent->setAttribute(Qt::WA_NativeWindow);
        }
        parent->winId(); // ensure native window

        // Install event filter to handle resizing automatically
        parent->installEventFilter(this);

        m_hwnd = (HWND)parent->winId();
        // win32_edge_engine expects pointer to HWND value; pass address of stable member
        m_impl = std::make_unique<Impl>(static_cast<void *>(&m_hwnd));
        m_resizeTimer = new QTimer(this);
        m_resizeTimer->setSingleShot(true);
        m_resizeTimer->setInterval(15); // Throttle to ~15ms

        QPointer<QWidget> p = m_parentWidget;
        connect(m_resizeTimer, &QTimer::timeout, this, [this, p]()
                {
            if (!p)
                return;
            if (!p->isVisible())
                return;
            if (p->width() <= 0 || p->height() <= 0)
                return;
            setSize(p->width(), p->height()); });

        // Start polling for readiness
        m_initTimer = new QTimer(this);
        connect(m_initTimer, &QTimer::timeout, this, &EmbedWebView::checkReady);
        m_initTimer->start(100); // Check every 100ms

        connect(parent, &QObject::destroyed, this, [this]()
                {
                    if (m_resizeTimer)
                        m_resizeTimer->stop();
                    if (m_initTimer)
                        m_initTimer->stop();
                    m_pendingActions.clear();
                    m_impl.reset();
                    m_isReady = false;
#ifdef _WIN32
                    m_hwnd = nullptr;
#endif
                });
    }
    catch (const std::exception &e)
    {
        qCritical() << "EmbedWebView Constructor Exception: " << e.what();
    }
}

EmbedWebView::~EmbedWebView() = default;

bool EmbedWebView::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == parent())
    {
        if (event->type() == QEvent::Resize)
        {
            auto *re = static_cast<QResizeEvent *>(event);
            if (re->size().isValid() && re->size().width() > 0 && re->size().height() > 0)
            {
                if (m_resizeTimer)
                    m_resizeTimer->start();
            }
        }
        else if (event->type() == QEvent::Move)
        {
            if (m_resizeTimer)
                m_resizeTimer->start();
        }
    }
    return QObject::eventFilter(watched, event);
}

void EmbedWebView::checkReady()
{
    if (m_impl && m_impl->isReady())
    {
        m_isReady = true;
        m_initTimer->stop();

        // Force initial size and visibility
        QWidget *p = m_parentWidget;
        if (p)
        {
            if (m_impl)
                m_impl->setBackgroundColor(30, 30, 30, 255);

            m_impl->resize_controller(p->width(), p->height());
            m_impl->setVisible(true);
        }

#ifdef _WIN32
        // WebView2 focus tracing + auto-refocus fallback.
        // 用于确认 JS blur 是否对应 WV2 LostFocus，并在丢焦时快速抢回。
        if (m_impl->m_controller)
        {
            EventRegistrationToken gotToken{};
            EventRegistrationToken lostToken{};
            QPointer<EmbedWebView> self(this);

            m_impl->m_controller->add_GotFocus(
                Callback<ICoreWebView2FocusChangedEventHandler>(
                    [self](ICoreWebView2Controller *, IUnknown *) -> HRESULT
                    {
                        if (!self)
                            return S_OK;
                        qDebug() << "[WV2] GotFocus";
                        self->m_wv2RefocusPending = false;
                        return S_OK;
                    })
                    .Get(),
                &gotToken);

            m_impl->m_controller->add_LostFocus(
                Callback<ICoreWebView2FocusChangedEventHandler>(
                    [self](ICoreWebView2Controller *, IUnknown *) -> HRESULT
                    {
                        if (!self)
                            return S_OK;
                        qDebug() << "[WV2] LostFocus";

                        // IMPORTANT: do not auto-refocus here.
                        // WebView2 may emit internal focus transitions during navigation/resize/show.
                        // Forcing SetFocus in this callback can amplify into a GotFocus/LostFocus loop.
                        return S_OK;
                    })
                    .Get(),
                &lostToken);
        }

        // Ensure DevTools are enabled and capture F12/Ctrl+Shift+I even when focus is inside WebView
        if (m_impl->m_webview)
        {
            ICoreWebView2Settings *settings = nullptr;
            if (SUCCEEDED(m_impl->m_webview->get_Settings(&settings)) && settings)
            {
                settings->put_AreDevToolsEnabled(TRUE);
                settings->Release();
            }
        }
        if (m_impl->m_controller)
        {
            EventRegistrationToken token{};
            m_impl->m_controller->add_AcceleratorKeyPressed(
                Callback<ICoreWebView2AcceleratorKeyPressedEventHandler>(
                    [this](ICoreWebView2Controller *, ICoreWebView2AcceleratorKeyPressedEventArgs *args) -> HRESULT
                    {
                        COREWEBVIEW2_KEY_EVENT_KIND kind;
                        if (FAILED(args->get_KeyEventKind(&kind)))
                            return S_OK;
                        if (kind != COREWEBVIEW2_KEY_EVENT_KIND_KEY_DOWN &&
                            kind != COREWEBVIEW2_KEY_EVENT_KIND_SYSTEM_KEY_DOWN)
                        {
                            return S_OK;
                        }
                        UINT key = 0;
                        if (FAILED(args->get_VirtualKey(&key)))
                            return S_OK;
                        bool isCtrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                        bool isShift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                        if (key == VK_F12 || (isCtrl && isShift && key == 'I'))
                        {
                            qDebug() << "[DevTools] AcceleratorKeyPressed -> open";
                            openDevTools();
                            args->put_Handled(TRUE);
                        }
                        return S_OK;
                    })
                    .Get(),
                &token);
        }
#endif

        for (const auto &action : m_pendingActions)
        {
            action();
        }
        m_pendingActions.clear();

        emit ready();
    }
}

void EmbedWebView::implSetHtml(const std::string &html)
{
    if (m_impl)
        m_impl->set_html(html);
}

void EmbedWebView::implEval(const std::string &js)
{
    if (m_impl)
        m_impl->eval(js);
}

void EmbedWebView::implResolve(const std::string &seq, int status, const std::string &result)
{
    if (m_impl)
        m_impl->resolve(seq, status, result);
}

void EmbedWebView::implSetSize(int width, int height)
{
    if (m_impl)
        m_impl->resize_controller(width, height);
}

void EmbedWebView::implFocus()
{
    if (m_impl)
        m_impl->focus();
}

void EmbedWebView::implFocusNative()
{
    if (m_impl)
        m_impl->focus_native();
}

void EmbedWebView::implSetBackgroundColor(int r, int g, int b, int a)
{
    if (m_impl)
        m_impl->setBackgroundColor(r, g, b, a);
}

void EmbedWebView::implBind(const std::string &name, BindCallback fn)
{
    if (m_impl)
        m_impl->bind(name, fn, nullptr);
}

void EmbedWebView::implSetVisible(bool visible)
{
    if (m_impl)
        m_impl->setVisible(visible);
}

void EmbedWebView::implOpenDevTools()
{
#ifdef _WIN32
    if (!m_impl)
        return;

    ICoreWebView2 *webview = nullptr;
    if (m_impl->m_webview)
    {
        webview = m_impl->m_webview;
        webview->AddRef();
    }
    else if (m_impl->m_controller)
    {
        m_impl->m_controller->get_CoreWebView2(&webview);
    }

    if (!webview)
        return;

    ICoreWebView2Settings *settings = nullptr;
    if (SUCCEEDED(webview->get_Settings(&settings)) && settings)
    {
        settings->put_AreDevToolsEnabled(TRUE);
        settings->Release();
    }
    webview->OpenDevToolsWindow();
    webview->Release();
#endif
}
