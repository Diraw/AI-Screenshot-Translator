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
#include <QGridLayout>
#include <QGuiApplication>
#include <QGroupBox>
#include <QLabel>
#include <QPointer>
#include <QScreen>
#include <QRegularExpression>

static bool tryParseColorText(QString text, QColor &out)
{
    text = text.trimmed();
    if (text.isEmpty())
        return false;

    // Hex formats: #RGB, #RRGGBB, #RRGGBBAA (CSS style)
    if (text.startsWith('#'))
    {
        QString hex = text.mid(1).trimmed();
        if (hex.size() == 3)
        {
            auto h = [&](int i)
            { return QString(hex[i]) + QString(hex[i]); };
            bool okR = false, okG = false, okB = false;
            int r = h(0).toInt(&okR, 16);
            int g = h(1).toInt(&okG, 16);
            int b = h(2).toInt(&okB, 16);
            if (!okR || !okG || !okB)
                return false;
            out = QColor(r, g, b);
            return out.isValid();
        }
        if (hex.size() == 6 || hex.size() == 8)
        {
            bool ok = false;
            int r = hex.mid(0, 2).toInt(&ok, 16);
            if (!ok)
                return false;
            int g = hex.mid(2, 2).toInt(&ok, 16);
            if (!ok)
                return false;
            int b = hex.mid(4, 2).toInt(&ok, 16);
            if (!ok)
                return false;
            int a = 255;
            if (hex.size() == 8)
            {
                a = hex.mid(6, 2).toInt(&ok, 16);
                if (!ok)
                    return false;
            }
            out = QColor(r, g, b, a);
            return out.isValid();
        }
        return false;
    }

    // rgb(...) / rgba(...)
    {
        static const QRegularExpression reRgb(
            R"(^\s*rgba?\s*\(\s*([^\)]*)\s*\)\s*$)",
            QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatch m = reRgb.match(text);
        if (m.hasMatch())
        {
            const QString inside = m.captured(1);
            QStringList parts = inside.split(',', Qt::SkipEmptyParts);
            for (auto &p : parts)
                p = p.trimmed();

            if (parts.size() < 3)
                return false;
            bool okR = false, okG = false, okB = false;
            int r = parts[0].toInt(&okR);
            int g = parts[1].toInt(&okG);
            int b = parts[2].toInt(&okB);
            if (!okR || !okG || !okB)
                return false;

            int a = 255;
            if (parts.size() >= 4)
            {
                bool okAInt = false;
                int aInt = parts[3].toInt(&okAInt);
                if (okAInt)
                {
                    a = qBound(0, aInt, 255);
                }
                else
                {
                    bool okAF = false;
                    double af = parts[3].toDouble(&okAF);
                    if (!okAF)
                        return false;
                    if (af <= 1.0)
                        a = qBound(0, (int)qRound(af * 255.0), 255);
                    else
                        a = qBound(0, (int)qRound(af), 255);
                }
            }
            out = QColor(qBound(0, r, 255), qBound(0, g, 255), qBound(0, b, 255), a);
            return out.isValid();
        }
    }

    // "r,g,b" or "r,g,b,a" or "r,g,b,aF"
    if (text.contains(','))
    {
        QStringList parts = text.split(',', Qt::SkipEmptyParts);
        for (auto &p : parts)
            p = p.trimmed();

        if (parts.size() < 3)
            return false;
        bool okR = false, okG = false, okB = false;
        int r = parts[0].toInt(&okR);
        int g = parts[1].toInt(&okG);
        int b = parts[2].toInt(&okB);
        if (!okR || !okG || !okB)
            return false;

        int a = 255;
        if (parts.size() >= 4)
        {
            bool okAInt = false;
            int aInt = parts[3].toInt(&okAInt);
            if (okAInt)
            {
                a = qBound(0, aInt, 255);
            }
            else
            {
                bool okAF = false;
                double af = parts[3].toDouble(&okAF);
                if (!okAF)
                    return false;
                if (af <= 1.0)
                    a = qBound(0, (int)qRound(af * 255.0), 255);
                else
                    a = qBound(0, (int)qRound(af), 255);
            }
        }
        out = QColor(qBound(0, r, 255), qBound(0, g, 255), qBound(0, b, 255), a);
        return out.isValid();
    }

    // Named colors fallback
    QColor c;
    c.setNamedColor(text);
    if (!c.isValid())
        return false;
    out = c;
    return true;
}

static void updateColorPreviewLabel(QLabel *label, const QString &text)
{
    if (!label)
        return;
    QColor c;
    if (!tryParseColorText(text, c))
    {
        label->setToolTip(QString());
        label->setStyleSheet("QLabel{background: transparent; border: 1px solid rgba(127,127,127,120); border-radius: 3px;}");
        return;
    }

    label->setToolTip(QString("rgba(%1,%2,%3,%4)").arg(c.red()).arg(c.green()).arg(c.blue()).arg(c.alpha()));
    label->setStyleSheet(QString(
                             "QLabel{background-color: rgba(%1,%2,%3,%4); border: 1px solid rgba(127,127,127,160); border-radius: 3px;}")
                             .arg(c.red())
                             .arg(c.green())
                             .arg(c.blue())
                             .arg(c.alpha()));
}

ConfigDialog::ConfigDialog(ConfigManager *configManager, QWidget *parent)
    : QDialog(parent), m_configManager(configManager)
{
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

    connect(m_languageCombo, &QComboBox::currentIndexChanged, [this](int index)
            {
        QString lang = m_languageCombo->currentData().toString();
        TranslationManager::instance().setLanguage(lang);
        retranslateUi();
        
        AppConfig cfg = m_configManager->getConfig();
        cfg.language = lang;
        m_configManager->setConfig(cfg); });

    m_screenCombo = new QComboBox(this);
    QList<QScreen *> screens = QGuiApplication::screens();
    for (int i = 0; i < screens.size(); ++i)
    {
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
    connect(browseBtn, &QPushButton::clicked, [this]()
            {
        QString dir = QFileDialog::getExistingDirectory(this, "Select Storage Directory", m_storagePathEdit->text());
        if (!dir.isEmpty()) {
            m_storagePathEdit->setText(dir);
        } });

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
    m_cardBorderColorEdit->setPlaceholderText("R,G,B,A (e.g., 100,100,100,128) 或 rgba(100,100,100,0.5)");
    m_useBorderCheck = new QCheckBox(this);
    m_lblCardBorderColor = new QLabel(this);

    m_cardBorderColorPreview = new QLabel(this);
    m_cardBorderColorPreview->setFixedSize(18, 18);
    updateColorPreviewLabel(m_cardBorderColorPreview, m_cardBorderColorEdit->text());
    connect(m_cardBorderColorEdit, &QLineEdit::textChanged, this, [this](const QString &t)
            { updateColorPreviewLabel(m_cardBorderColorPreview, t); });

    QHBoxLayout *borderColorLayout = new QHBoxLayout();
    borderColorLayout->addWidget(m_cardBorderColorEdit);
    borderColorLayout->addWidget(m_cardBorderColorPreview);
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

    m_selectionToggleHotkeyEdit = new QLineEdit(this);
    viewLayout->addRow(TranslationManager::instance().tr("lbl_selection_toggle_hotkey"), m_selectionToggleHotkeyEdit);

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

    m_highlightMarkColorEdit = new QLineEdit(this);
    m_highlightMarkColorEdit->setPlaceholderText("#ffeb3b80 或 rgba(255,235,59,0.5) 或 255,235,59,0.5");
    m_highlightMarkColorPreview = new QLabel(this);
    m_highlightMarkColorPreview->setFixedSize(18, 18);
    updateColorPreviewLabel(m_highlightMarkColorPreview, m_highlightMarkColorEdit->text());
    connect(m_highlightMarkColorEdit, &QLineEdit::textChanged, this, [this](const QString &t)
            { updateColorPreviewLabel(m_highlightMarkColorPreview, t); });
    QHBoxLayout *hl1 = new QHBoxLayout();
    hl1->setContentsMargins(0, 0, 0, 0);
    hl1->addWidget(m_highlightMarkColorEdit);
    hl1->addWidget(m_highlightMarkColorPreview);
    QWidget *hl1w = new QWidget(this);
    hl1w->setLayout(hl1);
    editLayout->addRow("Highlight Color:", hl1w);

    m_highlightMarkColorDarkEdit = new QLineEdit(this);
    m_highlightMarkColorDarkEdit->setPlaceholderText("#d4af3780 或 rgba(212,175,55,0.5) 或 212,175,55,0.5");
    m_highlightMarkColorDarkPreview = new QLabel(this);
    m_highlightMarkColorDarkPreview->setFixedSize(18, 18);
    updateColorPreviewLabel(m_highlightMarkColorDarkPreview, m_highlightMarkColorDarkEdit->text());
    connect(m_highlightMarkColorDarkEdit, &QLineEdit::textChanged, this, [this](const QString &t)
            { updateColorPreviewLabel(m_highlightMarkColorDarkPreview, t); });
    QHBoxLayout *hl2 = new QHBoxLayout();
    hl2->setContentsMargins(0, 0, 0, 0);
    hl2->addWidget(m_highlightMarkColorDarkEdit);
    hl2->addWidget(m_highlightMarkColorDarkPreview);
    QWidget *hl2w = new QWidget(this);
    hl2w->setLayout(hl2);
    editLayout->addRow("Highlight Color (Dark):", hl2w);

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

    QWidget *quitRow = new QWidget(this);
    QHBoxLayout *quitRowLayout = new QHBoxLayout(quitRow);
    quitRowLayout->setContentsMargins(0, 0, 0, 0);
    quitRowLayout->setSpacing(8);
    m_quitHotkeyLabel = new QLabel("Quit Hotkey:", this);
    m_quitHotkeyEdit = new QLineEdit(this);
    m_quitHotkeyEdit->setPlaceholderText("alt+q");
    m_enableQuitHotkeyCheck = new QCheckBox("Enable", this);
    quitRowLayout->addWidget(m_quitHotkeyLabel);
    quitRowLayout->addWidget(m_quitHotkeyEdit, 1);
    quitRowLayout->addWidget(m_enableQuitHotkeyCheck);
    advLayout->addWidget(quitRow);

    connect(m_enableQuitHotkeyCheck, &QCheckBox::toggled, this, [this](bool on)
            {
                if (m_quitHotkeyEdit)
                    m_quitHotkeyEdit->setEnabled(on); });
    m_quitHotkeyEdit->setEnabled(false);

    otherMainLayout->addWidget(grpAdvanced);
    otherMainLayout->addStretch();

    m_tabWidget->addTab(m_otherTab, "Other");

    // Auto-resize window when switching tabs
    connect(m_tabWidget, &QTabWidget::currentChanged, [this]()
            {
        this->layout()->setSizeConstraint(QLayout::SetFixedSize);
        QTimer::singleShot(100, [this](){
             this->layout()->setSizeConstraint(QLayout::SetDefaultConstraint);
        }); });

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

    setupProfilesWatcher();

    retranslateUi();
}

ConfigDialog::~ConfigDialog() = default;
