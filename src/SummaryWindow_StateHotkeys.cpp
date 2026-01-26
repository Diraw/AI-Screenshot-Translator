#include "SummaryWindow.h"

#include "EmbedWebView.h"

#include <QDebug>
#include <QKeySequence>
#include <QSettings>
#include <QShortcut>

void SummaryWindow::saveState()
{
    QSettings settings(ConfigManager::settingsIniPath(), QSettings::IniFormat);
    settings.setValue("summaryWindow/geometry", saveGeometry());
    settings.setValue("summaryWindow/zoom", m_currentZoom);
    captureScrollPosition();
    settings.setValue("summaryWindow/scrollY", m_lastScrollY);

    // Save scroll position via JavaScript
    if (m_webView)
    {
        QString jsGetScroll = "window.scrollY;";
        Q_UNUSED(jsGetScroll);
        // Note: eval doesn't return values directly, would need binding for this
        // For now just save geometry
    }
}

void SummaryWindow::restoreState()
{
    QSettings settings(ConfigManager::settingsIniPath(), QSettings::IniFormat);

    // Restore geometry
    QByteArray geom = settings.value("summaryWindow/geometry").toByteArray();
    if (!geom.isEmpty())
    {
        restoreGeometry(geom);
    }

    // Restore zoom
    qreal zoom = settings.value("summaryWindow/zoom", 1.0).toReal();
    setZoomFactor(zoom);
    m_lastScrollY = settings.value("summaryWindow/scrollY", 0.0).toDouble();
}

void SummaryWindow::configureHotkeys(const QString &editKey, const QString &viewKey, const QString &screenshotKey,
                                     const QString &boldKey, const QString &underlineKey, const QString &highlightKey)
{
    auto normalizeHotkey = [](QString key)
    {
        key = key.trimmed().toLower();
        key.replace(" ", ""); // remove inner blanks like "ctrl + e"
        return key;
    };

    m_editKey = normalizeHotkey(editKey);
    m_viewKey = normalizeHotkey(viewKey);
    m_screenshotKey = normalizeHotkey(screenshotKey);
    m_boldKey = normalizeHotkey(boldKey);
    m_underlineKey = normalizeHotkey(underlineKey);
    m_highlightKey = normalizeHotkey(highlightKey);

    qDeleteAll(m_shortcuts);
    m_shortcuts.clear();
    auto addShortcut = [this](const QString &key, const QString &js)
    {
        if (key.isEmpty())
            return;
        QShortcut *sc = new QShortcut(QKeySequence(key), this);
        sc->setContext(Qt::ApplicationShortcut);
        connect(sc, &QShortcut::activated, this, [this, js, key]()
                {
            qDebug() << "[QShortcut activated]" << key;
            if (m_webView) m_webView->eval(js.toStdString()); });
        m_shortcuts.append(sc);
    };
    addShortcut(m_editKey, "if(window.currentEntry){var e=currentEntry(); if(e) toggleEdit(e);}");
    addShortcut(m_viewKey, "if(window.currentEntry){var e=currentEntry(); if(e) toggleView(e.getAttribute('data-id'));}");
    addShortcut(m_screenshotKey, "if(window.currentEntry && window.cmd_restore){var e=currentEntry(); window.cmd_restore(e.getAttribute('data-id'));}");

    qDebug() << "[Hotkeys]"
             << "edit=" << m_editKey
             << "view=" << m_viewKey
             << "shot=" << m_screenshotKey
             << "bold=" << m_boldKey
             << "underline=" << m_underlineKey
             << "highlight=" << m_highlightKey;
    refreshHtml();
}
