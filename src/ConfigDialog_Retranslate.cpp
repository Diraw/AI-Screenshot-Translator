#include "ConfigDialog.h"

#include "TranslationManager.h"

#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>

void ConfigDialog::retranslateUi()
{
    TranslationManager &tm = TranslationManager::instance();

    setWindowTitle(tm.tr("settings_title"));

    if (m_generalTab)
        m_tabWidget->setTabText(m_tabWidget->indexOf(m_generalTab), tm.tr("tab_general"));
    if (m_transTab)
        m_tabWidget->setTabText(m_tabWidget->indexOf(m_transTab), tm.tr("tab_translation"));

    auto updateLabel = [&](QFormLayout *layout, int row, const QString &key)
    {
        QWidget *w = layout->itemAt(row, QFormLayout::LabelRole)->widget();
        if (QLabel *l = qobject_cast<QLabel *>(w))
            l->setText(tm.tr(key));
    };

    QFormLayout *gLayout = m_generalFormLayout;
    if (gLayout)
    {
        updateLabel(gLayout, 0, "lbl_language");
        updateLabel(gLayout, 1, "lbl_capture_screen");
        updateLabel(gLayout, 2, "lbl_api_key");
        updateLabel(gLayout, 3, "lbl_api_provider");
        updateLabel(gLayout, 4, "lbl_base_url");
        updateLabel(gLayout, 5, "lbl_model");
        updateLabel(gLayout, 6, "lbl_prompt");
        updateLabel(gLayout, 7, "lbl_proxy");
        m_useProxyCheck->setToolTip(tm.tr("tip_proxy_toggle"));
        if (m_proxyUrlEdit)
            m_proxyUrlEdit->setPlaceholderText(tm.tr("proxy_placeholder"));
        updateLabel(gLayout, 8, "lbl_storage");

        if (m_testConnectionBtn)
            m_testConnectionBtn->setText(tm.tr("btn_test_connection"));

        if (m_endpointPathEdit)
            m_endpointPathEdit->setPlaceholderText(tm.tr("endpoint_placeholder"));

        m_showPreviewCheck->setText(tm.tr("chk_preview"));
        m_showResultCheck->setText(tm.tr("chk_result"));

        if (m_browseBtn)
            m_browseBtn->setText(tm.tr("btn_browse"));
    }

    QGroupBox *grpCard = m_transTab->findChild<QGroupBox *>("grpCard");
    if (grpCard)
    {
        grpCard->setTitle(tm.tr("grp_card_settings"));
        m_lblCardBorderColor->setText(tm.tr("lbl_border_color"));
        m_useBorderCheck->setText(tm.tr("chk_use_border"));

        QFormLayout *layout = qobject_cast<QFormLayout *>(grpCard->layout());
        if (layout)
            updateLabel(layout, 0, "lbl_zoom_sens");
    }

    QGroupBox *grpTrans = m_transTab->findChild<QGroupBox *>("grpTrans");
    if (grpTrans)
    {
        grpTrans->setTitle(tm.tr("grp_trans_settings"));
        m_lblInitialFontSize->setText(tm.tr("lbl_font_size"));
        m_defaultLookCheck->setText(tm.tr("lbl_default_lock"));

        QFormLayout *layout = qobject_cast<QFormLayout *>(grpTrans->layout());
        if (layout)
        {
            updateLabel(layout, 2, "lbl_lock_behavior");
            m_lockBehaviorCombo->setItemText(0, tm.tr("opt_lock_reset"));
            m_lockBehaviorCombo->setItemText(1, tm.tr("opt_lock_keep"));
            updateLabel(layout, 3, "lbl_prev_hotkey");
            updateLabel(layout, 4, "lbl_next_hotkey");
            updateLabel(layout, 5, "lbl_tag_hotkey");
            updateLabel(layout, 6, "lbl_retranslate_hotkey");
        }
    }

    if (m_archiveTab)
        m_tabWidget->setTabText(m_tabWidget->indexOf(m_archiveTab), tm.tr("tab_archive_interface"));

    QGroupBox *grpView = m_archiveTab->findChild<QGroupBox *>("grpView");
    if (grpView)
    {
        grpView->setTitle(tm.tr("grp_view_toggle"));
        QFormLayout *layout = qobject_cast<QFormLayout *>(grpView->layout());
        if (layout)
        {
            updateLabel(layout, 0, "lbl_view_hotkey");
            updateLabel(layout, 1, "lbl_shot_toggle_hotkey");
            updateLabel(layout, 2, "lbl_selection_toggle_hotkey");
        }
    }

    QGroupBox *grpEdit = m_archiveTab->findChild<QGroupBox *>("grpEdit");
    if (grpEdit)
    {
        grpEdit->setTitle(tm.tr("grp_edit_mode"));
        QFormLayout *layout = qobject_cast<QFormLayout *>(grpEdit->layout());
        if (layout)
        {
            updateLabel(layout, 0, "lbl_edit_hotkey");
            updateLabel(layout, 1, "lbl_bold");
            updateLabel(layout, 2, "lbl_underline");
            updateLabel(layout, 3, "lbl_highlight");
            updateLabel(layout, 4, "lbl_highlight_color");
            updateLabel(layout, 5, "lbl_highlight_color_dark");
        }
    }

    QGroupBox *grpData = m_archiveTab->findChild<QGroupBox *>("grpDataMigration");
    if (grpData)
    {
        grpData->setTitle(tm.tr("grp_data_migration"));
        if (m_importLegacyHistoryBtn)
            m_importLegacyHistoryBtn->setText(tm.tr("btn_import_legacy_history"));
        if (m_exportHistoryBtn)
            m_exportHistoryBtn->setText(tm.tr("btn_export_history"));
    }

    QGroupBox *grpApi = m_generalTab->findChild<QGroupBox *>("grpApi");
    if (grpApi)
    {
        grpApi->setTitle(tm.tr("grp_api"));
        m_proxyLabel->setText(tm.tr("lbl_proxy_url"));
        m_useProxyCheck->setText(tm.tr("chk_use_proxy"));

        QFormLayout *layout = qobject_cast<QFormLayout *>(grpApi->layout());
        if (layout)
        {
            updateLabel(layout, 0, "lbl_api_key");
            updateLabel(layout, 1, "lbl_base_url");
            updateLabel(layout, 2, "lbl_model");
            updateLabel(layout, 3, "lbl_prompt");
            updateLabel(layout, 5, "lbl_target_screen");
        }
    }

    if (m_otherTab)
        m_tabWidget->setTabText(m_tabWidget->indexOf(m_otherTab), tm.tr("tab_other"));

    if (m_advancedApiTab)
        m_tabWidget->setTabText(m_tabWidget->indexOf(m_advancedApiTab), QStringLiteral("高级 API"));
    if (m_enableAdvancedApiCheck)
        m_enableAdvancedApiCheck->setText(QStringLiteral("开启高级 API 模式"));
    if (m_deleteAdvancedApiConfigBtn)
        m_deleteAdvancedApiConfigBtn->setText(QStringLiteral("删除高级 API 配置"));
    if (m_testAdvancedApiBtn)
        m_testAdvancedApiBtn->setText(QStringLiteral("测试 JSON 与 API 连通性"));
    if (m_pickAdvancedJsonFieldsBtn)
        m_pickAdvancedJsonFieldsBtn->setText(tm.tr("btn_select_json_fields"));
    updateAdvancedTemplateStatusLabel();

    QGroupBox *grpShortcuts = m_otherTab->findChild<QGroupBox *>("grpShortcuts");
    if (grpShortcuts)
    {
        grpShortcuts->setTitle(tm.tr("grp_shortcuts"));
        QFormLayout *layout = qobject_cast<QFormLayout *>(grpShortcuts->layout());
        if (layout)
        {
            updateLabel(layout, 0, "lbl_shot_hotkey");
            updateLabel(layout, 1, "lbl_sum_hotkey");
            updateLabel(layout, 2, "lbl_set_hotkey");
        }
    }

    const QList<QGroupBox *> groups = findChildren<QGroupBox *>();
    for (auto *g : groups)
    {
        if (g->objectName() == "profileDetails")
            continue;
        if (g->title().contains("Profiles") || g->title().contains("é…ç½®"))
            g->setTitle(tm.tr("grp_profiles"));
    }

    QGroupBox *grpAdvanced = m_otherTab->findChild<QGroupBox *>("grpAdvanced");
    if (grpAdvanced)
    {
        grpAdvanced->setTitle(tm.tr("grp_advanced"));
        if (m_launchAtStartupCheck)
            m_launchAtStartupCheck->setText(tm.tr("chk_launch_startup"));
        if (m_enableUmamiAnalyticsCheck)
            m_enableUmamiAnalyticsCheck->setText(tm.tr("chk_umami_analytics"));
        m_debugModeCheck->setText(tm.tr("chk_debug"));
        if (m_enableQuitHotkeyCheck)
            m_enableQuitHotkeyCheck->setText(tm.tr("chk_quit_hotkey"));
        if (m_quitHotkeyLabel)
            m_quitHotkeyLabel->setText(tm.tr("lbl_quit_hotkey"));
    }

    m_newProfileBtn->setText(tm.tr("btn_new"));
    m_deleteProfileBtn->setText(tm.tr("btn_delete"));
    m_renameProfileBtn->setText(tm.tr("btn_rename"));
    m_copyProfileBtn->setText(tm.tr("btn_copy"));
    m_importProfileBtn->setText(tm.tr("btn_import"));
    m_exportProfileBtn->setText(tm.tr("btn_export"));

    const QList<QPushButton *> mainBtns = findChildren<QPushButton *>();
    for (auto *b : mainBtns)
    {
        if (b->property("isSaveBtn").toBool())
        {
            b->setText(tm.tr("btn_save"));
        }
        else if (b->text().contains("Save") || b->text().contains("ä¿å­˜"))
        {
            b->setText(tm.tr("btn_save"));
        }
    }

    applyGlobalHotkeyConflictIndicators(false);
}
