#include "ApiClient.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkProxy>
#include <QNetworkRequest>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QDateTime>

#include <limits>

struct JsonPathStep
{
    bool isIndex = false;
    QString key;
    int index = -1;
};

static QUrl joinBaseAndEndpoint(const QString &baseUrl, const QString &endpoint)
{
    QUrl base = QUrl::fromUserInput(baseUrl.trimmed());
    QString baseStr = base.toString();
    if (!baseStr.endsWith('/'))
        baseStr += '/';
    base = QUrl(baseStr);

    QString ep = endpoint.trimmed();
    if (ep.isEmpty())
        return base;
    while (ep.startsWith('/'))
        ep.remove(0, 1);

    return base.resolved(QUrl(ep));
}

static QStringList parseAdvancedDebugFields(const QString &advancedTemplate)
{
    QStringList fields;
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(advancedTemplate.toUtf8(), &err);
    if (doc.isNull() || !doc.isObject() || err.error != QJsonParseError::NoError)
        return fields;

    const QJsonArray arr = doc.object().value("debug_fields").toArray();
    for (const QJsonValue &v : arr)
    {
        const QString path = v.toString().trimmed();
        if (!path.isEmpty() &&
            path != QStringLiteral("_meta.total_elapsed_seconds") &&
            path != QStringLiteral("_meta.total_elapsed_text"))
            fields.append(path);
    }
    fields.removeDuplicates();
    return fields;
}

static int parseAdvancedRequestTimeoutMs(const QString &advancedTemplate, int fallbackMs)
{
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(advancedTemplate.toUtf8(), &err);
    if (doc.isNull() || !doc.isObject() || err.error != QJsonParseError::NoError)
        return fallbackMs;

    const QJsonObject root = doc.object();

    bool ok = false;
    qint64 timeoutMs = -1;
    const QJsonValue timeoutMsValue = root.value("timeout_ms");
    if (timeoutMsValue.isDouble())
    {
        timeoutMs = static_cast<qint64>(timeoutMsValue.toDouble());
        ok = true;
    }
    else if (timeoutMsValue.isString())
    {
        timeoutMs = timeoutMsValue.toString().trimmed().toLongLong(&ok);
    }

    if (!ok || timeoutMs <= 0)
    {
        ok = false;
        qint64 timeoutSeconds = -1;
        const QJsonValue timeoutSecondsValue = root.value("timeout_seconds");
        if (timeoutSecondsValue.isDouble())
        {
            timeoutSeconds = static_cast<qint64>(timeoutSecondsValue.toDouble());
            ok = true;
        }
        else if (timeoutSecondsValue.isString())
        {
            timeoutSeconds = timeoutSecondsValue.toString().trimmed().toLongLong(&ok);
        }

        if (ok && timeoutSeconds > 0)
            timeoutMs = timeoutSeconds * 1000;
    }

    if (timeoutMs <= 0)
        return fallbackMs;
    if (timeoutMs > std::numeric_limits<int>::max())
        return std::numeric_limits<int>::max();
    return static_cast<int>(timeoutMs);
}

static bool parseJsonPathSteps(const QString &path, QList<JsonPathStep> &outSteps)
{
    outSteps.clear();
    const QString s = path.trimmed();
    if (s.isEmpty())
        return false;

    int i = 0;
    const int n = s.size();
    while (i < n)
    {
        if (s.at(i) == '.')
        {
            ++i;
            continue;
        }

        if (s.at(i) == '[')
        {
            const int close = s.indexOf(']', i + 1);
            if (close <= i + 1)
                return false;
            bool ok = false;
            const int idx = s.mid(i + 1, close - i - 1).toInt(&ok);
            if (!ok || idx < 0)
                return false;

            JsonPathStep step;
            step.isIndex = true;
            step.index = idx;
            outSteps.append(step);
            i = close + 1;
            continue;
        }

        int j = i;
        while (j < n && s.at(j) != '.' && s.at(j) != '[')
            ++j;
        const QString key = s.mid(i, j - i).trimmed();
        if (key.isEmpty())
            return false;

        JsonPathStep step;
        step.key = key;
        outSteps.append(step);
        i = j;
    }

    return !outSteps.isEmpty();
}

static bool resolveJsonPath(const QJsonValue &rootValue, const QString &path, QJsonValue &outValue)
{
    QList<JsonPathStep> steps;
    if (!parseJsonPathSteps(path, steps))
        return false;

    QJsonValue cur = rootValue;
    for (const JsonPathStep &step : steps)
    {
        if (step.isIndex)
        {
            if (!cur.isArray())
                return false;
            const QJsonArray arr = cur.toArray();
            if (step.index < 0 || step.index >= arr.size())
                return false;
            cur = arr.at(step.index);
        }
        else
        {
            if (!cur.isObject())
                return false;
            const QJsonObject obj = cur.toObject();
            if (!obj.contains(step.key))
                return false;
            cur = obj.value(step.key);
        }
    }

    outValue = cur;
    return true;
}

static QString toDebugValueString(const QJsonValue &value)
{
    if (value.isString())
        return value.toString();
    if (value.isBool())
        return value.toBool() ? "true" : "false";
    if (value.isDouble())
        return QString::number(value.toDouble(), 'g', 15);
    if (value.isNull() || value.isUndefined())
        return "null";
    if (value.isObject())
        return QString::fromUtf8(QJsonDocument(value.toObject()).toJson(QJsonDocument::Compact));
    if (value.isArray())
        return QString::fromUtf8(QJsonDocument(value.toArray()).toJson(QJsonDocument::Compact));
    return {};
}

static QString buildAdvancedDebugHeader(const QJsonObject &responseRoot, const QString &advancedTemplate)
{
    const QStringList fields = parseAdvancedDebugFields(advancedTemplate);
    if (fields.isEmpty())
        return {};

    QStringList lines;
    lines.reserve(fields.size() + 1);
    lines << "[Advanced API Debug]";

    for (const QString &path : fields)
    {
        QJsonValue value;
        QString valueText = resolveJsonPath(responseRoot, path, value) ? toDebugValueString(value) : QString("<missing>");
        valueText.replace('\n', ' ');
        if (valueText.size() > 200)
            valueText = valueText.left(197) + "...";
        lines << QString("%1 = %2").arg(path, valueText);
    }

    return QString("```text\n%1\n```\n\n").arg(lines.join('\n'));
}

static QJsonObject buildAdvancedMetaObject(qint64 elapsedMs)
{
    QJsonObject meta;
    const qint64 normalizedElapsedMs = qMax<qint64>(0, elapsedMs);
    meta["total_elapsed_ms"] = static_cast<double>(normalizedElapsedMs);
    return meta;
}

static QString timeoutSecondsText(int timeoutMs)
{
    return QString::number(timeoutMs / 1000.0, 'g', 6);
}

static QStringList toStringList(const QList<QByteArray> &images)
{
    QStringList out;
    out.reserve(images.size());
    for (const QByteArray &image : images)
        out.append(QString::fromLatin1(image));
    return out;
}

static QList<QByteArray> normalizedImages(const QList<QByteArray> &images)
{
    QList<QByteArray> normalized = images;
    QList<QByteArray> filtered;
    filtered.reserve(normalized.size());
    for (const QByteArray &image : normalized)
    {
        if (!image.isEmpty())
            filtered.append(image);
    }
    return filtered;
}

static bool advancedTemplateContainsMultiImageTokens(const QJsonObject &root)
{
    const QString compact = QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
    return compact.contains("{{gemini_parts}}") ||
           compact.contains("{{claude_user_content}}") ||
           compact.contains("{{openai_user_content}}");
}

ApiClient::ApiClient(QObject *parent)
    : QObject(parent)
{
}

ApiClient::~ApiClient()
{
}

ApiClient::RequestSettings ApiClient::currentRequestSettings() const
{
    RequestSettings settings;
    settings.apiKey = m_apiKey;
    settings.baseUrl = m_baseUrl;
    settings.endpointPath = m_endpointPath;
    settings.modelName = m_modelName;
    settings.provider = m_provider;
    settings.useProxy = m_useProxy;
    settings.proxyUrl = m_proxyUrl;
    settings.useAdvancedApi = m_useAdvancedApi;
    settings.advancedApiTemplate = m_advancedApiTemplate;
    if (settings.useAdvancedApi)
        settings.requestTimeoutMs = parseAdvancedRequestTimeoutMs(settings.advancedApiTemplate, kDefaultRequestTimeoutMs);
    return settings;
}

QNetworkAccessManager *ApiClient::createRequestManager(const RequestSettings &settings) const
{
    auto *manager = new QNetworkAccessManager(const_cast<ApiClient *>(this));
    if (settings.useProxy && !settings.proxyUrl.isEmpty())
    {
        const QUrl url = QUrl::fromUserInput(settings.proxyUrl);
        QNetworkProxy proxy;
        const QString scheme = url.scheme().toLower();
        proxy.setType((scheme == "socks5" || scheme == "socks") ? QNetworkProxy::Socks5Proxy : QNetworkProxy::HttpProxy);
        proxy.setHostName(url.host());
        proxy.setPort(url.port(8080));
        if (!url.userName().isEmpty())
            proxy.setUser(url.userName());
        if (!url.password().isEmpty())
            proxy.setPassword(url.password());
        manager->setProxy(proxy);
    }
    else
    {
        manager->setProxy(QNetworkProxy::DefaultProxy);
    }
    return manager;
}

void ApiClient::configure(const QString &apiKey, const QString &baseUrl, const QString &modelName,
                          ApiProvider provider, bool useProxy, const QString &proxyUrl, const QString &endpointPath,
                          bool useAdvancedApi, const QString &advancedApiTemplate)
{
    m_apiKey = apiKey;
    m_baseUrl = baseUrl;
    m_endpointPath = endpointPath;
    m_modelName = modelName;
    m_provider = provider;
    m_useProxy = useProxy;
    m_proxyUrl = proxyUrl;
    m_useAdvancedApi = useAdvancedApi;
    m_advancedApiTemplate = advancedApiTemplate;

    if (m_useProxy && !m_proxyUrl.isEmpty())
        qDebug() << "Proxy configured for future requests:" << proxyUrl;
    else
        qDebug() << "Requests will use the system proxy/default network settings";
}

void ApiClient::processImage(const QByteArray &base64Image, const QString &promptText, const QString &requestId)
{
    processImages(QList<QByteArray>{base64Image}, promptText, requestId);
}

void ApiClient::processImages(const QList<QByteArray> &base64Images, const QString &promptText, const QString &requestId)
{
    processImagesInternal(base64Images, promptText, requestId, 0, currentRequestSettings(),
                          QDateTime::currentMSecsSinceEpoch());
}

void ApiClient::processImagesInternal(const QList<QByteArray> &base64Images, const QString &promptText,
                                      const QString &requestId, int retryCount, const RequestSettings &settings,
                                      qint64 requestStartMs)
{
    const QList<QByteArray> images = normalizedImages(base64Images);
    if (images.isEmpty())
    {
        emit error("Image payload is empty.", requestId, 0);
        return;
    }

    QNetworkRequest request((QUrl()));
    QByteArray data;

    if (settings.useAdvancedApi)
    {
        QString advancedErr;
        if (!buildAdvancedRequest(settings, images, promptText, request, data, advancedErr))
        {
            emit error(QString("Advanced API Error: %1").arg(advancedErr), requestId, 0);
            return;
        }
    }

    if ((settings.apiKey.isEmpty() || settings.baseUrl.isEmpty() || settings.modelName.isEmpty()) && !settings.useAdvancedApi)
    {
        emit error("API Configuration invalid. Please check settings.", requestId, 0);
        return;
    }

    if (!settings.useAdvancedApi)
    {
        const QString endpoint = getEndpoint(settings);
        QUrl url = joinBaseAndEndpoint(settings.baseUrl, endpoint);
        if (settings.provider == ApiProvider::Gemini)
            url.setQuery(QString("key=%1").arg(settings.apiKey));

        request = QNetworkRequest(url);
        setProviderHeaders(request, settings);

        switch (settings.provider)
        {
        case ApiProvider::OpenAI:
            data = formatOpenAIRequest(settings, images, promptText);
            break;
        case ApiProvider::Gemini:
            data = formatGeminiRequest(images, promptText);
            break;
        case ApiProvider::Claude:
            data = formatClaudeRequest(settings, images, promptText);
            break;
        }
    }

    qDebug() << "ApiClient: Sending POST request to" << request.url().toString();

    QNetworkAccessManager *manager = createRequestManager(settings);
    QNetworkReply *reply = manager->post(request, data);
    reply->setProperty("originalBase64", images.first());
    reply->setProperty("originalBase64List", toStringList(images));
    reply->setProperty("originalPrompt", promptText);
    reply->setProperty("requestId", requestId);
    reply->setProperty("retryCount", retryCount);
    reply->setProperty("requestTimedOut", false);
    reply->setProperty("requestStartMs", requestStartMs);

    auto *timeoutTimer = new QTimer(reply);
    timeoutTimer->setObjectName(QStringLiteral("requestTimeoutTimer"));
    timeoutTimer->setSingleShot(true);
    timeoutTimer->setInterval(settings.requestTimeoutMs);
    QPointer<QNetworkReply> guardedReply(reply);
    connect(timeoutTimer, &QTimer::timeout, this, [guardedReply, requestTimeoutMs = settings.requestTimeoutMs]()
            {
                if (!guardedReply || guardedReply->isFinished())
                    return;

                guardedReply->setProperty("requestTimedOut", true);
                qWarning() << "ApiClient: Request timed out after" << requestTimeoutMs
                           << "ms. URL:" << guardedReply->url().toString();
                guardedReply->abort();
            });
    timeoutTimer->start();

    connect(reply, &QNetworkReply::finished, this, [this, reply, settings]()
            {
                qDebug() << "ApiClient: Reply finished. Error:" << reply->error();
                onReplyFinished(reply, settings);
            });

    connect(reply, &QNetworkReply::errorOccurred, this, [reply](QNetworkReply::NetworkError code)
            { qWarning() << "ApiClient: Network Error occurred:" << code << reply->errorString()
                         << "URL:" << reply->url().toString(); });

    connect(reply, &QNetworkReply::sslErrors, this, [reply](const QList<QSslError> &errors)
            {
                for (const QSslError &error : errors)
                    qWarning() << "SSL Error:" << error.errorString();
            });
}

void ApiClient::onReplyFinished(QNetworkReply *reply, const RequestSettings &settings)
{
    if (QTimer *timer = reply->findChild<QTimer *>(QStringLiteral("requestTimeoutTimer")))
        timer->stop();
    const auto cleanupReply = [reply]()
    {
        if (QNetworkAccessManager *manager = reply->manager())
            manager->deleteLater();
        reply->deleteLater();
    };

    const QString requestId = reply->property("requestId").toString();
    const QByteArray originalBase64 = reply->property("originalBase64").toByteArray();
    const QStringList originalBase64List = reply->property("originalBase64List").toStringList();
    const QString originalPrompt = reply->property("originalPrompt").toString();
    const int retryCount = reply->property("retryCount").toInt();
    const bool requestTimedOut = reply->property("requestTimedOut").toBool();
    const qint64 requestStartMs = reply->property("requestStartMs").toLongLong();
    const qint64 elapsedMs = requestStartMs > 0
                                 ? qMax<qint64>(0, QDateTime::currentMSecsSinceEpoch() - requestStartMs)
                                 : 0;

    if (reply->error() != QNetworkReply::NoError)
    {
        if (requestTimedOut)
        {
            emit error(QString("Request timed out after %1 seconds.")
                           .arg(timeoutSecondsText(settings.requestTimeoutMs)),
                       requestId, elapsedMs);
            cleanupReply();
            return;
        }

        if (reply->error() == QNetworkReply::RemoteHostClosedError ||
            reply->error() == QNetworkReply::ConnectionRefusedError)
        {
            if (retryCount < kMaxNetworkRetries)
            {
                qWarning() << "Network transition error detected (" << reply->error() << "), retrying request:"
                           << requestId << "attempt" << (retryCount + 1) << "of" << kMaxNetworkRetries;
                QList<QByteArray> images;
                images.reserve(originalBase64List.size());
                for (const QString &image : originalBase64List)
                    images.append(image.toLatin1());
                processImagesInternal(images, originalPrompt, requestId, retryCount + 1, settings, requestStartMs);
                cleanupReply();
                return;
            }
        }

        const QString err = reply->errorString();
        const QByteArray response = reply->readAll();
        qWarning() << "API Error:" << err << "Response:" << response;
        emit error(QString("Network Error: %1").arg(err), requestId, elapsedMs);
        cleanupReply();
        return;
    }

    const QByteArray response = reply->readAll();
    qDebug() << "ApiClient: Response Body:" << response;

    const QJsonDocument doc = QJsonDocument::fromJson(response);
    if (doc.isNull())
    {
        qWarning() << "ApiClient: Failed to parse JSON response";
        emit error("Failed to parse API response as JSON", requestId, elapsedMs);
        cleanupReply();
        return;
    }

    const QJsonObject root = doc.object();
    if (root.contains("error"))
    {
        const QJsonObject errObj = root["error"].toObject();
        const QString errMsg = errObj["message"].toString();
        emit error(QString("API Error: %1").arg(errMsg), requestId, elapsedMs);
        cleanupReply();
        return;
    }

    QString content;
    if (settings.useAdvancedApi)
    {
        content = parseOpenAIResponse(root);
        if (content.isEmpty())
            content = parseGeminiResponse(root);
        if (content.isEmpty())
            content = parseClaudeResponse(root);
        if (content.isEmpty())
            content = extractGenericText(root);
    }
    else
    {
        switch (settings.provider)
        {
        case ApiProvider::OpenAI:
            content = parseOpenAIResponse(root);
            break;
        case ApiProvider::Gemini:
            content = parseGeminiResponse(root);
            break;
        case ApiProvider::Claude:
            content = parseClaudeResponse(root);
            break;
        }
    }

    qDebug() << "ApiClient: Parsed Content Length:" << content.length();
    if (content.isEmpty())
    {
        qWarning() << "ApiClient: Parsed content is empty. Check parser logic.";
        emit error("Failed to extract content from API response", requestId, elapsedMs);
        cleanupReply();
        return;
    }

    if (settings.useAdvancedApi)
    {
        QJsonObject rootWithMeta = root;
        rootWithMeta.insert("_meta", buildAdvancedMetaObject(elapsedMs));
        const QString debugHeader = buildAdvancedDebugHeader(rootWithMeta, settings.advancedApiTemplate);
        if (!debugHeader.isEmpty())
            content.prepend(debugHeader);
    }

    emit success(content, QString::fromUtf8(originalBase64), originalPrompt, requestId, elapsedMs);
    cleanupReply();
}


