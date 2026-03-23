#ifndef CONFIGDIALOG_H
#define CONFIGDIALOG_H

#include <QDialog>
#include <QPushButton>
#include <QLineEdit>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QGroupBox>
#include <QTabWidget>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPointer>
#include <QPalette>
#include <QJsonDocument>
#include <QJsonObject>
#include <QHash>

#include "ConfigManager.h"

#include <QListWidget>
#include <QNetworkAccessManager>
#include <QNetworkReply>

class QFileSystemWatcher;
class QColor;
class QTimer;
class HistoryManager;
class QEvent;

// ...

class ConfigDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ConfigDialog(ConfigManager *configManager, HistoryManager *historyManager = nullptr, QWidget *parent = nullptr);
    ~ConfigDialog();

    void updateTheme(bool isDark);
    void setGlobalHotkeyConflictKeys(const QStringList &labelKeys, bool focusConflicts = false);

private slots:
    void save();
    void onTestConnection();
    void onProfileChanged(const QString &name);
    void newProfile();
    void deleteProfile();
    void renameProfile();
    void copyProfile();
    void importProfile();
    void exportProfile();
    void onImportLegacyHistory();
    void onExportHistory();

    // Live refresh when profiles/*.json changes on disk
    void onProfilesDirChanged(const QString &path);
    void onProfileFileChanged(const QString &path);
    void onProfilesWatcherTimeout();

signals:
    void saved();
    void languageChanged(const QString &lang);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    ConfigManager *m_configManager;
    HistoryManager *m_historyManager = nullptr;

    QString defaultEndpointForProvider(const QString &provider) const;
    void maybeApplyEndpointDefaultForProvider(const QString &provider);
    void setupDialogUi();
    void setupProfileSection(QVBoxLayout *mainLayout);
    void setupTabs(QVBoxLayout *mainLayout);
    void setupGeneralTab();
    void setupTranslationTab();
    void setupArchiveTab();
    void setupOtherTab();
    void setupAdvancedApiTab();
    void setupActionButtons(QVBoxLayout *mainLayout);
    bool tryParseColorText(QString text, QColor &out) const;
    void updateColorPreviewLabel(QLabel *label, const QString &text) const;
    void browseForStoragePath();
    bool validateStoragePathInput(const QString &pathText, QString *resolvedPath = nullptr);
    void refreshStoragePathPlaceholder();
    QString buildAdvancedTemplateFromRegular(const QString &provider) const;
    QString normalizeProviderForAdvancedTemplate(const QString &provider) const;
    void syncAdvancedTemplateFromRegular();
    void syncRegularFieldsFromAdvancedTemplate();
    void updateAdvancedApiUiState();
    void ensureRegularApiInteractionHooks();
    void updateRegularApiTextGrayState(bool advancedOn);
    QList<QWidget *> regularApiControlWidgets() const;
    QList<QLabel *> regularApiLabelWidgets() const;
    void applyRegularApiTextColor(QWidget *widget, bool advancedOn);
    void recordRegularApiClickAndMaybeWarn(QObject *clickedObject);
    void updateAdvancedTemplateStatusLabel();
    void updateAdvancedJsonFieldsButtonState();
    void ensureAdvancedProviderOption(bool enabled);
    void resetAdvancedApiToDefault();
    void onTestAdvancedApi();
    void onPickAdvancedJsonFields();
    bool parseAdvancedTemplateJson(QJsonObject &outRoot, QString &outError) const;
    bool m_isLoadingConfig = false;
    bool m_isSyncingAdvanced = false;
    bool m_advancedTemplateDetached = false;
    QString m_lastAutoEndpoint;
    QString m_lastRegularProvider = "openai";
    bool m_isDarkTheme = false;
    QObject *m_lastRegularApiClickObject = nullptr;
    qint64 m_lastRegularApiClickMs = 0;
    qint64 m_advancedJsonFieldsLastDblClickMs = 0;
    bool m_advancedJsonFieldsDoubleClickArmed = false;
    QHash<QWidget *, QPalette> m_regularApiOriginalPalettes;

    // Profile UI
    QGroupBox *m_profileGroup = nullptr;
    QListWidget *m_profileList = nullptr;
    QPushButton *m_newProfileBtn = nullptr;
    QPushButton *m_deleteProfileBtn = nullptr;
    QPushButton *m_renameProfileBtn = nullptr;
    QPushButton *m_copyProfileBtn = nullptr;
    QPushButton *m_importProfileBtn = nullptr;
    QPushButton *m_exportProfileBtn = nullptr;

    // Settings UI
    QLineEdit *m_apiKeyEdit = nullptr;
    QLineEdit *m_baseUrlEdit = nullptr;
    QLineEdit *m_endpointPathEdit = nullptr;
    QLineEdit *m_modelNameEdit = nullptr;
    QPushButton *m_testConnectionBtn = nullptr;
    QTextEdit *m_promptEdit = nullptr;
    QComboBox *m_apiProviderCombo = nullptr;
    QLineEdit *m_proxyUrlEdit = nullptr;
    QCheckBox *m_useProxyCheck = nullptr;
    QLabel *m_proxyLabel = nullptr;
    QSpinBox *m_targetScreenSpin = nullptr;

    // Test connectivity helpers
    QNetworkAccessManager *m_testNam = nullptr;
    QPointer<QNetworkReply> m_testReply;

    QLineEdit *m_hotkeyEdit = nullptr;
    QLineEdit *m_batchScreenshotToggleHotkeyEdit = nullptr;
    QLineEdit *m_summaryHotkeyEdit = nullptr;
    QLineEdit *m_settingsHotkeyEdit = nullptr;
    QLineEdit *m_editHotkeyEdit = nullptr;
    QLineEdit *m_viewToggleHotkeyEdit = nullptr;
    QLineEdit *m_screenshotToggleHotkeyEdit = nullptr;
    QLineEdit *m_selectionToggleHotkeyEdit = nullptr;
    QComboBox *m_archiveLoadModeCombo = nullptr;
    QSpinBox *m_archivePageSizeSpin = nullptr;

    QLineEdit *m_boldHotkeyEdit = nullptr;
    QLineEdit *m_underlineHotkeyEdit = nullptr;
    QLineEdit *m_highlightHotkeyEdit = nullptr;

    QLineEdit *m_highlightMarkColorEdit = nullptr;
    QLineEdit *m_highlightMarkColorDarkEdit = nullptr;
    QLabel *m_highlightMarkColorPreview = nullptr;
    QLabel *m_highlightMarkColorDarkPreview = nullptr;
    QPushButton *m_importLegacyHistoryBtn = nullptr;
    QPushButton *m_exportHistoryBtn = nullptr;

    QTabWidget *m_tabWidget = nullptr;
    // Tabs
    QWidget *m_generalTab = nullptr;
    QFormLayout *m_generalFormLayout = nullptr;
    QWidget *m_transTab = nullptr;   // Translation Window
    QWidget *m_archiveTab = nullptr; // Archive Window
    QWidget *m_otherTab = nullptr;   // Other
    QWidget *m_advancedApiTab = nullptr;
    // Deprecated m_advTab variable to be removed or reused for one of these, removing to avoid confusion

    // Translation Tab UI
    QCheckBox *m_defaultLookCheck = nullptr;
    QComboBox *m_lockBehaviorCombo = nullptr;
    QLineEdit *m_prevPageHotkeyEdit = nullptr;
    QLineEdit *m_nextPageHotkeyEdit = nullptr;
    QLineEdit *m_tagHotkeyEdit = nullptr;
    QLineEdit *m_retranslateHotkeyEdit = nullptr;

    QLineEdit *m_cardBorderColorEdit = nullptr;
    QLabel *m_batchScreenshotToggleHotkeyLabel = nullptr;
    QLabel *m_lblCardBorderColor = nullptr;
    QLabel *m_cardBorderColorPreview = nullptr;
    QCheckBox *m_useBorderCheck = nullptr;
    QSpinBox *m_initialFontSizeSpin = nullptr;
    QLabel *m_lblInitialFontSize = nullptr;
    QDoubleSpinBox *m_zoomSensitivitySpin = nullptr;

    QCheckBox *m_showPreviewCheck = nullptr;
    QCheckBox *m_showResultCheck = nullptr;
    QCheckBox *m_launchAtStartupCheck = nullptr;
    QCheckBox *m_enableUmamiAnalyticsCheck = nullptr;
    QCheckBox *m_debugModeCheck = nullptr;

    QCheckBox *m_enableQuitHotkeyCheck = nullptr;
    QLabel *m_quitHotkeyLabel = nullptr;
    QLineEdit *m_quitHotkeyEdit = nullptr;

    QLineEdit *m_storagePathEdit = nullptr;
    QPushButton *m_browseBtn = nullptr;

    // Advanced API Tab UI
    QCheckBox *m_enableAdvancedApiCheck = nullptr;
    QPushButton *m_deleteAdvancedApiConfigBtn = nullptr;
    QLabel *m_advancedTemplateStatusLabel = nullptr;
    QPlainTextEdit *m_advancedApiTemplateEdit = nullptr;
    QPushButton *m_testAdvancedApiBtn = nullptr;
    QPushButton *m_pickAdvancedJsonFieldsBtn = nullptr;
    QLabel *m_advancedDebugDisplayLabel = nullptr;
    QCheckBox *m_showAdvancedDebugInResultCheck = nullptr;
    QCheckBox *m_showAdvancedDebugInArchiveCheck = nullptr;
    QPlainTextEdit *m_advancedApiResultEdit = nullptr;
    QJsonDocument m_lastAdvancedApiTestJson;
    bool m_hasLastAdvancedApiTestJson = false;

    void loadFromConfig();
    void updateProfileList();

    void setupProfilesWatcher();
    void refreshProfilesWatcherPaths();

    QFileSystemWatcher *m_profilesWatcher = nullptr;
    QTimer *m_profilesWatchTimer = nullptr;
    bool m_profilesDirDirty = false;
    QString m_profilesChangedFile;

    // Localization
    QComboBox *m_languageCombo = nullptr;
    QComboBox *m_screenCombo = nullptr;
    void retranslateUi();
    QLineEdit *globalHotkeyEditForKey(const QString &labelKey) const;
    void applyGlobalHotkeyConflictIndicators(bool focusConflicts);

    QStringList m_globalHotkeyConflictKeys;

    // CDN Removed
};

#endif // CONFIGDIALOG_H
