#include "ConfigDialog.h"
#include "ThemeUtils.h"
#include "TranslationManager.h"
#include <QFormLayout>
#include <QPushButton>
#include <QMessageBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QTimer>
#include <QFileDialog>
#include <QGuiApplication>
#include <QScreen>
#include <QLabel>
#include <QGroupBox>
#include <QPointer>

ConfigDialog::ConfigDialog(ConfigManager *configManager, QWidget *parent)
    : QDialog(parent), m_configManager(configManager) {
    setWindowTitle("Settings");
    
    // Initial Theme Update
    updateTheme(ThemeUtils::isSystemDark());
    // resize(500, 700); // Removed fixed size to let layout decide

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    
    // --- Profile Section ---
    QGroupBox *profileGroup = new QGroupBox("Profiles", this);
    QHBoxLayout *profileMainLayout = new QHBoxLayout(profileGroup);
    
    m_profileList = new QListWidget(this);
    m_profileList->setFixedHeight(60); // Roughly height of 2 rows of buttons
    connect(m_profileList, &QListWidget::currentTextChanged, this, &ConfigDialog::onProfileChanged);
    
    m_newProfileBtn = new QPushButton("New", this);
    connect(m_newProfileBtn, &QPushButton::clicked, this, &ConfigDialog::newProfile);
    
    m_deleteProfileBtn = new QPushButton("Delete", this);
    connect(m_deleteProfileBtn, &QPushButton::clicked, this, &ConfigDialog::deleteProfile);
    
    m_renameProfileBtn = new QPushButton("Rename", this);
    connect(m_renameProfileBtn, &QPushButton::clicked, this, &ConfigDialog::renameProfile);
    
    m_copyProfileBtn = new QPushButton("Copy", this);
    connect(m_copyProfileBtn, &QPushButton::clicked, this, &ConfigDialog::copyProfile);
    
    m_importProfileBtn = new QPushButton("Import", this);
    m_importProfileBtn->setToolTip("Import Profile");
    connect(m_importProfileBtn, &QPushButton::clicked, this, &ConfigDialog::importProfile);
    
    m_exportProfileBtn = new QPushButton("Export", this);
    m_exportProfileBtn->setToolTip("Export Profile");
    connect(m_exportProfileBtn, &QPushButton::clicked, this, &ConfigDialog::exportProfile);

    QGridLayout *buttonGrid = new QGridLayout();
    buttonGrid->addWidget(m_newProfileBtn, 0, 0);
    buttonGrid->addWidget(m_deleteProfileBtn, 0, 1);
    buttonGrid->addWidget(m_importProfileBtn, 0, 2);
    buttonGrid->addWidget(m_renameProfileBtn, 1, 0);
    buttonGrid->addWidget(m_copyProfileBtn, 1, 1);
    buttonGrid->addWidget(m_exportProfileBtn, 1, 2);
    
    profileMainLayout->addWidget(m_profileList, 1);
    profileMainLayout->addLayout(buttonGrid);
    
    mainLayout->addWidget(profileGroup);

    // --- Language Section (Top) ---
    // Added early to affect layout if needed
    
    // --- Tabs ---
    m_tabWidget = new QTabWidget(this);
    mainLayout->addWidget(m_tabWidget);

    // 1. General Tab
    m_generalTab = new QWidget();
    QFormLayout *layout = new QFormLayout(m_generalTab);
    
    m_languageCombo = new QComboBox(this);
    m_languageCombo->addItem("Chinese", "zh");
    m_languageCombo->addItem("English", "en");
    layout->addRow("Language:", m_languageCombo);
    
    connect(m_languageCombo, &QComboBox::currentIndexChanged, [this](int index){
        QString lang = m_languageCombo->currentData().toString();
        TranslationManager::instance().setLanguage(lang);
        retranslateUi();
        
        AppConfig cfg = m_configManager->getConfig();
        cfg.language = lang;
        m_configManager->setConfig(cfg);
    });
    
    m_screenCombo = new QComboBox(this);
    QList<QScreen*> screens = QGuiApplication::screens();
    for (int i = 0; i < screens.size(); ++i) {
        m_screenCombo->addItem(screens[i]->name(), i);
    }
    layout->addRow("Capture Screen:", m_screenCombo);

    m_apiKeyEdit = new QLineEdit(this);
    m_apiKeyEdit->setEchoMode(QLineEdit::Password);
    layout->addRow("API Key:", m_apiKeyEdit);

    m_apiProviderCombo = new QComboBox(this);
    m_apiProviderCombo->addItem("OpenAI", "openai");
    m_apiProviderCombo->addItem("Google Gemini", "gemini");
    m_apiProviderCombo->addItem("Anthropic Claude", "claude");
    layout->addRow("API Provider:", m_apiProviderCombo);

    m_baseUrlEdit = new QLineEdit(this);
    layout->addRow("Base URL:", m_baseUrlEdit);

    m_modelNameEdit = new QLineEdit(this);
    layout->addRow("Model:", m_modelNameEdit);
    
    m_promptEdit = new QTextEdit(this);
    m_promptEdit->setMaximumHeight(60);
    layout->addRow("Prompt:", m_promptEdit);

    m_proxyUrlEdit = new QLineEdit(this);
    m_proxyUrlEdit->setPlaceholderText("例如 http://127.0.0.1:1080 或 socks5://127.0.0.1:1080");
    m_useProxyCheck = new QCheckBox(this);
    m_proxyLabel = new QLabel(this);
    
    QHBoxLayout *proxyLayout = new QHBoxLayout();
    proxyLayout->addWidget(m_proxyUrlEdit);
    proxyLayout->addWidget(m_useProxyCheck);
    layout->addRow(m_proxyLabel, proxyLayout);
    

    m_storagePathEdit = new QLineEdit(this);
    m_storagePathEdit->setPlaceholderText("./storage"); 
    QPushButton *browseBtn = new QPushButton("Browse...", this);
    connect(browseBtn, &QPushButton::clicked, [this](){
        QString dir = QFileDialog::getExistingDirectory(this, "Select Storage Directory", m_storagePathEdit->text());
        if (!dir.isEmpty()) {
            m_storagePathEdit->setText(dir);
        }
    });
    
    QHBoxLayout *storageLayout = new QHBoxLayout();
    storageLayout->addWidget(m_storagePathEdit);
    storageLayout->addWidget(browseBtn);
    layout->addRow("Storage Path:", storageLayout);

    m_showPreviewCheck = new QCheckBox("Show Preview Card after Screenshot", this);
    layout->addRow(m_showPreviewCheck);
    
    m_showResultCheck = new QCheckBox("Show Translation Result after Screenshot", this);
    layout->addRow(m_showResultCheck);
    
    m_tabWidget->addTab(m_generalTab, "General");

    // 2. Translation Tab
    m_transTab = new QWidget();
    QVBoxLayout *transMainLayout = new QVBoxLayout(m_transTab);
    
    // Group 1: Screenshot Card Settings
    QGroupBox *grpCard = new QGroupBox("Screenshot Card Settings", m_transTab);
    grpCard->setObjectName("grpCard"); 
    QFormLayout *cardLayout = new QFormLayout(grpCard);
    
    m_zoomSensitivitySpin = new QDoubleSpinBox(this);
    m_zoomSensitivitySpin->setRange(10.0, 2000.0);
    m_zoomSensitivitySpin->setSingleStep(10.0);
    cardLayout->addRow("Zoom Sensitivity:", m_zoomSensitivitySpin);
    
    m_cardBorderColorEdit = new QLineEdit(this);
    m_cardBorderColorEdit->setPlaceholderText("R,G,B (e.g., 100,100,100)");
    m_useBorderCheck = new QCheckBox(this);
    m_lblCardBorderColor = new QLabel(this);
    
    QHBoxLayout *borderColorLayout = new QHBoxLayout();
    borderColorLayout->addWidget(m_cardBorderColorEdit);
    borderColorLayout->addWidget(m_useBorderCheck);
    
    cardLayout->addRow(m_lblCardBorderColor, borderColorLayout);
    
    transMainLayout->addWidget(grpCard);

    // Group 2: Translation Window Settings
    QGroupBox *grpTrans = new QGroupBox("Translation Window Settings", m_transTab);
    grpTrans->setObjectName("grpTrans");
    QFormLayout *transWinLayout = new QFormLayout(grpTrans);
    
    m_initialFontSizeSpin = new QSpinBox(this);
    m_initialFontSizeSpin->setRange(8, 72);
    m_lblInitialFontSize = new QLabel(this); // Create the label
    transWinLayout->addRow(m_lblInitialFontSize, m_initialFontSizeSpin);

    m_defaultLookCheck = new QCheckBox("Default Window State is Locked", this);
    transWinLayout->addRow(m_defaultLookCheck);
    
    m_lockBehaviorCombo = new QComboBox(this);
    m_lockBehaviorCombo->addItem("Reset to Unlocked", 0);
    m_lockBehaviorCombo->addItem("Maintain Previous State", 1);
    transWinLayout->addRow("After Closing Locked Windows:", m_lockBehaviorCombo);

    m_prevPageHotkeyEdit = new QLineEdit(this);
    transWinLayout->addRow("Prev Page Hotkey:", m_prevPageHotkeyEdit);
    
    m_nextPageHotkeyEdit = new QLineEdit(this);
    transWinLayout->addRow("Next Page Hotkey:", m_nextPageHotkeyEdit);
    
    m_tagHotkeyEdit = new QLineEdit(this);
    transWinLayout->addRow("Tag Dialog Hotkey:", m_tagHotkeyEdit);
    
    transMainLayout->addWidget(grpTrans);
    transMainLayout->addStretch(); 

    m_tabWidget->addTab(m_transTab, "Translation");

    // 3. Archive Interface Tab
    m_archiveTab = new QWidget();
    QVBoxLayout *archiveMainLayout = new QVBoxLayout(m_archiveTab);

    // Group: View Toggle
    QGroupBox *grpView = new QGroupBox("View Toggle", m_archiveTab);
    grpView->setObjectName("grpView");
    QFormLayout *viewLayout = new QFormLayout(grpView);

    m_viewToggleHotkeyEdit = new QLineEdit(this);
    viewLayout->addRow("Summary View Toggle:", m_viewToggleHotkeyEdit); 

    m_screenshotToggleHotkeyEdit = new QLineEdit(this);
    viewLayout->addRow("Summary Screenshot Toggle:", m_screenshotToggleHotkeyEdit); 

    archiveMainLayout->addWidget(grpView);

    // Group: Edit Mode
    QGroupBox *grpEdit = new QGroupBox("Edit Mode", m_archiveTab);
    grpEdit->setObjectName("grpEdit");
    QFormLayout *editLayout = new QFormLayout(grpEdit);
    
    m_editHotkeyEdit = new QLineEdit(this);
    editLayout->addRow("Summary Edit Toggle:", m_editHotkeyEdit); 
    
    m_boldHotkeyEdit = new QLineEdit(this);
    editLayout->addRow("Bold Shortcut:", m_boldHotkeyEdit);
    
    m_underlineHotkeyEdit = new QLineEdit(this);
    editLayout->addRow("Underline Shortcut:", m_underlineHotkeyEdit);
    
    m_highlightHotkeyEdit = new QLineEdit(this);
    editLayout->addRow("Highlight Shortcut:", m_highlightHotkeyEdit);
    
    archiveMainLayout->addWidget(grpEdit);
    archiveMainLayout->addStretch();

    m_tabWidget->addTab(m_archiveTab, "Archive Interface");
    
    // 4. Other Tab
    m_otherTab = new QWidget();
    QVBoxLayout *otherMainLayout = new QVBoxLayout(m_otherTab); 
    
    // Group: Shortcut Settings
    QGroupBox *grpShortcuts = new QGroupBox("Shortcut Settings", m_otherTab);
    grpShortcuts->setObjectName("grpShortcuts");
    QFormLayout *shortcutsLayout = new QFormLayout(grpShortcuts);

    m_hotkeyEdit = new QLineEdit(this); 
    shortcutsLayout->addRow("Screenshot Hotkey:", m_hotkeyEdit); 
    
    m_summaryHotkeyEdit = new QLineEdit(this); 
    shortcutsLayout->addRow("Summary Hotkey:", m_summaryHotkeyEdit); 
    
    m_settingsHotkeyEdit = new QLineEdit(this); 
    shortcutsLayout->addRow("Settings Hotkey:", m_settingsHotkeyEdit); 
    
    otherMainLayout->addWidget(grpShortcuts);

    // Group: Advanced Settings
    QGroupBox *grpAdvanced = new QGroupBox("Advanced Settings", m_otherTab);
    grpAdvanced->setObjectName("grpAdvanced");
    QVBoxLayout *advLayout = new QVBoxLayout(grpAdvanced);
    
    m_debugModeCheck = new QCheckBox("Enable Debug Mode", this);
    advLayout->addWidget(m_debugModeCheck);
    
    otherMainLayout->addWidget(grpAdvanced);
    otherMainLayout->addStretch();
    
    m_tabWidget->addTab(m_otherTab, "Other");
    


    // Auto-resize window when switching tabs
    connect(m_tabWidget, &QTabWidget::currentChanged, [this](){
        this->layout()->setSizeConstraint(QLayout::SetFixedSize); 
        QTimer::singleShot(100, [this](){
             this->layout()->setSizeConstraint(QLayout::SetDefaultConstraint);
        });
    });
    
    // --- Buttons ---
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    QPushButton *saveBtn = new QPushButton("Save and Apply", this);
    saveBtn->setDefault(true); 
    connect(saveBtn, &QPushButton::clicked, this, &ConfigDialog::save);
    btnLayout->addWidget(saveBtn);
    
    mainLayout->addLayout(btnLayout);

    updateProfileList();
    loadFromConfig(); // Uses m_configManager -> AppConfig
    
    retranslateUi();
}

// ... retranslateUi implementation ...

void ConfigDialog::retranslateUi() {
    TranslationManager &tm = TranslationManager::instance();
    
    setWindowTitle(tm.tr("settings_title"));
    
    m_tabWidget->setTabText(0, tm.tr("tab_general"));
    m_tabWidget->setTabText(1, tm.tr("tab_translation"));

    
    // ... existing retranslate logic ...
    

    
    // ... rest of logic
    
    // General Form
    // Since QFormLayout stores LayoutItems, we need to access labels if possible.
    // But QFormLayout::labelForField works if we have the field widget.
    // However, labels are created implicitly.
    // To update them dynamically, strict approach: iterate form layout or store pointers.
    // Or just re-set labels on the layout using itemAt?
    // QFormLayout::itemAt(row, QFormLayout::LabelRole)->widget()->setText(...)
    
    auto updateLabel = [&](QFormLayout* layout, int row, const QString& key) {
        QWidget *w = layout->itemAt(row, QFormLayout::LabelRole)->widget();
        if (QLabel* l = qobject_cast<QLabel*>(w)) {
            l->setText(tm.tr(key));
        }
    };
    
    // ... existing general/advanced tab updates ...
    
    QFormLayout *gLayout = qobject_cast<QFormLayout*>(m_generalTab->layout());
    if (gLayout) {
        updateLabel(gLayout, 0, "lbl_language");
        // ... (rest of general tab updates)
        updateLabel(gLayout, 1, "lbl_capture_screen");  // New row
        updateLabel(gLayout, 2, "lbl_api_key");
        updateLabel(gLayout, 3, "lbl_api_provider");
        updateLabel(gLayout, 4, "lbl_base_url");
        updateLabel(gLayout, 5, "lbl_model");
        updateLabel(gLayout, 6, "lbl_prompt");
        updateLabel(gLayout, 7, "lbl_proxy");
        m_useProxyCheck->setToolTip(tm.tr("tip_proxy_toggle"));
        updateLabel(gLayout, 8, "lbl_storage");
        
        m_showPreviewCheck->setText(tm.tr("chk_preview"));
        m_showResultCheck->setText(tm.tr("chk_result"));
        
        QList<QPushButton*> btns = m_generalTab->findChildren<QPushButton*>();
        for(auto b : btns) {
            if (b->text().contains("Browse") || b->text().contains("浏览")) {
                b->setText(tm.tr("btn_browse"));
            }
        }
    }
    
    QGroupBox *grpCard = m_transTab->findChild<QGroupBox*>("grpCard");
    if (grpCard) {
        grpCard->setTitle(tm.tr("grp_card_settings"));
        m_lblCardBorderColor->setText(tm.tr("lbl_border_color"));
        m_useBorderCheck->setText(tm.tr("chk_use_border"));
        
        QFormLayout *layout = qobject_cast<QFormLayout*>(grpCard->layout());
        if (layout) {
            updateLabel(layout, 0, "lbl_zoom_sens");
        }
    }
    
    QGroupBox *grpTrans = m_transTab->findChild<QGroupBox*>("grpTrans");
    if (grpTrans) {
        grpTrans->setTitle(tm.tr("grp_trans_settings"));
        m_lblInitialFontSize->setText(tm.tr("lbl_font_size"));
        m_defaultLookCheck->setText(tm.tr("lbl_default_lock"));

        QFormLayout *layout = qobject_cast<QFormLayout*>(grpTrans->layout());
        if (layout) {
            updateLabel(layout, 2, "lbl_lock_behavior");
             m_lockBehaviorCombo->setItemText(0, tm.tr("opt_lock_reset"));
             m_lockBehaviorCombo->setItemText(1, tm.tr("opt_lock_keep"));
            updateLabel(layout, 3, "lbl_prev_hotkey");
            updateLabel(layout, 4, "lbl_next_hotkey");
            updateLabel(layout, 5, "lbl_tag_hotkey");
        }
    }

    // Archive Interface Tab
    m_tabWidget->setTabText(2, tm.tr("tab_archive_interface"));

    QGroupBox *grpView = m_archiveTab->findChild<QGroupBox*>("grpView");
    if (grpView) {
        grpView->setTitle(tm.tr("grp_view_toggle"));
        QFormLayout *layout = qobject_cast<QFormLayout*>(grpView->layout());
        if (layout) {
            updateLabel(layout, 0, "lbl_view_hotkey");
            updateLabel(layout, 1, "lbl_shot_toggle_hotkey");
        }
    }

    QGroupBox *grpEdit = m_archiveTab->findChild<QGroupBox*>("grpEdit");
    if (grpEdit) {
        grpEdit->setTitle(tm.tr("grp_edit_mode"));
        QFormLayout *layout = qobject_cast<QFormLayout*>(grpEdit->layout());
        if (layout) {
            updateLabel(layout, 0, "lbl_edit_hotkey");
            updateLabel(layout, 1, "lbl_bold");
            updateLabel(layout, 2, "lbl_underline");
            updateLabel(layout, 3, "lbl_highlight");
        }
    }

    QGroupBox *grpApi = m_generalTab->findChild<QGroupBox*>("grpApi");
    if (grpApi) {
        grpApi->setTitle(tm.tr("grp_api"));
        m_proxyLabel->setText(tm.tr("lbl_proxy_url"));
        m_useProxyCheck->setText(tm.tr("chk_use_proxy"));
        
        QFormLayout *layout = qobject_cast<QFormLayout*>(grpApi->layout());
        if (layout) {
            updateLabel(layout, 0, "lbl_api_key");
            updateLabel(layout, 1, "lbl_base_url");
            updateLabel(layout, 2, "lbl_model");
            updateLabel(layout, 3, "lbl_prompt");
            // Row 4 is proxy (already handled by m_proxyLabel)
            updateLabel(layout, 5, "lbl_target_screen");
        }
    }

    // Other Tab
    m_tabWidget->setTabText(3, tm.tr("tab_other"));
    
    QGroupBox *grpShortcuts = m_otherTab->findChild<QGroupBox*>("grpShortcuts");
    if (grpShortcuts) {
        grpShortcuts->setTitle(tm.tr("grp_shortcuts"));
        QFormLayout *layout = qobject_cast<QFormLayout*>(grpShortcuts->layout());
        if (layout) {
             updateLabel(layout, 0, "lbl_shot_hotkey");
             updateLabel(layout, 1, "lbl_sum_hotkey");
             updateLabel(layout, 2, "lbl_set_hotkey");
        }
    }
    
    QList<QGroupBox*> groups = this->findChildren<QGroupBox*>();
    for(auto g : groups) {
        if (g->objectName() == "profileDetails") continue; 
        if (g->title().contains("Profiles") || g->title().contains("配置")) {
            g->setTitle(tm.tr("grp_profiles"));
        }
    }
    
    QGroupBox *grpAdvanced = m_otherTab->findChild<QGroupBox*>("grpAdvanced");
    if (grpAdvanced) {
        grpAdvanced->setTitle(tm.tr("grp_advanced"));
        m_debugModeCheck->setText(tm.tr("chk_debug"));
    }
    
    m_newProfileBtn->setText(tm.tr("btn_new"));
    m_deleteProfileBtn->setText(tm.tr("btn_delete"));
    m_renameProfileBtn->setText(tm.tr("btn_rename"));
    m_copyProfileBtn->setText(tm.tr("btn_copy"));
    m_importProfileBtn->setText(tm.tr("btn_import"));
    m_exportProfileBtn->setText(tm.tr("btn_export"));
    
    QList<QPushButton*> mainBtns = this->findChildren<QPushButton*>();
    for(auto b : mainBtns) {
        if (b->property("isSaveBtn").toBool()) { 
             b->setText(tm.tr("btn_save"));
        } else if (b->text().contains("Save") || b->text().contains("保存")) {
             b->setText(tm.tr("btn_save"));
        }
    }
}

// ... updateProfileList ...

void ConfigDialog::updateProfileList() { // kept
    m_profileList->blockSignals(true);
    m_profileList->clear();
    m_profileList->addItems(m_configManager->listProfiles());
    
    // Select current
    QString current = m_configManager->currentProfileName();
    auto items = m_profileList->findItems(current, Qt::MatchExactly);
    if (!items.isEmpty()) {
        m_profileList->setCurrentItem(items.first());
    }
    m_profileList->blockSignals(false);
}
//...
// ... loadFromConfig ...
void ConfigDialog::loadFromConfig() {
    AppConfig cfg = m_configManager->getConfig();
    m_apiKeyEdit->setText(cfg.apiKey);
    m_baseUrlEdit->setText(cfg.baseUrl);
    m_modelNameEdit->setText(cfg.modelName);
    m_promptEdit->setPlainText(cfg.promptText);
    m_proxyUrlEdit->setText(cfg.proxyUrl);
    m_useProxyCheck->setChecked(cfg.useProxy);
    
    int providerIndex = m_apiProviderCombo->findData(cfg.apiProvider);
    if (providerIndex >= 0) m_apiProviderCombo->setCurrentIndex(providerIndex);

    m_hotkeyEdit->setText(cfg.screenshotHotkey);
    m_summaryHotkeyEdit->setText(cfg.summaryHotkey);
    m_settingsHotkeyEdit->setText(cfg.settingsHotkey);
    m_editHotkeyEdit->setText(cfg.editHotkey);
    m_viewToggleHotkeyEdit->setText(cfg.viewToggleHotkey);
    m_screenshotToggleHotkeyEdit->setText(cfg.screenshotToggleHotkey);
    m_boldHotkeyEdit->setText(cfg.boldHotkey);
    m_underlineHotkeyEdit->setText(cfg.underlineHotkey);
    m_highlightHotkeyEdit->setText(cfg.highlightHotkey);
    
    m_debugModeCheck->setChecked(cfg.debugMode);
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
    if (lockIndex >= 0) m_lockBehaviorCombo->setCurrentIndex(lockIndex);
    
    m_prevPageHotkeyEdit->setText(cfg.prevResultShortcut);
    m_nextPageHotkeyEdit->setText(cfg.nextResultShortcut);
    m_tagHotkeyEdit->setText(cfg.tagHotkey);


    // Screen selection
    for (int i = 0; i < m_screenCombo->count(); ++i) {
        if (m_screenCombo->itemData(i).toInt() == cfg.targetScreenIndex) {
            m_screenCombo->setCurrentIndex(i);
            break;
        }
    }
}

void ConfigDialog::save() {
    AppConfig cfg = m_configManager->getConfig();
    cfg.apiKey = m_apiKeyEdit->text();
    // ...
    cfg.baseUrl = m_baseUrlEdit->text();
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
    cfg.boldHotkey = m_boldHotkeyEdit->text();
    cfg.underlineHotkey = m_underlineHotkeyEdit->text().trimmed();
    cfg.highlightHotkey = m_highlightHotkeyEdit->text().trimmed();
    
    // Other Tab
    cfg.settingsHotkey = m_settingsHotkeyEdit->text();
    
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



ConfigDialog::~ConfigDialog() {
}

void ConfigDialog::onProfileChanged(const QString& name) {
    if (name.isEmpty()) return;
    m_configManager->loadProfile(name);
    loadFromConfig();
}

void ConfigDialog::newProfile() {
    TranslationManager &tm = TranslationManager::instance();
    bool ok;
    QString text = QInputDialog::getText(this, tm.tr("new_profile_title"),
                                         tm.tr("new_profile_label"), QLineEdit::Normal,
                                         "", &ok);
    if (ok && !text.isEmpty()) {
        if (m_configManager->createProfile(text)) {
            updateProfileList();
            auto items = m_profileList->findItems(text, Qt::MatchExactly);
            if (!items.isEmpty()) m_profileList->setCurrentItem(items.first());

            // Force reload to clear UI
            onProfileChanged(text);
        } else {
            QMessageBox::warning(this, tm.tr("new_profile_title"), tm.tr("msg_profile_exists"));
        }
    }
}

void ConfigDialog::deleteProfile() {
    TranslationManager &tm = TranslationManager::instance();
    if (!m_profileList->currentItem()) return;
    QString current = m_profileList->currentItem()->text();
    if (current == "Default") {
        QMessageBox::warning(this, tm.tr("delete_profile_title"), tm.tr("msg_cannot_delete_default"));
        return;
    }
    
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, tm.tr("delete_profile_title"),
                                  tm.tr("delete_profile_msg").arg(current),
                                  QMessageBox::Yes|QMessageBox::No);
    if (reply == QMessageBox::Yes) {
        m_configManager->deleteProfile(current);
        updateProfileList();
        if (m_profileList->currentItem())
             onProfileChanged(m_profileList->currentItem()->text());
    }
}

void ConfigDialog::renameProfile() {
    TranslationManager &tm = TranslationManager::instance();
    if (!m_profileList->currentItem()) return;
    QString current = m_profileList->currentItem()->text();
    if (current == "Default") {
        QMessageBox::warning(this, tm.tr("rename_profile_title"), tm.tr("msg_cannot_rename_default"));
        return;
    }
    
    bool ok;
    QString newName = QInputDialog::getText(this, tm.tr("rename_profile_title"),
                                            tm.tr("rename_profile_label"), QLineEdit::Normal,
                                            current, &ok);
    if (ok && !newName.isEmpty() && newName != current) {
        if (m_configManager->renameProfile(current, newName)) {
            updateProfileList();
            auto items = m_profileList->findItems(newName, Qt::MatchExactly);
            if (!items.isEmpty()) m_profileList->setCurrentItem(items.first());
            
            QMessageBox::information(this, tm.tr("rename_profile_title"),
                                     tm.tr("msg_rename_success"));
        } else {
            QMessageBox::warning(this, tm.tr("rename_profile_title"),
                                 tm.tr("msg_rename_error"));
        }
    }
}

void ConfigDialog::copyProfile() {
    if (!m_profileList->currentItem()) return;
    QString current = m_profileList->currentItem()->text();
    if (current.isEmpty()) return;

    bool ok;
    QString newName = QInputDialog::getText(this, 
        TranslationManager::instance().tr("new_profile_title"),
        TranslationManager::instance().tr("new_profile_label"), 
        QLineEdit::Normal, current + "_Copy", &ok);

    if (ok && !newName.isEmpty()) {
        if (m_configManager->copyProfile(current, newName)) {
            updateProfileList();
            auto items = m_profileList->findItems(newName, Qt::MatchExactly);
            if (!items.isEmpty()) m_profileList->setCurrentItem(items.first());
        } else {
            QMessageBox::warning(this, "Error", TranslationManager::instance().tr("msg_profile_exists"));
        }
    }
}

void ConfigDialog::importProfile() {
    TranslationManager &tm = TranslationManager::instance();
    QString fileName = QFileDialog::getOpenFileName(this, tm.tr("import_profile_title"), "", tm.tr("json_files"));
    if (fileName.isEmpty()) return;
    
    if (m_configManager->importProfile(fileName)) {
        updateProfileList();
        QMessageBox::information(this, tm.tr("import_profile_title"), tm.tr("msg_import_success"));
    } else {
        QMessageBox::warning(this, tm.tr("import_profile_title"), tm.tr("msg_import_error"));
    }
}

void ConfigDialog::exportProfile() {
    TranslationManager &tm = TranslationManager::instance();
    QString fileName = QFileDialog::getSaveFileName(this, tm.tr("export_profile_title"), "profile.json", tm.tr("json_files"));
    if (fileName.isEmpty()) return;
    
    if (m_configManager->exportProfile(m_configManager->currentProfileName(), fileName)) {
        QMessageBox::information(this, tm.tr("export_profile_title"), tm.tr("msg_export_success"));
    } else {
        QMessageBox::warning(this, tm.tr("export_profile_title"), tm.tr("msg_export_error"));
    }
}

void ConfigDialog::updateTheme(bool isDark) {
    Q_UNUSED(isDark);
    setStyleSheet(""); // Keep default widget look
}
