#include "ConfigDialog.h"

#include "TranslationManager.h"

#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QHash>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QSet>
#include <QSignalBlocker>
#include <QTcpSocket>
#include <QTimer>
#include <QUrl>

static bool tryBuildProxyFromUrl(const QString &proxyUrl, QNetworkProxy &outProxy, QString &outErr)
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

static QUrl joinBaseAndEndpointUi(const QString &baseUrl, const QString &endpoint)
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

static QJsonValue substituteTemplateTokens(const QJsonValue &value, const QHash<QString, QString> &tokens)
{
    if (value.isString())
    {
        const QString raw = value.toString();
        const QString trimmed = raw.trimmed();

        // Keep numeric fields as JSON numbers instead of string literals.
        if (trimmed == "{{temperature}}" || trimmed == "{{top_p}}")
            return QJsonValue(tokens.value(trimmed.mid(2, trimmed.length() - 4)).toDouble());
        if (trimmed == "{{max_tokens}}")
            return QJsonValue(tokens.value("max_tokens").toInt());

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

static QString jsonValueToSingleLine(const QJsonValue &value)
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

static void collectJsonLeafPaths(const QJsonValue &value, const QString &path,
                                 QStringList &outPaths, QHash<QString, QString> &outPreview,
                                 int depth = 0)
{
    constexpr int kMaxDepth = 8;
    constexpr int kMaxArrayItems = 24;
    constexpr int kMaxPreviewLen = 140;

    if (depth > kMaxDepth)
        return;

    if (value.isObject())
    {
        const QJsonObject obj = value.toObject();
        if (obj.isEmpty())
        {
            if (!path.isEmpty())
            {
                outPaths.append(path);
                outPreview.insert(path, "{}");
            }
            return;
        }

        for (auto it = obj.constBegin(); it != obj.constEnd(); ++it)
        {
            const QString childPath = path.isEmpty() ? it.key() : QString("%1.%2").arg(path, it.key());
            collectJsonLeafPaths(it.value(), childPath, outPaths, outPreview, depth + 1);
        }
        return;
    }

    if (value.isArray())
    {
        const QJsonArray arr = value.toArray();
        if (arr.isEmpty())
        {
            if (!path.isEmpty())
            {
                outPaths.append(path);
                outPreview.insert(path, "[]");
            }
            return;
        }

        const int limit = qMin(arr.size(), kMaxArrayItems);
        for (int i = 0; i < limit; ++i)
        {
            const QString childPath = path.isEmpty() ? QString("[%1]").arg(i) : QString("%1[%2]").arg(path).arg(i);
            collectJsonLeafPaths(arr.at(i), childPath, outPaths, outPreview, depth + 1);
        }
        return;
    }

    if (path.isEmpty())
        return;

    QString preview = jsonValueToSingleLine(value);
    preview.replace('\n', ' ');
    if (preview.size() > kMaxPreviewLen)
        preview = preview.left(kMaxPreviewLen - 3) + "...";
    outPaths.append(path);
    outPreview.insert(path, preview);
}

static QStringList readDebugFieldsFromTemplateRoot(const QJsonObject &root)
{
    QStringList fields;
    const QJsonArray arr = root.value("debug_fields").toArray();
    for (const QJsonValue &v : arr)
    {
        const QString f = v.toString().trimmed();
        if (!f.isEmpty())
            fields.append(f);
    }
    fields.removeDuplicates();
    return fields;
}

static QString loadAdvancedApiTestImageBase64()
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

    // Fallback to a small valid PNG if assets/test.png is unavailable.
    return QStringLiteral("iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAIAAACQkWg2AAAAMElEQVR4nGK68+H//w//iScZ/3/4f5eBQZmBgUiSiSTVygwMozaM2jBgNgACAAD//8tKvDmEFTFvAAAAAElFTkSuQmCC");
}

QString ConfigDialog::defaultEndpointForProvider(const QString &provider) const
{
    const QString p = provider.trimmed().toLower();
    if (p == "gemini")
        return "/v1beta";
    if (p == "claude")
        return "/v1/messages";

    // OpenAI-compatible providers differ: some base URLs already include /v1, some do not.
    const QString baseLower = m_baseUrlEdit ? m_baseUrlEdit->text().trimmed().toLower() : QString();
    if (baseLower.endsWith("/v1") || baseLower.endsWith("/v1/") || baseLower.contains("/compatible-mode/v1"))
        return "/chat/completions";
    return "/v1/chat/completions";
}

void ConfigDialog::maybeApplyEndpointDefaultForProvider(const QString &provider)
{
    if (!m_endpointPathEdit)
        return;

    const QString newDefault = defaultEndpointForProvider(provider);
    const QString cur = m_endpointPathEdit->text().trimmed();

    if (cur.isEmpty() || (!m_lastAutoEndpoint.isEmpty() && cur == m_lastAutoEndpoint))
    {
        m_endpointPathEdit->setText(newDefault);
        m_lastAutoEndpoint = newDefault;
    }
}

QString ConfigDialog::normalizeProviderForAdvancedTemplate(const QString &provider) const
{
    const QString p = provider.trimmed().toLower();
    if (p == "gemini" || p == "claude")
        return p;
    return "openai";
}

QString ConfigDialog::buildAdvancedTemplateFromRegular(const QString &provider) const
{
    const QString normalizedProvider = normalizeProviderForAdvancedTemplate(provider);
    const QString endpoint = m_endpointPathEdit ? m_endpointPathEdit->text().trimmed() : QString();
    const QString providerDefaultEndpoint = defaultEndpointForProvider(normalizedProvider);

    QString normalizedEndpoint = endpoint;
    if (normalizedEndpoint.isEmpty())
    {
        normalizedEndpoint = providerDefaultEndpoint;
    }
    else
    {
        const QString ep = normalizedEndpoint.trimmed().toLower();
        const bool staleForClaude = (normalizedProvider == "claude" &&
                                     (ep == "/chat/completions" || ep == "/v1/chat/completions" || ep == "/v1beta"));
        const bool staleForOpenAI = (normalizedProvider == "openai" &&
                                     (ep == "/v1/messages" || ep == "/v1beta"));
        const bool staleForGemini = (normalizedProvider == "gemini" &&
                                     (ep == "/chat/completions" || ep == "/v1/chat/completions" || ep == "/v1/messages"));
        if (staleForClaude || staleForOpenAI || staleForGemini)
            normalizedEndpoint = providerDefaultEndpoint;
    }

    QJsonObject root;
    root["provider"] = normalizedProvider;
    root["api_key"] = m_apiKeyEdit ? m_apiKeyEdit->text() : QString();
    root["base_url"] = m_baseUrlEdit ? m_baseUrlEdit->text().trimmed() : QString();
    root["endpoint"] = normalizedEndpoint;
    root["model"] = m_modelNameEdit ? m_modelNameEdit->text().trimmed() : QString();
    root["prompt"] = m_promptEdit ? m_promptEdit->toPlainText() : QString();
    root["temperature"] = 0.2;
    root["top_p"] = 1.0;
    root["max_tokens"] = 1024;

    QJsonObject headers;
    headers["Content-Type"] = "application/json";
    if (normalizedProvider == "claude")
    {
        headers["x-api-key"] = "{{api_key}}";
        headers["anthropic-version"] = "2023-06-01";
    }
    else if (normalizedProvider == "openai")
    {
        headers["Authorization"] = "Bearer {{api_key}}";
    }
    root["headers"] = headers;

    QJsonObject requestBody;
    if (normalizedProvider == "gemini")
    {
        QJsonArray parts;
        QJsonObject imagePart;
        QJsonObject inlineData;
        inlineData["mime_type"] = "image/png";
        inlineData["data"] = "{{base64_image}}";
        imagePart["inline_data"] = inlineData;

        QJsonObject textPart;
        textPart["text"] = "{{prompt}}";

        parts.append(imagePart);
        parts.append(textPart);

        QJsonObject content;
        content["parts"] = parts;

        QJsonArray contents;
        contents.append(content);
        requestBody["contents"] = contents;
    }
    else if (normalizedProvider == "claude")
    {
        QJsonArray content;
        QJsonObject image;
        image["type"] = "image";
        QJsonObject source;
        source["type"] = "base64";
        source["media_type"] = "image/png";
        source["data"] = "{{base64_image}}";
        image["source"] = source;
        content.append(image);

        QJsonObject text;
        text["type"] = "text";
        text["text"] = "{{prompt}}";
        content.append(text);

        QJsonObject msg;
        msg["role"] = "user";
        msg["content"] = content;
        QJsonArray msgs;
        msgs.append(msg);
        requestBody["messages"] = msgs;
        requestBody["model"] = "{{model}}";
        requestBody["max_tokens"] = "{{max_tokens}}";
    }
    else
    {
        QJsonObject systemMsg;
        systemMsg["role"] = "system";
        systemMsg["content"] = "You are a helpful assistant.";

        QJsonObject imageContent;
        imageContent["type"] = "image_url";
        QJsonObject imageUrl;
        imageUrl["url"] = "data:image/png;base64,{{base64_image}}";
        imageContent["image_url"] = imageUrl;

        QJsonObject textContent;
        textContent["type"] = "text";
        textContent["text"] = "{{prompt}}";

        QJsonArray userContent;
        userContent.append(imageContent);
        userContent.append(textContent);

        QJsonObject userMsg;
        userMsg["role"] = "user";
        userMsg["content"] = userContent;

        QJsonArray msgs;
        msgs.append(systemMsg);
        msgs.append(userMsg);

        requestBody["model"] = "{{model}}";
        requestBody["messages"] = msgs;
        requestBody["temperature"] = "{{temperature}}";
        requestBody["top_p"] = "{{top_p}}";
        requestBody["max_tokens"] = "{{max_tokens}}";
    }
    root["request_body"] = requestBody;

    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

bool ConfigDialog::parseAdvancedTemplateJson(QJsonObject &outRoot, QString &outError) const
{
    outRoot = QJsonObject();
    outError.clear();

    if (!m_advancedApiTemplateEdit)
    {
        outError = "高级模板编辑器不可用。";
        return false;
    }

    const QByteArray raw = m_advancedApiTemplateEdit->toPlainText().toUtf8();
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(raw, &err);
    if (doc.isNull() || !doc.isObject() || err.error != QJsonParseError::NoError)
    {
        outError = QString("JSON 解析失败: %1 (offset=%2)").arg(err.errorString()).arg(err.offset);
        return false;
    }

    outRoot = doc.object();
    return true;
}

void ConfigDialog::syncAdvancedTemplateFromRegular()
{
    if (m_isLoadingConfig || m_isSyncingAdvanced || !m_advancedApiTemplateEdit)
        return;
    if (m_advancedTemplateDetached)
        return;
    if (m_enableAdvancedApiCheck && m_enableAdvancedApiCheck->isChecked())
        return;

    const QString provider = m_apiProviderCombo ? m_apiProviderCombo->currentData().toString() : QString("openai");

    m_isSyncingAdvanced = true;
    m_advancedApiTemplateEdit->setPlainText(buildAdvancedTemplateFromRegular(provider));
    m_isSyncingAdvanced = false;
}

void ConfigDialog::syncRegularFieldsFromAdvancedTemplate()
{
    if (m_isLoadingConfig || m_isSyncingAdvanced)
        return;

    QJsonObject root;
    QString err;
    if (!parseAdvancedTemplateJson(root, err))
        return;

    m_isSyncingAdvanced = true;
    if (root.contains("api_key") && m_apiKeyEdit)
        m_apiKeyEdit->setText(root.value("api_key").toString());
    if (root.contains("base_url") && m_baseUrlEdit)
        m_baseUrlEdit->setText(root.value("base_url").toString());
    if (root.contains("endpoint") && m_endpointPathEdit)
        m_endpointPathEdit->setText(root.value("endpoint").toString());
    if (root.contains("model") && m_modelNameEdit)
        m_modelNameEdit->setText(root.value("model").toString());
    if (root.contains("prompt") && m_promptEdit)
        m_promptEdit->setPlainText(root.value("prompt").toString());

    if (root.contains("provider") && m_apiProviderCombo)
    {
        const QString provider = normalizeProviderForAdvancedTemplate(root.value("provider").toString());
        if (!provider.isEmpty())
            m_lastRegularProvider = provider;
    }
    m_isSyncingAdvanced = false;
}

void ConfigDialog::ensureAdvancedProviderOption(bool enabled)
{
    Q_UNUSED(enabled);
    if (!m_apiProviderCombo)
        return;

    const int advancedIndex = m_apiProviderCombo->findData("advanced");
    if (advancedIndex >= 0)
        m_apiProviderCombo->removeItem(advancedIndex);
}

void ConfigDialog::updateAdvancedApiUiState()
{
    const bool advancedOn = m_enableAdvancedApiCheck && m_enableAdvancedApiCheck->isChecked();

    ensureAdvancedProviderOption(advancedOn);

    if (m_advancedApiTemplateEdit)
        m_advancedApiTemplateEdit->setReadOnly(!advancedOn);

    if (m_apiKeyEdit)
        m_apiKeyEdit->setEnabled(!advancedOn);
    if (m_apiProviderCombo)
        m_apiProviderCombo->setEnabled(!advancedOn);
    if (m_baseUrlEdit)
        m_baseUrlEdit->setEnabled(!advancedOn);
    if (m_endpointPathEdit)
        m_endpointPathEdit->setEnabled(!advancedOn);
    if (m_modelNameEdit)
        m_modelNameEdit->setEnabled(!advancedOn);
    if (m_promptEdit)
        m_promptEdit->setEnabled(!advancedOn);
    if (m_testConnectionBtn)
        m_testConnectionBtn->setEnabled(!advancedOn);
    if (m_pickAdvancedJsonFieldsBtn)
        m_pickAdvancedJsonFieldsBtn->setEnabled(advancedOn && m_hasLastAdvancedApiTestJson);

    updateAdvancedTemplateStatusLabel();
}

void ConfigDialog::updateAdvancedTemplateStatusLabel()
{
    if (!m_advancedTemplateStatusLabel)
        return;

    if (m_advancedTemplateDetached)
    {
        m_advancedTemplateStatusLabel->setText("已独立");
        m_advancedTemplateStatusLabel->setStyleSheet("color: #f1c40f;");
    }
    else
    {
        m_advancedTemplateStatusLabel->setText("跟随常规");
        m_advancedTemplateStatusLabel->setStyleSheet("color: #7fd38a;");
    }
}

void ConfigDialog::resetAdvancedApiToDefault()
{
    if (!m_enableAdvancedApiCheck || !m_advancedApiTemplateEdit)
        return;

    m_isSyncingAdvanced = true;
    m_enableAdvancedApiCheck->setChecked(false);
    m_advancedTemplateDetached = false;
    const QString provider = m_apiProviderCombo ? m_apiProviderCombo->currentData().toString() : QString("openai");
    m_advancedApiTemplateEdit->setPlainText(buildAdvancedTemplateFromRegular(provider));
    m_isSyncingAdvanced = false;

    updateAdvancedApiUiState();
}

void ConfigDialog::onTestConnection()
{
    if (m_enableAdvancedApiCheck && m_enableAdvancedApiCheck->isChecked())
    {
        onTestAdvancedApi();
        return;
    }

    TranslationManager &tm = TranslationManager::instance();

    if (!m_testNam)
        m_testNam = new QNetworkAccessManager(this);

    if (m_testReply)
    {
        m_testReply->abort();
        m_testReply->deleteLater();
        m_testReply = nullptr;
    }

    const QString baseUrl = m_baseUrlEdit ? m_baseUrlEdit->text().trimmed() : QString();
    const QString endpointPath = m_endpointPathEdit ? m_endpointPathEdit->text().trimmed() : QString();
    const QString apiKey = m_apiKeyEdit ? m_apiKeyEdit->text().trimmed() : QString();
    const bool useProxy = m_useProxyCheck ? m_useProxyCheck->isChecked() : false;
    const QString proxyUrl = m_proxyUrlEdit ? m_proxyUrlEdit->text().trimmed() : QString();
    const QString providerStr = m_apiProviderCombo ? m_apiProviderCombo->currentData().toString().trimmed().toLower() : QString("openai");

    if (baseUrl.isEmpty())
    {
        QMessageBox::warning(this, tm.tr("test_title"), tm.tr("test_err_baseurl_empty"));
        return;
    }

    const QUrl testUrl = joinBaseAndEndpointUi(baseUrl, endpointPath);
    if (!testUrl.isValid() || testUrl.scheme().isEmpty() || testUrl.host().isEmpty())
    {
        QMessageBox::warning(this, tm.tr("test_title"), tm.tr("test_err_baseurl_invalid"));
        return;
    }

    if (m_testConnectionBtn)
        m_testConnectionBtn->setEnabled(false);

    if (useProxy && !proxyUrl.isEmpty())
    {
        QNetworkProxy proxy;
        QString proxyErr;
        if (!tryBuildProxyFromUrl(proxyUrl, proxy, proxyErr))
        {
            if (m_testConnectionBtn)
                m_testConnectionBtn->setEnabled(true);
            QMessageBox::warning(this, tm.tr("test_title"), tm.tr("test_err_proxy_invalid"));
            return;
        }

        QTcpSocket sock;
        sock.connectToHost(proxy.hostName(), proxy.port());
        if (!sock.waitForConnected(2500))
        {
            if (m_testConnectionBtn)
                m_testConnectionBtn->setEnabled(true);
            QMessageBox::warning(this, tm.tr("test_title"), tm.tr("test_err_proxy_unreachable").arg(sock.errorString()));
            return;
        }
        sock.disconnectFromHost();

        m_testNam->setProxy(proxy);
    }
    else
    {
        m_testNam->setProxy(QNetworkProxy::DefaultProxy);
    }

    QNetworkRequest req(testUrl);
    req.setRawHeader("Accept", "application/json");
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");

    QByteArray payload;
    if (providerStr == "openai")
    {
        const QString model = m_modelNameEdit ? m_modelNameEdit->text().trimmed() : QString();
        if (model.isEmpty())
        {
            if (m_testConnectionBtn)
                m_testConnectionBtn->setEnabled(true);
            QMessageBox::warning(this, tm.tr("test_title"), tm.tr("test_err_model_empty"));
            return;
        }

        if (!apiKey.isEmpty())
            req.setRawHeader("Authorization", QByteArray("Bearer ") + apiKey.toUtf8());

        QJsonObject msg;
        msg["role"] = "user";
        msg["content"] = "ping";
        QJsonArray msgs;
        msgs.append(msg);
        QJsonObject root;
        root["model"] = model;
        root["messages"] = msgs;
        root["max_tokens"] = 1;
        payload = QJsonDocument(root).toJson(QJsonDocument::Compact);

        m_testReply = m_testNam->post(req, payload);
    }
    else
    {
        m_testReply = m_testNam->get(req);
    }

    QTimer *timeoutTimer = new QTimer(this);
    timeoutTimer->setSingleShot(true);
    timeoutTimer->setInterval(8000);
    connect(timeoutTimer, &QTimer::timeout, this, [this]()
            {
        if (m_testReply)
            m_testReply->abort(); });
    timeoutTimer->start();

    connect(m_testReply, &QNetworkReply::finished, this, [this, timeoutTimer]()
            {
        TranslationManager &tm = TranslationManager::instance();
        timeoutTimer->stop();
        timeoutTimer->deleteLater();

        QPointer<QNetworkReply> reply = m_testReply;
        m_testReply = nullptr;

        if (m_testConnectionBtn)
            m_testConnectionBtn->setEnabled(true);

        if (!reply)
            return;

        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = reply->readAll();

        const bool okHttp = (status >= 200 && status < 300) || status == 401 || status == 403 || status == 405;
        if ((reply->error() == QNetworkReply::NoError || reply->error() == QNetworkReply::ContentOperationNotPermittedError) && okHttp)
        {
            QMessageBox::information(this, tm.tr("test_title"), tm.tr("test_ok").arg(reply->url().toString()));
        }
        else if (status == 401 || status == 403)
        {
            QMessageBox::warning(this, tm.tr("test_title"), tm.tr("test_auth_failed").arg(status));
        }
        else
        {
            const QString err = reply->errorString();
            const QString bodyPreview = QString::fromUtf8(body.left(800));
            QMessageBox::warning(this, tm.tr("test_title"), tm.tr("test_failed").arg(status).arg(err).arg(bodyPreview));
        }

        reply->deleteLater(); });
}

void ConfigDialog::onTestAdvancedApi()
{
    if (!m_testNam)
        m_testNam = new QNetworkAccessManager(this);

    if (m_testReply)
    {
        m_testReply->abort();
        m_testReply->deleteLater();
        m_testReply = nullptr;
    }

    if (m_advancedApiResultEdit)
        m_advancedApiResultEdit->clear();
    m_lastAdvancedApiTestJson = QJsonDocument();
    m_hasLastAdvancedApiTestJson = false;
    if (m_pickAdvancedJsonFieldsBtn)
        m_pickAdvancedJsonFieldsBtn->setEnabled(false);

    QJsonObject root;
    QString parseErr;
    if (!parseAdvancedTemplateJson(root, parseErr))
    {
        if (m_advancedApiResultEdit)
            m_advancedApiResultEdit->setPlainText(parseErr);
        return;
    }

    const QString baseUrl = root.value("base_url").toString().trimmed();
    const QString endpoint = root.value("endpoint").toString().trimmed();
    const QString apiKey = root.value("api_key").toString();
    const QString model = root.value("model").toString().trimmed();
    const QString prompt = root.value("prompt").toString("ping");
    const bool useProxy = root.contains("use_proxy") ? root.value("use_proxy").toBool(false)
                                                     : (m_useProxyCheck ? m_useProxyCheck->isChecked() : false);
    const QString proxyUrl = root.contains("proxy") ? root.value("proxy").toString().trimmed()
                                                    : (m_proxyUrlEdit ? m_proxyUrlEdit->text().trimmed() : QString());

    const QUrl testUrl = joinBaseAndEndpointUi(baseUrl, endpoint);
    if (baseUrl.isEmpty() || !testUrl.isValid() || testUrl.scheme().isEmpty() || testUrl.host().isEmpty())
    {
        if (m_advancedApiResultEdit)
            m_advancedApiResultEdit->setPlainText("base_url 或 endpoint 无效。");
        return;
    }

    QNetworkRequest req(testUrl);
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Accept", "application/json");
    req.setRawHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");

    const QString provider = normalizeProviderForAdvancedTemplate(root.value("provider").toString());

    if (useProxy && !proxyUrl.isEmpty())
    {
        QNetworkProxy proxy;
        QString proxyErr;
        if (!tryBuildProxyFromUrl(proxyUrl, proxy, proxyErr))
        {
            if (m_advancedApiResultEdit)
                m_advancedApiResultEdit->setPlainText(QString("代理配置无效：%1").arg(proxyErr));
            return;
        }

        QTcpSocket sock;
        sock.connectToHost(proxy.hostName(), proxy.port());
        if (!sock.waitForConnected(2500))
        {
            if (m_advancedApiResultEdit)
                m_advancedApiResultEdit->setPlainText(QString("代理不可达：%1").arg(sock.errorString()));
            return;
        }
        sock.disconnectFromHost();

        m_testNam->setProxy(proxy);
    }
    else
    {
        m_testNam->setProxy(QNetworkProxy::DefaultProxy);
    }

    const QHash<QString, QString> tokens = {
        {"api_key", apiKey},
        {"model", model},
        {"prompt", prompt},
        {"temperature", QString::number(root.value("temperature").toDouble(0.2), 'g', 6)},
        {"top_p", QString::number(root.value("top_p").toDouble(1.0), 'g', 6)},
        {"max_tokens", QString::number(root.value("max_tokens").toInt(1024))},
        {"base64_image", loadAdvancedApiTestImageBase64()},
    };

    if (provider == "openai" && !apiKey.isEmpty())
        req.setRawHeader("Authorization", QByteArray("Bearer ") + apiKey.toUtf8());
    if (provider == "claude" && !apiKey.isEmpty())
    {
        req.setRawHeader("x-api-key", apiKey.toUtf8());
        req.setRawHeader("anthropic-version", "2023-06-01");
    }

    if (root.contains("headers") && root.value("headers").isObject())
    {
        const QJsonObject headers = root.value("headers").toObject();
        for (auto it = headers.constBegin(); it != headers.constEnd(); ++it)
        {
            QString headerValue = it.value().toString();
            for (auto tk = tokens.constBegin(); tk != tokens.constEnd(); ++tk)
                headerValue.replace(QString("{{%1}}").arg(tk.key()), tk.value());
            req.setRawHeader(it.key().toUtf8(), headerValue.toUtf8());
        }
    }

    // OpenAI-compatible endpoints usually require Bearer auth even for non-openai provider labels.
    const QString epLower = endpoint.toLower();
    const QString baseLower = baseUrl.toLower();
    const bool looksOpenAICompatible = epLower.contains("chat/completions") || baseLower.contains("compatible-mode");
    if (looksOpenAICompatible && !apiKey.isEmpty() && !req.hasRawHeader("Authorization"))
        req.setRawHeader("Authorization", QByteArray("Bearer ") + apiKey.toUtf8());

    QJsonObject body;
    if (root.contains("request_body") && root.value("request_body").isObject())
    {
        body = substituteTemplateTokens(root.value("request_body"), tokens).toObject();
    }
    else
    {
        body["model"] = model;
        body["messages"] = QJsonArray{QJsonObject{{"role", "user"}, {"content", "ping"}}};
        body["max_tokens"] = 1;
    }

    if (m_testAdvancedApiBtn)
        m_testAdvancedApiBtn->setEnabled(false);

    m_testReply = m_testNam->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));

    QTimer *timeoutTimer = new QTimer(this);
    timeoutTimer->setSingleShot(true);
    timeoutTimer->setInterval(8000);
    connect(timeoutTimer, &QTimer::timeout, this, [this]()
            {
        if (m_testReply)
            m_testReply->abort(); });
    timeoutTimer->start();

    connect(m_testReply, &QNetworkReply::finished, this, [this, timeoutTimer]()
            {
        timeoutTimer->stop();
        timeoutTimer->deleteLater();

        QPointer<QNetworkReply> reply = m_testReply;
        m_testReply = nullptr;
        if (m_testAdvancedApiBtn)
            m_testAdvancedApiBtn->setEnabled(true);
        if (!reply)
            return;

        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray body = reply->readAll();
        QString output = QString("HTTP %1").arg(status);
        if (status == 0 || reply->error() != QNetworkReply::NoError)
        {
            output += QString("\nNetwork Error (%1): %2")
                          .arg(static_cast<int>(reply->error()))
                          .arg(reply->errorString());
        }
        if (!body.isEmpty())
            output += QString("\n%1").arg(QString::fromUtf8(body));
        if (m_advancedApiResultEdit)
            m_advancedApiResultEdit->setPlainText(output);

        m_lastAdvancedApiTestJson = QJsonDocument();
        m_hasLastAdvancedApiTestJson = false;
        if (status >= 200 && status < 300)
        {
            QJsonParseError jerr;
            const QJsonDocument doc = QJsonDocument::fromJson(body, &jerr);
            if (!doc.isNull() && (doc.isObject() || doc.isArray()) && jerr.error == QJsonParseError::NoError)
            {
                m_lastAdvancedApiTestJson = doc;
                m_hasLastAdvancedApiTestJson = true;
            }
        }
        if (m_pickAdvancedJsonFieldsBtn)
            m_pickAdvancedJsonFieldsBtn->setEnabled((m_enableAdvancedApiCheck && m_enableAdvancedApiCheck->isChecked()) && m_hasLastAdvancedApiTestJson);
        reply->deleteLater(); });
}

void ConfigDialog::onPickAdvancedJsonFields()
{
    TranslationManager &tm = TranslationManager::instance();

    if (!m_hasLastAdvancedApiTestJson || m_lastAdvancedApiTestJson.isNull())
    {
        if (m_advancedApiResultEdit)
            m_advancedApiResultEdit->appendPlainText(QString("\n%1").arg(tm.tr("adv_json_no_test_response")));
        return;
    }

    QJsonObject templateRoot;
    QString parseErr;
    if (!parseAdvancedTemplateJson(templateRoot, parseErr))
    {
        if (m_advancedApiResultEdit)
            m_advancedApiResultEdit->appendPlainText(QString("\n%1").arg(tm.tr("adv_json_template_parse_failed").arg(parseErr)));
        return;
    }

    QStringList currentFields = readDebugFieldsFromTemplateRoot(templateRoot);
    QStringList candidatePaths;
    QHash<QString, QString> previews;
    if (m_lastAdvancedApiTestJson.isObject())
        collectJsonLeafPaths(m_lastAdvancedApiTestJson.object(), QString(), candidatePaths, previews);
    else if (m_lastAdvancedApiTestJson.isArray())
        collectJsonLeafPaths(m_lastAdvancedApiTestJson.array(), QString(), candidatePaths, previews);

    candidatePaths.removeDuplicates();
    if (candidatePaths.isEmpty())
    {
        if (m_advancedApiResultEdit)
            m_advancedApiResultEdit->appendPlainText(QString("\n%1").arg(tm.tr("adv_json_no_selectable_fields")));
        return;
    }

    QDialog picker(this);
    picker.setWindowTitle(tm.tr("adv_json_picker_title"));
    picker.resize(760, 520);

    auto *layout = new QVBoxLayout(&picker);
    auto *hint = new QLabel(tm.tr("adv_json_picker_hint"), &picker);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto *list = new QListWidget(&picker);
    list->setSelectionMode(QAbstractItemView::MultiSelection);
    QSet<QString> currentSet;
    for (const QString &field : currentFields)
        currentSet.insert(field);

    for (const QString &path : candidatePaths)
    {
        const QString label = QString("%1 = %2").arg(path, previews.value(path));
        auto *item = new QListWidgetItem(label, list);
        item->setData(Qt::UserRole, path);
        if (currentSet.contains(path))
            item->setSelected(true);
    }
    layout->addWidget(list, 1);

    auto *quickRow = new QHBoxLayout();
    auto *selectAllBtn = new QPushButton(tm.tr("btn_select_all"), &picker);
    auto *clearBtn = new QPushButton(tm.tr("btn_clear"), &picker);
    quickRow->addWidget(selectAllBtn, 0);
    quickRow->addWidget(clearBtn, 0);
    quickRow->addStretch(1);
    layout->addLayout(quickRow);
    connect(selectAllBtn, &QPushButton::clicked, list, [list]()
            { list->selectAll(); });
    connect(clearBtn, &QPushButton::clicked, list, [list]()
            { list->clearSelection(); });

    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &picker);
    if (QPushButton *okBtn = box->button(QDialogButtonBox::Ok))
        okBtn->setText(tm.tr("ok"));
    if (QPushButton *cancelBtn = box->button(QDialogButtonBox::Cancel))
        cancelBtn->setText(tm.tr("cancel"));
    connect(box, &QDialogButtonBox::accepted, &picker, &QDialog::accept);
    connect(box, &QDialogButtonBox::rejected, &picker, &QDialog::reject);
    layout->addWidget(box);

    if (picker.exec() != QDialog::Accepted)
        return;

    QStringList selectedFields;
    const QList<QListWidgetItem *> pickedItems = list->selectedItems();
    for (QListWidgetItem *item : pickedItems)
    {
        const QString path = item->data(Qt::UserRole).toString().trimmed();
        if (!path.isEmpty())
            selectedFields.append(path);
    }
    selectedFields.removeDuplicates();

    QJsonArray debugFields;
    for (const QString &f : selectedFields)
        debugFields.append(f);
    templateRoot["debug_fields"] = debugFields;

    const QString templateText = QString::fromUtf8(QJsonDocument(templateRoot).toJson(QJsonDocument::Indented));
    if (m_advancedApiTemplateEdit)
    {
        QSignalBlocker blocker(m_advancedApiTemplateEdit);
        m_isSyncingAdvanced = true;
        m_advancedApiTemplateEdit->setPlainText(templateText);
        m_isSyncingAdvanced = false;
    }

    m_advancedTemplateDetached = true;
    updateAdvancedTemplateStatusLabel();

    if (m_advancedApiResultEdit)
        m_advancedApiResultEdit->appendPlainText(tm.tr("adv_json_selected_count").arg(selectedFields.size()));
}

ConfigDialog::~ConfigDialog() = default;

void ConfigDialog::setGlobalHotkeyConflictKeys(const QStringList &labelKeys, bool focusConflicts)
{
    m_globalHotkeyConflictKeys = labelKeys;
    m_globalHotkeyConflictKeys.removeDuplicates();
    applyGlobalHotkeyConflictIndicators(focusConflicts);
}

QLineEdit *ConfigDialog::globalHotkeyEditForKey(const QString &labelKey) const
{
    if (labelKey == QStringLiteral("lbl_shot_hotkey"))
        return m_hotkeyEdit;
    if (labelKey == QStringLiteral("lbl_sum_hotkey"))
        return m_summaryHotkeyEdit;
    if (labelKey == QStringLiteral("lbl_set_hotkey"))
        return m_settingsHotkeyEdit;
    if (labelKey == QStringLiteral("lbl_quit_hotkey"))
        return m_quitHotkeyEdit;
    return nullptr;
}

void ConfigDialog::applyGlobalHotkeyConflictIndicators(bool focusConflicts)
{
    const QStringList trackedKeys = {
        QStringLiteral("lbl_shot_hotkey"),
        QStringLiteral("lbl_sum_hotkey"),
        QStringLiteral("lbl_set_hotkey"),
        QStringLiteral("lbl_quit_hotkey"),
    };

    for (const QString &key : trackedKeys)
    {
        if (QLineEdit *edit = globalHotkeyEditForKey(key))
        {
            const bool conflicted = m_globalHotkeyConflictKeys.contains(key);
            edit->setStyleSheet(conflicted ? QStringLiteral("border: 2px solid #d94b4b; border-radius: 4px;")
                                           : QString());
        }
    }

    const int otherTabIndex = m_tabWidget ? m_tabWidget->indexOf(m_otherTab) : -1;
    if (otherTabIndex >= 0)
    {
        QString baseText = TranslationManager::instance().tr("tab_other");
        if (!m_globalHotkeyConflictKeys.isEmpty())
            baseText += QStringLiteral(" *");
        m_tabWidget->setTabText(otherTabIndex, baseText);
    }

    if (!focusConflicts || m_globalHotkeyConflictKeys.isEmpty() || !m_tabWidget)
        return;

    m_tabWidget->setCurrentWidget(m_otherTab);

    QLineEdit *target = nullptr;
    for (const QString &key : trackedKeys)
    {
        if (m_globalHotkeyConflictKeys.contains(key))
        {
            target = globalHotkeyEditForKey(key);
            if (target)
                break;
        }
    }

    if (!target)
        return;

    QPointer<QLineEdit> targetEdit = target;
    QTimer::singleShot(0, this, [targetEdit]()
                       {
        if (!targetEdit)
            return;
        targetEdit->setFocus(Qt::OtherFocusReason);
        targetEdit->selectAll(); });
}
