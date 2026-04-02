#include "ConfigDialog.h"

#include "TranslationManager.h"

#include <QDateTime>
#include <QEvent>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QHash>
#include <QMessageBox>
#include <QSet>
#include <QSignalBlocker>
#include <QVariant>


QString ConfigDialog::defaultEndpointForProvider(const QString &provider) const
{
    const QString p = provider.trimmed().toLower();
    if (p == "gemini")
        return "/v1beta";
    if (p == "claude")
        return "/v1/messages";
    if (p == "aihubmix")
        return "/v1/chat/completions"; // AIHubMix uses OpenAI-compatible endpoint for all models

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
    root["timeout_ms"] = 30000;

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
        QJsonObject content;
        content["parts"] = "{{gemini_parts}}";

        QJsonArray contents;
        contents.append(content);
        requestBody["contents"] = contents;
    }
    else if (normalizedProvider == "claude")
    {
        QJsonObject msg;
        msg["role"] = "user";
        msg["content"] = "{{claude_user_content}}";
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

        QJsonObject userMsg;
        userMsg["role"] = "user";
        userMsg["content"] = "{{openai_user_content}}";

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

QList<QWidget *> ConfigDialog::regularApiControlWidgets() const
{
    QList<QWidget *> widgets;
    widgets << m_apiKeyEdit
            << m_apiProviderCombo
            << m_baseUrlEdit
            << m_endpointPathEdit
            << m_modelNameEdit
            << m_promptEdit
            << m_testConnectionBtn;
    return widgets;
}

QList<QLabel *> ConfigDialog::regularApiLabelWidgets() const
{
    QList<QLabel *> labels;
    if (!m_generalFormLayout)
        return labels;

    const int labelRows[] = {2, 3, 4, 5, 6};
    for (const int row : labelRows)
    {
        QLayoutItem *item = m_generalFormLayout->itemAt(row, QFormLayout::LabelRole);
        if (!item)
            continue;
        if (QLabel *label = qobject_cast<QLabel *>(item->widget()))
            labels << label;
    }
    return labels;
}

void ConfigDialog::applyRegularApiTextColor(QWidget *widget, bool advancedOn)
{
    if (!widget)
        return;

    if (!m_regularApiOriginalPalettes.contains(widget))
        m_regularApiOriginalPalettes.insert(widget, widget->palette());

    if (!advancedOn)
    {
        widget->setPalette(m_regularApiOriginalPalettes.value(widget));
        return;
    }

    const QColor grayText = m_isDarkTheme ? QColor(165, 165, 165) : QColor(135, 135, 135);
    const QColor grayPlaceholder = m_isDarkTheme ? QColor(125, 125, 125) : QColor(165, 165, 165);

    QPalette pal = m_regularApiOriginalPalettes.value(widget);
    if (qobject_cast<QLabel *>(widget))
    {
        pal.setColor(QPalette::WindowText, grayText);
    }
    else if (qobject_cast<QLineEdit *>(widget))
    {
        pal.setColor(QPalette::Text, grayText);
        pal.setColor(QPalette::PlaceholderText, grayPlaceholder);
    }
    else if (qobject_cast<QTextEdit *>(widget))
    {
        pal.setColor(QPalette::Text, grayText);
        pal.setColor(QPalette::PlaceholderText, grayPlaceholder);
    }
    else if (qobject_cast<QComboBox *>(widget))
    {
        pal.setColor(QPalette::Text, grayText);
        pal.setColor(QPalette::ButtonText, grayText);
        pal.setColor(QPalette::WindowText, grayText);
    }
    else if (qobject_cast<QPushButton *>(widget))
    {
        pal.setColor(QPalette::ButtonText, grayText);
    }

    widget->setPalette(pal);
}

void ConfigDialog::updateRegularApiTextGrayState(bool advancedOn)
{
    const QList<QWidget *> controls = regularApiControlWidgets();
    for (QWidget *widget : controls)
        applyRegularApiTextColor(widget, advancedOn);

    const QList<QLabel *> labels = regularApiLabelWidgets();
    for (QLabel *label : labels)
        applyRegularApiTextColor(label, advancedOn);
}

void ConfigDialog::recordRegularApiClickAndMaybeWarn(QObject *clickedObject)
{
    if (!clickedObject)
        return;

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    constexpr qint64 kDoubleClickWindowMs = 650;
    const bool isSecondClick = (m_lastRegularApiClickObject == clickedObject) &&
                               (nowMs - m_lastRegularApiClickMs <= kDoubleClickWindowMs);

    m_lastRegularApiClickObject = clickedObject;
    m_lastRegularApiClickMs = nowMs;

    if (isSecondClick)
    {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("常规模式和高级模式只能二选一"));
        m_lastRegularApiClickObject = nullptr;
        m_lastRegularApiClickMs = 0;
    }
}

void ConfigDialog::ensureRegularApiInteractionHooks()
{
    const QList<QWidget *> controls = regularApiControlWidgets();
    for (QWidget *widget : controls)
    {
        if (!widget)
            continue;

        if (!widget->property("regularApiControl").toBool())
            widget->setProperty("regularApiControl", true);

        if (!widget->property("regularApiFilterInstalled").toBool())
        {
            widget->installEventFilter(this);
            widget->setProperty("regularApiFilterInstalled", true);
        }

        if (QTextEdit *textEdit = qobject_cast<QTextEdit *>(widget))
        {
            QWidget *viewport = textEdit->viewport();
            if (viewport && !viewport->property("regularApiFilterInstalled").toBool())
            {
                viewport->setProperty("regularApiControlRoot", QVariant::fromValue(static_cast<QObject *>(widget)));
                viewport->installEventFilter(this);
                viewport->setProperty("regularApiFilterInstalled", true);
            }
        }
    }
}

bool ConfigDialog::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_pickAdvancedJsonFieldsBtn && event)
    {
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        constexpr qint64 kRepeatWindowMs = 1800;
        if (m_advancedJsonFieldsDoubleClickArmed &&
            nowMs - m_advancedJsonFieldsLastDblClickMs > kRepeatWindowMs)
        {
            m_advancedJsonFieldsDoubleClickArmed = false;
            m_advancedJsonFieldsLastDblClickMs = 0;
        }

        if (event->type() == QEvent::MouseButtonDblClick &&
            (!m_hasLastAdvancedApiTestJson || m_lastAdvancedApiTestJson.isNull()))
        {
            if (m_advancedJsonFieldsDoubleClickArmed)
            {
                TranslationManager &tm = TranslationManager::instance();
                const QString title = (tm.getLanguage() == QStringLiteral("zh"))
                                          ? QStringLiteral("提示")
                                          : tm.tr("test_title");
                const QString message = (tm.getLanguage() == QStringLiteral("zh"))
                                            ? QStringLiteral("请先测试 JSON 与 API 连通性")
                                            : QStringLiteral("Please test JSON and API connectivity first.");
                QMessageBox::information(this, title, message);
                m_advancedJsonFieldsDoubleClickArmed = false;
                m_advancedJsonFieldsLastDblClickMs = 0;
            }
            else
            {
                m_advancedJsonFieldsDoubleClickArmed = true;
                m_advancedJsonFieldsLastDblClickMs = nowMs;
            }
            return true;
        }

        if (event->type() == QEvent::MouseButtonPress &&
            (!m_hasLastAdvancedApiTestJson || m_lastAdvancedApiTestJson.isNull()))
        {
            return false;
        }
    }

    QObject *regularRoot = nullptr;
    if (watched && watched->property("regularApiControl").toBool())
    {
        regularRoot = watched;
    }
    else if (watched)
    {
        const QVariant rootVar = watched->property("regularApiControlRoot");
        if (rootVar.isValid())
            regularRoot = qvariant_cast<QObject *>(rootVar);
    }

    const bool advancedOn = m_enableAdvancedApiCheck && m_enableAdvancedApiCheck->isChecked();
    if (!regularRoot || !advancedOn)
        return QDialog::eventFilter(watched, event);

    const QEvent::Type type = event->type();
    const bool isClickEvent = (type == QEvent::MouseButtonPress || type == QEvent::MouseButtonDblClick);
    if (isClickEvent)
        recordRegularApiClickAndMaybeWarn(regularRoot);

    const bool blockComboInteraction = (regularRoot == m_apiProviderCombo) &&
                                       (type == QEvent::MouseButtonPress ||
                                        type == QEvent::MouseButtonRelease ||
                                        type == QEvent::MouseButtonDblClick ||
                                        type == QEvent::Wheel ||
                                        type == QEvent::KeyPress ||
                                        type == QEvent::KeyRelease);

    const bool blockTestButtonInteraction = (regularRoot == m_testConnectionBtn) &&
                                            (type == QEvent::MouseButtonPress ||
                                             type == QEvent::MouseButtonRelease ||
                                             type == QEvent::MouseButtonDblClick ||
                                             type == QEvent::KeyPress ||
                                             type == QEvent::KeyRelease);

    if (blockComboInteraction || blockTestButtonInteraction)
        return true;

    return QDialog::eventFilter(watched, event);
}

void ConfigDialog::updateAdvancedApiUiState()
{
    const bool advancedOn = m_enableAdvancedApiCheck && m_enableAdvancedApiCheck->isChecked();

    ensureAdvancedProviderOption(advancedOn);
    ensureRegularApiInteractionHooks();

    if (m_advancedApiTemplateEdit)
        m_advancedApiTemplateEdit->setReadOnly(!advancedOn);

    if (m_apiKeyEdit)
        m_apiKeyEdit->setReadOnly(advancedOn);
    if (m_baseUrlEdit)
        m_baseUrlEdit->setReadOnly(advancedOn);
    if (m_endpointPathEdit)
        m_endpointPathEdit->setReadOnly(advancedOn);
    if (m_modelNameEdit)
        m_modelNameEdit->setReadOnly(advancedOn);
    if (m_promptEdit)
        m_promptEdit->setReadOnly(advancedOn);

    updateRegularApiTextGrayState(advancedOn);

    updateAdvancedJsonFieldsButtonState();
    if (m_advancedDebugDisplayLabel)
        m_advancedDebugDisplayLabel->setEnabled(advancedOn);
    if (m_showAdvancedDebugInResultCheck)
        m_showAdvancedDebugInResultCheck->setEnabled(advancedOn);
    if (m_showAdvancedDebugInArchiveCheck)
        m_showAdvancedDebugInArchiveCheck->setEnabled(advancedOn);

    updateAdvancedTemplateStatusLabel();
}

void ConfigDialog::updateAdvancedJsonFieldsButtonState()
{
    if (!m_pickAdvancedJsonFieldsBtn)
        return;

    const bool advancedOn = m_enableAdvancedApiCheck && m_enableAdvancedApiCheck->isChecked();
    const bool buttonReady = advancedOn && m_hasLastAdvancedApiTestJson;
    if (buttonReady)
    {
        m_advancedJsonFieldsDoubleClickArmed = false;
        m_advancedJsonFieldsLastDblClickMs = 0;
    }

    m_pickAdvancedJsonFieldsBtn->setEnabled(true);
    applyRegularApiTextColor(m_pickAdvancedJsonFieldsBtn, !buttonReady);
}

void ConfigDialog::updateAdvancedTemplateStatusLabel()
{
    if (!m_advancedTemplateStatusLabel)
        return;

    TranslationManager &tm = TranslationManager::instance();

    if (m_advancedTemplateDetached)
    {
        m_advancedTemplateStatusLabel->setText(tm.tr("adv_template_status_detached"));
        m_advancedTemplateStatusLabel->setStyleSheet(
            QString("color: #f1c40f; font-weight: %1;").arg(m_isDarkTheme ? 400 : 600));
    }
    else
    {
        m_advancedTemplateStatusLabel->setText(tm.tr("adv_template_status_follow_regular"));
        m_advancedTemplateStatusLabel->setStyleSheet(
            QString("color: #7fd38a; font-weight: %1;").arg(m_isDarkTheme ? 400 : 600));
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


