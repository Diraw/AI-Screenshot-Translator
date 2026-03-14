#include "WinKeyForwarder.h"

#ifdef _WIN32

#include <QApplication>
#include <QDir>
#include <QMutexLocker>
#include <QMetaObject>
#include <QtGlobal>
#include <cstdio>
#include <windows.h>

#include "ResultWindow.h"

// From main.cpp
extern bool g_enableLogging;
extern QString g_logDirectoryPath;

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

    // Only emit wkf.log when debug logging is enabled (or env override).
    // Default release runs should not create this file.
    const bool fileLoggingEnabled = g_enableLogging || qEnvironmentVariableIsSet("FORCE_DEBUG_LOG");
    if (!fileLoggingEnabled)
        return;

    static bool wkfLogTruncatedThisRun = false;
    const wchar_t *openMode = (!wkfLogTruncatedThisRun) ? L"w" : L"a";

    FILE *f = nullptr;
    const QString logPath = QDir(g_logDirectoryPath).filePath("wkf.log");
    if (_wfopen_s(&f, reinterpret_cast<const wchar_t *>(logPath.utf16()), openMode) == 0 && f)
    {
        if (!wkfLogTruncatedThisRun)
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

    QMutexLocker locker(&m_windowsMutex);
    for (const QPointer<ResultWindow> &existingPtr : m_windows)
    {
        if (existingPtr.data() == window)
            return;
    }
    m_windows.append(QPointer<ResultWindow>(window));
}

void WinKeyForwarder::unregisterResultWindow(ResultWindow *window)
{
    if (!window)
        return;

    QMutexLocker locker(&m_windowsMutex);
    for (int i = m_windows.size() - 1; i >= 0; --i)
    {
        ResultWindow *existing = m_windows.at(i).data();
        if (!existing || existing == window)
            m_windows.removeAt(i);
    }
}

QList<QPointer<ResultWindow>> WinKeyForwarder::snapshotWindows()
{
    QMutexLocker locker(&m_windowsMutex);
    for (int i = m_windows.size() - 1; i >= 0; --i)
    {
        if (m_windows.at(i).isNull())
            m_windows.removeAt(i);
    }
    return m_windows;
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
    const bool isUp = (wParam == WM_KEYUP || wParam == WM_SYSKEYUP);
    const int vk = static_cast<int>(ks->vkCode);

    // Handle 'S' key (screenshot toggle)
    if (vk == 'S')
    {
        if (isUp)
        {
            const bool swallow = m_sForwardedDown;
            m_sForwardedDown = false;
            return swallow ? 1 : 0;
        }

        if (!isDown)
            return 0;

        if (m_sForwardedDown)
            return 1; // Suppress key-repeat while the key remains held.

        if (anyModifierDown())
            return 0; // require no modifiers

        HWND fg = GetForegroundWindow();
        if (!fg)
            return 0;

        const QList<QPointer<ResultWindow>> windows = snapshotWindows();

        // Exact match to foreground window
        for (const QPointer<ResultWindow> &rwPtr : windows)
        {
            ResultWindow *rw = rwPtr.data();
            if (!rw)
                continue;
            HWND rwHwnd = reinterpret_cast<HWND>(rw->winId());
            if (rwHwnd == fg)
            {
                trace("[WKF] HIT S (hook) -> DISPATCH");
                m_sForwardedDown = true;
                QMetaObject::invokeMethod(rw, "triggerScreenshotFromNative", Qt::QueuedConnection);
                return 1;
            }
        }

        // Fallback: match by root window
        HWND root = GetAncestor(fg, GA_ROOT);
        if (root)
        {
            for (const QPointer<ResultWindow> &rwPtr : windows)
            {
                ResultWindow *rw = rwPtr.data();
                if (!rw)
                    continue;
                HWND rwHwnd = reinterpret_cast<HWND>(rw->winId());
                if (rwHwnd == root)
                {
                    trace("[WKF] HIT S (hook/root) -> DISPATCH");
                    m_sForwardedDown = true;
                    QMetaObject::invokeMethod(rw, "triggerScreenshotFromNative", Qt::QueuedConnection);
                    return 1;
                }
            }
        }

        trace("[WKF] HIT S (hook) but no matching ResultWindow");
        return 0;
    }

    // Handle retranslate hotkey (configurable, default 'F')
    // Check if any ResultWindow has this key configured
    HWND fg = GetForegroundWindow();
    if (fg && isDown && !anyModifierDown())
    {
        const QList<QPointer<ResultWindow>> windows = snapshotWindows();

        // Check all registered ResultWindows
        for (const QPointer<ResultWindow> &rwPtr : windows)
        {
            ResultWindow *rw = rwPtr.data();
            if (!rw)
                continue;
            HWND rwHwnd = reinterpret_cast<HWND>(rw->winId());
            // Match foreground window or its root
            HWND root = GetAncestor(fg, GA_ROOT);
            if (rwHwnd == fg || rwHwnd == root)
            {
                QString hotkey = rw->retranslateHotkey();
                if (hotkey.isEmpty())
                    hotkey = "f";
                
                // Parse hotkey - support single key only for native hook
                // Convert to uppercase for comparison
                QString keyName = hotkey.toUpper();
                int expectedVk = 0;
                
                // Single character keys A-Z, 0-9
                if (keyName.length() == 1)
                {
                    QChar c = keyName[0];
                    if (c >= 'A' && c <= 'Z')
                        expectedVk = c.unicode();
                    else if (c >= '0' && c <= '9')
                        expectedVk = c.unicode();
                }
                
                if (expectedVk != 0 && vk == expectedVk)
                {
                    if (m_fForwardedDown)
                        return 1; // Suppress key-repeat
                    
                    char buf[128];
                    sprintf_s(buf, sizeof(buf), "[WKF] HIT %c (retranslate hook) -> DISPATCH", (char)expectedVk);
                    trace(buf);
                    m_fForwardedDown = true;
                    QMetaObject::invokeMethod(rw, "triggerRetranslateFromNative", Qt::QueuedConnection);
                    return 1;
                }
                break; // Found matching window, don't check others
            }
        }
    }
    
    if (isUp && m_fForwardedDown)
    {
        m_fForwardedDown = false;
        return 1;
    }

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
