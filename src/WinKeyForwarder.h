#pragma once

#ifdef _WIN32

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

#include <QAbstractNativeEventFilter>
#include <QSet>

class ResultWindow;

// Windows-only: WebView2 often consumes WM_KEYDOWN in its child HWND, so Qt shortcuts/DOM keydown
// might never fire. This native event filter forwards specific key presses to the owning ResultWindow.
class WinKeyForwarder : public QAbstractNativeEventFilter
{
public:
    static WinKeyForwarder &instance();

    // Hard trace that bypasses Qt message handler: writes to OutputDebugStringA and, when debug logging is enabled, wkf.log.
    static void trace(const char *msg);

    // Install/uninstall low-level keyboard hook (Windows only).
    void install();
    void uninstall();

    void registerResultWindow(ResultWindow *window);
    void unregisterResultWindow(ResultWindow *window);

    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;

private:
    WinKeyForwarder() = default;
    ~WinKeyForwarder() = default;

    // Track active windows. ResultWindow unregisters itself in its destructor.
    QSet<ResultWindow *> m_windows;
    bool m_sForwardedDown = false;
    bool m_fForwardedDown = false;

    // Low-level keyboard hook state
    HHOOK m_kbHook = nullptr;
    static LRESULT CALLBACK LowLevelProc(int nCode, WPARAM wParam, LPARAM lParam);
    LRESULT handleLowLevel(WPARAM wParam, KBDLLHOOKSTRUCT *ks);

    static bool anyModifierDown();
};

#endif
