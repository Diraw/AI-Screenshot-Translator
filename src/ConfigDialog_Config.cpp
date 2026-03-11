#include "ConfigDialog.h"
#include "ThemeUtils.h"

#include <QDir>
#include <QDebug>
#include <QMessageBox>
#include <QSignalBlocker>

void ConfigDialog::loadFromConfig()
{
    m_isLoadingConfig = true;
    m_lastAdvancedApiTestJson = QJsonDocument();
    m_hasLastAdvancedApiTestJson = false;
    if (m_pickAdvancedJsonFieldsBtn)
        m_pickAdvancedJsonFieldsBtn->setEnabled(false);

    AppConfig cfg = m_configManager->getConfig();
    m_apiKeyEdit->setText(cfg.apiKey);
    m_baseUrlEdit->setText(cfg.baseUrl);
    if (m_endpointPathEdit)
        m_endpointPathEdit->setText(cfg.endpointPath);
    m_modelNameEdit->setText(cfg.modelName);
    m_promptEdit->setPlainText(cfg.promptText);
    m_proxyUrlEdit->setText(cfg.proxyUrl);
    m_useProxyCheck->setChecked(cfg.useProxy);

    {
        const QSignalBlocker blocker(m_apiProviderCombo);
        QString loadProvider = cfg.apiProvider.trimmed().toLower();
        if (loadProvider != "openai" && loadProvider != "gemini" && loadProvider != "claude")
            loadProvider = "openai";
        int providerIndex = m_apiProviderCombo->findData(loadProvider);
        if (providerIndex >= 0)
            m_apiProviderCombo->setCurrentIndex(providerIndex);
    }

    if (!cfg.apiProvider.trimmed().isEmpty())
        m_lastRegularProvider = cfg.apiProvider.trimmed().toLower();

    m_advancedTemplateDetached = cfg.advancedApiCustomized;

    if (m_advancedApiTemplateEdit)
    {
        QString templateText;
        if (m_advancedTemplateDetached && !cfg.advancedApiTemplate.trimmed().isEmpty())
            templateText = cfg.advancedApiTemplate;
        else
            templateText = buildAdvancedTemplateFromRegular(m_apiProviderCombo ? m_apiProviderCombo->currentData().toString() : QString("openai"));
        m_advancedApiTemplateEdit->setPlainText(templateText);
    }

    if (m_enableAdvancedApiCheck)
    {
        const QSignalBlocker blocker(m_enableAdvancedApiCheck);
        m_enableAdvancedApiCheck->setChecked(cfg.useAdvancedApiMode);
    }

    // If the stored endpoint is exactly the provider default, remember it as auto-filled.
    if (m_endpointPathEdit)
    {
        const QString ep = m_endpointPathEdit->text().trimmed();
        const QString def = defaultEndpointForProvider(cfg.apiProvider);
        m_lastAutoEndpoint = (ep == def) ? ep : QString();
    }

    m_hotkeyEdit->setText(cfg.screenshotHotkey);
    m_summaryHotkeyEdit->setText(cfg.summaryHotkey);
    m_settingsHotkeyEdit->setText(cfg.settingsHotkey);
    m_editHotkeyEdit->setText(cfg.editHotkey);
    m_viewToggleHotkeyEdit->setText(cfg.viewToggleHotkey);
    m_screenshotToggleHotkeyEdit->setText(cfg.screenshotToggleHotkey);
    m_selectionToggleHotkeyEdit->setText(cfg.selectionToggleHotkey);
    if (m_archiveLoadModeCombo)
    {
        const int modeIndex = m_archiveLoadModeCombo->findData(cfg.archiveUsePagination);
        if (modeIndex >= 0)
            m_archiveLoadModeCombo->setCurrentIndex(modeIndex);
    }
    if (m_archivePageSizeSpin)
    {
        m_archivePageSizeSpin->setValue(cfg.archivePageSize > 0 ? cfg.archivePageSize : 50);
        m_archivePageSizeSpin->setEnabled(cfg.archiveUsePagination);
    }
    m_boldHotkeyEdit->setText(cfg.boldHotkey);
    m_underlineHotkeyEdit->setText(cfg.underlineHotkey);
    m_highlightHotkeyEdit->setText(cfg.highlightHotkey);
    m_highlightMarkColorEdit->setText(cfg.highlightMarkColor);
    m_highlightMarkColorDarkEdit->setText(cfg.highlightMarkColorDark);

    m_launchAtStartupCheck->setChecked(cfg.launchAtStartup);
    m_enableUmamiAnalyticsCheck->setChecked(cfg.enableUmamiAnalytics);
    m_debugModeCheck->setChecked(cfg.debugMode);
    m_enableQuitHotkeyCheck->setChecked(cfg.enableQuitHotkey);
    m_quitHotkeyEdit->setText(cfg.quitHotkey);
    m_quitHotkeyEdit->setEnabled(cfg.enableQuitHotkey);
    m_storagePathEdit->setText(cfg.storagePath);
    refreshStoragePathPlaceholder();
    m_showPreviewCheck->setChecked(cfg.showPreviewCard);
    m_showResultCheck->setChecked(cfg.showResultWindow);

    // Translation Tab - Screenshot Card
    m_zoomSensitivitySpin->setValue(cfg.zoomSensitivity);
    m_cardBorderColorEdit->setText(cfg.cardBorderColor);
    m_useBorderCheck->setChecked(cfg.useCardBorder);

    // Translation Tab - Window Settings
    m_initialFontSizeSpin->setValue(cfg.initialFontSize);

    m_defaultLookCheck->setChecked(cfg.defaultResultWindowLocked);
    int lockIndex = m_lockBehaviorCombo->findData(cfg.lockBehavior);
    if (lockIndex >= 0)
        m_lockBehaviorCombo->setCurrentIndex(lockIndex);

    m_prevPageHotkeyEdit->setText(cfg.prevResultShortcut);
    m_nextPageHotkeyEdit->setText(cfg.nextResultShortcut);
    m_tagHotkeyEdit->setText(cfg.tagHotkey);
    m_retranslateHotkeyEdit->setText(cfg.retranslateHotkey);

    // Language selection
    for (int i = 0; i < m_languageCombo->count(); ++i)
    {
        if (m_languageCombo->itemData(i).toString() == cfg.language)
        {
            m_languageCombo->setCurrentIndex(i);
            break;
        }
    }

    // Screen selection
    for (int i = 0; i < m_screenCombo->count(); ++i)
    {
        if (m_screenCombo->itemData(i).toInt() == cfg.targetScreenIndex)
        {
            m_screenCombo->setCurrentIndex(i);
            break;
        }
    }

    m_isLoadingConfig = false;

    syncAdvancedTemplateFromRegular();

    updateAdvancedApiUiState();
}

void ConfigDialog::save()
{
    AppConfig cfg = m_configManager->getConfig();
    QString resolvedStoragePath;
    if (!validateStoragePathInput(m_storagePathEdit->text(), &resolvedStoragePath))
        return;

    const QString rawStoragePath = m_storagePathEdit->text().trimmed();
    const QString normalizedStoragePath = rawStoragePath.isEmpty() ? QString() : QDir::cleanPath(rawStoragePath);
    const QString storageSetting =
        (QDir::cleanPath(resolvedStoragePath) == QDir::cleanPath(ConfigManager::defaultStoragePath()))
            ? QString()
            : normalizedStoragePath;

    cfg.apiKey = m_apiKeyEdit->text();
    // ...
    cfg.baseUrl = m_baseUrlEdit->text();
    if (m_endpointPathEdit)
        cfg.endpointPath = m_endpointPathEdit->text();
    cfg.modelName = m_modelNameEdit->text();
    cfg.promptText = m_promptEdit->toPlainText();
    cfg.proxyUrl = m_proxyUrlEdit->text();
    cfg.useProxy = m_useProxyCheck->isChecked();
    cfg.storagePath = storageSetting;
    cfg.useAdvancedApiMode = m_enableAdvancedApiCheck && m_enableAdvancedApiCheck->isChecked();
    if (m_advancedApiTemplateEdit)
        cfg.advancedApiTemplate = m_advancedApiTemplateEdit->toPlainText();

    if (cfg.useAdvancedApiMode)
    {
        QJsonObject parsedRoot;
        QString parseErr;
        if (!parseAdvancedTemplateJson(parsedRoot, parseErr))
        {
            QMessageBox::warning(this, "高级 API", QString("高级模板 JSON 无效，无法保存。\n%1").arg(parseErr));
            return;
        }
    }

    cfg.advancedApiCustomized = m_advancedTemplateDetached;
    cfg.apiProvider = m_apiProviderCombo->currentData().toString();
    cfg.showPreviewCard = m_showPreviewCheck->isChecked();
    cfg.showResultWindow = m_showResultCheck->isChecked();
    cfg.targetScreenIndex = m_screenCombo->currentData().toInt();

    // Translation Tab
    cfg.screenshotHotkey = m_hotkeyEdit->text();
    cfg.defaultResultWindowLocked = m_defaultLookCheck->isChecked();
    cfg.lockBehavior = m_lockBehaviorCombo->currentData().toInt();
    qInfo() << "[ConfigDialog] Saving config (Snapshot):";
    qInfo() << "  Prev Shortcut:" << m_prevPageHotkeyEdit->text();
    qInfo() << "  Next Shortcut:" << m_nextPageHotkeyEdit->text();

    cfg.prevResultShortcut = m_prevPageHotkeyEdit->text();
    cfg.nextResultShortcut = m_nextPageHotkeyEdit->text();
    cfg.tagHotkey = m_tagHotkeyEdit->text();
    cfg.retranslateHotkey = m_retranslateHotkeyEdit->text().trimmed();
    if (cfg.retranslateHotkey.isEmpty())
        cfg.retranslateHotkey = "f";

    // Screenshot Card settings
    cfg.zoomSensitivity = m_zoomSensitivitySpin->value();
    cfg.cardBorderColor = m_cardBorderColorEdit->text();
    cfg.useCardBorder = m_useBorderCheck->isChecked();

    // Translation Window Settings
    cfg.initialFontSize = m_initialFontSizeSpin->value();

    // Archive Tab
    cfg.summaryHotkey = m_summaryHotkeyEdit->text();
    cfg.editHotkey = m_editHotkeyEdit->text();
    cfg.viewToggleHotkey = m_viewToggleHotkeyEdit->text();
    cfg.screenshotToggleHotkey = m_screenshotToggleHotkeyEdit->text();
    cfg.selectionToggleHotkey = m_selectionToggleHotkeyEdit->text().trimmed();
    cfg.archiveUsePagination = m_archiveLoadModeCombo && m_archiveLoadModeCombo->currentData().toBool();
    cfg.archivePageSize = m_archivePageSizeSpin ? m_archivePageSizeSpin->value() : 50;
    cfg.boldHotkey = m_boldHotkeyEdit->text();
    cfg.underlineHotkey = m_underlineHotkeyEdit->text().trimmed();
    cfg.highlightHotkey = m_highlightHotkeyEdit->text().trimmed();
    cfg.highlightMarkColor = m_highlightMarkColorEdit->text().trimmed();
    cfg.highlightMarkColorDark = m_highlightMarkColorDarkEdit->text().trimmed();

    // Other Tab
    cfg.settingsHotkey = m_settingsHotkeyEdit->text();

    cfg.enableQuitHotkey = m_enableQuitHotkeyCheck->isChecked();
    cfg.quitHotkey = m_quitHotkeyEdit->text().trimmed();
    if (cfg.quitHotkey.isEmpty())
        cfg.quitHotkey = "alt+q";

    cfg.launchAtStartup = m_launchAtStartupCheck->isChecked();
    cfg.enableUmamiAnalytics = m_enableUmamiAnalyticsCheck->isChecked();
    cfg.debugMode = m_debugModeCheck->isChecked();
    cfg.storagePath = storageSetting; // Keep auto-default behavior when the chosen path matches the default.

    m_configManager->setConfig(cfg); // Saves to current profile

    // QMessageBox::information(this, "Settings", "Settings saved!"); // Removed to reduce click fatigue? User requested modeless flow.
    // Or keep it? User didn't complain about success popup.
    // But "Settings saved!" is modal.
    // Let's keep it for now but maybe make it less intrusive later.

    // ... (previous content)

    emit saved();
    accept(); // close the settings window after successful save
}

void ConfigDialog::updateTheme(bool isDark)
{
    ThemeUtils::applyThemeToWindow(this, isDark);
    setStyleSheet(""); // Keep default widget look
}
