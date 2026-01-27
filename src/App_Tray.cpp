#include "App.h"

#include <QApplication>
#include <QCoreApplication>
#include <QIcon>
#include <QTimer>

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

    m_trayMenuOpen = false;
    m_pendingTrayScreenshot = false;

    m_trayMenu->addAction(tm.tr("tray_screenshot"), this, &App::onScreenshotRequested);
    m_trayMenu->addAction(tm.tr("tray_summary"), this, &App::showSummary);
    m_trayMenu->addAction(tm.tr("tray_settings"), this, &App::showConfig);
    m_trayMenu->addSeparator();
    m_trayMenu->addAction(tm.tr("tray_quit"), qApp, &QCoreApplication::quit);

    m_trayIcon->setContextMenu(m_trayMenu);
    m_trayIcon->show();

    connect(m_trayMenu, &QMenu::aboutToShow, this, [this]()
            {
                m_trayMenuOpen = true;
                m_pendingTrayScreenshot = false; });
    connect(m_trayMenu, &QMenu::aboutToHide, this, [this]()
            { m_trayMenuOpen = false; });

    connect(m_trayIcon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason)
            {
        if (reason == QSystemTrayIcon::Trigger) {
            // On Windows, right-click/context menu can occasionally emit Trigger.
            // Delay starting screenshot; cancel if the context menu opens.
            m_pendingTrayScreenshot = true;
            QTimer::singleShot(120, this, [this]()
                               {
                                   if (!m_pendingTrayScreenshot)
                                       return;
                                   m_pendingTrayScreenshot = false;
                                   if (m_trayMenuOpen)
                                       return;
                                   onScreenshotRequested();
                               });
        } else if (reason == QSystemTrayIcon::DoubleClick) {
            m_pendingTrayScreenshot = false;
            showSummary();
        } else if (reason == QSystemTrayIcon::Context) {
            m_pendingTrayScreenshot = false;
        } });
}
