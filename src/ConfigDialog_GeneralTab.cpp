#include "ConfigDialog.h"

#include "TranslationManager.h"

#include <QFormLayout>
#include <QGuiApplication>
#include <QScreen>

void ConfigDialog::setupGeneralTab()
{
    m_generalTab = new QWidget();
    auto *layout = new QFormLayout(m_generalTab);

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
        m_configManager->setConfig(cfg); });

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
        maybeApplyEndpointDefaultForProvider(provider); });

    auto *baseRowLayout = new QHBoxLayout();
    baseRowLayout->setContentsMargins(0, 0, 0, 0);
    baseRowLayout->setSpacing(8);
    baseRowLayout->addWidget(m_baseUrlEdit, 1);
    baseRowLayout->addWidget(m_endpointPathEdit, 0);
    auto *baseRowWidget = new QWidget(this);
    baseRowWidget->setLayout(baseRowLayout);
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
    auto *modelRowWidget = new QWidget(this);
    modelRowWidget->setLayout(modelRowLayout);
    layout->addRow("Model:", modelRowWidget);

    m_promptEdit = new QTextEdit(this);
    m_promptEdit->setMaximumHeight(60);
    layout->addRow("Prompt:", m_promptEdit);

    m_proxyUrlEdit = new QLineEdit(this);
    m_proxyUrlEdit->setPlaceholderText("ä¾‹å¦‚ http://127.0.0.1:1080 æˆ– socks5://127.0.0.1:1080");
    m_useProxyCheck = new QCheckBox(this);
    m_proxyLabel = new QLabel(this);

    auto *proxyLayout = new QHBoxLayout();
    proxyLayout->addWidget(m_proxyUrlEdit);
    proxyLayout->addWidget(m_useProxyCheck);
    layout->addRow(m_proxyLabel, proxyLayout);

    m_storagePathEdit = new QLineEdit(this);
    refreshStoragePathPlaceholder();
    auto *browseBtn = new QPushButton("Browse...", this);
    connect(browseBtn, &QPushButton::clicked, this, &ConfigDialog::browseForStoragePath);

    auto *storageLayout = new QHBoxLayout();
    storageLayout->addWidget(m_storagePathEdit);
    storageLayout->addWidget(browseBtn);
    layout->addRow("Storage Path:", storageLayout);

    m_showPreviewCheck = new QCheckBox("Show Preview Card after Screenshot", this);
    layout->addRow(m_showPreviewCheck);

    m_showResultCheck = new QCheckBox("Show Translation Result after Screenshot", this);
    layout->addRow(m_showResultCheck);

    m_tabWidget->addTab(m_generalTab, "General");
}
