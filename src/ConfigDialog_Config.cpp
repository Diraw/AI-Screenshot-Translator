#include "ConfigDialog.h"

#include <QDebug>

void ConfigDialog::loadFromConfig()
{
    AppConfig cfg = m_configManager->getConfig();
    m_apiKeyEdit->setText(cfg.apiKey);
    m_baseUrlEdit->setText(cfg.baseUrl);
    if (m_endpointPathEdit)
        m_endpointPathEdit->setText(cfg.endpointPath);
    m_modelNameEdit->setText(cfg.modelName);
    m_promptEdit->setPlainText(cfg.promptText);
    m_proxyUrlEdit->setText(cfg.proxyUrl);
    m_useProxyCheck->setChecked(cfg.useProxy);

    int providerIndex = m_apiProviderCombo->findData(cfg.apiProvider);
    if (providerIndex >= 0)
        m_apiProviderCombo->setCurrentIndex(providerIndex);

    m_hotkeyEdit->setText(cfg.screenshotHotkey);
    m_summaryHotkeyEdit->setText(cfg.summaryHotkey);
    m_settingsHotkeyEdit->setText(cfg.settingsHotkey);
    m_editHotkeyEdit->setText(cfg.editHotkey);
    m_viewToggleHotkeyEdit->setText(cfg.viewToggleHotkey);
    m_screenshotToggleHotkeyEdit->setText(cfg.screenshotToggleHotkey);
    m_selectionToggleHotkeyEdit->setText(cfg.selectionToggleHotkey);
    m_boldHotkeyEdit->setText(cfg.boldHotkey);
    m_underlineHotkeyEdit->setText(cfg.underlineHotkey);
    m_highlightHotkeyEdit->setText(cfg.highlightHotkey);
    m_highlightMarkColorEdit->setText(cfg.highlightMarkColor);
    m_highlightMarkColorDarkEdit->setText(cfg.highlightMarkColorDark);

    m_debugModeCheck->setChecked(cfg.debugMode);
    m_enableQuitHotkeyCheck->setChecked(cfg.enableQuitHotkey);
    m_quitHotkeyEdit->setText(cfg.quitHotkey);
    m_quitHotkeyEdit->setEnabled(cfg.enableQuitHotkey);
    m_storagePathEdit->setText(cfg.storagePath);
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

    // Screen selection
    for (int i = 0; i < m_screenCombo->count(); ++i)
    {
        if (m_screenCombo->itemData(i).toInt() == cfg.targetScreenIndex)
        {
            m_screenCombo->setCurrentIndex(i);
            break;
        }
    }
}

void ConfigDialog::save()
{
    AppConfig cfg = m_configManager->getConfig();
    cfg.apiKey = m_apiKeyEdit->text();
    // ...
    cfg.baseUrl = m_baseUrlEdit->text();
    if (m_endpointPathEdit)
        cfg.endpointPath = m_endpointPathEdit->text();
    cfg.modelName = m_modelNameEdit->text();
    cfg.promptText = m_promptEdit->toPlainText();
    cfg.proxyUrl = m_proxyUrlEdit->text();
    cfg.useProxy = m_useProxyCheck->isChecked();
    cfg.storagePath = m_storagePathEdit->text();
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

    cfg.debugMode = m_debugModeCheck->isChecked();
    cfg.storagePath = m_storagePathEdit->text(); // Ensure persistence

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
    Q_UNUSED(isDark);
    setStyleSheet(""); // Keep default widget look
}
