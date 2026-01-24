#include "EmbedWebView.h"
#include <QDebug>
#include <QWidget>
#include <QEvent>
#include <QResizeEvent>
#include <functional>

// Define helper macros to ensure static linkage of webview.h functions
// and prevent LNK2005
#define WEBVIEW_API static
#include "webview.h"

#include <QTimer>

#ifdef _WIN32
#include "WebView2.h"
#include <wrl.h>
using Microsoft::WRL::Callback;

static HWND findWebViewChild(HWND parent)
{
    if (!IsWindow(parent))
        return nullptr;

    struct Ctx
    {
        HWND found = nullptr;
    } ctx;

    EnumChildWindows(parent, [](HWND h, LPARAM lParam) -> BOOL
                     {
        auto *ctx = reinterpret_cast<Ctx *>(lParam);
        if (!IsWindowVisible(h) || !IsWindowEnabled(h))
            return TRUE;

        wchar_t cls[256] = {0};
        GetClassNameW(h, cls, 255);
        if (wcsstr(cls, L"Chrome_WidgetWin") != nullptr)
        {
            ctx->found = h;
            return FALSE;
        }
        return TRUE; }, (LPARAM)&ctx);

    if (!ctx.found)
    {
        EnumChildWindows(parent, [](HWND h, LPARAM lParam) -> BOOL
                         {
            auto *ctx = reinterpret_cast<Ctx *>(lParam);
            if (!IsWindowVisible(h) || !IsWindowEnabled(h))
                return TRUE;
            ctx->found = h;
            return FALSE; }, (LPARAM)&ctx);
    }
    return ctx.found;
}
#endif

class EmbedWebView::Impl : public webview::webview
{
public:
    Impl(void *hwnd) : webview::webview(false, hwnd) {}

    bool isReady() const { return is_init(); }

    void setVisible(bool visible)
    {
        if (is_init())
        {
            m_controller->put_IsVisible(visible ? TRUE : FALSE);
        }
    }

    void resize_controller(int width, int height)
    {
        if (!is_init() || !m_controller)
            return;

        HWND hwnd = (HWND)m_window;
        if (!IsWindow(hwnd))
            return;

        int dpi = GetDpiForWindow(hwnd);
        if (dpi <= 0)
            dpi = 96;
        double scale = dpi / 96.0;

        int physicalWidth = static_cast<int>(width * scale);
        int physicalHeight = static_cast<int>(height * scale);

        qDebug() << "resize_controller: Logical" << width << "x" << height
                 << "DPI:" << dpi << "Scale:" << scale
                 << "Physical:" << physicalWidth << "x" << physicalHeight;

        RECT bounds = {0, 0, physicalWidth, physicalHeight};
        m_controller->put_Bounds(bounds);
    }

    void focus()
    {
        if (is_init())
        {
            m_controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
        }
    }

    void focus_native()
    {
        if (!is_init())
            return;

        HWND host = (HWND)m_window;
        if (!IsWindow(host))
            return;

#ifdef _WIN32
        if (!m_webChild || !IsWindow(m_webChild))
        {
            m_webChild = findWebViewChild(host);
            wchar_t cls[256] = {0};
            if (m_webChild)
                GetClassNameW(m_webChild, cls, 255);
            qDebug() << "[WebView] host=" << (void *)host << " child=" << (void *)m_webChild << " class=" << QString::fromWCharArray(cls);
        }

        HWND target = (m_webChild && IsWindow(m_webChild)) ? m_webChild : host;
        ::SetFocus(target);
#else
        ::SetFocus(host);
#endif
        m_controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
    }

    void setBackgroundColor(int r, int g, int b, int a)
    {
        if (is_init())
        {
            ICoreWebView2Controller2 *controller2 = nullptr;
            HRESULT hr = m_controller->QueryInterface(__uuidof(ICoreWebView2Controller2), (void **)&controller2);
            if (SUCCEEDED(hr) && controller2)
            {
                COREWEBVIEW2_COLOR color;
                color.A = a;
                color.R = r;
                color.G = g;
                color.B = b;
                controller2->put_DefaultBackgroundColor(color);
                controller2->Release();
            }
        }
    }

#ifdef _WIN32
private:
    HWND m_webChild = nullptr;
#endif
};

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
            if (!p) return;
            if (!p->isVisible()) return;
            if (p->width() <= 0 || p->height() <= 0) return;
            setSize(p->width(), p->height()); });

        // Start polling for readiness
        m_initTimer = new QTimer(this);
        connect(m_initTimer, &QTimer::timeout, this, &EmbedWebView::checkReady);
        m_initTimer->start(100); // Check every 100ms

        connect(parent, &QObject::destroyed, this, [this]()
                {
                    if (m_resizeTimer) m_resizeTimer->stop();
                    if (m_initTimer) m_initTimer->stop();
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

void EmbedWebView::setHtml(const std::string &html)
{
    if (!m_impl)
        return;
    if (m_hasEverSetHtml)
    {
        qWarning() << "[Native] setHtml ignored (already loaded once)" << (void *)this;
        return;
    }
    m_hasEverSetHtml = true;

    if (!m_isReady)
    {
        m_pendingActions.push_back([this, html]()
                                   { m_impl->set_html(html); });
        return;
    }
    m_impl->set_html(html);
}

void EmbedWebView::eval(const std::string &js)
{
    if (!m_impl)
        return;
    if (!m_isReady)
    {
        m_pendingActions.push_back([this, js]()
                                   { m_impl->eval(js); });
        return;
    }
    m_impl->eval(js);
}

void EmbedWebView::resolve(const std::string &seq, int status, const std::string &result)
{
    if (!m_impl)
        return;
    if (!m_isReady)
    {
        m_pendingActions.push_back([this, seq, status, result]()
                                   { m_impl->resolve(seq, status, result); });
        return;
    }
    m_impl->resolve(seq, status, result);
}

void EmbedWebView::setSize(int width, int height)
{
    if (!m_impl)
        return;

    if (!m_isReady)
    {
        m_pendingActions.push_back([this, width, height]()
                                   { m_impl->resize_controller(width, height); });
        return;
    }
    m_impl->resize_controller(width, height);
}

void EmbedWebView::focus()
{
    if (!m_impl)
        return;
    if (!m_isReady)
    {
        m_pendingActions.push_back([this]()
                                   { m_impl->focus(); });
        return;
    }
    m_impl->focus();
}

void EmbedWebView::focusNative()
{
    if (!m_impl)
        return;
    if (!m_isReady)
    {
        m_pendingActions.push_back([this]()
                                   { m_impl->focus_native(); });
        return;
    }
    m_impl->focus_native();
}

void EmbedWebView::setBackgroundColor(int r, int g, int b, int a)
{
    if (!m_impl)
        return;
    if (!m_isReady)
    {
        m_pendingActions.push_back([this, r, g, b, a]()
                                   { m_impl->setBackgroundColor(r, g, b, a); });
        return;
    }
    m_impl->setBackgroundColor(r, g, b, a);
}

void EmbedWebView::bind(const std::string &name, BindCallback fn)
{
    if (!m_isReady)
    {
        m_pendingActions.push_back([this, name, fn]()
                                   { m_impl->bind(name, fn, nullptr); });
        return;
    }
    m_impl->bind(name, fn, nullptr);
}

void EmbedWebView::setVisible(bool visible)
{
    if (m_isReady && m_impl)
    {
        m_impl->setVisible(visible);
    }
    else if (!m_isReady)
    {
        m_pendingActions.push_back([this, visible]()
                                   { if (m_impl) m_impl->setVisible(visible); });
    }
}

void EmbedWebView::openDevTools()
{
#ifdef _WIN32
    if (!m_impl)
        return;

    auto action = [this]()
    {
        if (!m_impl)
            return;

        ICoreWebView2 *webview = nullptr;
        // Prefer cached webview
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
    };

    if (!m_isReady)
    {
        m_pendingActions.push_back(action);
        return;
    }
    action();
#endif
}
