#pragma once

#include <QWidget>
#include <QSettings>
#include <QApplication>
#include <QPalette>
#include <QColor>
#ifdef _WIN32
#include <windows.h>
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")
#endif

namespace ThemeUtils {

    inline bool isSystemDark() {
#ifdef _WIN32
        QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", QSettings::NativeFormat);
        bool lightTheme = settings.value("AppsUseLightTheme", 1).toInt() != 0;
        return !lightTheme;
#else
        QPalette p = qApp->palette();
        return p.window().color().value() < 128;
#endif
    }

#ifdef _WIN32
    inline void applyDarkTitleBar(QWidget *window, bool dark) {
        if (!window)
            return;
        HWND hwnd = reinterpret_cast<HWND>(window->winId());
        if (!hwnd)
            return;

        BOOL useDark = dark ? TRUE : FALSE;
        static auto dwmSetWindowAttribute = reinterpret_cast<decltype(&DwmSetWindowAttribute)>(
            GetProcAddress(GetModuleHandleW(L"dwmapi.dll"), "DwmSetWindowAttribute"));
        if (!dwmSetWindowAttribute)
            return;

        // Try modern attribute first, then the legacy Win10 one.
        if (FAILED(dwmSetWindowAttribute(hwnd, 20, &useDark, sizeof(useDark)))) {
            dwmSetWindowAttribute(hwnd, 19, &useDark, sizeof(useDark));
        }
    }
#endif

    inline void applyThemeToWindow(QWidget* window, bool dark) {
        Q_UNUSED(dark);
        if (!window) return;
        window->setPalette(qApp ? qApp->palette() : QPalette());
#ifdef _WIN32
        applyDarkTitleBar(window, dark);
#endif
    }
}
