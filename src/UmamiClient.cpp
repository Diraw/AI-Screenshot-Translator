#include "UmamiClient.h"

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QDebug>

static QString cleanPath(const QString &path)
{
    if (path.isEmpty())
        return "/";
    if (path.startsWith('/'))
        return path;
    return "/" + path;
}

UmamiClient::UmamiClient(QObject *parent)
    : QObject(parent)
{
    m_manager = new QNetworkAccessManager(this);
}

void UmamiClient::setConfig(const UmamiConfig &cfg)
{
    m_cfg = cfg;
}

bool UmamiClient::isEnabled() const
{
    return m_cfg.serverBaseUrl.isValid() && !m_cfg.websiteId.isEmpty();
}

void UmamiClient::trackPageview(const QString &urlPath, const QString &title)
{
    if (!isEnabled())
        return;

    QJsonObject payload;
    payload["website"] = m_cfg.websiteId;
    payload["url"] = cleanPath(urlPath);
    payload["title"] = title;
    payload["hostname"] = "desktop-app";
    payload["language"] = "zh-CN";
    payload["screen"] = "0x0";

    if (!m_cfg.distinctId.isEmpty())
        payload["id"] = m_cfg.distinctId;

    QJsonObject body;
    body["type"] = "event";
    body["payload"] = payload;

    post(body);
}

void UmamiClient::trackEvent(const QString &eventName, const QJsonObject &data, const QString &urlPath, const QString &title)
{
    if (!isEnabled())
        return;

    QJsonObject payload;
    payload["website"] = m_cfg.websiteId;
    payload["url"] = cleanPath(urlPath);
    payload["title"] = title;
    payload["hostname"] = "desktop-app";
    payload["language"] = "zh-CN";
    payload["screen"] = "0x0";
    payload["name"] = eventName;

    if (!data.isEmpty())
        payload["data"] = data;

    if (!m_cfg.distinctId.isEmpty())
        payload["id"] = m_cfg.distinctId;

    QJsonObject body;
    body["type"] = "event";
    body["payload"] = payload;

    post(body);
}

void UmamiClient::identify(const QJsonObject &data)
{
    if (!isEnabled())
        return;

    QJsonObject payload;
    payload["website"] = m_cfg.websiteId;

    if (!m_cfg.distinctId.isEmpty())
        payload["id"] = m_cfg.distinctId;

    if (!data.isEmpty())
        payload["data"] = data;

    QJsonObject body;
    body["type"] = "identify";
    body["payload"] = payload;

    post(body);
}

void UmamiClient::post(const QJsonObject &body)
{
    if (!isEnabled())
        return;

    QUrl url = m_cfg.serverBaseUrl;
    url.setPath("/api/send");

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // Avoid Umami bot check for curl-like UAs
    const QString ua = m_cfg.userAgent.isEmpty()
                           ? QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36")
                           : m_cfg.userAgent;
    req.setRawHeader("User-Agent", ua.toUtf8());

    if (!m_cacheToken.isEmpty())
        req.setRawHeader("x-umami-cache", m_cacheToken.toUtf8());

    const QByteArray json = QJsonDocument(body).toJson(QJsonDocument::Compact);

    QNetworkReply *reply = m_manager->post(req, json);
    connect(reply, &QNetworkReply::finished, this, [this, reply]()
            {
        const QByteArray resp = reply->readAll();

        if (reply->error() != QNetworkReply::NoError)
        {
            qWarning() << "[Umami] request failed:" << reply->error() << reply->errorString() << "resp=" << resp;
            reply->deleteLater();
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(resp);
        if (doc.isObject())
        {
            const QJsonObject obj = doc.object();
            const QString cache = obj.value("cache").toString();
            if (!cache.isEmpty())
                m_cacheToken = cache;
        }

        reply->deleteLater(); });

    connect(reply, &QNetworkReply::errorOccurred, this, [reply](QNetworkReply::NetworkError code)
            { qWarning() << "[Umami] network error:" << code << reply->errorString() << "url=" << reply->url(); });
}
