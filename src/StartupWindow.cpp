#include "StartupWindow.h"

#include <QApplication>
#include <QColor>
#include <QDateTime>
#include <QDebug>
#include <QDesktopServices>
#include <QEvent>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QUrl>

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
    QList<SemVerId> pre;
};

static SemVer parseSemVer(QString v)
{
    v = normalizeTagToVersion(v);
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
        return a.isNumeric ? -1 : 1;

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
        return aStable ? 1 : -1;
    if (aStable && bStable)
        return 0;

    const int n = qMin(va.pre.size(), vb.pre.size());
    for (int i = 0; i < n; ++i)
    {
        const int cmp = compareSemVerId(va.pre[i], vb.pre[i]);
        if (cmp != 0)
            return cmp;
    }

    if (va.pre.size() < vb.pre.size())
        return -1;
    if (va.pre.size() > vb.pre.size())
        return 1;
    return 0;
}

bool StartupWindow::applyCachedUpdateStatusIfFresh()
{
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
    if (nowMs - checkedAtMs > kMaxAgeMs || status.isEmpty())
        return false;

    m_latestUrl = latestUrl;

    if (status == QStringLiteral("ok"))
    {
        if (latestVer.isEmpty())
        {
            setUpdateStatus(formatText(m_updateNoVersion));
            updateUpdateHighlight(false);
            return true;
        }

        const QString current = m_currentVersion.isEmpty() ? QString::fromUtf8(APP_VERSION) : m_currentVersion;
        const int cmp = compareVersions(current, latestVer);
        QString s = (cmp >= 0) ? m_updateLatestTpl : m_updateNewTpl;
        s.replace("{latest}", latestVer);
        setUpdateStatus(formatText(s));
        updateUpdateHighlight(cmp < 0);
        return true;
    }
    if (status == QStringLiteral("neterr"))
    {
        setUpdateStatus(formatText(m_updateNetworkError));
        updateUpdateHighlight(false);
        return true;
    }
    if (status == QStringLiteral("parseerr"))
    {
        setUpdateStatus(formatText(m_updateParseError));
        updateUpdateHighlight(false);
        return true;
    }
    if (status == QStringLiteral("noversion"))
    {
        setUpdateStatus(formatText(m_updateNoVersion));
        updateUpdateHighlight(false);
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

void StartupWindow::updateUpdateHighlight(bool hasNewVersion)
{
    m_hasNewVersion = hasNewVersion;

    const QColor bg = palette().color(QPalette::Window);
    const bool isDark = bg.lightness() < 128;
    const QString red = isDark ? QStringLiteral("#FF6B6B") : QStringLiteral("#B24A4A");

    if (m_updateStatusLabel)
    {
        if (m_hasNewVersion)
            m_updateStatusLabel->setStyleSheet(QStringLiteral("color:%1;font-weight:bold;").arg(red));
        else
            m_updateStatusLabel->setStyleSheet(QString());
    }

    if (m_openReleasesBtn)
        m_openReleasesBtn->setStyleSheet(QString());
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

    if (!forceNetwork && applyCachedUpdateStatusIfFresh())
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
    updateUpdateHighlight(false);
    m_checkUpdateBtn->setEnabled(false);

    QNetworkRequest req{QUrl(m_updateApiUrl)};
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
        updateUpdateHighlight(false);
        saveUpdateCache(QStringLiteral("neterr"));
        return;
    }

    QJsonParseError jerr{};
    const QJsonDocument doc = QJsonDocument::fromJson(body, &jerr);
    if (jerr.error != QJsonParseError::NoError || !doc.isObject())
    {
        setUpdateStatus(formatText(m_updateParseError));
        updateUpdateHighlight(false);
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
        updateUpdateHighlight(false);
        saveUpdateCache(QStringLiteral("noversion"));
        return;
    }

    const int cmp = compareVersions(current, latestVer);
    QString s = (cmp >= 0) ? m_updateLatestTpl : m_updateNewTpl;
    s.replace("{latest}", latestVer);
    setUpdateStatus(formatText(s));
    updateUpdateHighlight(cmp < 0);

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

    if (eventTarget == m_checkUpdateBtn || eventTarget == m_openReleasesBtn || eventTarget == m_privacyBtn)
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
        if (m_showingChildDialog)
            return QDialog::eventFilter(watched, event);
        accept();
        return false;
    case QEvent::ApplicationPaletteChange:
    case QEvent::PaletteChange:
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

    QObject *focusObj = QApplication::focusObject();
    const bool focusIsButton = (focusObj == m_checkUpdateBtn || focusObj == m_openReleasesBtn || focusObj == m_privacyBtn);
    if (focusIsButton && (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter || event->key() == Qt::Key_Space))
    {
        QDialog::keyPressEvent(event);
        return;
    }

    accept();
}
