#ifndef CONFIGDIALOG_H
#define CONFIGDIALOG_H

#include <QDialog>
#include <QPushButton>
#include <QLineEdit>
#include <QTextEdit>
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

#include "ConfigManager.h"

#include <QListWidget>
#include <QNetworkAccessManager>
#include <QNetworkReply>

class QFileSystemWatcher;
class QTimer;

// ...

class ConfigDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ConfigDialog(ConfigManager *configManager, QWidget *parent = nullptr);
    ~ConfigDialog();

    void updateTheme(bool isDark);

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

    // Live refresh when profiles/*.json changes on disk
    void onProfilesDirChanged(const QString &path);
    void onProfileFileChanged(const QString &path);
    void onProfilesWatcherTimeout();

signals:
    void saved();

private:
    ConfigManager *m_configManager;

    QString defaultEndpointForProvider(const QString &provider) const;
    void maybeApplyEndpointDefaultForProvider(const QString &provider);
    bool m_isLoadingConfig = false;
    QString m_lastAutoEndpoint;

    // Profile UI
    QListWidget *m_profileList;
    QPushButton *m_newProfileBtn;
    QPushButton *m_deleteProfileBtn;
    QPushButton *m_renameProfileBtn;
    QPushButton *m_copyProfileBtn;
    QPushButton *m_importProfileBtn;
    QPushButton *m_exportProfileBtn;

    // Settings UI
    QLineEdit *m_apiKeyEdit;
    QLineEdit *m_baseUrlEdit;
    QLineEdit *m_endpointPathEdit = nullptr;
    QLineEdit *m_modelNameEdit;
    QPushButton *m_testConnectionBtn = nullptr;
    QTextEdit *m_promptEdit;
    QComboBox *m_apiProviderCombo;
    QLineEdit *m_proxyUrlEdit;
    QCheckBox *m_useProxyCheck;
    QLabel *m_proxyLabel;
    QSpinBox *m_targetScreenSpin;

    // Test connectivity helpers
    QNetworkAccessManager *m_testNam = nullptr;
    QPointer<QNetworkReply> m_testReply;

    QLineEdit *m_hotkeyEdit;
    QLineEdit *m_summaryHotkeyEdit;
    QLineEdit *m_settingsHotkeyEdit;
    QLineEdit *m_editHotkeyEdit;
    QLineEdit *m_viewToggleHotkeyEdit;
    QLineEdit *m_screenshotToggleHotkeyEdit;
    QLineEdit *m_selectionToggleHotkeyEdit;

    QLineEdit *m_boldHotkeyEdit;
    QLineEdit *m_underlineHotkeyEdit;
    QLineEdit *m_highlightHotkeyEdit;

    QLineEdit *m_highlightMarkColorEdit;
    QLineEdit *m_highlightMarkColorDarkEdit;
    QLabel *m_highlightMarkColorPreview;
    QLabel *m_highlightMarkColorDarkPreview;

    QTabWidget *m_tabWidget;
    // Tabs
    QWidget *m_generalTab;
    QWidget *m_transTab;   // Translation Window
    QWidget *m_archiveTab; // Archive Window
    QWidget *m_otherTab;   // Other
    // Deprecated m_advTab variable to be removed or reused for one of these, removing to avoid confusion

    // Translation Tab UI
    QCheckBox *m_defaultLookCheck;
    QComboBox *m_lockBehaviorCombo;
    QLineEdit *m_prevPageHotkeyEdit;
    QLineEdit *m_nextPageHotkeyEdit;
    QLineEdit *m_tagHotkeyEdit;

    QLineEdit *m_cardBorderColorEdit;
    QLabel *m_lblCardBorderColor;
    QLabel *m_cardBorderColorPreview;
    QCheckBox *m_useBorderCheck;
    QSpinBox *m_initialFontSizeSpin;
    QLabel *m_lblInitialFontSize;
    QDoubleSpinBox *m_zoomSensitivitySpin;

    QCheckBox *m_showPreviewCheck;
    QCheckBox *m_showResultCheck;
    QCheckBox *m_debugModeCheck;

    QCheckBox *m_enableQuitHotkeyCheck;
    QLabel *m_quitHotkeyLabel;
    QLineEdit *m_quitHotkeyEdit;

    QLineEdit *m_storagePathEdit;

    void loadFromConfig();
    void updateProfileList();

    void setupProfilesWatcher();
    void refreshProfilesWatcherPaths();

    QFileSystemWatcher *m_profilesWatcher = nullptr;
    QTimer *m_profilesWatchTimer = nullptr;
    bool m_profilesDirDirty = false;
    QString m_profilesChangedFile;

    // Localization
    QComboBox *m_languageCombo;
    QComboBox *m_screenCombo;
    void retranslateUi();

    // CDN Removed
};

#endif // CONFIGDIALOG_H
