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
#include <QTimer>
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

    v = v.trimmed();

    QString core = v;
    QString pre;
    const int dashIdx = v.indexOf('-');
    if (dashIdx >= 0)
    {
        core = v.left(dashIdx);
        pre = v.mid(dashIdx + 1);
    }
    else
    {
        static const QRegularExpression spacedPreRe(
            QStringLiteral(R"(^\s*(\d+(?:\.\d+){0,2})\s+([A-Za-z0-9][A-Za-z0-9.\-_]*)\s*$)"));
        const QRegularExpressionMatch spacedPre = spacedPreRe.match(v);
        if (spacedPre.hasMatch())
        {
            core = spacedPre.captured(1);
            pre = spacedPre.captured(2);
        }
    }

    SemVer out;
    core = core.trimmed();
    const QStringList coreParts = core.split('.', Qt::SkipEmptyParts);
    auto parseCoreNumber = [](const QString &part, QString *suffix = nullptr)
    {
        static const QRegularExpression partRe(QStringLiteral(R"(^\s*(\d+)(?:\s*([A-Za-z][A-Za-z0-9.\-_]*))?\s*$)"));
        const QRegularExpressionMatch match = partRe.match(part);
        if (!match.hasMatch())
            return 0;
        if (suffix)
            *suffix = match.captured(2).trimmed();
        return match.captured(1).toInt();
    };

    if (coreParts.size() >= 1)
        out.major = parseCoreNumber(coreParts[0]);
    if (coreParts.size() >= 2)
        out.minor = parseCoreNumber(coreParts[1]);
    if (coreParts.size() >= 3)
    {
        QString suffixFromPatch;
        out.patch = parseCoreNumber(coreParts[2], &suffixFromPatch);
        if (pre.isEmpty() && !suffixFromPatch.isEmpty())
            pre = suffixFromPatch;
    }

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
    const QString displayText = settings.value(QStringLiteral("displayText"), QString()).toString();
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
        setUpdateStatus(displayText.isEmpty() ? formatText(m_updateNetworkError) : displayText);
        updateUpdateHighlight(false);
        return true;
    }
    if (status == QStringLiteral("parseerr"))
    {
        setUpdateStatus(displayText.isEmpty() ? formatText(m_updateParseError) : displayText);
        updateUpdateHighlight(false);
        return true;
    }
    if (status == QStringLiteral("noversion"))
    {
        setUpdateStatus(displayText.isEmpty() ? formatText(m_updateNoVersion) : displayText);
        updateUpdateHighlight(false);
        return true;
    }

    return false;
}

void StartupWindow::saveUpdateCache(const QString &status,
                                    const QString &latestVer,
                                    const QString &latestUrl,
                                    const QString &displayText)
{
    QSettings settings(ConfigManager::settingsIniPath(), QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("updateCache"));
    settings.setValue(QStringLiteral("checkedAtMs"), QDateTime::currentMSecsSinceEpoch());
    settings.setValue(QStringLiteral("status"), status);
    settings.setValue(QStringLiteral("latestVer"), latestVer);
    settings.setValue(QStringLiteral("latestUrl"), latestUrl);
    settings.setValue(QStringLiteral("displayText"), displayText);
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

QString StartupWindow::buildUpdateNetworkErrorStatus(const QNetworkReply *reply) const
{
    const QString base = formatText(m_updateNetworkError);
    if (!reply)
        return base;

    QStringList details;
    details << QStringLiteral("\u9519\u8BEF\u7801 %1").arg(static_cast<int>(reply->error()));

    const QVariant httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
    if (httpStatus.isValid())
        details << QStringLiteral("HTTP %1").arg(httpStatus.toInt());

    const QString errorText = reply->errorString().trimmed();
    if (!errorText.isEmpty())
        details << errorText;

    if (details.isEmpty())
        return base;

    return base + QStringLiteral("\n") + details.join(QStringLiteral(" | "));
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
    m_reply->setProperty("requestTimedOut", false);

    auto *timeoutTimer = new QTimer(m_reply);
    timeoutTimer->setObjectName(QStringLiteral("startupUpdateTimeoutTimer"));
    timeoutTimer->setSingleShot(true);
    timeoutTimer->setInterval(kUpdateCheckTimeoutMs);
    QPointer<QNetworkReply> reply = m_reply;
    connect(timeoutTimer, &QTimer::timeout, this, [reply]()
            {
                if (!reply || reply->isFinished())
                    return;

                reply->setProperty("requestTimedOut", true);
                reply->abort();
            });
    timeoutTimer->start();
    connect(m_reply, &QNetworkReply::finished, this, [this, reply]()
            { onUpdateReplyFinished(reply.data()); });
}

void StartupWindow::onUpdateReplyFinished(QNetworkReply *reply)
{
    if (!reply)
        return;

    if (reply != m_reply)
    {
        reply->deleteLater();
        return;
    }

    if (QTimer *timer = reply->findChild<QTimer *>(QStringLiteral("startupUpdateTimeoutTimer")))
        timer->stop();

    const QByteArray body = reply->readAll();
    const auto err = reply->error();
    QString networkErrorText;
    if (err != QNetworkReply::NoError)
    {
        if (reply->property("requestTimedOut").toBool())
        {
            networkErrorText = formatText(m_updateNetworkError)
                               + QStringLiteral("\n")
                               + QStringLiteral("Request timed out after %1 seconds.")
                                     .arg(kUpdateCheckTimeoutMs / 1000);
        }
        else
        {
            networkErrorText = buildUpdateNetworkErrorStatus(reply);
        }
    }

    reply->deleteLater();
    m_reply = nullptr;

    if (m_checkUpdateBtn)
        m_checkUpdateBtn->setEnabled(true);

    if (err != QNetworkReply::NoError)
    {
        setUpdateStatus(networkErrorText);
        updateUpdateHighlight(false);
        saveUpdateCache(QStringLiteral("neterr"), QString(), QString(), networkErrorText);
        return;
    }

    QJsonParseError jerr{};
    const QJsonDocument doc = QJsonDocument::fromJson(body, &jerr);
    if (jerr.error != QJsonParseError::NoError || !doc.isObject())
    {
        const QString parseErrorText = formatText(m_updateParseError)
                                       + QStringLiteral("\n%1 (offset %2)")
                                             .arg(jerr.errorString())
                                             .arg(jerr.offset);
        setUpdateStatus(parseErrorText);
        updateUpdateHighlight(false);
        saveUpdateCache(QStringLiteral("parseerr"), QString(), QString(), parseErrorText);
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
        const QString noVersionText = formatText(m_updateNoVersion);
        setUpdateStatus(noVersionText);
        updateUpdateHighlight(false);
        saveUpdateCache(QStringLiteral("noversion"), QString(), QString(), noVersionText);
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
