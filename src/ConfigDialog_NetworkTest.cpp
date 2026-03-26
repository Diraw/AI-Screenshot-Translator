#include "ConfigDialog.h"

#include "ConfigDialog_NetworkTestUtils.h"
#include "TranslationManager.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QTimer>
#include <QUrl>

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

    const QUrl testUrl = ConfigDialogNetworkTestUtils::joinBaseAndEndpointUi(baseUrl, endpointPath);
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
        if (!ConfigDialogNetworkTestUtils::tryBuildProxyFromUrl(proxyUrl, proxy, proxyErr))
        {
            if (m_testConnectionBtn)
                m_testConnectionBtn->setEnabled(true);
            QMessageBox::warning(this, tm.tr("test_title"), tm.tr("test_err_proxy_invalid"));
            return;
        }

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

        QNetworkReply *issuedReply = m_testNam->post(req, payload);
        m_testReply = issuedReply;
        QPointer<QNetworkReply> activeReply = issuedReply;

        QTimer *timeoutTimer = new QTimer(this);
        timeoutTimer->setSingleShot(true);
        timeoutTimer->setInterval(8000);
        connect(timeoutTimer, &QTimer::timeout, this, [activeReply]()
                {
                    if (activeReply && activeReply->isRunning())
                        activeReply->abort();
                });
        timeoutTimer->start();

        connect(issuedReply, &QNetworkReply::finished, this, [this, timeoutTimer, activeReply]()
                {
                    TranslationManager &tm = TranslationManager::instance();
                    timeoutTimer->stop();
                    timeoutTimer->deleteLater();

                    if (!activeReply)
                        return;

                    const bool isLatestReply = (m_testReply == activeReply);
                    if (isLatestReply)
                        m_testReply = nullptr;
                    if (!isLatestReply)
                    {
                        activeReply->deleteLater();
                        return;
                    }

                    if (m_testConnectionBtn)
                        m_testConnectionBtn->setEnabled(true);

                    const int status = activeReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                    const QByteArray body = activeReply->readAll();

                    const bool okHttp = (status >= 200 && status < 300) || status == 401 || status == 403 || status == 405;
                    if ((activeReply->error() == QNetworkReply::NoError || activeReply->error() == QNetworkReply::ContentOperationNotPermittedError) && okHttp)
                    {
                        QMessageBox::information(this, tm.tr("test_title"), tm.tr("test_ok").arg(activeReply->url().toString()));
                    }
                    else if (status == 401 || status == 403)
                    {
                        QMessageBox::warning(this, tm.tr("test_title"), tm.tr("test_auth_failed").arg(status));
                    }
                    else
                    {
                        const QNetworkReply::NetworkError netErr = activeReply->error();
                        const QString detailedError = ConfigDialogNetworkTestUtils::buildDetailedNetworkError(netErr, activeReply->errorString(), status, activeReply->url());
                        QMessageBox::warning(this, tm.tr("test_title"), detailedError);
                    }

                    activeReply->deleteLater();
                });
        return;
    }

    QNetworkReply *issuedReply = m_testNam->get(req);
    m_testReply = issuedReply;
    QPointer<QNetworkReply> activeReply = issuedReply;

    QTimer *timeoutTimer = new QTimer(this);
    timeoutTimer->setSingleShot(true);
    timeoutTimer->setInterval(8000);
    connect(timeoutTimer, &QTimer::timeout, this, [activeReply]()
            {
                if (activeReply && activeReply->isRunning())
                    activeReply->abort();
            });
    timeoutTimer->start();

    connect(issuedReply, &QNetworkReply::finished, this, [this, timeoutTimer, activeReply]()
            {
                TranslationManager &tm = TranslationManager::instance();
                timeoutTimer->stop();
                timeoutTimer->deleteLater();

                if (!activeReply)
                    return;

                const bool isLatestReply = (m_testReply == activeReply);
                if (isLatestReply)
                    m_testReply = nullptr;
                if (!isLatestReply)
                {
                    activeReply->deleteLater();
                    return;
                }

                if (m_testConnectionBtn)
                    m_testConnectionBtn->setEnabled(true);

                const int status = activeReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                const QByteArray body = activeReply->readAll();

                const bool okHttp = (status >= 200 && status < 300) || status == 401 || status == 403 || status == 405;
                if ((activeReply->error() == QNetworkReply::NoError || activeReply->error() == QNetworkReply::ContentOperationNotPermittedError) && okHttp)
                {
                    QMessageBox::information(this, tm.tr("test_title"), tm.tr("test_ok").arg(activeReply->url().toString()));
                }
                else if (status == 401 || status == 403)
                {
                    QMessageBox::warning(this, tm.tr("test_title"), tm.tr("test_auth_failed").arg(status));
                }
                else
                {
                    const QNetworkReply::NetworkError netErr = activeReply->error();
                    const QString detailedError = ConfigDialogNetworkTestUtils::buildDetailedNetworkError(netErr, activeReply->errorString(), status, activeReply->url());
                    QMessageBox::warning(this, tm.tr("test_title"), detailedError);
                }

                activeReply->deleteLater();
            });
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
    updateAdvancedJsonFieldsButtonState();

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

    const QUrl testUrl = ConfigDialogNetworkTestUtils::joinBaseAndEndpointUi(baseUrl, endpoint);
    if (baseUrl.isEmpty() || !testUrl.isValid() || testUrl.scheme().isEmpty() || testUrl.host().isEmpty())
    {
        if (m_advancedApiResultEdit)
            m_advancedApiResultEdit->setPlainText("base_url æˆ– endpoint æ— æ•ˆã€‚");
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
        if (!ConfigDialogNetworkTestUtils::tryBuildProxyFromUrl(proxyUrl, proxy, proxyErr))
        {
            if (m_advancedApiResultEdit)
                m_advancedApiResultEdit->setPlainText(QString("ä»£ç†é…ç½®æ— æ•ˆï¼š%1").arg(proxyErr));
            return;
        }

        m_testNam->setProxy(proxy);
    }
    else
    {
        m_testNam->setProxy(QNetworkProxy::DefaultProxy);
    }

    const QString testImageBase64 = ConfigDialogNetworkTestUtils::loadAdvancedApiTestImageBase64();
    const QHash<QString, QString> tokens = {
        {"api_key", apiKey},
        {"model", model},
        {"prompt", prompt},
        {"temperature", QString::number(root.value("temperature").toDouble(0.2), 'g', 6)},
        {"top_p", QString::number(root.value("top_p").toDouble(1.0), 'g', 6)},
        {"max_tokens", QString::number(root.value("max_tokens").toInt(1024))},
        {"base64_image", testImageBase64},
        {"gemini_parts", QString::fromUtf8(QJsonDocument(QJsonArray{
                              QJsonObject{{"inline_data", QJsonObject{{"mime_type", "image/png"}, {"data", testImageBase64}}}},
                              QJsonObject{{"text", prompt}}}).toJson(QJsonDocument::Compact))},
        {"claude_user_content", QString::fromUtf8(QJsonDocument(QJsonArray{
                                       QJsonObject{{"type", "image"},
                                                   {"source", QJsonObject{{"type", "base64"},
                                                                          {"media_type", "image/png"},
                                                                          {"data", testImageBase64}}}},
                                       QJsonObject{{"type", "text"}, {"text", prompt}}}).toJson(QJsonDocument::Compact))},
        {"openai_user_content", QString::fromUtf8(QJsonDocument(QJsonArray{
                                        QJsonObject{{"type", "image_url"},
                                                    {"image_url", QJsonObject{{"url", QString("data:image/png;base64,%1").arg(testImageBase64)}}}},
                                        QJsonObject{{"type", "text"}, {"text", prompt}}}).toJson(QJsonDocument::Compact))},
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

    const QString epLower = endpoint.toLower();
    const QString baseLower = baseUrl.toLower();
    const bool looksOpenAICompatible = epLower.contains("chat/completions") || baseLower.contains("compatible-mode");
    if (looksOpenAICompatible && !apiKey.isEmpty() && !req.hasRawHeader("Authorization"))
        req.setRawHeader("Authorization", QByteArray("Bearer ") + apiKey.toUtf8());

    QJsonObject body;
    if (root.contains("request_body") && root.value("request_body").isObject())
    {
        body = ConfigDialogNetworkTestUtils::substituteTemplateTokens(root.value("request_body"), tokens).toObject();
    }
    else
    {
        body["model"] = model;
        body["messages"] = QJsonArray{QJsonObject{{"role", "user"}, {"content", "ping"}}};
        body["max_tokens"] = 1;
    }

    if (m_testAdvancedApiBtn)
        m_testAdvancedApiBtn->setEnabled(false);

    QNetworkReply *issuedReply = m_testNam->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    m_testReply = issuedReply;
    QPointer<QNetworkReply> activeReply = issuedReply;

    QTimer *timeoutTimer = new QTimer(this);
    timeoutTimer->setSingleShot(true);
    timeoutTimer->setInterval(8000);
    connect(timeoutTimer, &QTimer::timeout, this, [activeReply]()
            {
                if (activeReply && activeReply->isRunning())
                    activeReply->abort();
            });
    timeoutTimer->start();

    connect(issuedReply, &QNetworkReply::finished, this, [this, timeoutTimer, activeReply]()
            {
                timeoutTimer->stop();
                timeoutTimer->deleteLater();

                if (!activeReply)
                    return;

                const bool isLatestReply = (m_testReply == activeReply);
                if (isLatestReply)
                    m_testReply = nullptr;
                if (!isLatestReply)
                {
                    activeReply->deleteLater();
                    return;
                }

                if (m_testAdvancedApiBtn)
                    m_testAdvancedApiBtn->setEnabled(true);

                const int status = activeReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                const QByteArray body = activeReply->readAll();
                QString output;
                if (status == 0 || activeReply->error() != QNetworkReply::NoError)
                {
                    output = ConfigDialogNetworkTestUtils::buildDetailedNetworkError(activeReply->error(), activeReply->errorString(), status, activeReply->url());
                }
                else
                {
                    output = QString("HTTP %1").arg(status);
                }
                if (!body.isEmpty())
                    output += QString("\n\nå“åº”å†…å®¹ï¼š\n%1").arg(QString::fromUtf8(body));
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
                updateAdvancedJsonFieldsButtonState();
                activeReply->deleteLater();
            });
}

