#include "StartupWindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QEvent>
#include <QFile>
#include <QHBoxLayout>
#include <QColor>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>
#include <QRegularExpression>
#include <QUrl>
#include <QVBoxLayout>

#ifndef APP_NAME
#define APP_NAME "AI Screenshot Translator"
#endif

#ifndef APP_VERSION
#define APP_VERSION "0.0.0"
#endif

#ifndef APP_COPYRIGHT
#define APP_COPYRIGHT "Copyright © 2026"
#endif

#ifndef APP_REPO_URL
#define APP_REPO_URL "https://github.com/Diraw/AI-Screenshot-Translator"
#endif

static QString normalizeTagToVersion(QString tag)
{
    tag = tag.trimmed();
    if (tag.startsWith('v') || tag.startsWith('V'))
        tag = tag.mid(1);
    return tag;
}

static QList<int> parseVersionParts(QString version)
{
    version = normalizeTagToVersion(version);
    // Keep only digits and dots (e.g. "1.2.3-beta" -> "1.2.3")
    version.replace(QRegularExpression("[^0-9\\.]"), "");
    QStringList parts = version.split('.', Qt::SkipEmptyParts);
    QList<int> nums;
    for (const QString &p : parts)
        nums.append(p.toInt());
    while (nums.size() < 3)
        nums.append(0);
    return nums;
}

static int compareVersions(const QString &a, const QString &b)
{
    const QList<int> va = parseVersionParts(a);
    const QList<int> vb = parseVersionParts(b);
    for (int i = 0; i < qMin(va.size(), vb.size()); ++i)
    {
        if (va[i] < vb[i])
            return -1;
        if (va[i] > vb[i])
            return 1;
    }
    return 0;
}

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

StartupWindow::StartupWindow(const AppConfig &cfg, QWidget *parent)
    : QDialog(parent), m_cfg(cfg)
{
    loadUiConfig();

    const QString windowTitle = uiString("windowTitle", QStringLiteral("%1").arg(APP_NAME));
    setWindowTitle(formatText(windowTitle));
    setModal(true);
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    resize(360, 260);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(14, 12, 14, 12);
    root->setSpacing(6);

    const QString titleText = uiString("title", QStringLiteral("%1").arg(APP_NAME));
    m_titleLabel = new QLabel(formatText(titleText), this);
    QFont titleFont = m_titleLabel->font();
    titleFont.setPointSize(titleFont.pointSize() + 3);
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    root->addWidget(m_titleLabel);

    const QString versionFmt = uiString("versionFormat", QStringLiteral("版本：{version}"));
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
            "提示：\n"
            "• 截图快捷键：{shot}\n"
            "• 打开归档窗口：{view}\n"));
    m_hotkeysLabel->setText(formatText(hotkeys));
    root->addWidget(m_hotkeysLabel);

    auto *updateRow = new QHBoxLayout();
    updateRow->setSpacing(6);

    m_updateStatusLabel = new QLabel(formatText(m_updateNotChecked.isEmpty() ? QStringLiteral("更新：未检查") : m_updateNotChecked), this);
    m_updateStatusLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    updateRow->addWidget(m_updateStatusLabel);

    m_checkUpdateBtn = new QPushButton(formatText(uiString("buttons.check", QStringLiteral("检查更新"))), this);
    connect(m_checkUpdateBtn, &QPushButton::clicked, this, &StartupWindow::startUpdateCheck);
    updateRow->addWidget(m_checkUpdateBtn);

    m_openReleasesBtn = new QPushButton(formatText(uiString("buttons.open", QStringLiteral("打开发布页"))), this);
    m_openReleasesBtn->setEnabled(true);
    connect(m_openReleasesBtn, &QPushButton::clicked, this, &StartupWindow::openReleasesPage);
    updateRow->addWidget(m_openReleasesBtn);

    root->addLayout(updateRow);

    const QString hint = uiString("hint", QStringLiteral("提示：点击空白处或按任意键继续"));
    m_hintLabel = new QLabel(formatText(hint), this);
    m_hintLabel->setWordWrap(true);
    QFont hintFont = m_hintLabel->font();
    hintFont.setBold(true);
    hintFont.setPointSize(hintFont.pointSize() + 1);
    m_hintLabel->setFont(hintFont);
    updateHintColor();
    root->addWidget(m_hintLabel);

    // Close-on-input behavior (but keep buttons usable)
    installEventFilter(this);
    if (m_titleLabel)
        m_titleLabel->installEventFilter(this);
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

    // Set application version for other parts of the app
    QCoreApplication::setApplicationVersion(m_currentVersion.isEmpty() ? QString::fromUtf8(APP_VERSION) : m_currentVersion);
}

void StartupWindow::updateHintColor()
{
    if (!m_hintLabel)
        return;
    if (m_updatingHintColor)
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

    // Current version is sourced from JSON (so you can hardcode it there)
    m_currentVersion = uiString("version", QString::fromUtf8(APP_VERSION));

    m_updateApiUrl = uiString("repoApiLatest", QStringLiteral("https://api.github.com/repos/Diraw/AI-Screenshot-Translator/releases/latest"));
    m_repoUrl = uiString("repoUrl", QString::fromUtf8(APP_REPO_URL));

    m_updateNotChecked = uiString("update.statusNotChecked", QStringLiteral("更新：未检查"));
    m_updateChecking = uiString("update.statusChecking", QStringLiteral("更新：检查中..."));
    m_updateNetworkError = uiString("update.statusNetworkError", QStringLiteral("更新：检查失败（网络错误）"));
    m_updateParseError = uiString("update.statusParseError", QStringLiteral("更新：检查失败（解析错误）"));
    m_updateNoVersion = uiString("update.statusNoVersion", QStringLiteral("更新：未获取到版本信息"));
    m_updateLatestTpl = uiString("update.statusLatest", QStringLiteral("更新：已是最新（{latest}）"));
    m_updateNewTpl = uiString("update.statusNew", QStringLiteral("更新：发现新版本（{latest}）"));
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

void StartupWindow::setUpdateStatus(const QString &text)
{
    if (m_updateStatusLabel)
        m_updateStatusLabel->setText(text);
}

void StartupWindow::startUpdateCheck()
{
    if (!m_checkUpdateBtn)
        return;

    if (!m_nam)
        m_nam = new QNetworkAccessManager(this);

    if (m_reply)
    {
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }

    setUpdateStatus(formatText(m_updateChecking));
    m_checkUpdateBtn->setEnabled(false);

    // GitHub latest release API
    const QUrl url(m_updateApiUrl);

    QNetworkRequest req(url);
    req.setRawHeader("User-Agent", "AI-Screenshot-Translator");
    req.setRawHeader("Accept", "application/vnd.github+json");

    m_reply = m_nam->get(req);
    connect(m_reply, &QNetworkReply::finished, this, &StartupWindow::onUpdateReplyFinished);
}

void StartupWindow::onUpdateReplyFinished()
{
    if (!m_reply)
        return;

    const QByteArray body = m_reply->readAll();
    const auto err = m_reply->error();

    m_reply->deleteLater();
    m_reply = nullptr;

    if (m_checkUpdateBtn)
        m_checkUpdateBtn->setEnabled(true);

    if (err != QNetworkReply::NoError)
    {
        setUpdateStatus(formatText(m_updateNetworkError));
        return;
    }

    QJsonParseError jerr{};
    const QJsonDocument doc = QJsonDocument::fromJson(body, &jerr);
    if (jerr.error != QJsonParseError::NoError || !doc.isObject())
    {
        setUpdateStatus(formatText(m_updateParseError));
        return;
    }

    const QJsonObject obj = doc.object();
    m_latestTag = obj.value(QStringLiteral("tag_name")).toString();
    m_latestUrl = obj.value(QStringLiteral("html_url")).toString();

    const QString current = m_currentVersion.isEmpty() ? QString::fromUtf8(APP_VERSION) : m_currentVersion;
    const QString latestVer = normalizeTagToVersion(m_latestTag);

    qInfo().noquote() << "[StartupWindow] Update check versions: current=" << current
                      << ", latest=" << latestVer;

    if (latestVer.isEmpty())
    {
        setUpdateStatus(formatText(m_updateNoVersion));
        return;
    }

    const int cmp = compareVersions(current, latestVer);
    if (cmp >= 0)
    {
        QString s = m_updateLatestTpl;
        s.replace("{latest}", latestVer);
        setUpdateStatus(formatText(s));
    }
    else
    {
        QString s = m_updateNewTpl;
        s.replace("{latest}", latestVer);
        setUpdateStatus(formatText(s));
    }
}

void StartupWindow::openReleasesPage()
{
    const QString url = !m_latestUrl.isEmpty() ? m_latestUrl : m_repoUrl;
    QDesktopServices::openUrl(QUrl(url));
}

void StartupWindow::closeIfNonInteractive(QObject *eventTarget)
{
    if (!eventTarget)
    {
        accept();
        return;
    }

    // Do not auto-close if interacting with buttons
    if (eventTarget == m_checkUpdateBtn || eventTarget == m_openReleasesBtn)
        return;

    accept();
}

bool StartupWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (!event)
        return QDialog::eventFilter(watched, event);

    switch (event->type())
    {
    case QEvent::WindowDeactivate:
        accept();
        return false;
    case QEvent::ApplicationPaletteChange:
    case QEvent::PaletteChange:
        // Avoid setStyleSheet here (can recurse into palette propagation)
        if (watched == this || watched == m_hintLabel)
            updateHintColor();
        break;
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonDblClick:
    case QEvent::Wheel:
        closeIfNonInteractive(watched);
        return false;
    default:
        break;
    }

    return QDialog::eventFilter(watched, event);
}

void StartupWindow::keyPressEvent(QKeyEvent *event)
{
    if (!event)
    {
        QDialog::keyPressEvent(event);
        return;
    }

    // If focus is on update button, allow Enter/Space to trigger without closing.
    QObject *focusObj = QApplication::focusObject();
    const bool focusIsButton = (focusObj == m_checkUpdateBtn || focusObj == m_openReleasesBtn);
    if (focusIsButton && (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter || event->key() == Qt::Key_Space))
    {
        QDialog::keyPressEvent(event);
        return;
    }

    accept();
}
