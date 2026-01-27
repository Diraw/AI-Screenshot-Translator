#include "WinKeyForwarder.h"

#ifdef _WIN32

#include <QApplication>
#include <QMetaObject>
#include <QtGlobal>
#include <cstdio>
#include <windows.h>

#include "ResultWindow.h"

// From main.cpp
extern bool g_enableLogging;

void WinKeyForwarder::trace(const char *msg)
{
    if (!msg)
        return;

    // Timestamp prefix (local time)
    SYSTEMTIME st;
    GetLocalTime(&st);
    char ts[64];
    sprintf_s(ts, sizeof(ts), "%04u-%02u-%02u %02u:%02u:%02u.%03u ",
              (unsigned)st.wYear, (unsigned)st.wMonth, (unsigned)st.wDay,
              (unsigned)st.wHour, (unsigned)st.wMinute, (unsigned)st.wSecond,
              (unsigned)st.wMilliseconds);

    OutputDebugStringA(ts);
    OutputDebugStringA(msg);
    OutputDebugStringA("\n");

    // Keep writing wkf.log for hard traces, but when app file logging is enabled
    // (debug mode or env override), truncate it once per process so each run starts fresh.
    static bool wkfLogTruncatedThisRun = false;
    const bool allowFreshRun = g_enableLogging || qEnvironmentVariableIsSet("FORCE_DEBUG_LOG");
    const char *openMode = (allowFreshRun && !wkfLogTruncatedThisRun) ? "w" : "a";

    FILE *f = nullptr;
    if (fopen_s(&f, "wkf.log", openMode) == 0 && f)
    {
        if (allowFreshRun && !wkfLogTruncatedThisRun)
            wkfLogTruncatedThisRun = true;
        fprintf(f, "%s%s\n", ts, msg);
        fclose(f);
    }
}

WinKeyForwarder &WinKeyForwarder::instance()
{
    static WinKeyForwarder inst;
    return inst;
}

void WinKeyForwarder::install()
{
    if (m_kbHook)
        return;

    trace("[WKF] installing WH_KEYBOARD_LL hook");
    m_kbHook = SetWindowsHookExW(WH_KEYBOARD_LL, &WinKeyForwarder::LowLevelProc, GetModuleHandleW(nullptr), 0);
    if (m_kbHook)
        trace("[WKF] hook installed");
    else
        trace("[WKF] hook install FAILED");
}

void WinKeyForwarder::uninstall()
{
    if (!m_kbHook)
        return;
    UnhookWindowsHookEx(m_kbHook);
    m_kbHook = nullptr;
    trace("[WKF] hook uninstalled");
}

void WinKeyForwarder::registerResultWindow(ResultWindow *window)
{
    if (!window)
        return;
    m_windows.insert(window);
}

void WinKeyForwarder::unregisterResultWindow(ResultWindow *window)
{
    if (!window)
        return;
    m_windows.remove(window);
}

bool WinKeyForwarder::anyModifierDown()
{
    const bool ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    const bool alt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
    const bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    return ctrl || alt || shift;
}

LRESULT CALLBACK WinKeyForwarder::LowLevelProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION && lParam)
    {
        auto *ks = reinterpret_cast<KBDLLHOOKSTRUCT *>(lParam);
        auto &inst = WinKeyForwarder::instance();
        LRESULT r = inst.handleLowLevel(wParam, ks);
        if (r != 0)
            return r; // eat the key
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

LRESULT WinKeyForwarder::handleLowLevel(WPARAM wParam, KBDLLHOOKSTRUCT *ks)
{
    if (!ks)
        return 0;

    const bool isDown = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
    if (!isDown)
        return 0;

    const int vk = static_cast<int>(ks->vkCode);
    if (vk != 'S')
        return 0; // only plain S

    if (anyModifierDown())
        return 0; // require no modifiers

    HWND fg = GetForegroundWindow();
    if (!fg)
        return 0;

    // Exact match to foreground window
    for (ResultWindow *rw : m_windows)
    {
        if (!rw)
            continue;
        HWND rwHwnd = reinterpret_cast<HWND>(rw->winId());
        if (rwHwnd == fg)
        {
            trace("[WKF] HIT S (hook) -> DISPATCH");
            QMetaObject::invokeMethod(rw, "triggerScreenshotFromNative", Qt::QueuedConnection);
            return 1;
        }
    }

    // Fallback: match by root window
    HWND root = GetAncestor(fg, GA_ROOT);
    if (root)
    {
        for (ResultWindow *rw : m_windows)
        {
            if (!rw)
                continue;
            HWND rwHwnd = reinterpret_cast<HWND>(rw->winId());
            if (rwHwnd == root)
            {
                trace("[WKF] HIT S (hook/root) -> DISPATCH");
                QMetaObject::invokeMethod(rw, "triggerScreenshotFromNative", Qt::QueuedConnection);
                return 1;
            }
        }
    }

    trace("[WKF] HIT S (hook) but no matching ResultWindow");
    return 0;
}

// We keep the interface satisfied; not used now that hook is in place.
bool WinKeyForwarder::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result)
{
    Q_UNUSED(eventType);
    Q_UNUSED(message);
    Q_UNUSED(result);
    return false;
}

#endif // _WIN32
