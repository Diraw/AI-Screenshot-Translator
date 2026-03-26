#pragma once

#include <QHash>
#include <QJsonValue>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QString>
#include <QUrl>

namespace ConfigDialogNetworkTestUtils
{
bool tryBuildProxyFromUrl(const QString &proxyUrl, QNetworkProxy &outProxy, QString &outErr);
QString buildDetailedNetworkError(QNetworkReply::NetworkError error, const QString &errorString, int httpStatus, const QUrl &url);
QUrl joinBaseAndEndpointUi(const QString &baseUrl, const QString &endpoint);
QJsonValue substituteTemplateTokens(const QJsonValue &value, const QHash<QString, QString> &tokens);
QString loadAdvancedApiTestImageBase64();
} // namespace ConfigDialogNetworkTestUtils
