#ifndef TRANSLATIONMANAGER_H
#define TRANSLATIONMANAGER_H

#include <QString>
#include <QMap>
#include <QObject>

class TranslationManager
{
public:
    static TranslationManager &instance()
    {
        static TranslationManager instance;
        return instance;
    }

    void setLanguage(const QString &lang)
    {
        m_currentLang = lang;
    }

    QString getLanguage() const
    {
        return m_currentLang;
    }

    QString tr(const QString &key)
    {
        if (m_translations.contains(m_currentLang) && m_translations[m_currentLang].contains(key))
        {
            return m_translations[m_currentLang][key];
        }
        // Fallback to English/Key
        if (m_translations["en"].contains(key))
        {
            return m_translations["en"][key];
        }
        return key;
    }

private:
    TranslationManager()
    {
        m_currentLang = "zh"; // Default to Chinese as requested (or user setting)

        // English
        m_translations["en"]["settings_title"] = "Settings";
        m_translations["en"]["summary_title"] = "Archive";
        m_translations["en"]["result_window_title"] = "Translation Result";
        m_translations["en"]["tray_screenshot"] = "Screenshot";
        m_translations["en"]["tray_summary"] = "Archive";
        m_translations["en"]["tray_settings"] = "Settings";
        m_translations["en"]["tray_quit"] = "Quit";

        m_translations["en"]["grp_profiles"] = "Profiles";
        m_translations["en"]["btn_new"] = "New";
        m_translations["en"]["btn_delete"] = "Delete";
        m_translations["en"]["btn_rename"] = "Rename";
        m_translations["en"]["btn_copy"] = "Copy";
        m_translations["en"]["btn_import"] = "Import";
        m_translations["en"]["btn_export"] = "Export";
        m_translations["en"]["btn_browse"] = "Browse";

        m_translations["en"]["rename_profile_title"] = "Rename Profile";
        m_translations["en"]["rename_profile_label"] = "New Name:";
        m_translations["en"]["new_profile_title"] = "New Profile";
        m_translations["en"]["new_profile_label"] = "Profile Name:";
        m_translations["en"]["delete_profile_title"] = "Delete Profile";
        m_translations["en"]["delete_profile_msg"] = "Are you sure you want to delete profile '%1'?";
        m_translations["en"]["msg_rename_success"] = "Profile renamed successfully.";
        m_translations["en"]["msg_rename_error"] = "Failed to rename profile. Name might exist or is invalid.";
        m_translations["en"]["msg_profile_exists"] = "Failed to create profile. Name might exist.";
        m_translations["en"]["msg_cannot_delete_default"] = "Cannot delete Default profile.";
        m_translations["en"]["msg_cannot_rename_default"] = "Cannot rename Default profile.";

        m_translations["en"]["import_profile_title"] = "Import Profile";
        m_translations["en"]["export_profile_title"] = "Export Profile";
        m_translations["en"]["msg_import_success"] = "Profile imported successfully.";
        m_translations["en"]["msg_export_success"] = "Profile exported successfully.";
        m_translations["en"]["msg_import_error"] = "Failed to import profile.";
        m_translations["en"]["msg_export_error"] = "Failed to export profile.";
        m_translations["en"]["json_files"] = "JSON Files (*.json)";

        m_translations["en"]["tab_general"] = "General";
        m_translations["en"]["tab_advanced"] = "Advanced";

        m_translations["en"]["lbl_language"] = "Language:";
        m_translations["en"]["lbl_capture_screen"] = "Capture Screen:";
        m_translations["en"]["lbl_api_key"] = "API Key:";
        m_translations["en"]["lbl_base_url"] = "Base URL:";
        m_translations["en"]["lbl_model"] = "Model:";
        m_translations["en"]["lbl_prompt"] = "Prompt:";
        m_translations["en"]["lbl_proxy"] = "Proxy URL:";
        m_translations["en"]["proxy_placeholder"] = "e.g. http://127.0.0.1:1080 or socks5://127.0.0.1:1080";
        m_translations["en"]["tip_proxy_toggle"] = "Toggle Proxy ON/OFF";
        m_translations["en"]["lbl_max_windows"] = "Max Windows:";
        m_translations["en"]["lbl_storage"] = "Storage Path:";
        m_translations["en"]["btn_browse"] = "Browse...";
        m_translations["en"]["chk_preview"] = "Show Preview Card after Screenshot";
        m_translations["en"]["chk_result"] = "Show Translation Result after Screenshot";

        m_translations["en"]["lbl_shot_hotkey"] = "Screenshot Hotkey:";
        m_translations["en"]["lbl_sum_hotkey"] = "Summary Hotkey:";
        m_translations["en"]["lbl_set_hotkey"] = "Settings Hotkey:";
        m_translations["en"]["lbl_edit_hotkey"] = "Summary Edit Toggle:";
        m_translations["en"]["lbl_view_hotkey"] = "Summary View Toggle:";
        m_translations["en"]["lbl_shot_toggle_hotkey"] = "Summary Screenshot Toggle:";
        m_translations["en"]["lbl_selection_toggle_hotkey"] = "Batch Selection Toggle:";
        m_translations["en"]["lbl_bold"] = "Bold Shortcut:";
        m_translations["en"]["lbl_underline"] = "Underline Shortcut:";
        m_translations["en"]["lbl_highlight"] = "Highlight Shortcut:";
        m_translations["en"]["lbl_border_color"] = "Card Border Color:";
        m_translations["en"]["lbl_font_size"] = "Initial Font Size:";
        m_translations["en"]["lbl_zoom_sens"] = "Zoom Sensitivity:";

        m_translations["en"]["btn_save"] = "Save and Apply";
        m_translations["en"]["msg_saved"] = "Settings saved!";
        m_translations["en"]["tag_dialog_title"] = "Manage Tags";
        m_translations["en"]["tag_dialog_label"] = "Select existing tags or create new ones:";
        m_translations["en"]["tag_dialog_new"] = "Add new tags (comma-separated):";
        m_translations["en"]["tag_dialog_placeholder"] = "e.g., work, important, math";
        m_translations["en"]["ok"] = "OK";
        m_translations["en"]["cancel"] = "Cancel";
        m_translations["en"]["filter_from_date"] = "From";
        m_translations["en"]["filter_to_date"] = "To";
        m_translations["en"]["filter_tags"] = "Tags";
        m_translations["en"]["filter_all_tags"] = "All Tags";
        m_translations["en"]["filter_clear"] = "Clear Filters";
        m_translations["en"]["lbl_api_provider"] = "API Provider:";
        m_translations["en"]["btn_selection_mode"] = "Select";
        m_translations["en"]["btn_cancel_selection_mode"] = "Cancel";
        m_translations["en"]["btn_select_all"] = "Select All";
        m_translations["en"]["btn_deselect_all"] = "Deselect All";
        m_translations["en"]["btn_batch_delete"] = "Delete";
        m_translations["en"]["btn_batch_add_tag"] = "Add Tags";
        m_translations["en"]["btn_batch_remove_tag"] = "Remove Tags";
        m_translations["en"]["chk_debug"] = "Enable Debug Mode";
        m_translations["en"]["grp_advanced"] = "Advanced Settings";
        m_translations["en"]["chk_quit_hotkey"] = "Enable";
        m_translations["en"]["lbl_quit_hotkey"] = "Quit:";

        // Chinese
        m_translations["zh"]["settings_title"] = "设置";
        m_translations["zh"]["summary_title"] = "归档";
        m_translations["zh"]["result_window_title"] = "翻译结果";
        m_translations["zh"]["tray_screenshot"] = "截图";
        m_translations["zh"]["tray_summary"] = "归档";
        m_translations["zh"]["tray_settings"] = "设置";
        m_translations["zh"]["tray_quit"] = "退出快捷键";

        m_translations["zh"]["grp_profiles"] = "配置方案";
        m_translations["zh"]["btn_new"] = "新建";
        m_translations["zh"]["btn_delete"] = "删除";
        m_translations["zh"]["btn_rename"] = "重命名";
        m_translations["zh"]["btn_copy"] = "复制";
        m_translations["zh"]["btn_import"] = "导入";
        m_translations["zh"]["btn_export"] = "导出";
        m_translations["zh"]["btn_browse"] = "浏览...";

        m_translations["zh"]["rename_profile_title"] = "重命名配置";
        m_translations["zh"]["rename_profile_label"] = "新名称:";
        m_translations["zh"]["new_profile_title"] = "新建配置";
        m_translations["zh"]["new_profile_label"] = "配置名称:";
        m_translations["zh"]["delete_profile_title"] = "删除配置";
        m_translations["zh"]["delete_profile_msg"] = "你确定要删除配置 '%1' 吗？";
        m_translations["zh"]["msg_rename_success"] = "配置重命名成功。";
        m_translations["zh"]["msg_rename_error"] = "重新命名失败。名称可能已存在或无效。";
        m_translations["zh"]["msg_profile_exists"] = "创建配置失败。名称可能已存在。";
        m_translations["zh"]["msg_cannot_delete_default"] = "无法删除默认配置。";
        m_translations["zh"]["msg_cannot_rename_default"] = "无法重命名默认配置。";

        m_translations["zh"]["import_profile_title"] = "导入配置";
        m_translations["zh"]["export_profile_title"] = "导出配置";
        m_translations["zh"]["msg_import_success"] = "配置导入成功。";
        m_translations["zh"]["msg_export_success"] = "配置导出成功。";
        m_translations["zh"]["msg_import_error"] = "导入配置失败。";
        m_translations["zh"]["msg_export_error"] = "导出配置失败。";
        m_translations["zh"]["json_files"] = "JSON 文件 (*.json)";
        m_translations["zh"]["tag_dialog_title"] = "管理标签";
        m_translations["zh"]["tag_dialog_label"] = "选择已有标签或创建新标签:";
        m_translations["zh"]["tag_dialog_new"] = "添加新标签(逗号分隔):";
        m_translations["zh"]["tag_dialog_placeholder"] = "例如: 工作, 重要, 数学";
        m_translations["zh"]["ok"] = "确定";
        m_translations["zh"]["cancel"] = "取消";
        m_translations["zh"]["filter_from_date"] = "开始日期";
        m_translations["zh"]["filter_to_date"] = "结束日期";
        m_translations["zh"]["filter_tags"] = "标签";
        m_translations["zh"]["filter_all_tags"] = "所有标签";
        m_translations["zh"]["filter_clear"] = "清除筛选";
        m_translations["zh"]["lbl_api_provider"] = "API 格式:";
        m_translations["zh"]["btn_selection_mode"] = "批量选择";
        m_translations["zh"]["btn_cancel_selection_mode"] = "取消选择";
        m_translations["zh"]["btn_select_all"] = "全选";
        m_translations["zh"]["btn_deselect_all"] = "取消全选";
        m_translations["zh"]["btn_batch_delete"] = "批量删除";
        m_translations["zh"]["btn_batch_add_tag"] = "批量加标签";
        m_translations["zh"]["btn_batch_remove_tag"] = "批量减标签";
        m_translations["zh"]["chk_debug"] = "开启 Debug 模式";
        m_translations["zh"]["grp_advanced"] = "高级设置";
        m_translations["zh"]["chk_quit_hotkey"] = "启用快捷键";
        m_translations["zh"]["lbl_quit_hotkey"] = "退出:";

        m_translations["zh"]["tab_general"] = "常规";
        m_translations["zh"]["tab_advanced"] = "高级";

        m_translations["zh"]["lbl_language"] = "语言:";
        m_translations["zh"]["lbl_capture_screen"] = "捕获屏幕:";
        m_translations["zh"]["lbl_api_key"] = "API 密钥:";
        m_translations["zh"]["lbl_base_url"] = "API 基础 URL";
        m_translations["zh"]["lbl_model"] = "模型名称";
        m_translations["zh"]["lbl_prompt"] = "提示词";
        m_translations["zh"]["lbl_proxy_url"] = "代理地址";
        m_translations["zh"]["chk_use_proxy"] = "启用代理";
        m_translations["zh"]["lbl_target_screen"] = "目标屏幕";
        m_translations["zh"]["lbl_proxy"] = "代理地址:";
        m_translations["zh"]["proxy_placeholder"] = "如 http://127.0.0.1:1080 或 socks5://127.0.0.1:1080";
        m_translations["zh"]["tip_proxy_toggle"] = "开启/关闭代理";
        m_translations["zh"]["lbl_max_windows"] = "最大窗口数:";
        m_translations["zh"]["lbl_storage"] = "存储路径:";
        m_translations["zh"]["btn_browse"] = "浏览...";
        m_translations["zh"]["chk_preview"] = "截图后显示截图卡片";
        m_translations["zh"]["chk_result"] = "截图后显示翻译结果";

        m_translations["zh"]["lbl_shot_hotkey"] = "截图快捷键:";
        m_translations["zh"]["lbl_sum_hotkey"] = "摘要窗口快捷键:";
        m_translations["zh"]["lbl_set_hotkey"] = "设置快捷键:";
        m_translations["zh"]["lbl_edit_hotkey"] = "编辑模式切换:";
        m_translations["zh"]["lbl_view_hotkey"] = "原始视图切换:";
        m_translations["zh"]["lbl_shot_toggle_hotkey"] = "截图卡片切换:";
        m_translations["zh"]["lbl_selection_toggle_hotkey"] = "批量选择切换:";
        m_translations["zh"]["lbl_bold"] = "加粗快捷键:";
        m_translations["zh"]["lbl_underline"] = "下划线快捷键:";
        m_translations["zh"]["lbl_highlight"] = "高亮快捷键:";
        m_translations["zh"]["lbl_highlight_color"] = "高亮颜色(浅色):";
        m_translations["zh"]["lbl_highlight_color_dark"] = "高亮颜色(深色):";
        m_translations["zh"]["lbl_border_color"] = "卡片边框颜色:";
        m_translations["zh"]["lbl_font_size"] = "初始字体大小:";
        m_translations["zh"]["lbl_zoom_sens"] = "缩放灵敏度:";

        m_translations["zh"]["btn_save"] = "保存并应用";
        m_translations["zh"]["msg_saved"] = "设置已保存！";

        // New Settings Groups
        m_translations["en"]["tab_translation"] = "Translation Window";
        m_translations["en"]["tab_archive"] = "Archive Window";
        m_translations["en"]["tab_other"] = "Other";

        m_translations["zh"]["tab_translation"] = "翻译窗口";
        m_translations["zh"]["tab_archive"] = "归档窗口";
        m_translations["zh"]["tab_other"] = "其他";

        // Lock Settings
        m_translations["en"]["lbl_default_lock"] = "Default Locked State:";
        m_translations["en"]["lbl_lock_behavior"] = "When All Locked Windows Close:";
        m_translations["en"]["opt_lock_reset"] = "Reset to Unlocked";
        m_translations["en"]["opt_lock_keep"] = "Maintain Previous State";
        m_translations["en"]["lbl_prev_hotkey"] = "Prev Page Hotkey:";
        m_translations["en"]["lbl_next_hotkey"] = "Next Page Hotkey:";
        m_translations["en"]["lbl_tag_hotkey"] = "Tag Dialog Hotkey:";

        m_translations["zh"]["lbl_default_lock"] = "默认锁定状态";
        m_translations["zh"]["lbl_lock_behavior"] = "所有锁定窗口关闭后:";
        m_translations["zh"]["opt_lock_reset"] = "重置为未锁定";
        m_translations["zh"]["opt_lock_keep"] = "保持之前的状态";
        m_translations["zh"]["lbl_prev_hotkey"] = "上一页快捷键:";
        m_translations["zh"]["lbl_next_hotkey"] = "下一页快捷键:";
        m_translations["zh"]["lbl_tag_hotkey"] = "标签快捷键:";

        // New Layout Groups
        m_translations["en"]["grp_card_settings"] = "Screenshot Card Settings";
        m_translations["en"]["grp_trans_settings"] = "Translation Window Settings";
        m_translations["en"]["chk_use_border"] = "Enable Border";

        m_translations["zh"]["grp_card_settings"] = "截图卡片设置";
        m_translations["zh"]["grp_trans_settings"] = "翻译窗口设置";
        m_translations["zh"]["chk_use_border"] = "启用边框";

        m_translations["zh"]["tab_translation"] = "翻译组"; // Requested "Translation Group"

        // New Archive/Other Groups
        m_translations["en"]["tab_archive_interface"] = "Archive Interface";
        m_translations["en"]["grp_shortcuts"] = "Shortcut Settings";
        m_translations["en"]["grp_view_toggle"] = "View Toggle";
        m_translations["en"]["grp_edit_mode"] = "Edit Mode";

        m_translations["en"]["lbl_highlight_color"] = "Highlight Color:";
        m_translations["en"]["lbl_highlight_color_dark"] = "Highlight Color (Dark):";

        m_translations["zh"]["tab_archive_interface"] = "归档界面";
        m_translations["zh"]["grp_shortcuts"] = "快捷键设置";
        m_translations["zh"]["grp_view_toggle"] = "视图切换";
        m_translations["zh"]["grp_edit_mode"] = "编辑模式";
    }

    QString m_currentLang;
    QMap<QString, QMap<QString, QString>> m_translations;
};

#endif // TRANSLATIONMANAGER_H
