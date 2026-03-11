#include "ConfigDialog.h"

#include "TranslationManager.h"

#include <QFormLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QPlainTextEdit>
#include <QScreen>
#include <QSizePolicy>
#include <QVBoxLayout>

void ConfigDialog::setupGeneralTab()
{
    m_generalTab = new QWidget();
    auto *tabLayout = new QVBoxLayout(m_generalTab);
    m_generalFormLayout = new QFormLayout();
    auto *layout = m_generalFormLayout;
    layout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    layout->setFormAlignment(Qt::AlignTop);
    layout->setLabelAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    tabLayout->addLayout(layout);

    TranslationManager &tm = TranslationManager::instance();

    m_languageCombo = new QComboBox(this);
    const QStringList languages = tm.availableLanguages();
    for (const QString &langCode : languages)
        m_languageCombo->addItem(tm.languageDisplayName(langCode), langCode);
    layout->addRow("Language:", m_languageCombo);

    connect(m_languageCombo, &QComboBox::currentIndexChanged, [this](int)
            {
        const QString lang = m_languageCombo->currentData().toString();
        TranslationManager::instance().setLanguage(lang);
        retranslateUi();

        AppConfig cfg = m_configManager->getConfig();
        cfg.language = lang;
        m_configManager->setConfig(cfg);
        
        // Notify App to update all windows
        emit languageChanged(lang); });

    m_screenCombo = new QComboBox(this);
    const QList<QScreen *> screens = QGuiApplication::screens();
    for (int i = 0; i < screens.size(); ++i)
        m_screenCombo->addItem(screens[i]->name(), i);
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
    m_endpointPathEdit = new QLineEdit(this);
    m_endpointPathEdit->setPlaceholderText(TranslationManager::instance().tr("endpoint_placeholder"));
    m_endpointPathEdit->setText("/chat/completions");
    m_lastAutoEndpoint = "/chat/completions";
    m_endpointPathEdit->setMinimumWidth(160);

    connect(m_endpointPathEdit, &QLineEdit::textEdited, this, [this](const QString &)
            { m_lastAutoEndpoint.clear(); });

    connect(m_apiProviderCombo, &QComboBox::currentIndexChanged, this, [this](int)
            {
        if (m_isLoadingConfig)
            return;
        const QString provider = m_apiProviderCombo ? m_apiProviderCombo->currentData().toString() : QString();
        if (!provider.isEmpty() && provider != "advanced")
            m_lastRegularProvider = provider;
        maybeApplyEndpointDefaultForProvider(provider); });

    connect(m_apiProviderCombo, &QComboBox::currentIndexChanged, this, [this](int)
            {
        if (m_isLoadingConfig || m_isSyncingAdvanced)
            return;
        syncAdvancedTemplateFromRegular(); });

    auto *baseRowLayout = new QHBoxLayout();
    baseRowLayout->setContentsMargins(0, 0, 0, 0);
    baseRowLayout->setSpacing(8);
    baseRowLayout->addWidget(m_baseUrlEdit, 1);
    baseRowLayout->addWidget(m_endpointPathEdit, 0);
    auto *baseRowWidget = new QWidget(m_generalTab);
    baseRowWidget->setLayout(baseRowLayout);
    baseRowWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    layout->addRow("Base URL:", baseRowWidget);

    m_modelNameEdit = new QLineEdit(this);
    m_testConnectionBtn = new QPushButton("Test", this);
    m_testConnectionBtn->setObjectName("btnTestConnection");
    connect(m_testConnectionBtn, &QPushButton::clicked, this, &ConfigDialog::onTestConnection);

    auto *modelRowLayout = new QHBoxLayout();
    modelRowLayout->setContentsMargins(0, 0, 0, 0);
    modelRowLayout->setSpacing(8);
    modelRowLayout->addWidget(m_modelNameEdit, 1);
    modelRowLayout->addWidget(m_testConnectionBtn, 0);
    auto *modelRowWidget = new QWidget(m_generalTab);
    modelRowWidget->setLayout(modelRowLayout);
    modelRowWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    layout->addRow("Model:", modelRowWidget);

    m_promptEdit = new QTextEdit(this);
    // Seed a compact initial height; setupDialogUi releases it after baseline sizing.
    m_promptEdit->setFixedHeight(30);
    m_promptEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addRow("Prompt:", m_promptEdit);

    m_proxyUrlEdit = new QLineEdit(this);
    m_proxyUrlEdit->setPlaceholderText("e.g., http://127.0.0.1:1080 or socks5://127.0.0.1:1080");
    m_useProxyCheck = new QCheckBox(this);
    m_proxyLabel = new QLabel(this);
    m_proxyUrlEdit->setPlaceholderText(TranslationManager::instance().tr("proxy_placeholder"));

    auto *proxyLayout = new QHBoxLayout();
    proxyLayout->addWidget(m_proxyUrlEdit);
    proxyLayout->addWidget(m_useProxyCheck);
    layout->addRow(m_proxyLabel, proxyLayout);

    m_storagePathEdit = new QLineEdit(this);
    refreshStoragePathPlaceholder();
    m_browseBtn = new QPushButton(TranslationManager::instance().tr("btn_browse"), this);
    connect(m_browseBtn, &QPushButton::clicked, this, &ConfigDialog::browseForStoragePath);

    auto *storageLayout = new QHBoxLayout();
    storageLayout->addWidget(m_storagePathEdit);
    storageLayout->addWidget(m_browseBtn);
    layout->addRow(TranslationManager::instance().tr("lbl_storage"), storageLayout);

    m_showPreviewCheck = new QCheckBox("Show Preview Card after Screenshot", this);
    layout->addRow(m_showPreviewCheck);

    m_showResultCheck = new QCheckBox("Show Translation Result after Screenshot", this);
    layout->addRow(m_showResultCheck);

    m_tabWidget->addTab(m_generalTab, "General");

    // Keep the advanced JSON template aligned while regular mode is active.
    connect(m_apiKeyEdit, &QLineEdit::textChanged, this, [this](const QString &)
            { syncAdvancedTemplateFromRegular(); });
    connect(m_baseUrlEdit, &QLineEdit::textChanged, this, [this](const QString &)
            { syncAdvancedTemplateFromRegular(); });
    connect(m_endpointPathEdit, &QLineEdit::textChanged, this, [this](const QString &)
            { syncAdvancedTemplateFromRegular(); });
    connect(m_modelNameEdit, &QLineEdit::textChanged, this, [this](const QString &)
            { syncAdvancedTemplateFromRegular(); });
    connect(m_promptEdit, &QTextEdit::textChanged, this, [this]()
            { syncAdvancedTemplateFromRegular(); });
}

void ConfigDialog::setupAdvancedApiTab()
{
    m_advancedApiTab = new QWidget();
    auto *rootLayout = new QVBoxLayout(m_advancedApiTab);

    auto *topRow = new QHBoxLayout();
    m_enableAdvancedApiCheck = new QCheckBox("开启高级 API 模式", this);
    m_deleteAdvancedApiConfigBtn = new QPushButton("删除高级 API 配置", this);
    m_advancedTemplateStatusLabel = new QLabel(this);
    topRow->addWidget(m_enableAdvancedApiCheck, 0);
    topRow->addWidget(m_deleteAdvancedApiConfigBtn, 0);
    topRow->addSpacing(12);
    topRow->addWidget(m_advancedTemplateStatusLabel, 0);
    topRow->addStretch(1);
    rootLayout->addLayout(topRow);

    m_advancedApiTemplateEdit = new QPlainTextEdit(this);
    m_advancedApiTemplateEdit->setPlaceholderText("完整请求体模板（JSON）");
    m_advancedApiTemplateEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_advancedApiTemplateEdit->setMinimumHeight(100);
    rootLayout->addWidget(m_advancedApiTemplateEdit, 1);

    auto *testRow = new QHBoxLayout();
    TranslationManager &tm = TranslationManager::instance();
    m_testAdvancedApiBtn = new QPushButton("测试 JSON 与 API 连通性", this);
    m_pickAdvancedJsonFieldsBtn = new QPushButton(tm.tr("btn_select_json_fields"), this);
    m_showAdvancedDebugInResultCheck = new QCheckBox(tm.tr("chk_adv_debug_result_short"), this);
    m_showAdvancedDebugInArchiveCheck = new QCheckBox(tm.tr("chk_adv_debug_archive_short"), this);
    m_pickAdvancedJsonFieldsBtn->setEnabled(false);
    testRow->addWidget(m_testAdvancedApiBtn, 0);
    testRow->addWidget(m_pickAdvancedJsonFieldsBtn, 0);
    testRow->addStretch(1);
    rootLayout->addLayout(testRow);

    auto *debugRow = new QHBoxLayout();
    m_advancedDebugDisplayLabel = new QLabel(tm.tr("lbl_adv_debug_display"), this);
    debugRow->addWidget(m_advancedDebugDisplayLabel, 0);
    debugRow->addWidget(m_showAdvancedDebugInResultCheck, 0);
    debugRow->addWidget(m_showAdvancedDebugInArchiveCheck, 0);
    debugRow->addStretch(1);
    rootLayout->addLayout(debugRow);

    m_advancedApiResultEdit = new QPlainTextEdit(this);
    m_advancedApiResultEdit->setReadOnly(true);
    m_advancedApiResultEdit->setPlaceholderText("测试结果输出区域");
    m_advancedApiResultEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_advancedApiResultEdit->setMinimumHeight(110);
    rootLayout->addWidget(m_advancedApiResultEdit, 1);

    connect(m_enableAdvancedApiCheck, &QCheckBox::toggled, this, [this](bool checked)
            {
        if (m_isLoadingConfig)
            return;

        if (!checked)
            syncAdvancedTemplateFromRegular();

        updateAdvancedApiUiState(); });

    connect(m_deleteAdvancedApiConfigBtn, &QPushButton::clicked, this, [this]()
            { resetAdvancedApiToDefault(); });

    connect(m_advancedApiTemplateEdit, &QPlainTextEdit::textChanged, this, [this]()
            {
        if (m_isLoadingConfig || m_isSyncingAdvanced)
            return;
        if (m_enableAdvancedApiCheck && m_enableAdvancedApiCheck->isChecked())
        {
            m_advancedTemplateDetached = true;
            updateAdvancedTemplateStatusLabel();
        } });

    connect(m_testAdvancedApiBtn, &QPushButton::clicked, this, &ConfigDialog::onTestAdvancedApi);
    connect(m_pickAdvancedJsonFieldsBtn, &QPushButton::clicked, this, &ConfigDialog::onPickAdvancedJsonFields);

    m_tabWidget->addTab(m_advancedApiTab, "高级 API");
}
