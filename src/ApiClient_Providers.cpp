#include "ApiClient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QStringList>

QString ApiClient::extractGenericText(const QJsonObject &root) const
{
    if (root.contains("text") && root.value("text").isString())
        return root.value("text").toString();
    if (root.contains("message") && root.value("message").isString())
        return root.value("message").toString();
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
}

QByteArray ApiClient::formatOpenAIRequest(const RequestSettings &settings, const QList<QByteArray> &base64Images,
                                          const QString &prompt)
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
    payload["model"] = settings.modelName;
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

QByteArray ApiClient::formatClaudeRequest(const RequestSettings &settings, const QList<QByteArray> &base64Images,
                                          const QString &prompt)
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
    payload["model"] = settings.modelName;
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

QString ApiClient::getEndpoint(const RequestSettings &settings) const
{
    if (!settings.endpointPath.trimmed().isEmpty())
        return settings.endpointPath;

    switch (settings.provider)
    {
    case ApiProvider::OpenAI:
        return "/v1/chat/completions";
    case ApiProvider::Gemini:
        return QString("/v1beta/models/%1:generateContent").arg(settings.modelName);
    case ApiProvider::Claude:
        return "/v1/messages";
    }
    return "/v1/chat/completions";
}

void ApiClient::setProviderHeaders(QNetworkRequest &request, const RequestSettings &settings) const
{
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");

    switch (settings.provider)
    {
    case ApiProvider::OpenAI:
        if (!settings.apiKey.isEmpty())
            request.setRawHeader("Authorization", "Bearer " + settings.apiKey.toUtf8());
        break;
    case ApiProvider::Claude:
        request.setRawHeader("x-api-key", settings.apiKey.toUtf8());
        request.setRawHeader("anthropic-version", "2023-06-01");
        break;
    case ApiProvider::Gemini:
        break;
    }
}
