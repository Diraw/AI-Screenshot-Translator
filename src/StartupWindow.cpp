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
#include <QTimer>
#include <QRegularExpression>
#include <QSettings>
#include <QDateTime>
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

struct SemVerId
{
    bool isNumeric = false;
    int num = 0;
    QString str;
};

static int stageRank(const QString &s)
{
    // Lower means older.
    // This matches typical SemVer expectations and the user's requested ordering.
    const QString t = s.toLower();
    if (t == "alpha")
        return 0;
    if (t == "beta")
        return 1;
    if (t == "rc")
        return 2;
    return -1;
}

static void appendPrereleaseToken(QString token, QList<SemVerId> &out)
{
    token = token.trimmed();
    if (token.isEmpty())
        return;

    // Split on '.' first (SemVer style), then further split alpha1/beta2 patterns.
    const QStringList pieces = token.split('.', Qt::SkipEmptyParts);
    for (QString p : pieces)
    {
        p = p.trimmed();
        if (p.isEmpty())
            continue;

        bool okNum = false;
        const int asNum = p.toInt(&okNum);
        if (okNum)
        {
            SemVerId id;
            id.isNumeric = true;
            id.num = asNum;
            out.append(id);
            continue;
        }

        static const QRegularExpression reAlphaNum(QStringLiteral(R"(^\s*([A-Za-z]+)(\d+)\s*$)"));
        const QRegularExpressionMatch m = reAlphaNum.match(p);
        if (m.hasMatch())
        {
            SemVerId a;
            a.isNumeric = false;
            a.str = m.captured(1).toLower();
            out.append(a);

            SemVerId n;
            n.isNumeric = true;
            n.num = m.captured(2).toInt();
            out.append(n);
            continue;
        }

        SemVerId id;
        id.isNumeric = false;
        id.str = p.toLower();
        out.append(id);
    }
}

struct SemVer
{
    int major = 0;
    int minor = 0;
    int patch = 0;
    QList<SemVerId> pre; // empty means stable
};

static SemVer parseSemVer(QString v)
{
    v = normalizeTagToVersion(v);
    // Drop build metadata: 1.2.3+build.5
    const int plusIdx = v.indexOf('+');
    if (plusIdx >= 0)
        v = v.left(plusIdx);

    QString core = v;
    QString pre;
    const int dashIdx = v.indexOf('-');
    if (dashIdx >= 0)
    {
        core = v.left(dashIdx);
        pre = v.mid(dashIdx + 1);
    }

    SemVer out;
    core = core.trimmed();
    const QStringList coreParts = core.split('.', Qt::SkipEmptyParts);
    if (coreParts.size() >= 1)
        out.major = coreParts[0].toInt();
    if (coreParts.size() >= 2)
        out.minor = coreParts[1].toInt();
    if (coreParts.size() >= 3)
        out.patch = coreParts[2].toInt();

    pre = pre.trimmed();
    if (!pre.isEmpty())
        appendPrereleaseToken(pre, out.pre);
    return out;
}

static int compareSemVerId(const SemVerId &a, const SemVerId &b)
{
    if (a.isNumeric && b.isNumeric)
    {
        if (a.num < b.num)
            return -1;
        if (a.num > b.num)
            return 1;
        return 0;
    }
    if (a.isNumeric != b.isNumeric)
    {
        // SemVer: numeric identifiers have lower precedence than non-numeric.
        return a.isNumeric ? -1 : 1;
    }

    const int ra = stageRank(a.str);
    const int rb = stageRank(b.str);
    if (ra >= 0 && rb >= 0 && ra != rb)
        return (ra < rb) ? -1 : 1;

    const int cmp = QString::compare(a.str, b.str, Qt::CaseInsensitive);
    if (cmp < 0)
        return -1;
    if (cmp > 0)
        return 1;
    return 0;
}

static int compareVersions(const QString &a, const QString &b)
{
    const SemVer va = parseSemVer(a);
    const SemVer vb = parseSemVer(b);

    if (va.major != vb.major)
        return (va.major < vb.major) ? -1 : 1;
    if (va.minor != vb.minor)
        return (va.minor < vb.minor) ? -1 : 1;
    if (va.patch != vb.patch)
        return (va.patch < vb.patch) ? -1 : 1;

    const bool aStable = va.pre.isEmpty();
    const bool bStable = vb.pre.isEmpty();
    if (aStable != bStable)
    {
        // Stable releases are newer than any pre-release of same core.
        return aStable ? 1 : -1;
    }
    if (aStable && bStable)
        return 0;

    const int n = qMin(va.pre.size(), vb.pre.size());
    for (int i = 0; i < n; ++i)
    {
        const int cmp = compareSemVerId(va.pre[i], vb.pre[i]);
        if (cmp != 0)
            return cmp;
    }

    // If all shared identifiers are equal, the shorter pre-release has lower precedence.
    if (va.pre.size() < vb.pre.size())
        return -1;
    if (va.pre.size() > vb.pre.size())
        return 1;
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
    connect(m_checkUpdateBtn, &QPushButton::clicked, this, [this]()
            { startUpdateCheck(true); });
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

    // Auto-check updates on every launch, but use 1-day cache unless forced by user.
    QTimer::singleShot(0, this, [this]()
                       { startUpdateCheck(false); });
}

bool StartupWindow::applyCachedUpdateStatusIfFresh()
{
    // Cache policy: 1 day
    static const qint64 kMaxAgeMs = 24LL * 60LL * 60LL * 1000LL;

    QSettings settings(ConfigManager::settingsIniPath(), QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("updateCache"));
    const qint64 checkedAtMs = settings.value(QStringLiteral("checkedAtMs"), 0).toLongLong();
    const QString status = settings.value(QStringLiteral("status"), QString()).toString();
    const QString latestVer = settings.value(QStringLiteral("latestVer"), QString()).toString();
    const QString latestUrl = settings.value(QStringLiteral("latestUrl"), QString()).toString();
    settings.endGroup();

    if (checkedAtMs <= 0)
        return false;
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (nowMs - checkedAtMs > kMaxAgeMs)
        return false;
    if (status.isEmpty())
        return false;

    m_latestUrl = latestUrl;

    if (status == QStringLiteral("ok"))
    {
        if (latestVer.isEmpty())
        {
            setUpdateStatus(formatText(m_updateNoVersion));
            return true;
        }

        const QString current = m_currentVersion.isEmpty() ? QString::fromUtf8(APP_VERSION) : m_currentVersion;
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
        return true;
    }
    if (status == QStringLiteral("neterr"))
    {
        setUpdateStatus(formatText(m_updateNetworkError));
        return true;
    }
    if (status == QStringLiteral("parseerr"))
    {
        setUpdateStatus(formatText(m_updateParseError));
        return true;
    }
    if (status == QStringLiteral("noversion"))
    {
        setUpdateStatus(formatText(m_updateNoVersion));
        return true;
    }

    return false;
}

void StartupWindow::saveUpdateCache(const QString &status, const QString &latestVer, const QString &latestUrl)
{
    QSettings settings(ConfigManager::settingsIniPath(), QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("updateCache"));
    settings.setValue(QStringLiteral("checkedAtMs"), QDateTime::currentMSecsSinceEpoch());
    settings.setValue(QStringLiteral("status"), status);
    settings.setValue(QStringLiteral("latestVer"), latestVer);
    settings.setValue(QStringLiteral("latestUrl"), latestUrl);
    settings.endGroup();
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

void StartupWindow::startUpdateCheck(bool forceNetwork)
{
    if (!m_checkUpdateBtn)
        return;

    // Auto-check uses cache for 1 day; manual click bypasses cache.
    if (!forceNetwork)
    {
        if (applyCachedUpdateStatusIfFresh())
            return;
    }

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
        saveUpdateCache(QStringLiteral("neterr"));
        return;
    }

    QJsonParseError jerr{};
    const QJsonDocument doc = QJsonDocument::fromJson(body, &jerr);
    if (jerr.error != QJsonParseError::NoError || !doc.isObject())
    {
        setUpdateStatus(formatText(m_updateParseError));
        saveUpdateCache(QStringLiteral("parseerr"));
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
        saveUpdateCache(QStringLiteral("noversion"));
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

    saveUpdateCache(QStringLiteral("ok"), latestVer, m_latestUrl);
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
