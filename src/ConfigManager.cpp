#include "ConfigManager.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QUuid>

namespace
{
const QString kAppDataFolderName = QStringLiteral("AI-Screenshot-Translator");

bool isReservedWindowsBaseName(const QString &name)
{
    static const QStringList kReserved = {
        QStringLiteral("CON"),  QStringLiteral("PRN"),  QStringLiteral("AUX"),  QStringLiteral("NUL"),
        QStringLiteral("COM1"), QStringLiteral("COM2"), QStringLiteral("COM3"), QStringLiteral("COM4"),
        QStringLiteral("COM5"), QStringLiteral("COM6"), QStringLiteral("COM7"), QStringLiteral("COM8"),
        QStringLiteral("COM9"), QStringLiteral("LPT1"), QStringLiteral("LPT2"), QStringLiteral("LPT3"),
        QStringLiteral("LPT4"), QStringLiteral("LPT5"), QStringLiteral("LPT6"), QStringLiteral("LPT7"),
        QStringLiteral("LPT8"), QStringLiteral("LPT9"),
    };
    return kReserved.contains(name.toUpper());
}
} // namespace

QString ConfigManager::validateProfileName(const QString &name)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty())
        return QStringLiteral("msg_profile_name_empty");
    if (trimmed == "." || trimmed == "..")
        return QStringLiteral("msg_profile_name_invalid");
    if (trimmed.endsWith('.'))
        return QStringLiteral("msg_profile_name_trailing_dot");

    static const QString kIllegalChars = QStringLiteral("<>:\"/\\|?*");
    for (const QChar ch : trimmed)
    {
        if (ch.unicode() < 0x20 || kIllegalChars.contains(ch))
            return QStringLiteral("msg_profile_name_unsupported_chars");
    }

    if (isReservedWindowsBaseName(trimmed))
        return QStringLiteral("msg_profile_name_reserved");

    return QString();
}

QString ConfigManager::appDataDirPath()
{
    QString base;
#ifdef _WIN32
    base = qEnvironmentVariable("APPDATA");
#endif
    if (base.isEmpty())
        base = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    if (base.isEmpty())
        base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);

    const QString dirPath = QDir::cleanPath(base + "/" + kAppDataFolderName);
    QDir dir(dirPath);
    if (!dir.exists())
        dir.mkpath(".");
    return dirPath;
}

QString ConfigManager::settingsJsonPath()
{
    return QDir::cleanPath(appDataDirPath() + "/settings.json");
}

QString ConfigManager::settingsIniPath()
{
    return QDir::cleanPath(appDataDirPath() + "/app.ini");
}

QString ConfigManager::defaultStoragePath()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    if (!appDir.isEmpty() && ensureWritableDirectory(appDir))
        return QDir(appDir).filePath("storage");

    return QDir(appDataDirPath()).filePath("storage");
}

QString ConfigManager::resolveStoragePath(const QString &path)
{
    const QString trimmedPath = path.trimmed();
    if (trimmedPath.isEmpty())
        return QDir::cleanPath(defaultStoragePath());

    const QFileInfo info(trimmedPath);
    if (info.isRelative())
        return QDir::cleanPath(QDir(QCoreApplication::applicationDirPath()).filePath(trimmedPath));

    return QDir::cleanPath(trimmedPath);
}

QString ConfigManager::resolveWritableStoragePath(const QString &path, bool *usedFallback, QString *errorMessage)
{
    if (usedFallback)
        *usedFallback = false;
    if (errorMessage)
        errorMessage->clear();

    const QString resolvedPath = resolveStoragePath(path);
    QString writeError;
    if (ensureWritableDirectory(resolvedPath, &writeError))
        return resolvedPath;

    const QString fallbackPath = QDir::cleanPath(defaultStoragePath());
    if (fallbackPath != resolvedPath)
    {
        if (ensureWritableDirectory(fallbackPath))
        {
            if (usedFallback)
                *usedFallback = true;
            if (errorMessage)
                *errorMessage = writeError;
            return fallbackPath;
        }
    }

    const QString tempBase = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (!tempBase.isEmpty())
    {
        const QString emergencyPath = QDir(tempBase).filePath(QString("%1/storage").arg(kAppDataFolderName));
        if (emergencyPath != resolvedPath && emergencyPath != fallbackPath)
        {
            if (ensureWritableDirectory(emergencyPath))
            {
                if (usedFallback)
                    *usedFallback = true;
                if (errorMessage)
                    *errorMessage = writeError;
                return emergencyPath;
            }
        }
    }

    if (errorMessage)
        *errorMessage = writeError;
    return resolvedPath;
}

bool ConfigManager::ensureWritableDirectory(const QString &path, QString *errorMessage)
{
    if (errorMessage)
        errorMessage->clear();

    const QString cleanPath = QDir::cleanPath(path.trimmed());
    if (cleanPath.isEmpty())
    {
        if (errorMessage)
            *errorMessage = "The storage path is empty.";
        return false;
    }

    const QFileInfo info(cleanPath);
    if (info.exists() && !info.isDir())
    {
        if (errorMessage)
            *errorMessage = QString("The path exists but is not a directory: %1").arg(cleanPath);
        return false;
    }

    QDir dir(cleanPath);
    if (!dir.exists() && !QDir().mkpath(cleanPath))
    {
        if (errorMessage)
            *errorMessage = QString("Unable to create directory: %1").arg(cleanPath);
        return false;
    }

    QFile probe(dir.filePath(QString(".write_test_%1.tmp").arg(QUuid::createUuid().toString(QUuid::WithoutBraces))));
    if (!probe.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        if (errorMessage)
            *errorMessage = QString("Write access denied: %1").arg(cleanPath);
        return false;
    }

    probe.write("ok");
    probe.close();
    probe.remove();
    return true;
}

void ConfigManager::parseJson(const QJsonObject &root)
{
    m_config = AppConfig();

    if (root.contains("api") && root["api"].isObject())
    {
        const QJsonObject api = root["api"].toObject();
        m_config.apiKey = api["api_key"].toString();
        m_config.baseUrl = api["base_url"].toString();
        m_config.endpointPath = api["endpoint"].toString();
        m_config.modelName = api["model"].toString();

        const QString promptFromConfig = api["prompt_text"].toString();
        if (promptFromConfig.isEmpty())
        {
            m_config.promptText = "# Role\nä½ æ˜¯ä¸€ä¸ªä¸“ä¸šçš„å­¦æœ¯ç¿»è¯‘ä¸“å®¶ï¼Œæ“…é•¿ä¸­è‹±æ–‡äº’è¯‘ï¼Œå¹¶èƒ½ç²¾ç¡®å¤„ç†æ•°å­¦ã€ç‰©ç†ç­‰é¢†åŸŸçš„ LaTeX å…¬å¼ã€‚\n\n# Task\nè¯·å°†å›¾ç‰‡ä¸­çš„è‹±æ–‡å†…å®¹ç¿»è¯‘ä¸ºä¸­æ–‡ã€‚\n\n# Requirements\n1. **ä»…è¿”å›žç¿»è¯‘åŽçš„ä¸­æ–‡æ–‡æœ¬**ã€‚ä¸¥ç¦åŒ…å«ä»»ä½•å¼€åœºç™½ã€è§£é‡Šè¯­ï¼ˆå¦‚\"å¥½çš„\"ã€\"è¿™æ˜¯ç¿»è¯‘å†…å®¹\"ï¼‰æˆ–ç»“æŸè¯­ã€‚\n2. **æ•°å­¦å…¬å¼å¤„ç†**ï¼šåŽŸæ–‡ä¸­æ‰€æœ‰çš„æ•°å­¦å˜é‡ã€å¸¸æ•°ã€å…¬å¼å’Œæ–¹ç¨‹å¿…é¡»ä½¿ç”¨ LaTeX æ ¼å¼è¾“å‡ºï¼ˆä¾‹å¦‚ä½¿ç”¨ $x$ æˆ– $$E=mc^2$$ï¼‰ã€‚\n3. **æœ¯è¯­å‡†ç¡®**ï¼šä¿æŒä¸“ä¸šé¢†åŸŸçš„æœ¯è¯­ç¿»è¯‘ä¸¥è°¨ã€è‡ªç„¶ã€‚\n4. **æ ¼å¼å¯¹é½**ï¼šä¿æŒåŽŸæœ‰çš„æ®µè½ç»“æž„ã€‚\n\n# Content";
        }
        else
        {
            m_config.promptText = promptFromConfig;
        }

        m_config.proxyUrl = api["proxy"].toString();
        m_config.useProxy = api["use_proxy"].toBool(false);
        m_config.apiProvider = api["provider"].toString("openai");
        m_config.useAdvancedApiMode = api["use_advanced_mode"].toBool(false);
        m_config.advancedApiTemplate = api["advanced_template"].toString();
        m_config.advancedApiCustomized = api["advanced_customized"].toBool(false);

        if (m_config.advancedApiTemplate.trimmed().isEmpty() && root.contains("advanced_api") && root["advanced_api"].isObject())
        {
            m_config.advancedApiTemplate = QString::fromUtf8(QJsonDocument(root["advanced_api"].toObject()).toJson(QJsonDocument::Indented));
        }

        if (m_config.endpointPath.trimmed().isEmpty())
        {
            const QString b = m_config.baseUrl.trimmed().toLower();
            if (b.endsWith("/v1") || b.endsWith("/v1/") || b.contains("/compatible-mode/v1"))
                m_config.endpointPath = "/chat/completions";
            else
                m_config.endpointPath = "/v1/chat/completions";
        }
    }

    if (root.contains("app_settings") && root["app_settings"].isObject())
    {
        const QJsonObject app = root["app_settings"].toObject();
        m_config.language = app["language"].toString("zh");
        m_config.zoomSensitivity = app["zoom_sensitivity"].toDouble(500.0);
        m_config.screenshotHotkey = app["screenshot_hotkey"].toString("ctrl+alt+s");
        m_config.batchScreenshotToggleHotkey = app["batch_screenshot_toggle_hotkey"].toString("d");

        QString sumKey = app["summary_hotkey"].toString();
        if (sumKey.isEmpty())
            sumKey = "alt+s";
        m_config.summaryHotkey = app["summary_hotkey"].toString("alt+s");
        m_config.settingsHotkey = app["settings_hotkey"].toString("");

        m_config.enableQuitHotkey = app["enable_quit_hotkey"].toBool(false);
        m_config.quitHotkey = app["quit_hotkey"].toString("alt+q");
        m_config.targetScreenIndex = app["target_screen_index"].toInt(-1);

        m_config.editHotkey = app["edit_hotkey"].toString("e");
        m_config.viewToggleHotkey = app["view_toggle_hotkey"].toString("r");
        m_config.screenshotToggleHotkey = app["screenshot_toggle_hotkey"].toString("s");
        m_config.selectionToggleHotkey = app["selection_toggle_hotkey"].toString("ctrl+s");
        m_config.archiveUsePagination = app["archive_use_pagination"].toBool(true);
        m_config.archivePageSize = qMax(1, app["archive_page_size"].toInt(50));
        m_config.showAdvancedDebugInResultWindow = app["show_advanced_debug_in_result"].toBool(true);
        m_config.showAdvancedDebugInArchiveWindow = app["show_advanced_debug_in_archive"].toBool(false);

        m_config.boldHotkey = app["bold_hotkey"].toString("ctrl+b");
        m_config.underlineHotkey = app["underline_hotkey"].toString("ctrl+u");
        m_config.highlightHotkey = app["highlight_hotkey"].toString("ctrl+h");

        m_config.highlightMarkColor = app["highlight_mark_color"].toString("#ffeb3b");
        m_config.highlightMarkColorDark = app["highlight_mark_color_dark"].toString("#d4af37");

        m_config.initialFontSize = app["initial_font_size"].toInt(16);
        m_config.cardBorderColor = app["card_border_color"].toString("100,100,100");
        m_config.debugMode = app["debug_mode"].toBool(false);
        m_config.launchAtStartup = app["launch_at_startup"].toBool(false);
        m_config.enableUmamiAnalytics = app["enable_umami_analytics"].toBool(true);
        m_config.showPreviewCard = app["show_preview_card"].toBool(true);
        if (app.contains("show_result_window"))
            m_config.showResultWindow = app["show_result_window"].toBool(true);
        if (app.contains("use_card_border"))
            m_config.useCardBorder = app["use_card_border"].toBool(true);
        if (app.contains("summary_geometry"))
            m_config.summaryWindowGeometry = QByteArray::fromBase64(app["summary_geometry"].toString().toLatin1());
        if (app.contains("summary_zoom"))
            m_config.summaryWindowZoom = app["summary_zoom"].toDouble(1.0);
        if (app.contains("config_geometry"))
            m_config.configWindowGeometry = QByteArray::fromBase64(app["config_geometry"].toString().toLatin1());

        m_config.storagePath = app["storage_path"].toString("");
        m_config.defaultResultWindowLocked = app["default_result_window_locked"].toBool(false);
        m_config.lockBehavior = app["lock_behavior"].toInt(0);
        m_config.prevResultShortcut = app["prev_result_shortcut"].toString("z");
        m_config.nextResultShortcut = app["next_result_shortcut"].toString("x");
        m_config.tagHotkey = app["tag_hotkey"].toString("t");
        m_config.retranslateHotkey = app["retranslate_hotkey"].toString("f");
    }
}

QJsonObject ConfigManager::toJson() const
{
    QJsonObject root;

    QJsonObject api;
    api["api_key"] = m_config.apiKey;
    api["base_url"] = m_config.baseUrl;
    api["endpoint"] = m_config.endpointPath;
    api["model"] = m_config.modelName;
    api["prompt_text"] = m_config.promptText;
    api["provider"] = m_config.apiProvider;
    api["use_advanced_mode"] = m_config.useAdvancedApiMode;
    api["advanced_customized"] = m_config.advancedApiCustomized;
    if (!m_config.advancedApiTemplate.trimmed().isEmpty())
        api["advanced_template"] = m_config.advancedApiTemplate;
    if (!m_config.proxyUrl.isEmpty())
    {
        api["proxy"] = m_config.proxyUrl;
        api["use_proxy"] = m_config.useProxy;
    }
    root["api"] = api;

    QJsonObject app;
    app["language"] = m_config.language;
    app["zoom_sensitivity"] = m_config.zoomSensitivity;
    app["screenshot_hotkey"] = m_config.screenshotHotkey;
    app["batch_screenshot_toggle_hotkey"] = m_config.batchScreenshotToggleHotkey;
    app["summary_hotkey"] = m_config.summaryHotkey;
    app["settings_hotkey"] = m_config.settingsHotkey;

    app["enable_quit_hotkey"] = m_config.enableQuitHotkey;
    app["quit_hotkey"] = m_config.quitHotkey;
    app["target_screen_index"] = m_config.targetScreenIndex;
    app["edit_hotkey"] = m_config.editHotkey;
    app["view_toggle_hotkey"] = m_config.viewToggleHotkey;
    app["screenshot_toggle_hotkey"] = m_config.screenshotToggleHotkey;
    app["selection_toggle_hotkey"] = m_config.selectionToggleHotkey;
    app["archive_use_pagination"] = m_config.archiveUsePagination;
    app["archive_page_size"] = m_config.archivePageSize;
    app["show_advanced_debug_in_result"] = m_config.showAdvancedDebugInResultWindow;
    app["show_advanced_debug_in_archive"] = m_config.showAdvancedDebugInArchiveWindow;

    app["bold_hotkey"] = m_config.boldHotkey;
    app["underline_hotkey"] = m_config.underlineHotkey;
    app["highlight_hotkey"] = m_config.highlightHotkey;
    app["highlight_mark_color"] = m_config.highlightMarkColor;
    app["highlight_mark_color_dark"] = m_config.highlightMarkColorDark;
    app["initial_font_size"] = m_config.initialFontSize;
    app["card_border_color"] = m_config.cardBorderColor;
    app["debug_mode"] = m_config.debugMode;
    app["launch_at_startup"] = m_config.launchAtStartup;
    app["enable_umami_analytics"] = m_config.enableUmamiAnalytics;
    app["show_preview_card"] = m_config.showPreviewCard;
    app["show_result_window"] = m_config.showResultWindow;
    app["use_card_border"] = m_config.useCardBorder;

    if (!m_config.summaryWindowGeometry.isEmpty())
        app["summary_geometry"] = QString::fromLatin1(m_config.summaryWindowGeometry.toBase64());
    app["summary_zoom"] = m_config.summaryWindowZoom;
    if (!m_config.configWindowGeometry.isEmpty())
        app["config_geometry"] = QString::fromLatin1(m_config.configWindowGeometry.toBase64());
    app["storage_path"] = m_config.storagePath;

    app["default_result_window_locked"] = m_config.defaultResultWindowLocked;
    app["lock_behavior"] = m_config.lockBehavior;
    app["prev_result_shortcut"] = m_config.prevResultShortcut;
    app["next_result_shortcut"] = m_config.nextResultShortcut;
    app["tag_hotkey"] = m_config.tagHotkey;
    app["retranslate_hotkey"] = m_config.retranslateHotkey;

    root["app_settings"] = app;
    return root;
}
