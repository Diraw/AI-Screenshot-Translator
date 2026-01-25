#pragma once

#include "EmbedWebView.h"

#include <QDebug>
#include <QString>

#include <functional>

// Ensure static linkage for webview.h symbols
#define WEBVIEW_API static
#include "webview.h"

#ifdef _WIN32
#include "WebView2.h"
#include <wrl.h>
using Microsoft::WRL::Callback;

static int parseChromeWidgetSuffix(const wchar_t *cls)
{
    if (!cls)
        return -1;
    const wchar_t *underscore = wcsrchr(cls, L'_');
    if (!underscore || !underscore[1])
        return -1;
    wchar_t *end = nullptr;
    long v = wcstol(underscore + 1, &end, 10);
    if (end == underscore + 1)
        return -1;
    if (v < 0 || v > 100000)
        return -1;
    return static_cast<int>(v);
}

static HWND findWebViewChild(HWND parent)
{
    if (!IsWindow(parent))
        return nullptr;

    auto scan = [](HWND p) -> HWND
    {
        if (!IsWindow(p))
            return nullptr;

        struct Ctx
        {
            HWND best = nullptr;
            int bestScore = -1;
        } ctx;

        EnumChildWindows(p, [](HWND h, LPARAM lParam) -> BOOL
                         {
            auto *ctx = reinterpret_cast<Ctx *>(lParam);
            wchar_t cls[256] = {0};
            GetClassNameW(h, cls, 255);
            if (wcsstr(cls, L"Chrome_WidgetWin") == nullptr)
                return TRUE;

            // Prefer a window that looks like the actual input target.
            // Score: visible + enabled + higher suffix wins.
            int score = 0;
            if (IsWindowVisible(h))
                score += 1000;
            if (IsWindowEnabled(h))
                score += 100;
            int suffix = parseChromeWidgetSuffix(cls);
            if (suffix >= 0)
                score += suffix;

            if (score >= ctx->bestScore)
            {
                ctx->bestScore = score;
                ctx->best = h;
            }
            return TRUE; }, (LPARAM)&ctx);

        return ctx.best;
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

        RECT bounds{};
        bounds.left = 0;
        bounds.top = 0;
        bounds.right = physicalWidth;
        bounds.bottom = physicalHeight;
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
        // If focus is already inside this WebView's Chrome widget subtree, do not interrupt it.
        // Repeated SetFocus can cause WV2 to emit internal focus/blur transitions.
        HWND root = GetAncestor(host, GA_ROOT);
        HWND f = ::GetFocus();
        if (f)
        {
            wchar_t fcls[256] = {0};
            ::GetClassNameW(f, fcls, 255);
            bool isChrome = (wcsstr(fcls, L"Chrome_WidgetWin") != nullptr);
            bool inTree = ::IsChild(host, f) || (root && ::IsChild(root, f));
            if (isChrome && inTree)
                return;
        }

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
