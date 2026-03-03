#include "StartupWindow.h"

#include "ThemeUtils.h"

#include <QColor>
#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTextBrowser>
#include <QTimer>
#include <QVBoxLayout>

#ifndef APP_NAME
#define APP_NAME "AI Screenshot Translator"
#endif

#ifndef APP_VERSION
#define APP_VERSION "0.0.0"
#endif

#ifndef APP_COPYRIGHT
#define APP_COPYRIGHT "Copyright Â© 2026"
#endif

#ifndef APP_REPO_URL
#define APP_REPO_URL "https://github.com/Diraw/AI-Screenshot-Translator"
#endif

static QString prettyHotkey(QString s)
{
    s = s.trimmed();
    if (s.isEmpty())
        return s;

    const QStringList parts = s.split('+', Qt::SkipEmptyParts);
    QStringList out;
    out.reserve(parts.size());
    for (QString p : parts)
    {
        p = p.trimmed();
        const QString lower = p.toLower();
        if (lower == "ctrl" || lower == "control")
            out << "Ctrl";
        else if (lower == "alt")
            out << "Alt";
        else if (lower == "shift")
            out << "Shift";
        else if (lower == "win" || lower == "meta" || lower == "super")
            out << "Win";
        else if (p.size() == 1)
            out << p.toUpper();
        else
            out << p;
    }
    return out.join('+');
}

static QString loadPrivacyPolicyMarkdown()
{
    const QString baseDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        QDir::cleanPath(baseDir + "/assets/privacy_policy.md"),
        QDir::cleanPath(baseDir + "/../assets/privacy_policy.md"),
        QDir::cleanPath(baseDir + "/../../assets/privacy_policy.md"),
    };

    for (const QString &path : candidates)
    {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;
        return QString::fromUtf8(f.readAll());
    }

    return QString();
}

StartupWindow::StartupWindow(const AppConfig &cfg, QWidget *parent)
    : QDialog(parent), m_cfg(cfg)
{
    loadUiConfig();
    setupWindowChrome();
    setupDialogUi();
    installCloseOnInputFilters();

    QCoreApplication::setApplicationVersion(m_currentVersion.isEmpty() ? QString::fromUtf8(APP_VERSION) : m_currentVersion);

    QTimer::singleShot(0, this, [this]()
                       { startUpdateCheck(false); });
}

void StartupWindow::setupWindowChrome()
{
    const QString windowTitle = uiString("windowTitle", QStringLiteral("%1").arg(APP_NAME));
    setWindowTitle(formatText(windowTitle));
    setModal(true);
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    resize(340, 220);
    ThemeUtils::applyThemeToWindow(this, ThemeUtils::isSystemDark());
}

void StartupWindow::setupDialogUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(12, 10, 12, 10);
    root->setSpacing(4);

    const QString versionFmt = uiString("versionFormat", QStringLiteral("ç‰ˆæœ¬ï¼š{version}"));
    m_versionLabel = new QLabel(formatText(versionFmt), this);
    root->addWidget(m_versionLabel);

    const QString cr = uiString("copyright", QString::fromUtf8(APP_COPYRIGHT));
    m_copyrightLabel = new QLabel(formatText(cr), this);
    root->addWidget(m_copyrightLabel);

    m_hotkeysLabel = new QLabel(this);
    m_hotkeysLabel->setWordWrap(true);
    const QString hotkeys = uiString(
        "hotkeysText",
        QStringLiteral(
            "æç¤ºï¼š\n"
            "â€¢ æˆªå›¾å¿«æ·é”®ï¼š{shot}\n"
            "â€¢ æ‰“å¼€å½’æ¡£çª—å£ï¼š{view}\n"));
    m_hotkeysLabel->setText(formatText(hotkeys));
    root->addWidget(m_hotkeysLabel);

    addUpdateRow(root);
    addFooterRow(root);
}

void StartupWindow::addUpdateRow(QVBoxLayout *root)
{
    auto *updateRow = new QHBoxLayout();
    updateRow->setSpacing(4);

    m_updateStatusLabel = new QLabel(formatText(m_updateNotChecked.isEmpty() ? QStringLiteral("æ›´æ–°ï¼šæœªæ£€æŸ¥") : m_updateNotChecked), this);
    m_updateStatusLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    updateRow->addWidget(m_updateStatusLabel);

    m_checkUpdateBtn = new QPushButton(formatText(uiString("buttons.check", QStringLiteral("æ£€æŸ¥æ›´æ–°"))), this);
    connect(m_checkUpdateBtn, &QPushButton::clicked, this, [this]()
            { startUpdateCheck(true); });
    updateRow->addWidget(m_checkUpdateBtn);

    m_openReleasesBtn = new QPushButton(formatText(uiString("buttons.open", QStringLiteral("æ‰“å¼€å‘å¸ƒé¡µ"))), this);
    m_openReleasesBtn->setEnabled(true);
    connect(m_openReleasesBtn, &QPushButton::clicked, this, &StartupWindow::openReleasesPage);
    updateRow->addWidget(m_openReleasesBtn);

    root->addLayout(updateRow);
}

void StartupWindow::addFooterRow(QVBoxLayout *root)
{
    auto *privacyRow = new QHBoxLayout();
    privacyRow->setSpacing(4);
    privacyRow->addStretch();

    m_privacyBtn = new QPushButton(formatText(uiString("buttons.privacy", QStringLiteral("éšç§åè®®"))), this);
    connect(m_privacyBtn, &QPushButton::clicked, this, &StartupWindow::openPrivacyPolicy);
    privacyRow->addWidget(m_privacyBtn);

    const QString hint = uiString("hint", QStringLiteral("æç¤ºï¼šç‚¹å‡»ç©ºç™½å¤„æˆ–æŒ‰ä»»æ„é”®ç»§ç»­"));
    m_hintLabel = new QLabel(formatText(hint), this);
    m_hintLabel->setWordWrap(false);
    QFont hintFont = m_hintLabel->font();
    hintFont.setBold(true);
    hintFont.setPointSize(hintFont.pointSize());
    m_hintLabel->setFont(hintFont);
    updateHintColor();
    privacyRow->insertWidget(0, m_hintLabel, 1);

    root->addLayout(privacyRow);
}

void StartupWindow::installCloseOnInputFilters()
{
    installEventFilter(this);
    if (m_versionLabel)
        m_versionLabel->installEventFilter(this);
    if (m_copyrightLabel)
        m_copyrightLabel->installEventFilter(this);
    if (m_hotkeysLabel)
        m_hotkeysLabel->installEventFilter(this);
    if (m_hintLabel)
        m_hintLabel->installEventFilter(this);
    if (m_updateStatusLabel)
        m_updateStatusLabel->installEventFilter(this);
}

void StartupWindow::updateHintColor()
{
    if (!m_hintLabel || m_updatingHintColor)
        return;

    m_updatingHintColor = true;

    const QColor bg = palette().color(QPalette::Window);
    const bool isDark = bg.lightness() < 128;
    const QColor desired = QColor(isDark ? QStringLiteral("#D3B24C") : QStringLiteral("#B24A4A"));

    QPalette pal = m_hintLabel->palette();
    if (pal.color(QPalette::WindowText) != desired)
    {
        pal.setColor(QPalette::WindowText, desired);
        m_hintLabel->setPalette(pal);
    }

    m_updatingHintColor = false;
}

void StartupWindow::loadUiConfig()
{
    m_uiConfig = QJsonObject();

    const QString path = QCoreApplication::applicationDirPath() + "/assets/startup_window.json";
    QFile f(path);
    if (f.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QJsonParseError err{};
        const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
        if (err.error == QJsonParseError::NoError && doc.isObject())
            m_uiConfig = doc.object();
    }

    m_currentVersion = uiString("version", QString::fromUtf8(APP_VERSION));
    m_updateApiUrl = uiString("repoApiLatest", QStringLiteral("https://api.github.com/repos/Diraw/AI-Screenshot-Translator/releases/latest"));
    m_repoUrl = uiString("repoUrl", QString::fromUtf8(APP_REPO_URL));

    m_updateNotChecked = uiString("update.statusNotChecked", QStringLiteral("æ›´æ–°ï¼šæœªæ£€æŸ¥"));
    m_updateChecking = uiString("update.statusChecking", QStringLiteral("æ›´æ–°ï¼šæ£€æŸ¥ä¸­..."));
    m_updateNetworkError = uiString("update.statusNetworkError", QStringLiteral("æ›´æ–°ï¼šæ£€æŸ¥å¤±è´¥ï¼ˆç½‘ç»œé”™è¯¯ï¼‰"));
    m_updateParseError = uiString("update.statusParseError", QStringLiteral("æ›´æ–°ï¼šæ£€æŸ¥å¤±è´¥ï¼ˆè§£æžé”™è¯¯ï¼‰"));
    m_updateNoVersion = uiString("update.statusNoVersion", QStringLiteral("æ›´æ–°ï¼šæœªèŽ·å–åˆ°ç‰ˆæœ¬ä¿¡æ¯"));
    m_updateLatestTpl = uiString("update.statusLatest", QStringLiteral("æ›´æ–°ï¼šå·²æ˜¯æœ€æ–°ï¼ˆ{latest}ï¼‰"));
    m_updateNewTpl = uiString("update.statusNew", QStringLiteral("æ›´æ–°ï¼šå‘çŽ°æ–°ç‰ˆæœ¬ï¼ˆ{latest}ï¼‰"));
}

QString StartupWindow::uiString(const QString &path, const QString &fallback) const
{
    if (m_uiConfig.isEmpty())
        return fallback;

    const QStringList keys = path.split('.', Qt::SkipEmptyParts);
    QJsonValue v(m_uiConfig);
    for (const QString &k : keys)
    {
        if (!v.isObject())
            return fallback;
        v = v.toObject().value(k);
    }
    if (!v.isString())
        return fallback;
    const QString s = v.toString();
    return s.isEmpty() ? fallback : s;
}

QString StartupWindow::formatText(QString text) const
{
    const QString version = m_currentVersion.isEmpty() ? QString::fromUtf8(APP_VERSION) : m_currentVersion;
    const QString shot = prettyHotkey(m_cfg.screenshotHotkey.isEmpty() ? QStringLiteral("ctrl+alt+s") : m_cfg.screenshotHotkey);
    const QString view = prettyHotkey(m_cfg.summaryHotkey.isEmpty() ? QStringLiteral("alt+s") : m_cfg.summaryHotkey);
    const QString edit = m_cfg.editHotkey.isEmpty() ? QStringLiteral("e") : m_cfg.editHotkey;

    text.replace("{version}", version);
    text.replace("{shot}", shot);
    text.replace("{view}", view);
    text.replace("{edit}", edit);
    return text;
}

void StartupWindow::openPrivacyPolicy()
{
    const QString privacyTitle = uiString("privacyDialog.title",
                                          uiString("buttons.privacy", QStringLiteral("éšç§åè®®")));
    const QString privacyMissingText = uiString("privacyDialog.missingFile",
                                                QStringLiteral("æœªæ‰¾åˆ°éšç§åè®®æ–‡ä»¶ã€‚"));
    const QString closeText = uiString("buttons.close", QStringLiteral("å…³é—­"));

    const QString markdown = loadPrivacyPolicyMarkdown();
    if (markdown.trimmed().isEmpty())
    {
        QMessageBox::warning(this, privacyTitle, privacyMissingText);
        return;
    }

    QDialog dlg(this);
    dlg.setWindowTitle(privacyTitle);
    dlg.setModal(true);
    dlg.resize(680, 520);
    ThemeUtils::applyThemeToWindow(&dlg, ThemeUtils::isSystemDark());

    auto *layout = new QVBoxLayout(&dlg);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    auto *viewer = new QTextBrowser(&dlg);
    viewer->setOpenExternalLinks(true);
    viewer->setMarkdown(markdown);
    layout->addWidget(viewer);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dlg);
    if (QPushButton *closeButton = buttons->button(QDialogButtonBox::Close))
        closeButton->setText(closeText);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    layout->addWidget(buttons);

    m_showingChildDialog = true;
    dlg.exec();
    m_showingChildDialog = false;
}
