#include "App.h"

#include "ThemeUtils.h"

#include <QApplication>
#include <QDebug>

void App::quitApp()
{
    qApp->quit();
}

void App::checkForThemeChange()
{
    bool currentDark = ThemeUtils::isSystemDark();
    if (currentDark != m_lastTopBarDark)
    {
        qDebug() << "Theme change detected: " << (currentDark ? "Dark" : "Light");
        m_lastTopBarDark = currentDark;
        updateAllWindowThemes(currentDark);
    }
}

void App::updateAllWindowThemes(bool isDark)
{
    if (m_summaryWindow)
    {
        m_summaryWindow->updateTheme(isDark);
    }

    if (m_activeConfigDialog)
    {
        m_activeConfigDialog->updateTheme(isDark);
    }

    for (auto w : m_activeWindows)
    {
        if (!w)
            continue;
        if (ResultWindow *rw = qobject_cast<ResultWindow *>(w.data()))
        {
            rw->updateTheme(isDark);
        }
    }
}
