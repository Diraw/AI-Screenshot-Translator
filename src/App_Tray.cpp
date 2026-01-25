#include "App.h"

#include <QApplication>
#include <QCoreApplication>
#include <QIcon>

#include "TranslationManager.h"

void App::setupTray()
{
    if (m_trayIcon)
        delete m_trayIcon; // Re-creating or creating first time
    if (m_trayMenu)
        delete m_trayMenu;

    m_trayIcon = new QSystemTrayIcon(this);
    QIcon icon(":/assets/icon.ico");
    if (icon.isNull())
    {
        icon = QIcon("assets/icon.ico");
    }
    m_trayIcon->setIcon(icon);

    m_trayMenu = new QMenu();
    TranslationManager &tm = TranslationManager::instance();

    m_trayMenu->addAction(tm.tr("tray_screenshot"), this, &App::onScreenshotRequested);
    m_trayMenu->addAction(tm.tr("tray_summary"), this, &App::showSummary);
    m_trayMenu->addAction(tm.tr("tray_settings"), this, &App::showConfig);
    m_trayMenu->addSeparator();
    m_trayMenu->addAction(tm.tr("tray_quit"), qApp, &QCoreApplication::quit);

    m_trayIcon->setContextMenu(m_trayMenu);
    m_trayIcon->show();

    connect(m_trayIcon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason)
            {
        if (reason == QSystemTrayIcon::Trigger) {
            onScreenshotRequested();
        } else if (reason == QSystemTrayIcon::DoubleClick) {
            showSummary();
        } });
}
