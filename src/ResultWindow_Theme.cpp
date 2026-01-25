#include "ResultWindow.h"

#include "EmbedWebView.h"
#include "ThemeUtils.h"
#include "ColorUtils.h"

void ResultWindow::updateTheme(bool isDark)
{
    ThemeUtils::applyThemeToWindow(this, isDark);
    if (m_webView)
    {
        QColor bg = isDark ? QColor(30, 30, 30) : QColor(255, 255, 255);
        m_webView->setBackgroundColor(bg.red(), bg.green(), bg.blue(), 255);
        QString toggleJs = QString("document.documentElement.classList.toggle('dark-mode', %1); document.body.classList.toggle('dark-mode', %1);")
                               .arg(isDark ? "true" : "false");
        m_webView->eval(toggleJs.toStdString());
    }
    if (m_toolBar)
        m_toolBar->setStyleSheet("");
    if (m_statusLabel)
        m_statusLabel->setStyleSheet("");
    if (m_pageLabel)
        m_pageLabel->setStyleSheet("");
}

void ResultWindow::setConfig(const AppConfig &config)
{
    m_config = config;

    if (m_webView)
    {
        const QString mark = ColorUtils::normalizeCssColor(m_config.highlightMarkColor, "#ffeb3b");
        const QString markDark = ColorUtils::normalizeCssColor(m_config.highlightMarkColorDark, "#d4af37");
        const QString js = QString(
                               "(()=>{try{document.documentElement.style.setProperty('--mark-bg', '%1');"
                               "document.documentElement.style.setProperty('--mark-bg-dark', '%2');}catch(e){}})();")
                               .arg(mark, markDark);
        m_webView->eval(js.toStdString());
    }

    // Apply default lock state from config (used when creating new result windows).
    if (m_lockAction)
    {
        const bool wantLocked = m_config.defaultResultWindowLocked;
        if (m_lockAction->isChecked() != wantLocked)
        {
            m_lockAction->setChecked(wantLocked);
            toggleLock();
        }
        else if (m_isLocked != wantLocked)
        {
            // Keep internal state consistent with the action state.
            toggleLock();
        }
    }
}
