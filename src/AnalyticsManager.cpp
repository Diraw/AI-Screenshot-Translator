#include "AnalyticsManager.h"

#include "ConfigManager.h"
#include "DotEnv.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonObject>
#include <QStandardPaths>
#include <QUuid>
#include <QDebug>

static QString appTitle()
{
#ifdef APP_NAME
    return QString::fromUtf8(APP_NAME);
#else
    return QStringLiteral("AI Screenshot Translator");
#endif
}

static QString appVersion()
{
#ifdef APP_VERSION
    return QString::fromUtf8(APP_VERSION);
#else
    return QStringLiteral("0.0.0");
#endif
}

AnalyticsManager::AnalyticsManager(QObject *parent)
    : QObject(parent)
{
    m_client = new UmamiClient(this);

    m_startDelayTimer.setSingleShot(true);
    connect(&m_startDelayTimer, &QTimer::timeout, this, &AnalyticsManager::startNow);

    connect(&m_heartbeatTimer, &QTimer::timeout, this, &AnalyticsManager::sendHeartbeat);
    m_heartbeatTimer.setInterval(60 * 1000);
}

void AnalyticsManager::startDelayed(int delayMs)
{
    if (!m_enabled)
    {
        qInfo() << "[Analytics] startup skipped: disabled by user.";
        return;
    }
    if (m_started)
    {
        qInfo() << "[Analytics] startup skipped: analytics session already active.";
        return;
    }
    if (m_startDelayTimer.isActive())
    {
        qInfo() << "[Analytics] startup already scheduled; keeping existing timer.";
        return;
    }

    qInfo() << "[Analytics] startup scheduled in" << delayMs << "ms.";
    m_startDelayTimer.start(delayMs);
}

void AnalyticsManager::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;

    m_enabled = enabled;
    if (!m_enabled)
    {
        qInfo() << "[Analytics] disabled by user; stopping timers and aborting pending requests.";
        m_startDelayTimer.stop();
        m_heartbeatTimer.stop();
        if (m_client)
        {
            if (m_started)
            {
                m_client->abortPendingRequests();
                qInfo() << "[Analytics] active session detected during disable; sending session_end.";
                sendEnd();
                m_client->clearConfig();
            }
            else
            {
                m_client->disableAndAbort();
            }
        }
        m_started = false;
    }
    else
    {
        qInfo() << "[Analytics] enabled by user; analytics can start when scheduled.";
    }
}

void AnalyticsManager::startNow()
{
    if (!m_enabled || m_started)
        return;

    const UmamiConfig cfg = loadConfig();
    m_client->setConfig(cfg);

    if (!m_client->isEnabled())
    {
        qInfo() << "[Analytics] startup skipped: Umami config unavailable (.env missing or website_uuid not set).";
        return;
    }

    m_started = true;
    m_uptime.start();
    qInfo() << "[Analytics] session started; heartbeat interval = 60s.";

    // Optional identify: helps attach stable user id
    QJsonObject idData;
    idData["app"] = appTitle();
    idData["version"] = appVersion();
    idData["platform"] = "windows";
    m_client->identify(idData);

    sendStart();

    m_heartbeatTimer.start();
}

void AnalyticsManager::stop()
{
    m_startDelayTimer.stop();
    if (!m_started)
    {
        qInfo() << "[Analytics] stop skipped: analytics session not active.";
        return;
    }

    m_heartbeatTimer.stop();
    qInfo() << "[Analytics] stopping active session; sending session_end.";
    sendEnd();
    m_started = false;
}

void AnalyticsManager::sendStart()
{
    // Send a pageview-like event to get sessions/DAU/MAU dashboards.
    m_client->trackPageview("/ai-screenshot-translator/app", appTitle());

    QJsonObject data;
    data["ts"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    data["version"] = appVersion();

    m_client->trackEvent("session_start", data, "/ai-screenshot-translator/app", appTitle());
}

void AnalyticsManager::sendHeartbeat()
{
    if (!m_enabled || !m_started)
        return;

    const qint64 ms = m_uptime.isValid() ? m_uptime.elapsed() : 0;
    qInfo() << "[Analytics] sending heartbeat; uptime_ms =" << ms << "interval_s = 60.";

    QJsonObject data;
    data["uptime_ms"] = static_cast<double>(ms);
    data["interval_s"] = 60;

    m_client->trackEvent("heartbeat", data, "/ai-screenshot-translator/app", appTitle());
}

void AnalyticsManager::sendEnd()
{
    const qint64 ms = m_uptime.isValid() ? m_uptime.elapsed() : 0;
    qInfo() << "[Analytics] sending session_end; uptime_ms =" << ms;

    QJsonObject data;
    data["uptime_ms"] = static_cast<double>(ms);

    m_client->trackEvent("session_end", data, "/ai-screenshot-translator/app", appTitle());
}

QString AnalyticsManager::loadOrCreateDistinctId()
{
    // Prefer explicit client_uuid in .env; otherwise create a persistent UUID under AppData.
    const QString baseDir = ConfigManager::appDataDirPath();
    const QString path = QDir::cleanPath(baseDir + "/analytics_client_id.txt");

    QFile f(path);
    if (f.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        const QString id = QString::fromUtf8(f.readAll()).trimmed();
        if (!id.isEmpty())
            return id;
    }

    const QString newId = QUuid::createUuid().toString(QUuid::WithoutBraces);

    QDir dir(baseDir);
    if (!dir.exists())
        dir.mkpath(".");

    if (f.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        f.write(newId.toUtf8());
        f.write("\n");
    }

    return newId;
}

UmamiConfig AnalyticsManager::loadConfig()
{
    UmamiConfig cfg;

    // Find .env from executable dir upwards, and also from current working directory.
    const QString exeDir = QCoreApplication::applicationDirPath();
    QString envPath = DotEnv::findEnvFileUpwards(exeDir, 8);
    if (envPath.isEmpty())
        envPath = DotEnv::findEnvFileUpwards(QDir::currentPath(), 8);

    // Hard requirement: no .env => analytics disabled.
    if (envPath.isEmpty())
        return cfg;

    const QMap<QString, QString> env = DotEnv::loadFile(envPath);

    // Support several key names to reduce friction.
    const QString websiteId = DotEnv::getValue(env, "website_uuid");
    const QString host = DotEnv::getValue(env, "umami_host", DotEnv::getValue(env, "UMAMI_HOST"));
    const QString clientUuid = DotEnv::getValue(env, "client_uuid", DotEnv::getValue(env, "CLIENT_UUID"));

    // Hard requirement: no website_uuid => analytics disabled.
    if (websiteId.trimmed().isEmpty())
        return cfg;

    if (!host.isEmpty())
        cfg.serverBaseUrl = QUrl(host);
    else
        cfg.serverBaseUrl = QUrl(QStringLiteral("https://umami.diraw.top"));

    cfg.websiteId = websiteId;

    if (!clientUuid.isEmpty())
        cfg.distinctId = clientUuid;
    else
        cfg.distinctId = loadOrCreateDistinctId();

    cfg.userAgent = QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");

    qInfo() << "[Analytics] loaded .env from" << envPath;

    return cfg;
}
