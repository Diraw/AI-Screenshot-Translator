#pragma once

#include <QWidget>
#include <QSettings>
#include <QApplication>
#include <QPalette>
#include <QColor>

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

    inline struct ThemeColors {
        QColor background;
        QColor text;
    } getColors(bool dark) {
        QPalette pal = qApp ? qApp->palette() : QPalette();
        Q_UNUSED(dark);
        return { pal.window().color(), pal.windowText().color() };
    }

    inline void applyThemeToWindow(QWidget* window, bool dark) {
        Q_UNUSED(dark);
        if (!window) return;
        window->setPalette(qApp ? qApp->palette() : QPalette());
    }
}
