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
        if (!path.isEmpty())
            fields.append(path);
    }
    fields.removeDuplicates();
    return fields;
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
    m_manager = new QNetworkAccessManager(this);
}

ApiClient::~ApiClient()
{
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
    {
        QUrl url(proxyUrl);
        QNetworkProxy proxy;
        const QString scheme = url.scheme().toLower();
        proxy.setType((scheme == "socks5" || scheme == "socks") ? QNetworkProxy::Socks5Proxy : QNetworkProxy::HttpProxy);
        proxy.setHostName(url.host());
        proxy.setPort(url.port(8080));
        if (!url.userName().isEmpty())
            proxy.setUser(url.userName());
        if (!url.password().isEmpty())
            proxy.setPassword(url.password());

        m_manager->setProxy(proxy);
        qDebug() << "Proxy set to:" << proxyUrl
                 << "Type:" << (proxy.type() == QNetworkProxy::Socks5Proxy ? "Socks5" : "Http");
    }
    else
    {
        m_manager->setProxy(QNetworkProxy::DefaultProxy);
        qDebug() << "Proxy set to Default (System settings)";
    }
}

void ApiClient::processImage(const QByteArray &base64Image, const QString &promptText, const QString &requestId)
{
    processImages(QList<QByteArray>{base64Image}, promptText, requestId);
}

void ApiClient::processImages(const QList<QByteArray> &base64Images, const QString &promptText, const QString &requestId)
{
    processImagesInternal(base64Images, promptText, requestId, 0);
}

void ApiClient::processImagesInternal(const QList<QByteArray> &base64Images, const QString &promptText,
                                      const QString &requestId, int retryCount)
{
    const QList<QByteArray> images = normalizedImages(base64Images);
    if (images.isEmpty())
    {
        emit error("Image payload is empty.", requestId);
        return;
    }

    QNetworkRequest request((QUrl()));
    QByteArray data;

    if (m_useAdvancedApi)
    {
        QString advancedErr;
        if (!buildAdvancedRequest(images, promptText, request, data, advancedErr))
        {
            emit error(QString("Advanced API Error: %1").arg(advancedErr), requestId);
            return;
        }
    }

    if ((m_apiKey.isEmpty() || m_baseUrl.isEmpty() || m_modelName.isEmpty()) && !m_useAdvancedApi)
    {
        emit error("API Configuration invalid. Please check settings.", requestId);
        return;
    }

    if (!m_useAdvancedApi)
    {
        QString endpoint = getEndpoint();
        QUrl url = joinBaseAndEndpoint(m_baseUrl, endpoint);
        if (m_provider == ApiProvider::Gemini)
            url.setQuery(QString("key=%1").arg(m_apiKey));

        request = QNetworkRequest(url);
        setProviderHeaders(request);

        switch (m_provider)
        {
        case ApiProvider::OpenAI:
            data = formatOpenAIRequest(images, promptText);
            break;
        case ApiProvider::Gemini:
            data = formatGeminiRequest(images, promptText);
            break;
        case ApiProvider::Claude:
            data = formatClaudeRequest(images, promptText);
            break;
        }
    }

    qDebug() << "ApiClient: Sending POST request to" << request.url().toString();

    QNetworkReply *reply = m_manager->post(request, data);
    reply->setProperty("originalBase64", images.first());
    reply->setProperty("originalBase64List", toStringList(images));
    reply->setProperty("originalPrompt", promptText);
    reply->setProperty("requestId", requestId);
    reply->setProperty("retryCount", retryCount);
    reply->setProperty("requestTimedOut", false);

    auto *timeoutTimer = new QTimer(reply);
    timeoutTimer->setObjectName(QStringLiteral("requestTimeoutTimer"));
    timeoutTimer->setSingleShot(true);
    timeoutTimer->setInterval(kRequestTimeoutMs);
    QPointer<QNetworkReply> guardedReply(reply);
    connect(timeoutTimer, &QTimer::timeout, this, [guardedReply]()
            {
                if (!guardedReply || guardedReply->isFinished())
                    return;

                guardedReply->setProperty("requestTimedOut", true);
                qWarning() << "ApiClient: Request timed out after" << ApiClient::kRequestTimeoutMs
                           << "ms. URL:" << guardedReply->url().toString();
                guardedReply->abort();
            });
    timeoutTimer->start();

    connect(reply, &QNetworkReply::finished, this, [this, reply]()
            {
                qDebug() << "ApiClient: Reply finished. Error:" << reply->error();
                onReplyFinished(reply);
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

void ApiClient::onReplyFinished(QNetworkReply *reply)
{
    if (QTimer *timer = reply->findChild<QTimer *>(QStringLiteral("requestTimeoutTimer")))
        timer->stop();

    reply->deleteLater();

    const QString requestId = reply->property("requestId").toString();
    const QByteArray originalBase64 = reply->property("originalBase64").toByteArray();
    const QStringList originalBase64List = reply->property("originalBase64List").toStringList();
    const QString originalPrompt = reply->property("originalPrompt").toString();
    const int retryCount = reply->property("retryCount").toInt();
    const bool requestTimedOut = reply->property("requestTimedOut").toBool();

    if (reply->error() != QNetworkReply::NoError)
    {
        if (requestTimedOut)
        {
            emit error(QString("Request timed out after %1 seconds.").arg(kRequestTimeoutMs / 1000), requestId);
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
                processImagesInternal(images, originalPrompt, requestId, retryCount + 1);
                return;
            }
        }

        const QString err = reply->errorString();
        const QByteArray response = reply->readAll();
        qWarning() << "API Error:" << err << "Response:" << response;
        emit error(QString("Network Error: %1").arg(err), requestId);
        return;
    }

    const QByteArray response = reply->readAll();
    qDebug() << "ApiClient: Response Body:" << response;

    const QJsonDocument doc = QJsonDocument::fromJson(response);
    if (doc.isNull())
    {
        qWarning() << "ApiClient: Failed to parse JSON response";
        emit error("Failed to parse API response as JSON", requestId);
        return;
    }

    const QJsonObject root = doc.object();
    if (root.contains("error"))
    {
        const QJsonObject errObj = root["error"].toObject();
        const QString errMsg = errObj["message"].toString();
        emit error(QString("API Error: %1").arg(errMsg), requestId);
        return;
    }

    QString content;
    if (m_useAdvancedApi)
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
        switch (m_provider)
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
        emit error("Failed to extract content from API response", requestId);
        return;
    }

    if (m_useAdvancedApi)
    {
        const QString debugHeader = buildAdvancedDebugHeader(root, m_advancedApiTemplate);
        if (!debugHeader.isEmpty())
            content.prepend(debugHeader);
    }

    emit success(content, QString::fromUtf8(originalBase64), originalPrompt, requestId);
}

bool ApiClient::buildAdvancedRequest(const QList<QByteArray> &base64Images, const QString &promptText,
                                     QNetworkRequest &request, QByteArray &payload, QString &outError) const
{
    outError.clear();

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(m_advancedApiTemplate.toUtf8(), &err);
    if (doc.isNull() || !doc.isObject() || err.error != QJsonParseError::NoError)
    {
        outError = QString("Template JSON parse failed: %1").arg(err.errorString());
        return false;
    }

    const QJsonObject root = doc.object();
    const QString apiKey = root.value("api_key").toString(m_apiKey).trimmed();
    const QString baseUrl = root.value("base_url").toString(m_baseUrl).trimmed();
    const QString endpoint = root.value("endpoint").toString(m_endpointPath).trimmed();
    const QString model = root.value("model").toString(m_modelName).trimmed();
    const QString provider = root.value("provider").toString("openai").trimmed().toLower();
    const QString prompt = root.value("prompt").toString(promptText);
    const double temperature = root.value("temperature").toDouble(0.2);
    const double topP = root.value("top_p").toDouble(1.0);
    const int maxTokens = root.value("max_tokens").toInt(1024);

    QUrl url = joinBaseAndEndpoint(baseUrl, endpoint);
    if (!url.isValid() || url.scheme().isEmpty() || url.host().isEmpty())
    {
        outError = "Invalid base_url/endpoint in advanced template";
        return false;
    }

    if (provider == "gemini" && !apiKey.isEmpty() && !url.query().contains("key="))
        url.setQuery(QString("key=%1").arg(apiKey));

    request = QNetworkRequest(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");

    const QHash<QString, QString> headerTokens = {
        {"api_key", apiKey},
        {"model", model},
        {"prompt", prompt},
        {"temperature", QString::number(temperature, 'g', 6)},
        {"top_p", QString::number(topP, 'g', 6)},
        {"max_tokens", QString::number(maxTokens)},
        {"base64_image", QString::fromLatin1(base64Images.first())},
        {"gemini_parts", QString::fromUtf8(QJsonDocument([&base64Images, &prompt]()
                                                         {
                                                             QJsonArray parts;
                                                             for (const QByteArray &image : base64Images)
                                                             {
                                                                 parts.append(QJsonObject{{"inline_data", QJsonObject{{"mime_type", "image/png"},
                                                                                                                      {"data", QString::fromLatin1(image)}}}});
                                                             }
                                                             parts.append(QJsonObject{{"text", prompt}});
                                                             return parts;
                                                         }()).toJson(QJsonDocument::Compact))},
        {"claude_user_content", QString::fromUtf8(QJsonDocument([&base64Images, &prompt]()
                                                                {
                                                                    QJsonArray content;
                                                                    for (const QByteArray &image : base64Images)
                                                                    {
                                                                        content.append(QJsonObject{{"type", "image"},
                                                                                                   {"source", QJsonObject{{"type", "base64"},
                                                                                                                          {"media_type", "image/png"},
                                                                                                                          {"data", QString::fromLatin1(image)}}}});
                                                                    }
                                                                    content.append(QJsonObject{{"type", "text"}, {"text", prompt}});
                                                                    return content;
                                                                }()).toJson(QJsonDocument::Compact))},
        {"openai_user_content", QString::fromUtf8(QJsonDocument([&base64Images, &prompt]()
                                                                {
                                                                    QJsonArray content;
                                                                    for (const QByteArray &image : base64Images)
                                                                    {
                                                                        content.append(QJsonObject{{"type", "image_url"},
                                                                                                   {"image_url", QJsonObject{{"url", QString("data:image/png;base64,%1").arg(QString::fromLatin1(image))}}}});
                                                                    }
                                                                    content.append(QJsonObject{{"type", "text"}, {"text", prompt}});
                                                                    return content;
                                                                }()).toJson(QJsonDocument::Compact))},
    };

    if (root.contains("headers") && root.value("headers").isObject())
    {
        const QJsonObject headers = root.value("headers").toObject();
        for (auto it = headers.constBegin(); it != headers.constEnd(); ++it)
        {
            QString headerValue = it.value().toString();
            for (auto tk = headerTokens.constBegin(); tk != headerTokens.constEnd(); ++tk)
                headerValue.replace(QString("{{%1}}").arg(tk.key()), tk.value());
            request.setRawHeader(it.key().toUtf8(), headerValue.toUtf8());
        }
    }
    else
    {
        if (provider == "openai" && !apiKey.isEmpty())
            request.setRawHeader("Authorization", QByteArray("Bearer ") + apiKey.toUtf8());
        if (provider == "claude" && !apiKey.isEmpty())
        {
            request.setRawHeader("x-api-key", apiKey.toUtf8());
            request.setRawHeader("anthropic-version", "2023-06-01");
        }
    }

    const QString epLower = endpoint.toLower();
    const QString baseLower = baseUrl.toLower();
    const bool looksOpenAICompatible = epLower.contains("chat/completions") || baseLower.contains("compatible-mode");
    if (looksOpenAICompatible && !apiKey.isEmpty() && !request.hasRawHeader("Authorization"))
        request.setRawHeader("Authorization", QByteArray("Bearer ") + apiKey.toUtf8());

    QJsonObject body;
    if (root.contains("request_body") && root.value("request_body").isObject() &&
        (base64Images.size() <= 1 || advancedTemplateContainsMultiImageTokens(root)))
    {
        body = applyTemplateTokens(root.value("request_body"), headerTokens).toObject();
    }
    else if (provider == "gemini")
    {
        QJsonArray parts;
        for (const QByteArray &image : base64Images)
        {
            QJsonObject inlineData;
            inlineData["mime_type"] = "image/png";
            inlineData["data"] = QString::fromLatin1(image);

            QJsonObject imagePart;
            imagePart["inline_data"] = inlineData;
            parts.append(imagePart);
        }
        QJsonObject textPart;
        textPart["text"] = prompt;
        parts.append(textPart);

        QJsonObject content;
        content["parts"] = parts;
        body["contents"] = QJsonArray{content};
    }
    else if (provider == "claude")
    {
        QJsonArray content;
        for (const QByteArray &image : base64Images)
        {
            QJsonObject source;
            source["type"] = "base64";
            source["media_type"] = "image/png";
            source["data"] = QString::fromLatin1(image);

            QJsonObject imagePart;
            imagePart["type"] = "image";
            imagePart["source"] = source;
            content.append(imagePart);
        }

        QJsonObject textPart;
        textPart["type"] = "text";
        textPart["text"] = prompt;
        content.append(textPart);

        QJsonObject msg;
        msg["role"] = "user";
        msg["content"] = content;
        body["messages"] = QJsonArray{msg};
        body["model"] = model;
        body["max_tokens"] = maxTokens;
    }
    else
    {
        QJsonArray contentArray;
        for (const QByteArray &image : base64Images)
        {
            QJsonObject imageContent;
            imageContent["type"] = "image_url";
            QJsonObject imageUrl;
            imageUrl["url"] = QString("data:image/png;base64,%1").arg(QString::fromLatin1(image));
            imageContent["image_url"] = imageUrl;
            contentArray.append(imageContent);
        }

        QJsonObject textContent;
        textContent["type"] = "text";
        textContent["text"] = prompt;
        contentArray.append(textContent);

        QJsonObject userMessage;
        userMessage["role"] = "user";
        userMessage["content"] = contentArray;
        body["model"] = model;
        body["messages"] = QJsonArray{userMessage};
        body["temperature"] = temperature;
        body["top_p"] = topP;
        body["max_tokens"] = maxTokens;
    }

    payload = QJsonDocument(body).toJson();
    return true;
}

QJsonValue ApiClient::applyTemplateTokens(const QJsonValue &value, const QHash<QString, QString> &tokens) const
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
        QJsonArray arr;
        const QJsonArray src = value.toArray();
        for (const QJsonValue &v : src)
            arr.append(applyTemplateTokens(v, tokens));
        return arr;
    }

    if (value.isObject())
    {
        QJsonObject obj;
        const QJsonObject src = value.toObject();
        for (auto it = src.constBegin(); it != src.constEnd(); ++it)
            obj.insert(it.key(), applyTemplateTokens(it.value(), tokens));
        return obj;
    }

    return value;
}

QString ApiClient::extractGenericText(const QJsonObject &root) const
{
    if (root.contains("text") && root.value("text").isString())
        return root.value("text").toString();
    if (root.contains("message") && root.value("message").isString())
        return root.value("message").toString();
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

QByteArray ApiClient::formatOpenAIRequest(const QByteArray &base64Image, const QString &prompt)
{
    return formatOpenAIRequest(QList<QByteArray>{base64Image}, prompt);
}

QByteArray ApiClient::formatOpenAIRequest(const QList<QByteArray> &base64Images, const QString &prompt)
{
    QJsonObject systemMessage;
    systemMessage["role"] = "system";
    systemMessage["content"] = "You are a helpful assistant.";

    QJsonObject userMessage;
    userMessage["role"] = "user";

    QJsonArray contentArray;
    for (const QByteArray &image : base64Images)
    {
        QJsonObject imageContent;
        imageContent["type"] = "image_url";
        QJsonObject imageUrl;
        imageUrl["url"] = QString("data:image/png;base64,%1").arg(QString::fromLatin1(image));
        imageContent["image_url"] = imageUrl;
        contentArray.append(imageContent);
    }

    QJsonObject textContent;
    textContent["type"] = "text";
    textContent["text"] = prompt;
    contentArray.append(textContent);
    userMessage["content"] = contentArray;

    QJsonArray messages;
    messages.append(systemMessage);
    messages.append(userMessage);

    QJsonObject payload;
    payload["model"] = m_modelName;
    payload["messages"] = messages;
    return QJsonDocument(payload).toJson();
}

QByteArray ApiClient::formatGeminiRequest(const QByteArray &base64Image, const QString &prompt)
{
    return formatGeminiRequest(QList<QByteArray>{base64Image}, prompt);
}

QByteArray ApiClient::formatGeminiRequest(const QList<QByteArray> &base64Images, const QString &prompt)
{
    QJsonArray parts;
    for (const QByteArray &image : base64Images)
    {
        QJsonObject inlineData;
        inlineData["mime_type"] = "image/png";
        inlineData["data"] = QString::fromLatin1(image);

        QJsonObject imagePart;
        imagePart["inline_data"] = inlineData;
        parts.append(imagePart);
    }

    QJsonObject textPart;
    textPart["text"] = prompt;
    parts.append(textPart);

    QJsonObject content;
    content["parts"] = parts;

    QJsonObject payload;
    payload["contents"] = QJsonArray{content};
    return QJsonDocument(payload).toJson();
}

QByteArray ApiClient::formatClaudeRequest(const QByteArray &base64Image, const QString &prompt)
{
    return formatClaudeRequest(QList<QByteArray>{base64Image}, prompt);
}

QByteArray ApiClient::formatClaudeRequest(const QList<QByteArray> &base64Images, const QString &prompt)
{
    QJsonArray content;
    for (const QByteArray &image : base64Images)
    {
        QJsonObject source;
        source["type"] = "base64";
        source["media_type"] = "image/png";
        source["data"] = QString::fromLatin1(image);

        QJsonObject imagePart;
        imagePart["type"] = "image";
        imagePart["source"] = source;
        content.append(imagePart);
    }

    QJsonObject textPart;
    textPart["type"] = "text";
    textPart["text"] = prompt;
    content.append(textPart);

    QJsonObject userMessage;
    userMessage["role"] = "user";
    userMessage["content"] = content;

    QJsonObject payload;
    payload["model"] = m_modelName;
    payload["max_tokens"] = 1024;
    payload["messages"] = QJsonArray{userMessage};
    return QJsonDocument(payload).toJson();
}

QString ApiClient::parseOpenAIResponse(const QJsonObject &root)
{
    if (root.contains("choices") && root["choices"].isArray())
    {
        const QJsonArray choices = root["choices"].toArray();
        if (!choices.isEmpty())
        {
            const QJsonObject firstChoice = choices[0].toObject();
            const QJsonObject message = firstChoice["message"].toObject();
            const QJsonValue contentVal = message.value("content");
            if (contentVal.isString())
                return contentVal.toString();
            if (contentVal.isArray())
            {
                QStringList parts;
                for (const QJsonValue &v : contentVal.toArray())
                {
                    if (!v.isObject())
                        continue;
                    const QJsonObject obj = v.toObject();
                    if (obj.value("type").toString() == "text")
                    {
                        const QString text = obj.value("text").toString();
                        if (!text.isEmpty())
                            parts << text;
                    }
                }
                return parts.join("\n");
            }
        }
    }
    return {};
}

QString ApiClient::parseGeminiResponse(const QJsonObject &root)
{
    if (root.contains("candidates") && root["candidates"].isArray())
    {
        const QJsonArray candidates = root["candidates"].toArray();
        if (!candidates.isEmpty())
        {
            const QJsonObject firstCandidate = candidates[0].toObject();
            const QJsonObject content = firstCandidate["content"].toObject();
            const QJsonArray parts = content["parts"].toArray();
            QStringList textParts;
            for (const QJsonValue &partValue : parts)
            {
                const QString text = partValue.toObject()["text"].toString();
                if (!text.isEmpty())
                    textParts << text;
            }
            return textParts.join("\n");
        }
    }
    return {};
}

QString ApiClient::parseClaudeResponse(const QJsonObject &root)
{
    if (root.contains("content") && root["content"].isArray())
    {
        const QJsonArray content = root["content"].toArray();
        QStringList textParts;
        for (const QJsonValue &partValue : content)
        {
            const QString text = partValue.toObject()["text"].toString();
            if (!text.isEmpty())
                textParts << text;
        }
        return textParts.join("\n");
    }
    return {};
}

QString ApiClient::getEndpoint() const
{
    if (!m_endpointPath.trimmed().isEmpty())
        return m_endpointPath;

    switch (m_provider)
    {
    case ApiProvider::OpenAI:
        return "/v1/chat/completions";
    case ApiProvider::Gemini:
        return QString("/v1beta/models/%1:generateContent").arg(m_modelName);
    case ApiProvider::Claude:
        return "/v1/messages";
    }
    return "/v1/chat/completions";
}

void ApiClient::setProviderHeaders(QNetworkRequest &request) const
{
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");

    switch (m_provider)
    {
    case ApiProvider::OpenAI:
        if (!m_apiKey.isEmpty())
            request.setRawHeader("Authorization", "Bearer " + m_apiKey.toUtf8());
        break;
    case ApiProvider::Claude:
        request.setRawHeader("x-api-key", m_apiKey.toUtf8());
        request.setRawHeader("anthropic-version", "2023-06-01");
        break;
    case ApiProvider::Gemini:
        break;
    }
}
