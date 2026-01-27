#ifndef UMAMICLIENT_H
#define UMAMICLIENT_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QJsonObject>
#include <QUrl>

struct UmamiConfig
{
    QUrl serverBaseUrl; // e.g. https://umami.example.com
    QString websiteId;  // UUID
    QString distinctId; // per-user/device id (optional)
    QString userAgent;  // avoid bot check
};

class UmamiClient : public QObject
{
    Q_OBJECT
public:
    explicit UmamiClient(QObject *parent = nullptr);

    void setConfig(const UmamiConfig &cfg);
    bool isEnabled() const;

    void trackPageview(const QString &urlPath, const QString &title);
    void trackEvent(const QString &eventName, const QJsonObject &data, const QString &urlPath, const QString &title);
    void identify(const QJsonObject &data);

private:
    void post(const QJsonObject &body);

    QNetworkAccessManager *m_manager = nullptr;
    UmamiConfig m_cfg;
    QString m_cacheToken;
};

#endif // UMAMICLIENT_H
