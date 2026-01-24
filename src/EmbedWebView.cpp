#include "EmbedWebView.h"

#include <QDebug>
#include <QEvent>
#include <QPointer>
#include <QResizeEvent>
#include <QMetaObject>
#include <QTimer>
#include <QWidget>
#include <QString>

#include <functional>

// Ensure static linkage for webview.h symbols
#define WEBVIEW_API static
#include "webview.h"

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

    auto scan = [](HWND p) -> HWND
    {
        if (!IsWindow(p))
            return nullptr;

        struct Ctx
        {
            HWND found = nullptr;
        } ctx;

        EnumChildWindows(p, [](HWND h, LPARAM lParam) -> BOOL
                         {
            auto *ctx = reinterpret_cast<Ctx *>(lParam);
            wchar_t cls[256] = {0};
            GetClassNameW(h, cls, 255);
            if (wcsstr(cls, L"Chrome_WidgetWin") != nullptr)
            {
                ctx->found = h;
                return FALSE;
            }
            return TRUE; }, (LPARAM)&ctx);

        return ctx.found;
    };

    HWND found = scan(parent);
    if (!found)
    {
        HWND root = GetAncestor(parent, GA_ROOT);
        if (root && root != parent)
            found = scan(root);
    }
    return found;
}
#endif

class EmbedWebView::Impl : public webview::webview
{
public:
    explicit Impl(void *hwnd) : webview::webview(false, hwnd) {}

    bool isReady() const { return is_init(); }

    void setVisible(bool visible)
    {
        if (is_init() && m_controller)
            m_controller->put_IsVisible(visible ? TRUE : FALSE);
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

        COREWEBVIEW2_RECT bounds{};
        bounds.X = 0;
        bounds.Y = 0;
        bounds.Width = physicalWidth;
        bounds.Height = physicalHeight;
        m_controller->put_Bounds(bounds);
    }

    void focus()
    {
        if (is_init() && m_controller)
            m_controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
    }

    void focus_native()
    {
        if (!is_init() || !m_controller)
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

        // NOTE: MoveFocus(PROGRAMMATIC) 很容易引起 WebView2 内部 focus/blur 抖动。
        // 这里先仅保留 OS 级 SetFocus 到子窗口，观察是否能消除 window blur。
    }

    void setBackgroundColor(int r, int g, int b, int a)
    {
        if (!is_init() || !m_controller)
            return;

        ICoreWebView2Controller2 *controller2 = nullptr;
        HRESULT hr = m_controller->QueryInterface(__uuidof(ICoreWebView2Controller2), (void **)&controller2);
        if (SUCCEEDED(hr) && controller2)
        {
            COREWEBVIEW2_COLOR color{};
            color.A = static_cast<BYTE>(a);
            color.R = static_cast<BYTE>(r);
            color.G = static_cast<BYTE>(g);
            color.B = static_cast<BYTE>(b);
            controller2->put_DefaultBackgroundColor(color);
            controller2->Release();
        }
    }

#ifdef _WIN32
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
                        qDebug() << "[WV2] LostFocus -> refocus";
                        QMetaObject::invokeMethod(
                            self,
                            [self]()
                            {
                                if (!self)
                                    return;
                                self->focusNative();
                                self->eval("(()=>{try{document.body.tabIndex=-1;document.body.focus({preventScroll:true});}catch(e){}})();");
                            },
                            Qt::QueuedConnection);
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

void EmbedWebView::setHtml(const std::string &html)
{
    static int setHtmlCount = 0;
    QWidget *p = m_parentWidget.data();
    QWidget *top = p ? p->window() : nullptr;
    qDebug() << "[Native] setHtml called" << ++setHtmlCount
             << "EmbedWebView=" << (void *)this
             << "parentWidget=" << (void *)p
             << "topWindow=" << (void *)top
             << (top ? top->windowTitle() : QString("<null>"))
             << "len=" << static_cast<int>(html.size());

    if (m_hasEverSetHtml)
    {
        qWarning() << "[Native] setHtml ignored (already loaded once)" << (void *)this;
        return;
    }
    m_hasEverSetHtml = true;

    if (!m_impl)
        return;
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
                                   {
            if (m_impl)
                m_impl->setVisible(visible); });
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
