#include "ConfigDialog_NetworkTestUtils.h"

#include <QObject>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

namespace ConfigDialogNetworkTestUtils
{
bool tryBuildProxyFromUrl(const QString &proxyUrl, QNetworkProxy &outProxy, QString &outErr)
{
    const QUrl url = QUrl::fromUserInput(proxyUrl);
    if (!url.isValid() || url.host().isEmpty())
    {
        outErr = QObject::tr("Invalid proxy URL.");
        return false;
    }

    QNetworkProxy proxy;
    const QString scheme = url.scheme().toLower();
    if (scheme == "socks5" || scheme == "socks")
        proxy.setType(QNetworkProxy::Socks5Proxy);
    else
        proxy.setType(QNetworkProxy::HttpProxy);

    proxy.setHostName(url.host());
    proxy.setPort(url.port(8080));
    if (!url.userName().isEmpty())
        proxy.setUser(url.userName());
    if (!url.password().isEmpty())
        proxy.setPassword(url.password());

    outProxy = proxy;
    return true;
}

QString buildDetailedNetworkError(QNetworkReply::NetworkError error, const QString &errorString, int httpStatus, const QUrl &url)
{
    QString diagnosis;

    switch (error)
    {
    case QNetworkReply::ConnectionRefusedError:
        diagnosis = QStringLiteral("âŒ è¿žæŽ¥è¢«æ‹’ç»\n\n"
                                   "å¯èƒ½åŽŸå› ï¼š\n"
                                   "â€¢ æœåŠ¡å™¨æœªå¯åŠ¨æˆ–æœªç›‘å¬è¯¥ç«¯å£\n"
                                   "â€¢ é˜²ç«å¢™é˜»æ­¢äº†è¿žæŽ¥\n"
                                   "â€¢ IP åœ°å€æˆ–ç«¯å£å·é”™è¯¯\n\n"
                                   "å»ºè®®æ£€æŸ¥ï¼šæœåŠ¡å™¨æ˜¯å¦è¿è¡Œï¼Œåœ°å€å’Œç«¯å£æ˜¯å¦æ­£ç¡®");
        break;

    case QNetworkReply::RemoteHostClosedError:
        diagnosis = QStringLiteral("âŒ æœåŠ¡å™¨ä¸»åŠ¨æ–­å¼€è¿žæŽ¥\n\n"
                                   "å¯èƒ½åŽŸå› ï¼š\n"
                                   "â€¢ æœåŠ¡å™¨æ‹’ç»äº†è¯·æ±‚ï¼ˆå¯èƒ½æ˜¯ TLS/SSL ç‰ˆæœ¬ä¸å…¼å®¹ï¼‰\n"
                                   "â€¢ è¯·æ±‚è¢«é˜²ç«å¢™æˆ–å®‰å…¨è½¯ä»¶æ‹¦æˆª\n"
                                   "â€¢ ä»£ç†æœåŠ¡å™¨é…ç½®é”™è¯¯\n\n"
                                   "å»ºè®®æ£€æŸ¥ï¼šå¦‚æžœä½¿ç”¨ä»£ç†ï¼Œè¯·æ£€æŸ¥ä»£ç†è®¾ç½®ï¼›å°è¯•å…³é—­é˜²ç«å¢™æµ‹è¯•");
        break;

    case QNetworkReply::HostNotFoundError:
        diagnosis = QStringLiteral("âŒ æ— æ³•è§£æžæœåŠ¡å™¨åœ°å€\n\n"
                                   "å¯èƒ½åŽŸå› ï¼š\n"
                                   "â€¢ DNS è§£æžå¤±è´¥\n"
                                   "â€¢ åŸŸåæ‹¼å†™é”™è¯¯\n"
                                   "â€¢ ç½‘ç»œè¿žæŽ¥æ–­å¼€\n\n"
                                   "å»ºè®®æ£€æŸ¥ï¼š\n"
                                   "â€¢ æ£€æŸ¥åŸŸåæ‹¼å†™æ˜¯å¦æ­£ç¡®\n"
                                   "â€¢ å°è¯•åœ¨æµè§ˆå™¨ä¸­è®¿é—®è¯¥åœ°å€\n"
                                   "â€¢ æ£€æŸ¥ç½‘ç»œè¿žæŽ¥");
        break;

    case QNetworkReply::TimeoutError:
        diagnosis = QStringLiteral("âŒ è¿žæŽ¥è¶…æ—¶ï¼ˆè¶…è¿‡ 8 ç§’ï¼‰\n\n"
                                   "å¯èƒ½åŽŸå› ï¼š\n"
                                   "â€¢ ç½‘ç»œå»¶è¿Ÿè¿‡é«˜\n"
                                   "â€¢ æœåŠ¡å™¨å“åº”ç¼“æ…¢\n"
                                   "â€¢ é˜²ç«å¢™æˆ–ä»£ç†å¯¼è‡´è¿žæŽ¥é˜»å¡ž\n\n"
                                   "å»ºè®®æ£€æŸ¥ï¼š\n"
                                   "â€¢ ç½‘ç»œè¿žæŽ¥æ˜¯å¦æ­£å¸¸\n"
                                   "â€¢ å¦‚æžœä½¿ç”¨ä»£ç†ï¼Œæ£€æŸ¥ä»£ç†æ˜¯å¦å¯ç”¨\n"
                                   "â€¢ å°è¯•åœ¨æµè§ˆå™¨ä¸­è®¿é—®è¯¥åœ°å€ç¡®è®¤å“åº”é€Ÿåº¦");
        break;

    case QNetworkReply::OperationCanceledError:
        if (httpStatus == 0)
        {
            diagnosis = QStringLiteral("âŒ è¯·æ±‚è¢«ä¸­æ–­ï¼ˆHTTP 0ï¼‰\n\n"
                                       "å¯èƒ½åŽŸå› ï¼š\n"
                                       "â€¢ è¯·æ±‚è¶…æ—¶ï¼ˆè¶…è¿‡ 8 ç§’æ— å“åº”ï¼‰\n"
                                       "â€¢ ç½‘ç»œè¿žæŽ¥ä¸­æ–­\n"
                                       "â€¢ è¯·æ±‚è¢«é˜²ç«å¢™/æ€æ¯’è½¯ä»¶æ‹¦æˆª\n"
                                       "â€¢ æœåŠ¡å™¨æ— å“åº”\n\n"
                                       "å»ºè®®æ£€æŸ¥ï¼š\n"
                                       "â€¢ æ£€æŸ¥ç½‘ç»œè¿žæŽ¥\n"
                                       "â€¢ å¦‚æžœä½¿ç”¨ä»£ç†ï¼Œç¡®è®¤ä»£ç†å¯ç”¨\n"
                                       "â€¢ æš‚æ—¶å…³é—­é˜²ç«å¢™/æ€æ¯’è½¯ä»¶æµ‹è¯•\n"
                                       "â€¢ åœ¨æµè§ˆå™¨ä¸­è®¿é—®ç›¸åŒåœ°å€æµ‹è¯•");
        }
        else
        {
            diagnosis = QStringLiteral("âŒ è¯·æ±‚è¢«å–æ¶ˆ");
        }
        break;

    case QNetworkReply::SslHandshakeFailedError:
        diagnosis = QStringLiteral("âŒ SSL/TLS æ¡æ‰‹å¤±è´¥\n\n"
                                   "å¯èƒ½åŽŸå› ï¼š\n"
                                   "â€¢ æœåŠ¡å™¨è¯ä¹¦è¿‡æœŸæˆ–ä¸å—ä¿¡ä»»\n"
                                   "â€¢ ç³»ç»Ÿæ—¶é—´ä¸æ­£ç¡®å¯¼è‡´è¯ä¹¦éªŒè¯å¤±è´¥\n"
                                   "â€¢ TLS ç‰ˆæœ¬ä¸å…¼å®¹\n\n"
                                   "å»ºè®®æ£€æŸ¥ï¼š\n"
                                   "â€¢ æ£€æŸ¥ç³»ç»Ÿæ—¶é—´æ˜¯å¦æ­£ç¡®\n"
                                   "â€¢ å°è¯•åœ¨æµè§ˆå™¨ä¸­è®¿é—®ï¼ŒæŸ¥çœ‹è¯ä¹¦è­¦å‘Š\n"
                                   "â€¢ å¦‚æžœæ˜¯è‡ªç­¾åè¯ä¹¦ï¼Œéœ€è¦æ·»åŠ åˆ°ä¿¡ä»»åˆ—è¡¨");
        break;

    case QNetworkReply::TemporaryNetworkFailureError:
    case QNetworkReply::NetworkSessionFailedError:
        diagnosis = QStringLiteral("âŒ ç½‘ç»œä¼šè¯å¤±è´¥\n\n"
                                   "å¯èƒ½åŽŸå› ï¼š\n"
                                   "â€¢ ç½‘ç»œè¿žæŽ¥å·²æ–­å¼€\n"
                                   "â€¢ WiFi è¿žæŽ¥ä¸ç¨³å®š\n\n"
                                   "å»ºè®®æ£€æŸ¥ï¼šç½‘ç»œè¿žæŽ¥çŠ¶æ€");
        break;

    case QNetworkReply::ProxyConnectionRefusedError:
        diagnosis = QStringLiteral("âŒ ä»£ç†æœåŠ¡å™¨æ‹’ç»è¿žæŽ¥\n\n"
                                   "å¯èƒ½åŽŸå› ï¼š\n"
                                   "â€¢ ä»£ç†æœåŠ¡å™¨æœªè¿è¡Œ\n"
                                   "â€¢ ä»£ç†åœ°å€æˆ–ç«¯å£é”™è¯¯\n\n"
                                   "å»ºè®®æ£€æŸ¥ï¼šä»£ç†è®¾ç½®æ˜¯å¦æ­£ç¡®");
        break;

    case QNetworkReply::ProxyNotFoundError:
        diagnosis = QStringLiteral("âŒ æ— æ³•è¿žæŽ¥åˆ°ä»£ç†æœåŠ¡å™¨\n\n"
                                   "å¯èƒ½åŽŸå› ï¼š\n"
                                   "â€¢ ä»£ç†æœåŠ¡å™¨åœ°å€é”™è¯¯\n"
                                   "â€¢ ä»£ç†æœåŠ¡å™¨ä¸å¯è¾¾\n\n"
                                   "å»ºè®®æ£€æŸ¥ï¼šä»£ç†åœ°å€å’Œç«¯å£é…ç½®");
        break;

    case QNetworkReply::ProxyTimeoutError:
        diagnosis = QStringLiteral("âŒ ä»£ç†è¿žæŽ¥è¶…æ—¶\n\n"
                                   "å¯èƒ½åŽŸå› ï¼š\n"
                                   "â€¢ ä»£ç†æœåŠ¡å™¨å“åº”ç¼“æ…¢\n"
                                   "â€¢ ä»£ç†æœåŠ¡å™¨ä¸å¯è¾¾\n\n"
                                   "å»ºè®®æ£€æŸ¥ï¼š\n"
                                   "â€¢ ç¡®è®¤ä»£ç†æœåŠ¡å™¨å¯ç”¨\n"
                                   "â€¢ å°è¯•ä¸ä½¿ç”¨ä»£ç†ç›´æŽ¥è¿žæŽ¥");
        break;

    case QNetworkReply::ProxyAuthenticationRequiredError:
        diagnosis = QStringLiteral("âŒ ä»£ç†éœ€è¦èº«ä»½éªŒè¯\n\n"
                                   "å»ºè®®æ£€æŸ¥ï¼šä»£ç†ç”¨æˆ·åå’Œå¯†ç æ˜¯å¦æ­£ç¡®");
        break;

    case QNetworkReply::ContentAccessDenied:
    case QNetworkReply::AuthenticationRequiredError:
        diagnosis = QStringLiteral("âŒ è®¿é—®è¢«æ‹’ç»ï¼ˆHTTP 401/403ï¼‰\n\n"
                                   "å¯èƒ½åŽŸå› ï¼šAPI Key æ— æ•ˆæˆ–è¿‡æœŸ\n\n"
                                   "å»ºè®®æ£€æŸ¥ï¼šAPI Key æ˜¯å¦æ­£ç¡®");
        break;

    case QNetworkReply::ContentNotFoundError:
        diagnosis = QStringLiteral("âŒ æŽ¥å£è·¯å¾„ä¸å­˜åœ¨ï¼ˆHTTP 404ï¼‰\n\n"
                                   "å»ºè®®æ£€æŸ¥ï¼šç«¯ç‚¹è·¯å¾„ï¼ˆEndpointï¼‰æ˜¯å¦æ­£ç¡®");
        break;

    case QNetworkReply::ProtocolInvalidOperationError:
        diagnosis = QStringLiteral("âŒ è¯·æ±‚æ–¹æ³•ä¸å…è®¸ï¼ˆHTTP 405ï¼‰\n\n"
                                   "å¯èƒ½åŽŸå› ï¼šAPI ä¸æ”¯æŒè¯¥è¯·æ±‚æ–¹æ³•\n\n"
                                   "å»ºè®®æ£€æŸ¥ï¼šAPI æ ¼å¼è®¾ç½®æ˜¯å¦æ­£ç¡®ï¼ˆOpenAI/Ollamaï¼‰");
        break;

    case QNetworkReply::ContentReSendError:
        diagnosis = QStringLiteral("âŒ é‡å®šå‘é”™è¯¯\n\n"
                                   "å¯èƒ½åŽŸå› ï¼šæœåŠ¡å™¨è¿”å›žäº†é‡å®šå‘ï¼Œä½†è¢«æ‹¦æˆª\n\n"
                                   "å»ºè®®æ£€æŸ¥ï¼šä½¿ç”¨æµè§ˆå™¨è®¿é—®çœ‹æ˜¯å¦è‡ªåŠ¨è·³è½¬");
        break;

    case QNetworkReply::ServiceUnavailableError:
    case QNetworkReply::InternalServerError:
        diagnosis = QStringLiteral("âŒ æœåŠ¡å™¨å†…éƒ¨é”™è¯¯ï¼ˆHTTP 5xxï¼‰\n\n"
                                   "å¯èƒ½åŽŸå› ï¼šæœåŠ¡å™¨æš‚æ—¶ä¸å¯ç”¨\n\n"
                                   "å»ºè®®ï¼šç¨åŽå†è¯•");
        break;

    case QNetworkReply::UnknownNetworkError:
        if (httpStatus == 0)
        {
            diagnosis = QStringLiteral("âŒ ç½‘ç»œé”™è¯¯ï¼ˆHTTP 0ï¼‰\n\n"
                                       "å¯èƒ½åŽŸå› ï¼š\n"
                                       "â€¢ è¯·æ±‚è¶…æ—¶ï¼ˆè¶…è¿‡ 8 ç§’ï¼‰\n"
                                       "â€¢ ç½‘ç»œè¿žæŽ¥ä¸­æ–­\n"
                                       "â€¢ è¯·æ±‚è¢«é˜²ç«å¢™/æ€æ¯’è½¯ä»¶æ‹¦æˆª\n"
                                       "â€¢ æœåŠ¡å™¨æ— å“åº”\n\n"
                                       "å»ºè®®æ£€æŸ¥ï¼š\n"
                                       "1. æ£€æŸ¥ Base URL æ ¼å¼æ˜¯å¦æ­£ç¡®ï¼ˆéœ€åŒ…å« https://ï¼‰\n"
                                       "2. åœ¨æµè§ˆå™¨ä¸­è®¿é—®ç›¸åŒåœ°å€æµ‹è¯•\n"
                                       "3. æ£€æŸ¥ç½‘ç»œè¿žæŽ¥å’Œä»£ç†è®¾ç½®\n"
                                       "4. æš‚æ—¶å…³é—­é˜²ç«å¢™/æ€æ¯’è½¯ä»¶æµ‹è¯•");
        }
        else
        {
            diagnosis = QStringLiteral("âŒ æœªçŸ¥ç½‘ç»œé”™è¯¯");
        }
        break;

    default:
        diagnosis = QStringLiteral("ç½‘ç»œé”™è¯¯ (%1)").arg(error);
        break;
    }

    QString result = QStringLiteral("æµ‹è¯•å¤±è´¥ï¼ˆHTTP %1ï¼‰\n\n").arg(httpStatus);
    result += diagnosis;
    result += QStringLiteral("\n\næŠ€æœ¯ä¿¡æ¯ï¼š\nâ€¢ é”™è¯¯ç±»åž‹: %1\nâ€¢ åŽŸå§‹é”™è¯¯: %2\nâ€¢ è¯·æ±‚åœ°å€: %3")
                  .arg(error)
                  .arg(errorString)
                  .arg(url.toString());

    return result;
}

QUrl joinBaseAndEndpointUi(const QString &baseUrl, const QString &endpoint)
{
    QUrl base = QUrl::fromUserInput(baseUrl.trimmed());
    QString s = base.toString();
    if (!s.endsWith('/'))
        s += '/';
    base = QUrl(s);

    QString ep = endpoint.trimmed();
    if (ep.isEmpty())
        return base;
    while (ep.startsWith('/'))
        ep.remove(0, 1);
    return base.resolved(QUrl(ep));
}

QJsonValue substituteTemplateTokens(const QJsonValue &value, const QHash<QString, QString> &tokens)
{
    if (value.isString())
    {
        const QString raw = value.toString();
        const QString trimmed = raw.trimmed();

        if (trimmed == "{{temperature}}" || trimmed == "{{top_p}}")
            return QJsonValue(tokens.value(trimmed.mid(2, trimmed.length() - 4)).toDouble());
        if (trimmed == "{{max_tokens}}")
            return QJsonValue(tokens.value("max_tokens").toInt());
        if (trimmed.startsWith("{{") && trimmed.endsWith("}}"))
        {
            const QString tokenName = trimmed.mid(2, trimmed.length() - 4);
            const QString tokenValue = tokens.value(tokenName);
            if (!tokenValue.isEmpty() && (tokenValue.trimmed().startsWith('[') || tokenValue.trimmed().startsWith('{')))
            {
                QJsonParseError err;
                const QJsonDocument doc = QJsonDocument::fromJson(tokenValue.toUtf8(), &err);
                if (err.error == QJsonParseError::NoError)
                {
                    if (doc.isArray())
                        return doc.array();
                    if (doc.isObject())
                        return doc.object();
                }
            }
        }

        QString out = raw;
        for (auto it = tokens.constBegin(); it != tokens.constEnd(); ++it)
            out.replace(QString("{{%1}}").arg(it.key()), it.value());
        return out;
    }

    if (value.isArray())
    {
        QJsonArray replaced;
        const QJsonArray src = value.toArray();
        for (const QJsonValue &v : src)
            replaced.append(substituteTemplateTokens(v, tokens));
        return replaced;
    }

    if (value.isObject())
    {
        QJsonObject replaced;
        const QJsonObject src = value.toObject();
        for (auto it = src.constBegin(); it != src.constEnd(); ++it)
            replaced.insert(it.key(), substituteTemplateTokens(it.value(), tokens));
        return replaced;
    }

    return value;
}

QString loadAdvancedApiTestImageBase64()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString candidatePaths[] = {
        QDir::cleanPath(appDir + "/assets/test.png"),
        QDir::cleanPath(appDir + "/../assets/test.png"),
        QDir::cleanPath(appDir + "/../../assets/test.png"),
        QDir::cleanPath(QDir::currentPath() + "/assets/test.png"),
    };

    for (const QString &path : candidatePaths)
    {
        QFile f(path);
        if (!f.exists() || !f.open(QIODevice::ReadOnly))
            continue;

        const QByteArray bytes = f.readAll();
        if (!bytes.isEmpty())
            return QString::fromLatin1(bytes.toBase64());
    }

    return QStringLiteral("iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAIAAACQkWg2AAAAMElEQVR4nGK68+H//w//iScZ/3/4f5eBQZmBgUiSiSTVygwMozaM2jBgNgACAAD//8tKvDmEFTFvAAAAAElFTkSuQmCC");
}
} // namespace ConfigDialogNetworkTestUtils

