#include "ApiClient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkRequest>
#include <QUrl>

namespace
{
QUrl joinBaseAndEndpointAdvanced(const QString &baseUrl, const QString &endpoint)
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

bool advancedTemplateContainsMultiImageTokensLocal(const QJsonObject &root)
{
    const QString compact = QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Compact));
    return compact.contains("{{gemini_parts}}") ||
           compact.contains("{{claude_user_content}}") ||
           compact.contains("{{openai_user_content}}");
}
} // namespace

bool ApiClient::buildAdvancedRequest(const RequestSettings &settings, const QList<QByteArray> &base64Images,
                                     const QString &promptText, QNetworkRequest &request, QByteArray &payload,
                                     QString &outError) const
{
    outError.clear();

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(settings.advancedApiTemplate.toUtf8(), &err);
    if (doc.isNull() || !doc.isObject() || err.error != QJsonParseError::NoError)
    {
        outError = QString("Template JSON parse failed: %1").arg(err.errorString());
        return false;
    }

    const QJsonObject root = doc.object();
    const QString apiKey = root.value("api_key").toString(settings.apiKey).trimmed();
    const QString baseUrl = root.value("base_url").toString(settings.baseUrl).trimmed();
    const QString endpoint = root.value("endpoint").toString(settings.endpointPath).trimmed();
    QString provider;
    switch (settings.provider)
    {
    case ApiProvider::OpenAI:
        provider = "openai";
        break;
    case ApiProvider::Gemini:
        provider = "gemini";
        break;
    case ApiProvider::Claude:
        provider = "claude";
        break;
    }
    provider = root.value("provider").toString(provider).trimmed().toLower();
    const QString model = root.value("model").toString(settings.modelName).trimmed();
    const QString prompt = root.value("prompt").toString(promptText);
    const double temperature = root.value("temperature").toDouble(0.2);
    const double topP = root.value("top_p").toDouble(1.0);
    const int maxTokens = root.value("max_tokens").toInt(1024);

    QUrl url = joinBaseAndEndpointAdvanced(baseUrl, endpoint);
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
        (base64Images.size() <= 1 || advancedTemplateContainsMultiImageTokensLocal(root)))
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
