#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QString>
#include <QVariant>
#include <QJsonObject>
#include <QJsonDocument>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QMap>

struct AppConfig
{
    // API Settings
    QString apiKey;
    QString baseUrl;
    QString endpointPath; // e.g. /chat/completions or /v1/chat/completions
    QString modelName;
    QString promptText = "# Role\n你是一个专业的学术翻译专家，擅长中英文互译，并能精确处理数学、物理等领域的 LaTeX 公式。\n\n# Task\n请将图片中的英文内容翻译为中文。\n\n# Requirements\n1. **仅返回翻译后的中文文本**。严禁包含任何开场白、解释语（如\"好的\"、\"这是翻译内容\"）或结束语。\n2. **数学公式处理**：原文中所有的数学变量、常数、公式和方程必须使用 LaTeX 格式输出（例如使用 $x$ 或 $$E=mc^2$$）。\n3. **术语准确**：保持专业领域的术语翻译严谨、自然。\n4. **格式对齐**：保持原有的段落结构。\n\n# Content";
    QString proxyUrl;
    bool useProxy = false;
    QString apiProvider = "openai"; // "openai", "gemini", "claude"
    bool useAdvancedApiMode = false;
    QString advancedApiTemplate;
    bool advancedApiCustomized = false;

    // Localization
    QString language = "zh"; // Default to Chinese as requested

    // App Settings
    float zoomSensitivity = 500.0f;
    QString screenshotHotkey = "ctrl+alt+s";
    QString batchScreenshotToggleHotkey = "d";
    QString summaryHotkey = "alt+s";
    QString settingsHotkey = "";
    int targetScreenIndex = -1; // -1 = all screens, 0+ = specific screen index
    // Shortcuts for Summary Window
    QString editHotkey = "e";
    QString viewToggleHotkey = "r";
    QString screenshotToggleHotkey = "s";
    // Batch selection mode toggle inside SummaryWindow (HTML + Qt fallback)
    QString selectionToggleHotkey = "ctrl+s";
    bool archiveUsePagination = true;
    int archivePageSize = 50;
    bool showAdvancedDebugInResultWindow = true;
    bool showAdvancedDebugInArchiveWindow = false;

    // Formatting Shortcuts
    QString boldHotkey = "ctrl+b";
    QString underlineHotkey = "ctrl+u";
    QString highlightHotkey = "ctrl+h";

    // Highlight (<mark>) colors used in ResultWindow + SummaryWindow render/edit views.
    // Accepts CSS color like "#ffeb3b" or "rgb(255,235,59)".
    QString highlightMarkColor = "#ffeb3b";
    QString highlightMarkColorDark = "#d4af37";

    int initialFontSize = 16;
    QString cardBorderColor = "100,100,100";
    bool debugMode = false;
    bool launchAtStartup = false;
    bool enableUmamiAnalytics = true;

    // New Settings
    bool showPreviewCard = false;
    bool showResultWindow = true;
    bool useCardBorder = false; // Default to false (no border)

    // Storage
    QString storagePath = ""; // Empty string implies default "./storage"

    // Window State
    QByteArray summaryWindowGeometry;
    float summaryWindowZoom = 1.0f;
    QByteArray configWindowGeometry;

    // Lock Behavior
    bool defaultResultWindowLocked = false; // Default unlocked as per user request
    int lockBehavior = 0;                   // 0 = Reset to Unlocked, 1 = Maintain Previous State (User "Interrupt" vs "Continue")

    // Configurable Nav Hotkeys
    QString prevResultShortcut = "z";
    QString nextResultShortcut = "x";
    QString tagHotkey = "t";         // New: Tag Dialog Hotkey
    QString retranslateHotkey = "f"; // Retranslate shortcut (default: F)

    // Quit Hotkey
    bool enableQuitHotkey = false;
    QString quitHotkey = "alt+q";

    // CDN Settings Removed (User requested local bundling)
};

class ConfigManager
{
public:
    static QString appDataDirPath();
    static QString settingsJsonPath();
    static QString settingsIniPath();
    static QString defaultStoragePath();
    static QString resolveStoragePath(const QString &path);
    static QString resolveWritableStoragePath(const QString &path, bool *usedFallback = nullptr,
                                              QString *errorMessage = nullptr);
    static bool ensureWritableDirectory(const QString &path, QString *errorMessage = nullptr);

    ConfigManager();
    ~ConfigManager() = default;

    // Profile Management
    QStringList listProfiles() const;
    bool createProfile(const QString &name);
    bool loadProfile(const QString &name);
    bool deleteProfile(const QString &name);
    bool renameProfile(const QString &oldName, const QString &newName);
    bool copyProfile(const QString &sourceName, const QString &newName);
    bool importProfile(const QString &path);
    bool exportProfile(const QString &name, const QString &destPath);
    QString currentProfileName() const;
    void saveConfig(); // Saves to current profile

    AppConfig getConfig() const;
    void setConfig(const AppConfig &config);

    QString configFilePath() const;  // Current profile path
    QString profilesDirPath() const; // Profiles directory path

private:
    QString m_appDataDir;
    QString m_profilesDir;
    QString m_currentProfileName;
    AppConfig m_config;

    void parseJson(const QJsonObject &root);
    QJsonObject toJson() const;

    // Meta settings (active profile)
    void loadMeta();
    void saveMeta();
};

#endif // CONFIGMANAGER_H
