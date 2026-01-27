#include "ConfigManager.h"
#include <QDebug>
#include <QCoreApplication>

#include <QJsonParseError>

static QString findDefaultProfileTemplatePath()
{
    const QString baseDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        QDir::cleanPath(baseDir + "/assets/default_profile.json"),
        QDir::cleanPath(baseDir + "/../assets/default_profile.json"),
        QDir::cleanPath(baseDir + "/../../assets/default_profile.json"),
    };
    for (const auto &p : candidates)
    {
        if (QFile::exists(p))
            return p;
    }

    // Resource fallback (embedded via assets.qrc)
    if (QFile::exists(QStringLiteral(":/assets/default_profile.json")))
        return QStringLiteral(":/assets/default_profile.json");

    return QString();
}

static bool tryLoadDefaultProfileTemplate(QJsonObject &outRoot)
{
    const QString path = findDefaultProfileTemplatePath();
    if (path.isEmpty())
    {
        qWarning() << "[ConfigManager] default_profile.json not found. Looked in:"
                   << "<exe>/assets, <exe>/../assets, <exe>/../../assets, :/assets/default_profile.json";
        return false;
    }

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    const QByteArray data = f.readAll();
    f.close();

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (doc.isNull() || !doc.isObject() || err.error != QJsonParseError::NoError)
    {
        qWarning() << "[ConfigManager] default_profile.json parse failed:" << path
                   << "error=" << err.errorString() << "offset=" << err.offset;
        return false;
    }

    qInfo() << "[ConfigManager] Loaded default profile template:" << path
            << "source=" << (path.startsWith(":/") ? "resource" : "disk");
    outRoot = doc.object();
    return true;
}

static const QString kAppDataFolderName = QStringLiteral("AI-Screenshot-Translator");

QString ConfigManager::appDataDirPath()
{
    // Force Roaming on Windows: %APPDATA% (C:\Users\<u>\AppData\Roaming)
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

ConfigManager::ConfigManager()
{
    // AppData storage
    m_appDataDir = appDataDirPath();
    m_profilesDir = m_appDataDir + "/profiles";

    QDir dir(m_profilesDir);
    if (!dir.exists())
    {
        dir.mkpath(".");
    }

    loadMeta(); // Load last used profile name

    if (m_currentProfileName.isEmpty() || !listProfiles().contains(m_currentProfileName))
    {
        // First run or missing profile
        if (listProfiles().isEmpty())
        {
            // No profiles at all, don't create yet? Or create default?
            // "Internal storage" implies we create one if none.
            // But requirement says "First open... auto jump to config".
            // Let's create a "Default" profile if none exists, to have something to load.
            // Or leave it empty and let Dialog handle it?
            // "loadConfig" usually needs *something*.
            // Let's safe-fall back to "Default".
            m_currentProfileName = "Default";
            if (!loadProfile("Default"))
            {
                // Create Default using the template (if provided in assets/)
                QJsonObject root;
                if (tryLoadDefaultProfileTemplate(root))
                {
                    m_config = AppConfig();
                    parseJson(root);

                    qInfo() << "[ConfigManager] default_profile.json prompt_prefix="
                            << m_config.promptText.left(80).replace("\n", "\\n");
                }
                saveConfig(); // Creates Default
            }
        }
        else
        {
            // Pick first available
            m_currentProfileName = listProfiles().first();
            loadProfile(m_currentProfileName);
        }
    }
    else
    {
        loadProfile(m_currentProfileName);
    }
}

QStringList ConfigManager::listProfiles() const
{
    QDir dir(m_profilesDir);
    QStringList filters;
    filters << "*.json";
    QStringList files = dir.entryList(filters, QDir::Files);
    QStringList profiles;
    for (const QString &f : files)
    {
        profiles << f.section(".json", 0, 0);
    }
    return profiles;
}

bool ConfigManager::createProfile(const QString &name)
{
    if (name.isEmpty())
        return false;
    QString path = m_profilesDir + "/" + name + ".json";
    if (QFile::exists(path))
        return false; // Already exists

    // Create with current/default settings
    if (name == "Default")
    {
        QJsonObject root;
        if (tryLoadDefaultProfileTemplate(root))
        {
            m_config = AppConfig();
            parseJson(root);
        }
        else
        {
            m_config = AppConfig();
        }
    }
    else
    {
        AppConfig defaultCfg;
        m_config = defaultCfg;
    }
    m_currentProfileName = name;
    saveConfig();
    saveMeta();
    return true;
}

bool ConfigManager::loadProfile(const QString &name)
{
    QString path = m_profilesDir + "/" + name + ".json";
    QFile file(path);
    if (!file.exists())
        return false;

    if (!file.open(QIODevice::ReadOnly))
        return false;
    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull())
        return false;

    parseJson(doc.object());
    m_currentProfileName = name;
    saveMeta();
    return true;
}

bool ConfigManager::deleteProfile(const QString &name)
{
    if (name == "Default")
        return false;
    QString path = m_profilesDir + "/" + name + ".json";
    if (QFile::remove(path))
    {
        if (m_currentProfileName == name)
        {
            loadProfile("Default");
        }
        return true;
    }
    return false;
}

bool ConfigManager::renameProfile(const QString &oldName, const QString &newName)
{
    if (oldName.isEmpty() || newName.isEmpty() || oldName == "Default")
        return false;

    QString oldPath = m_profilesDir + "/" + oldName + ".json";
    QString newPath = m_profilesDir + "/" + newName + ".json";

    if (!QFile::exists(oldPath))
        return false;
    if (QFile::exists(newPath))
        return false;

    if (QFile::rename(oldPath, newPath))
    {
        if (m_currentProfileName == oldName)
        {
            m_currentProfileName = newName;
            saveMeta();
        }
        return true;
    }
    return false;
}

bool ConfigManager::copyProfile(const QString &sourceName, const QString &newName)
{
    if (sourceName.isEmpty() || newName.isEmpty())
        return false;

    QString srcPath = m_profilesDir + "/" + sourceName + ".json";
    QString destPath = m_profilesDir + "/" + newName + ".json";

    if (!QFile::exists(srcPath))
        return false;
    if (QFile::exists(destPath))
        return false;

    return QFile::copy(srcPath, destPath);
}

bool ConfigManager::importProfile(const QString &path)
{
    QFile src(path);
    if (!src.open(QIODevice::ReadOnly))
        return false;
    QByteArray data = src.readAll();
    src.close();

    // Validate JSON
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject())
        return false;

    // Determine name from filename
    QString name = QFileInfo(path).baseName();
    QString dest = m_profilesDir + "/" + name + ".json";

    // Avoid overwrite? Or auto-rename?
    int counter = 1;
    while (QFile::exists(dest))
    {
        dest = m_profilesDir + "/" + name + QString("_%1").arg(counter++) + ".json";
    }

    QFile destFile(dest);
    if (!destFile.open(QIODevice::WriteOnly))
        return false;
    destFile.write(data);
    destFile.close();

    return true;
}

bool ConfigManager::exportProfile(const QString &name, const QString &destPath)
{
    QString srcPath = m_profilesDir + "/" + name + ".json";
    return QFile::copy(srcPath, destPath);
}

QString ConfigManager::currentProfileName() const
{
    return m_currentProfileName;
}

AppConfig ConfigManager::getConfig() const
{
    return m_config;
}

void ConfigManager::setConfig(const AppConfig &config)
{
    m_config = config;
    qInfo() << "[ConfigManager] setConfig received:";
    qInfo() << "  Prev Shortcut:" << m_config.prevResultShortcut;
    qInfo() << "  Next Shortcut:" << m_config.nextResultShortcut;
    saveConfig();
}

void ConfigManager::saveConfig()
{
    QString path = m_profilesDir + "/" + m_currentProfileName + ".json";
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
    {
        qWarning() << "Failed to save config profile:" << path;
        return;
    }
    QJsonDocument doc(toJson());
    file.write(doc.toJson());
    file.close();
}

QString ConfigManager::configFilePath() const
{
    return m_profilesDir + "/" + m_currentProfileName + ".json";
}

QString ConfigManager::profilesDirPath() const
{
    return QDir::cleanPath(m_profilesDir);
}

void ConfigManager::loadMeta()
{
    QFile file(settingsJsonPath());
    if (file.open(QIODevice::ReadOnly))
    {
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        m_currentProfileName = doc.object()["last_profile"].toString();
    }
}

void ConfigManager::saveMeta()
{
    QFile file(settingsJsonPath());
    if (file.open(QIODevice::WriteOnly))
    {
        QJsonObject root;
        root["last_profile"] = m_currentProfileName;
        file.write(QJsonDocument(root).toJson());
    }
}

void ConfigManager::parseJson(const QJsonObject &root)
{
    if (root.contains("api") && root["api"].isObject())
    {
        QJsonObject api = root["api"].toObject();
        m_config.apiKey = api["api_key"].toString();
        m_config.baseUrl = api["base_url"].toString();
        m_config.modelName = api["model"].toString();

        // Use default prompt if empty or not present
        QString promptFromConfig = api["prompt_text"].toString();
        if (promptFromConfig.isEmpty())
        {
            m_config.promptText = "# Role\n你是一个专业的学术翻译专家，擅长中英文互译，并能精确处理数学、物理等领域的 LaTeX 公式。\n\n# Task\n请将图片中的英文内容翻译为中文。\n\n# Requirements\n1. **仅返回翻译后的中文文本**。严禁包含任何开场白、解释语（如\"好的\"、\"这是翻译内容\"）或结束语。\n2. **数学公式处理**：原文中所有的数学变量、常数、公式和方程必须使用 LaTeX 格式输出（例如使用 $x$ 或 $$E=mc^2$$）。\n3. **术语准确**：保持专业领域的术语翻译严谨、自然。\n4. **格式对齐**：保持原有的段落结构。\n\n# Content";
        }
        else
        {
            m_config.promptText = promptFromConfig;
        }

        m_config.proxyUrl = api["proxy"].toString();
        m_config.useProxy = api["use_proxy"].toBool(false);
        m_config.apiProvider = api["provider"].toString("openai");
    }

    if (root.contains("app_settings") && root["app_settings"].isObject())
    {
        QJsonObject app = root["app_settings"].toObject();
        m_config.language = app["language"].toString("zh"); // Default Chinese
        m_config.zoomSensitivity = app["zoom_sensitivity"].toDouble(500.0);
        m_config.screenshotHotkey = app["screenshot_hotkey"].toString("ctrl+alt+s");

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

        m_config.boldHotkey = app["bold_hotkey"].toString("ctrl+b");
        m_config.underlineHotkey = app["underline_hotkey"].toString("ctrl+u");
        m_config.highlightHotkey = app["highlight_hotkey"].toString("ctrl+h");

        m_config.highlightMarkColor = app["highlight_mark_color"].toString("#ffeb3b");
        m_config.highlightMarkColorDark = app["highlight_mark_color_dark"].toString("#d4af37");

        m_config.initialFontSize = app["initial_font_size"].toInt(16);
        m_config.cardBorderColor = app["card_border_color"].toString("100,100,100");
        m_config.debugMode = app["debug_mode"].toBool(false);
        m_config.showPreviewCard = app["show_preview_card"].toBool(true);
        if (app.contains("show_result_window"))
        {
            m_config.showResultWindow = app["show_result_window"].toBool(true);
        }
        if (app.contains("use_card_border"))
        {
            m_config.useCardBorder = app["use_card_border"].toBool(true);
        }
        if (app.contains("summary_geometry"))
        {
            m_config.summaryWindowGeometry = QByteArray::fromBase64(app["summary_geometry"].toString().toLatin1());
        }
        if (app.contains("summary_zoom"))
        {
            m_config.summaryWindowZoom = app["summary_zoom"].toDouble(1.0);
        }
        if (app.contains("config_geometry"))
        {
            m_config.configWindowGeometry = QByteArray::fromBase64(app["config_geometry"].toString().toLatin1());
        }

        m_config.storagePath = app["storage_path"].toString("");

        // Lock Settings
        // Default should be UNLOCKED unless explicitly set by user.
        m_config.defaultResultWindowLocked = app["default_result_window_locked"].toBool(false);
        m_config.lockBehavior = app["lock_behavior"].toInt(0);
        m_config.prevResultShortcut = app["prev_result_shortcut"].toString("z");
        m_config.nextResultShortcut = app["next_result_shortcut"].toString("x");
        m_config.tagHotkey = app["tag_hotkey"].toString("t");
    }
}

QJsonObject ConfigManager::toJson() const
{
    QJsonObject root;

    QJsonObject api;
    api["api_key"] = m_config.apiKey;
    api["base_url"] = m_config.baseUrl;
    api["model"] = m_config.modelName;
    api["prompt_text"] = m_config.promptText;
    api["provider"] = m_config.apiProvider;
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
    app["summary_hotkey"] = m_config.summaryHotkey;
    app["settings_hotkey"] = m_config.settingsHotkey;

    app["enable_quit_hotkey"] = m_config.enableQuitHotkey;
    app["quit_hotkey"] = m_config.quitHotkey;
    app["target_screen_index"] = m_config.targetScreenIndex;
    app["edit_hotkey"] = m_config.editHotkey;
    app["view_toggle_hotkey"] = m_config.viewToggleHotkey;
    app["screenshot_toggle_hotkey"] = m_config.screenshotToggleHotkey;
    app["selection_toggle_hotkey"] = m_config.selectionToggleHotkey;

    app["bold_hotkey"] = m_config.boldHotkey;
    app["underline_hotkey"] = m_config.underlineHotkey;
    app["highlight_hotkey"] = m_config.highlightHotkey;
    app["highlight_mark_color"] = m_config.highlightMarkColor;
    app["highlight_mark_color_dark"] = m_config.highlightMarkColorDark;
    app["initial_font_size"] = m_config.initialFontSize;
    app["card_border_color"] = m_config.cardBorderColor;
    app["debug_mode"] = m_config.debugMode;
    app["show_preview_card"] = m_config.showPreviewCard;
    app["show_result_window"] = m_config.showResultWindow;
    app["use_card_border"] = m_config.useCardBorder;

    if (!m_config.summaryWindowGeometry.isEmpty())
    {
        app["summary_geometry"] = QString::fromLatin1(m_config.summaryWindowGeometry.toBase64());
    }
    app["summary_zoom"] = m_config.summaryWindowZoom;
    if (!m_config.configWindowGeometry.isEmpty())
    {
        app["config_geometry"] = QString::fromLatin1(m_config.configWindowGeometry.toBase64());
    }
    app["storage_path"] = m_config.storagePath;

    app["default_result_window_locked"] = m_config.defaultResultWindowLocked;
    app["lock_behavior"] = m_config.lockBehavior;
    app["prev_result_shortcut"] = m_config.prevResultShortcut;
    app["next_result_shortcut"] = m_config.nextResultShortcut;
    app["tag_hotkey"] = m_config.tagHotkey;

    root["app_settings"] = app;

    return root;
}
