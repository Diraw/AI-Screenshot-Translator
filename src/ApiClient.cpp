#include "ApiClient.h"
#include <QNetworkRequest>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkProxy>
#include <QUrl>
#include <QDebug>
#include <QStringList>

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

ApiClient::ApiClient(QObject *parent) : QObject(parent)
{
    m_manager = new QNetworkAccessManager(this);
}

ApiClient::~ApiClient()
{
    // m_manager is a child, so it cleans up itself
}

void ApiClient::configure(const QString &apiKey, const QString &baseUrl, const QString &modelName,
                          ApiProvider provider, bool useProxy, const QString &proxyUrl, const QString &endpointPath)
{
    m_apiKey = apiKey;
    m_baseUrl = baseUrl;
    m_endpointPath = endpointPath;
    m_modelName = modelName;
    m_provider = provider;
    m_useProxy = useProxy;
    m_proxyUrl = proxyUrl;

    if (m_useProxy && !m_proxyUrl.isEmpty())
    {
        QUrl url(proxyUrl);
        QNetworkProxy proxy;

        QString scheme = url.scheme().toLower();
        if (scheme == "socks5" || scheme == "socks")
        {
            proxy.setType(QNetworkProxy::Socks5Proxy);
        }
        else
        {
            proxy.setType(QNetworkProxy::HttpProxy);
        }

        proxy.setHostName(url.host());
        proxy.setPort(url.port(8080));

        if (!url.userName().isEmpty())
        {
            proxy.setUser(url.userName());
        }
        if (!url.password().isEmpty())
        {
            proxy.setPassword(url.password());
        }

        m_manager->setProxy(proxy);
        qDebug() << "Proxy set to:" << proxyUrl
                 << "Type:" << (proxy.type() == QNetworkProxy::Socks5Proxy ? "Socks5" : "Http");
    }
    else
    {
        // If empty, use system proxy settings instead of forcing NoProxy
        m_manager->setProxy(QNetworkProxy::DefaultProxy);
        qDebug() << "Proxy set to Default (System settings)";
    }
}

void ApiClient::processImage(const QByteArray &base64Image, const QString &promptText, void *context)
{
    if (m_apiKey.isEmpty() || m_baseUrl.isEmpty() || m_modelName.isEmpty())
    {
        emit error("API Configuration invalid. Please check settings.", context);
        return;
    }

    QString endpoint = getEndpoint();
    QUrl url = joinBaseAndEndpoint(m_baseUrl, endpoint);

    // For Gemini, add API key to URL parameter
    if (m_provider == ApiProvider::Gemini)
    {
        url.setQuery(QString("key=%1").arg(m_apiKey));
    }

    QNetworkRequest request(url);
    setProviderHeaders(request);

    // Format request based on provider
    QByteArray data;
    switch (m_provider)
    {
    case ApiProvider::OpenAI:
        data = formatOpenAIRequest(base64Image, promptText);
        break;
    case ApiProvider::Gemini:
        data = formatGeminiRequest(base64Image, promptText);
        break;
    case ApiProvider::Claude:
        data = formatClaudeRequest(base64Image, promptText);
        break;
    }

    qDebug() << "ApiClient: Sending POST request to" << url.toString();
    // qDebug() << "ApiClient: Payload size:" << data.size();

    QNetworkReply *reply = m_manager->post(request, data);

    // Attach context properties to the reply object
    reply->setProperty("originalBase64", base64Image);
    reply->setProperty("originalPrompt", promptText);
    reply->setProperty("requestContext", (qulonglong)context);

    connect(reply, &QNetworkReply::finished, this, [this, reply]()
            {
        qDebug() << "ApiClient: Reply finished. Error:" << reply->error();
        onReplyFinished(reply); });

    // Add detailed error logging
    connect(reply, &QNetworkReply::errorOccurred, this, [reply](QNetworkReply::NetworkError code)
            { qWarning() << "ApiClient: Network Error occurred:" << code << reply->errorString()
                         << "URL:" << reply->url().toString(); });

    connect(reply, &QNetworkReply::sslErrors, this, [reply](const QList<QSslError> &errors)
            {
        for (const QSslError &error : errors) {
            qWarning() << "SSL Error:" << error.errorString();
        } });
}

void ApiClient::onReplyFinished(QNetworkReply *reply)
{
    reply->deleteLater();

    void *context = (void *)reply->property("requestContext").toULongLong();
    QByteArray originalBase64 = reply->property("originalBase64").toByteArray();
    QString originalPrompt = reply->property("originalPrompt").toString();

    if (reply->error() != QNetworkReply::NoError)
    {
        // Retry logic for specific transitional network errors
        if (reply->error() == QNetworkReply::RemoteHostClosedError ||
            reply->error() == QNetworkReply::ConnectionRefusedError)
        {

            if (!m_retriedContexts.contains(context))
            {
                qWarning() << "Network transition error detected (" << reply->error() << "), retrying once for context:" << context;
                m_retriedContexts.insert(context);
                processImage(originalBase64, originalPrompt, context);
                return;
            }
        }

        m_retriedContexts.remove(context); // Clear retry state for this request
        QString err = reply->errorString();
        QByteArray response = reply->readAll();
        qWarning() << "API Error:" << err << "Response:" << response;
        emit error(QString("Network Error: %1").arg(err), context);
        return;
    }

    m_retriedContexts.remove(context); // Clear retry state on success
    QByteArray response = reply->readAll();
    qDebug() << "ApiClient: Response Body:" << response; // Debug log

    QJsonDocument doc = QJsonDocument::fromJson(response);

    if (doc.isNull())
    {
        qWarning() << "ApiClient: Failed to parse JSON response";
        emit error("Failed to parse API response as JSON", context);
        return;
    }

    QJsonObject root = doc.object();

    // Check for API errors first
    if (root.contains("error"))
    {
        QJsonObject errObj = root["error"].toObject();
        QString errMsg = errObj["message"].toString();
        emit error(QString("API Error: %1").arg(errMsg), context);
        return;
    }

    // Parse response based on provider
    QString content;
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

    qDebug() << "ApiClient: Parsed Content Length:" << content.length();

    if (content.isEmpty())
    {
        qWarning() << "ApiClient: Parsed content is empty. Check parser logic.";
        emit error("Failed to extract content from API response", context);
        return;
    }

    emit success(content, originalBase64, originalPrompt, context);
}

// Provider-specific request formatters

QByteArray ApiClient::formatOpenAIRequest(const QByteArray &base64Image, const QString &prompt)
{
    QJsonObject systemMessage;
    systemMessage["role"] = "system";
    systemMessage["content"] = "You are a helpful assistant.";

    QJsonObject userMessage;
    userMessage["role"] = "user";

    QJsonArray contentArray;

    QJsonObject imageContent;
    imageContent["type"] = "image_url";
    QJsonObject imageUrl;
    imageUrl["url"] = QString("data:image/png;base64,%1").arg(QString(base64Image));
    imageContent["image_url"] = imageUrl;

    QJsonObject textContent;
    textContent["type"] = "text";
    textContent["text"] = prompt;

    contentArray.append(imageContent);
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
    QJsonArray parts;

    QJsonObject inlineData;
    inlineData["mime_type"] = "image/png";
    inlineData["data"] = QString(base64Image);

    QJsonObject imagePart;
    imagePart["inline_data"] = inlineData;

    QJsonObject textPart;
    textPart["text"] = prompt;

    parts.append(imagePart);
    parts.append(textPart);

    QJsonObject content;
    content["parts"] = parts;

    QJsonArray contents;
    contents.append(content);

    QJsonObject payload;
    payload["contents"] = contents;

    return QJsonDocument(payload).toJson();
}

QByteArray ApiClient::formatClaudeRequest(const QByteArray &base64Image, const QString &prompt)
{
    QJsonArray content;

    QJsonObject source;
    source["type"] = "base64";
    source["media_type"] = "image/png";
    source["data"] = QString(base64Image);

    QJsonObject imagePart;
    imagePart["type"] = "image";
    imagePart["source"] = source;

    QJsonObject textPart;
    textPart["type"] = "text";
    textPart["text"] = prompt;

    content.append(imagePart);
    content.append(textPart);

    QJsonObject userMessage;
    userMessage["role"] = "user";
    userMessage["content"] = content;

    QJsonArray messages;
    messages.append(userMessage);

    QJsonObject payload;
    payload["model"] = m_modelName;
    payload["max_tokens"] = 1024;
    payload["messages"] = messages;

    return QJsonDocument(payload).toJson();
}

// Provider-specific response parsers

QString ApiClient::parseOpenAIResponse(const QJsonObject &root)
{
    if (root.contains("choices") && root["choices"].isArray())
    {
        QJsonArray choices = root["choices"].toArray();
        if (!choices.isEmpty())
        {
            QJsonObject firstChoice = choices[0].toObject();
            QJsonObject message = firstChoice["message"].toObject();
            QJsonValue contentVal = message.value("content");
            if (contentVal.isString())
            {
                return contentVal.toString();
            }
            if (contentVal.isArray())
            {
                QStringList parts;
                for (const QJsonValue &v : contentVal.toArray())
                {
                    if (!v.isObject())
                        continue;
                    QJsonObject obj = v.toObject();
                    QString type = obj.value("type").toString();
                    if (type == "text")
                    {
                        QString text = obj.value("text").toString();
                        if (!text.isEmpty())
                            parts << text;
                    }
                }
                return parts.join("\n");
            }
        }
    }
    return QString();
}

QString ApiClient::parseGeminiResponse(const QJsonObject &root)
{
    if (root.contains("candidates") && root["candidates"].isArray())
    {
        QJsonArray candidates = root["candidates"].toArray();
        if (!candidates.isEmpty())
        {
            QJsonObject firstCandidate = candidates[0].toObject();
            QJsonObject content = firstCandidate["content"].toObject();
            QJsonArray parts = content["parts"].toArray();
            if (!parts.isEmpty())
            {
                return parts[0].toObject()["text"].toString();
            }
        }
    }
    return QString();
}

QString ApiClient::parseClaudeResponse(const QJsonObject &root)
{
    if (root.contains("content") && root["content"].isArray())
    {
        QJsonArray content = root["content"].toArray();
        if (!content.isEmpty())
        {
            return content[0].toObject()["text"].toString();
        }
    }
    return QString();
}

// Helper methods

QString ApiClient::getEndpoint() const
{
    if (!m_endpointPath.trimmed().isEmpty())
    {
        return m_endpointPath;
    }

    switch (m_provider)
    {
    case ApiProvider::OpenAI:
        return "/v1/chat/completions";
    case ApiProvider::Gemini:
        return QString("/v1beta/models/%1:generateContent").arg(m_modelName);
    case ApiProvider::Claude:
        return "/v1/messages";
    }
    return "/v1/chat/completions"; // Default
}

void ApiClient::setProviderHeaders(QNetworkRequest &request) const
{
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    // Add standard browser User-Agent to avoid being blocked by some CDNs/providers
    request.setRawHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");

    switch (m_provider)
    {
    case ApiProvider::OpenAI:
        if (!m_apiKey.isEmpty())
        {
            request.setRawHeader("Authorization", "Bearer " + m_apiKey.toUtf8());
        }
        break;
    case ApiProvider::Claude:
        request.setRawHeader("x-api-key", m_apiKey.toUtf8());
        request.setRawHeader("anthropic-version", "2023-06-01");
        break;
    case ApiProvider::Gemini:
        // Gemini uses API key in URL, not header
        break;
    }
}
