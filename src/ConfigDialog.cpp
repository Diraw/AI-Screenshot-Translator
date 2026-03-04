#include "ConfigDialog.h"

#include "TranslationManager.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
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

QString ConfigDialog::defaultEndpointForProvider(const QString &provider) const
{
    const QString p = provider.trimmed().toLower();
    if (p == "gemini")
        return "/v1beta";
    if (p == "claude")
        return "/v1/messages";
    return "/chat/completions";
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

void ConfigDialog::onTestConnection()
{
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
