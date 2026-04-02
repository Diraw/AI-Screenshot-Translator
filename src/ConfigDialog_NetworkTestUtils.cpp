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
        diagnosis = QStringLiteral(
            "连接被拒绝\n\n"
            "可能原因：\n"
            "- 服务未启动，或没有监听该端口\n"
            "- 防火墙拦截了连接\n"
            "- IP 地址或端口填写错误\n\n"
            "建议检查：\n"
            "- 确认目标服务正在运行\n"
            "- 确认 Base URL 和端口正确");
        break;

    case QNetworkReply::RemoteHostClosedError:
        diagnosis = QStringLiteral(
            "远端主机主动断开连接\n\n"
            "可能原因：\n"
            "- 服务端拒绝了当前请求\n"
            "- TLS/SSL 配置不兼容\n"
            "- 代理或安全软件中断了连接\n\n"
            "建议检查：\n"
            "- 确认请求地址和协议正确\n"
            "- 如使用代理，确认代理配置可用");
        break;

    case QNetworkReply::HostNotFoundError:
        diagnosis = QStringLiteral(
            "无法解析主机地址\n\n"
            "可能原因：\n"
            "- 域名填写错误\n"
            "- DNS 解析失败\n"
            "- 网络连接异常\n\n"
            "建议检查：\n"
            "- 确认 Base URL 中的主机名正确\n"
            "- 尝试在浏览器中访问同一地址");
        break;

    case QNetworkReply::TimeoutError:
        diagnosis = QStringLiteral(
            "请求超时\n\n"
            "可能原因：\n"
            "- 网络延迟过高\n"
            "- 服务端响应较慢\n"
            "- 代理或安全软件阻塞了连接\n\n"
            "建议检查：\n"
            "- 确认网络连接正常\n"
            "- 如使用代理，确认代理可用\n"
            "- 尝试适当增大超时时间");
        break;

    case QNetworkReply::OperationCanceledError:
        if (httpStatus == 0)
        {
            diagnosis = QStringLiteral(
                "请求被中断（HTTP 0）\n\n"
                "可能原因：\n"
                "- 请求超时\n"
                "- 网络连接中断\n"
                "- 请求被防火墙或安全软件拦截\n"
                "- 服务端没有响应\n\n"
                "建议检查：\n"
                "- 检查网络连接\n"
                "- 如使用代理，确认代理可用\n"
                "- 暂时关闭防火墙或安全软件后重试\n"
                "- 在浏览器中访问相同地址进行测试");
        }
        else
        {
            diagnosis = QStringLiteral("请求被取消");
        }
        break;

    case QNetworkReply::SslHandshakeFailedError:
        diagnosis = QStringLiteral(
            "SSL/TLS 握手失败\n\n"
            "可能原因：\n"
            "- 证书无效或不受信任\n"
            "- 系统时间不正确\n"
            "- TLS 版本不兼容\n\n"
            "建议检查：\n"
            "- 确认系统时间正确\n"
            "- 在浏览器中访问同一地址查看证书情况");
        break;

    case QNetworkReply::TemporaryNetworkFailureError:
    case QNetworkReply::NetworkSessionFailedError:
        diagnosis = QStringLiteral(
            "网络会话失败\n\n"
            "可能原因：\n"
            "- 当前网络连接已断开\n"
            "- Wi-Fi 或代理连接不稳定\n\n"
            "建议检查：\n"
            "- 确认网络状态正常后重试");
        break;

    case QNetworkReply::ProxyConnectionRefusedError:
        diagnosis = QStringLiteral(
            "代理连接被拒绝\n\n"
            "可能原因：\n"
            "- 代理服务未启动\n"
            "- 代理地址或端口错误\n\n"
            "建议检查：\n"
            "- 确认代理配置正确");
        break;

    case QNetworkReply::ProxyNotFoundError:
        diagnosis = QStringLiteral(
            "找不到代理服务器\n\n"
            "可能原因：\n"
            "- 代理地址填写错误\n"
            "- 代理服务不可达\n\n"
            "建议检查：\n"
            "- 确认代理地址和端口配置");
        break;

    case QNetworkReply::ProxyTimeoutError:
        diagnosis = QStringLiteral(
            "代理连接超时\n\n"
            "可能原因：\n"
            "- 代理响应较慢\n"
            "- 代理不可达\n\n"
            "建议检查：\n"
            "- 确认代理服务可用\n"
            "- 尝试直连测试");
        break;

    case QNetworkReply::ProxyAuthenticationRequiredError:
        diagnosis = QStringLiteral(
            "代理需要身份验证\n\n"
            "建议检查：\n"
            "- 确认代理用户名和密码正确");
        break;

    case QNetworkReply::ContentAccessDenied:
    case QNetworkReply::AuthenticationRequiredError:
        diagnosis = QStringLiteral(
            "访问被拒绝（HTTP 401/403）\n\n"
            "可能原因：\n"
            "- API Key 无效、缺失或已过期\n\n"
            "建议检查：\n"
            "- 确认 API Key 正确");
        break;

    case QNetworkReply::ContentNotFoundError:
        diagnosis = QStringLiteral(
            "接口路径不存在（HTTP 404）\n\n"
            "建议检查：\n"
            "- 确认 Endpoint 配置正确");
        break;

    case QNetworkReply::ProtocolInvalidOperationError:
        diagnosis = QStringLiteral(
            "请求方法不被允许（HTTP 405）\n\n"
            "可能原因：\n"
            "- 当前 API 不支持该请求格式\n\n"
            "建议检查：\n"
            "- 确认接口格式与服务端兼容");
        break;

    case QNetworkReply::ContentReSendError:
        diagnosis = QStringLiteral(
            "重定向错误\n\n"
            "可能原因：\n"
            "- 服务端返回了重定向，但请求被拦截\n\n"
            "建议检查：\n"
            "- 在浏览器中访问同一地址确认是否发生跳转");
        break;

    case QNetworkReply::ServiceUnavailableError:
    case QNetworkReply::InternalServerError:
        diagnosis = QStringLiteral(
            "服务端错误（HTTP 5xx）\n\n"
            "可能原因：\n"
            "- 服务端暂时不可用\n\n"
            "建议检查：\n"
            "- 稍后重试");
        break;

    case QNetworkReply::UnknownNetworkError:
        if (httpStatus == 0)
        {
            diagnosis = QStringLiteral(
                "网络错误（HTTP 0）\n\n"
                "可能原因：\n"
                "- 请求超时\n"
                "- 网络连接中断\n"
                "- 请求被防火墙或安全软件拦截\n"
                "- 服务端无响应\n\n"
                "建议检查：\n"
                "1. 确认 Base URL 格式正确\n"
                "2. 在浏览器中访问相同地址测试\n"
                "3. 检查网络连接和代理设置\n"
                "4. 暂时关闭防火墙或安全软件后重试");
        }
        else
        {
            diagnosis = QStringLiteral("未知网络错误");
        }
        break;

    default:
        diagnosis = QStringLiteral("网络错误（%1）").arg(error);
        break;
    }

    QString result = QStringLiteral("测试失败（HTTP %1）\n\n").arg(httpStatus);
    result += diagnosis;
    result += QStringLiteral("\n\n技术信息：\n- 错误类型: %1\n- 原始错误: %2\n- 请求地址: %3")
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
