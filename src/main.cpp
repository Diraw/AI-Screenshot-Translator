#include "App.h"
#include <QApplication>
#include <QIcon>
#include <QLockFile>
#include <QDir>
#include <QMessageBox>

#include <QFile>
#include <QTextStream>
#include <QStandardPaths>
#include <QDateTime>
#include "ConfigManager.h"

#ifndef APP_NAME
#define APP_NAME "AI Screenshot Translator"
#endif

#ifndef APP_VERSION
#define APP_VERSION "0.0.0"
#endif

#ifdef _WIN32
#include <windows.h>
#endif

bool g_enableLogging = false;

void customMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QString text;
    switch (type)
    {
    case QtDebugMsg:
        text = "DEBUG: " + msg;
        break;
    case QtInfoMsg:
        text = "INFO: " + msg;
        break;
    case QtWarningMsg:
        text = "WARN: " + msg;
        break;
    case QtCriticalMsg:
        text = "CRIT: " + msg;
        break;
    case QtFatalMsg:
        text = "FATAL: " + msg;
        break;
    }

#ifdef _WIN32
    OutputDebugStringW(reinterpret_cast<const wchar_t *>(text.utf16()));
    OutputDebugStringW(L"\n");
#endif

    // Only write debug.log when debug mode enabled (or env override)
    bool allowFile = g_enableLogging || qEnvironmentVariableIsSet("FORCE_DEBUG_LOG");
    if (!allowFile)
        return;
    // Gate DEBUG entries if disabled
    bool shouldLog = (type != QtDebugMsg) || allowFile;
    if (!shouldLog)
        return;

    static bool logCleared = false;
    QFile outFile("debug.log");
    const QIODevice::OpenMode mode = QIODevice::WriteOnly | (logCleared ? QIODevice::Append : QIODevice::Truncate);
    if (outFile.open(mode))
    {
        if (!logCleared)
            logCleared = true;
        QTextStream ts(&outFile);
        ts << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz ") << text << Qt::endl;
    }
}

int main(int argc, char *argv[])
{
    // Determine debug mode from config before initializing logging
    {
        ConfigManager tempConfig;
        g_enableLogging = tempConfig.getConfig().debugMode;
    }
    bool forceDebug = qEnvironmentVariableIsSet("FORCE_DEBUG_LOG");
    bool enableFileLog = g_enableLogging || forceDebug;
    if (enableFileLog)
    {
        // Clear previous logs on startup when file logging is enabled.
        QFile::remove("debug.log");
        QFile::remove("wkf.log");
    }

    qInstallMessageHandler(customMessageHandler);
    qDebug() << "Application starting...";

    QApplication a(argc, argv);

    QCoreApplication::setApplicationName(QString::fromUtf8(APP_NAME));
    QCoreApplication::setApplicationVersion(QString::fromUtf8(APP_VERSION));

    // Ensure we don't quit when the last window closes (because we live in the tray)
    a.setQuitOnLastWindowClosed(false);
    a.setWindowIcon(QIcon(":/assets/icon.ico"));

    // Single Instance Check
    QLockFile lockFile(QDir::temp().filePath("AI-Screenshot-Translator.lock"));
    if (!lockFile.tryLock(100))
    {
        qDebug() << "App already running, exiting.";
        QMessageBox::warning(nullptr, "AI Screenshot Translator",
                             "The application is already running.\nCheck the system tray.");
        return 1;
    }

    try
    {
        qDebug() << "Initializing App...";
        App app;
        qDebug() << "App initialized. Entering exec loop.";
        return a.exec();
    }
    catch (const std::exception &e)
    {
        qCritical() << "Uncaught exception: " << e.what();
        QMessageBox::critical(nullptr, "Crash", QString("Uncaught exception failed: %1").arg(e.what()));
        return -1;
    }
    catch (...)
    {
        qCritical() << "Unknown crash occurred.";
        QMessageBox::critical(nullptr, "Crash", "Unknown error occurred.");
        return -1;
    }
}
