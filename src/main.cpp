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

#ifdef _WIN32
#include <windows.h>
#endif

bool g_enableLogging = false;

void customMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
    QString text;
    switch (type) {
        case QtDebugMsg: text = "DEBUG: " + msg; break;
        case QtInfoMsg: text = "INFO: " + msg; break;
        case QtWarningMsg: text = "WARN: " + msg; break;
        case QtCriticalMsg: text = "CRIT: " + msg; break;
        case QtFatalMsg: text = "FATAL: " + msg; break;
    }

#ifdef _WIN32
    OutputDebugStringW(reinterpret_cast<const wchar_t*>(text.utf16()));
    OutputDebugStringW(L"\n");
#endif

    // Always log INFO/WARN/CRIT; gate DEBUG by config/env to avoid noise
    bool allowDebug = g_enableLogging || qEnvironmentVariableIsSet("FORCE_DEBUG_LOG");
    bool shouldLog = (type != QtDebugMsg) || allowDebug;
    if (!shouldLog) return;
    
    QFile outFile("debug.log");
    if (outFile.open(QIODevice::WriteOnly | QIODevice::Append)) {
        QTextStream ts(&outFile);
        ts << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz ") << text << Qt::endl;
    }
}

int main(int argc, char *argv[]) {
    // Determine debug mode from config before initializing logging
    {
        ConfigManager tempConfig;
        g_enableLogging = tempConfig.getConfig().debugMode;
    }

    qInstallMessageHandler(customMessageHandler);
    qDebug() << "Application starting...";

    QApplication a(argc, argv);
    
    // Ensure we don't quit when the last window closes (because we live in the tray)
    a.setQuitOnLastWindowClosed(false);
    a.setWindowIcon(QIcon(":/assets/icon.ico"));
    
    // Single Instance Check
    QLockFile lockFile(QDir::temp().filePath("AI-Screenshot-Translator.lock"));
    if (!lockFile.tryLock(100)) {
        qDebug() << "App already running, exiting.";
        QMessageBox::warning(nullptr, "AI Screenshot Translator", 
                             "The application is already running.\nCheck the system tray.");
        return 1;
    }

    try {
        qDebug() << "Initializing App...";
        App app;
        qDebug() << "App initialized. Entering exec loop.";
        return a.exec();
    } catch (const std::exception& e) {
        qCritical() << "Uncaught exception: " << e.what();
        QMessageBox::critical(nullptr, "Crash", QString("Uncaught exception failed: %1").arg(e.what()));
        return -1;
    } catch (...) {
        qCritical() << "Unknown crash occurred.";
        QMessageBox::critical(nullptr, "Crash", "Unknown error occurred.");
        return -1;
    }
}
