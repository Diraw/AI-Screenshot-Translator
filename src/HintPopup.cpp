#include "HintPopup.h"

#include "ConfigManager.h"
#include "TranslationManager.h"

#include "ThemeUtils.h"

#include <QApplication>
#include <QCheckBox>
#include <QCoreApplication>
#include <QEvent>
#include <QFile>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonValue>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QSettings>
#include <QVBoxLayout>
#include <QRegularExpression>

namespace
{
    QRect preferredCenterRectFor(QWidget *w)
    {
        if (w)
            return w->frameGeometry();

        QScreen *screen = QGuiApplication::primaryScreen();
        if (!screen)
            return QRect(0, 0, 640, 480);
        return screen->availableGeometry();
    }
}

HintPopup::HintPopup(QWidget *parent)
    : QDialog(parent)
{
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::Tool);
    setModal(false);
    setObjectName(QStringLiteral("hintPopup"));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(14, 12, 14, 12);
    root->setSpacing(8);

    m_titleLabel = new QLabel(this);
    QFont titleFont = m_titleLabel->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 1);
    m_titleLabel->setFont(titleFont);
    root->addWidget(m_titleLabel);

    m_bodyLabel = new QLabel(this);
    m_bodyLabel->setWordWrap(true);
    root->addWidget(m_bodyLabel);

    auto *bottomRow = new QHBoxLayout();
    bottomRow->setContentsMargins(0, 0, 0, 0);

    m_dontShowAgain = new QCheckBox(this);
    bottomRow->addWidget(m_dontShowAgain);

    bottomRow->addStretch();

    m_closeBtn = new QPushButton(this);
    connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    bottomRow->addWidget(m_closeBtn);

    root->addLayout(bottomRow);

    const bool isDark = ThemeUtils::isSystemDark();
    if (isDark)
    {
        setStyleSheet(
            "#hintPopup{background:rgba(25,25,25,235);border:1px solid rgba(255,255,255,40);border-radius:10px;}"
            "#hintPopup QLabel{color:#ffffff;font-size:13px;}"
            "#hintPopup QCheckBox{color:#ffffff;font-size:13px;}"
            "#hintPopup QPushButton{color:#ffffff;background:rgba(255,255,255,20);border:1px solid rgba(255,255,255,50);border-radius:6px;padding:4px 10px;}"
            "#hintPopup QPushButton:hover{background:rgba(255,255,255,28);}"
            "#hintPopup QPushButton:pressed{background:rgba(255,255,255,18);}");
    }
    else
    {
        setStyleSheet(
            "#hintPopup{background:rgba(255,255,255,245);border:1px solid rgba(0,0,0,40);border-radius:10px;}"
            "#hintPopup QLabel{color:#111111;font-size:13px;}"
            "#hintPopup QCheckBox{color:#111111;font-size:13px;}"
            "#hintPopup QPushButton{color:#111111;background:rgba(0,0,0,6);border:1px solid rgba(0,0,0,35);border-radius:6px;padding:4px 10px;}"
            "#hintPopup QPushButton:hover{background:rgba(0,0,0,10);}"
            "#hintPopup QPushButton:pressed{background:rgba(0,0,0,6);}");
    }

    // Close-on-input behavior (but keep checkbox/button usable)
    installEventFilter(this);
    if (m_titleLabel)
        m_titleLabel->installEventFilter(this);
    if (m_bodyLabel)
        m_bodyLabel->installEventFilter(this);
}

QString HintPopup::prettyHotkey(QString s)
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

QString HintPopup::formatHintText(QString text, const AppConfig &cfg)
{
    // Defaults
    const QString shotToggle = prettyHotkey(cfg.screenshotToggleHotkey.isEmpty() ? QStringLiteral("s") : cfg.screenshotToggleHotkey);
    const QString viewToggle = prettyHotkey(cfg.viewToggleHotkey.isEmpty() ? QStringLiteral("r") : cfg.viewToggleHotkey);
    const QString edit = prettyHotkey(cfg.editHotkey.isEmpty() ? QStringLiteral("e") : cfg.editHotkey);
    const QString bold = prettyHotkey(cfg.boldHotkey.isEmpty() ? QStringLiteral("ctrl+b") : cfg.boldHotkey);
    const QString underline = prettyHotkey(cfg.underlineHotkey.isEmpty() ? QStringLiteral("ctrl+u") : cfg.underlineHotkey);
    const QString highlight = prettyHotkey(cfg.highlightHotkey.isEmpty() ? QStringLiteral("ctrl+h") : cfg.highlightHotkey);
    const QString exit = QStringLiteral("Esc");
    const QString tag = prettyHotkey(cfg.tagHotkey.isEmpty() ? QStringLiteral("Esc") : cfg.tagHotkey);
    const QString batchSelect = prettyHotkey(cfg.selectionToggleHotkey.isEmpty() ? QStringLiteral("Ctrl+S") : cfg.selectionToggleHotkey);
    const QString prevPage = prettyHotkey(cfg.prevResultShortcut.isEmpty() ? QStringLiteral("z") : cfg.prevResultShortcut);
    const QString nextPage = prettyHotkey(cfg.nextResultShortcut.isEmpty() ? QStringLiteral("x") : cfg.nextResultShortcut);

    text.replace("{shotToggle}", shotToggle);
    text.replace("{viewToggle}", viewToggle);
    text.replace("{edit}", edit);
    text.replace("{bold}", bold);
    text.replace("{underline}", underline);
    text.replace("{highlight}", highlight);
    text.replace("{exit}", exit);
    text.replace("{tag}", tag);
    text.replace("{batchSelect}", batchSelect);

    // Page navigation (ResultWindow)
    text.replace("{prevPage}", prevPage);
    text.replace("{nextPage}", nextPage);
    text.replace("{prevResult}", prevPage);
    text.replace("{nextResult}", nextPage);

    // Back-compat / convenience: if the user wrote the label but left it blank, auto-append the hotkey.
    // e.g. "• 向左翻页：\n" -> "• 向左翻页：Z\n"
    {
        static const QRegularExpression reLeft(QStringLiteral(R"((向左翻页：)\s*(\r?\n|$))"));
        static const QRegularExpression reRight(QStringLiteral(R"((向右翻页：)\s*(\r?\n|$))"));
        text.replace(reLeft, QStringLiteral("\\1%1\\2").arg(prevPage));
        text.replace(reRight, QStringLiteral("\\1%1\\2").arg(nextPage));
    }
    return text;
}

QJsonObject HintPopup::loadHintsJson()
{
    // Load language-specific hints file
    QString lang = TranslationManager::instance().getLanguage();
    if (lang != "en" && lang != "zh")
        lang = "zh"; // Default to Chinese for other languages
    
    const QString path = QCoreApplication::applicationDirPath() + "/assets/window_hints_" + lang + ".json";
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return QJsonObject();

    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject())
        return QJsonObject();

    return doc.object();
}

QString HintPopup::jsonString(const QJsonObject &root, const QString &path, const QString &fallback)
{
    QJsonValue v(root);
    const QStringList keys = path.split('.', Qt::SkipEmptyParts);
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

QString HintPopup::settingsKeyForKind(Kind kind)
{
    switch (kind)
    {
    case Kind::ResultWindow:
        return QStringLiteral("hints/hideResultWindowHint");
    case Kind::ArchiveWindow:
        return QStringLiteral("hints/hideArchiveWindowHint");
    }
    return QStringLiteral("hints/hideUnknownHint");
}

QString HintPopup::jsonPathForKind(Kind kind)
{
    switch (kind)
    {
    case Kind::ResultWindow:
        return QStringLiteral("resultWindow");
    case Kind::ArchiveWindow:
        return QStringLiteral("archiveWindow");
    }
    return QStringLiteral("resultWindow");
}

void HintPopup::setUiText(const QString &title, const QString &body, const QString &dontShowAgainText, const QString &closeText)
{
    if (m_titleLabel)
        m_titleLabel->setText(title);
    if (m_bodyLabel)
        m_bodyLabel->setText(body);
    if (m_dontShowAgain)
        m_dontShowAgain->setText(dontShowAgainText);
    if (m_closeBtn)
        m_closeBtn->setText(closeText);
}

void HintPopup::closeIfNonInteractive(QObject *eventTarget)
{
    if (!eventTarget)
    {
        accept();
        return;
    }

    // Do not auto-close if interacting with checkbox/button
    if (eventTarget == m_dontShowAgain || eventTarget == m_closeBtn)
        return;

    accept();
}

bool HintPopup::eventFilter(QObject *watched, QEvent *event)
{
    if (!event)
        return QDialog::eventFilter(watched, event);

    switch (event->type())
    {
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

void HintPopup::keyPressEvent(QKeyEvent *event)
{
    if (!event)
    {
        QDialog::keyPressEvent(event);
        return;
    }

    QObject *focusObj = QApplication::focusObject();
    const bool focusIsCheckbox = (focusObj == m_dontShowAgain);
    const bool focusIsButton = (focusObj == m_closeBtn);

    // Allow Space/Enter to operate checkbox/button without closing.
    if ((focusIsCheckbox || focusIsButton) && (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter || event->key() == Qt::Key_Space))
    {
        QDialog::keyPressEvent(event);
        return;
    }

    accept();
}

void HintPopup::maybeShow(Kind kind, QWidget *parentWindow, const AppConfig &cfg)
{
    QSettings settings(ConfigManager::settingsIniPath(), QSettings::IniFormat);
    const bool hide = settings.value(settingsKeyForKind(kind), false).toBool();
    if (hide)
        return;

    const QJsonObject root = loadHintsJson();
    const QString basePath = jsonPathForKind(kind);

    const QString title = jsonString(root, basePath + ".title", QStringLiteral("Hint"));
    const QString rawText = jsonString(root, basePath + ".text", QStringLiteral("Hint:"));
    const QString dontShow = jsonString(root, QStringLiteral("ui.dontShowAgain"), QStringLiteral("Don't show again"));
    const QString closeText = jsonString(root, QStringLiteral("ui.close"), QStringLiteral("Close"));

    HintPopup *dlg = new HintPopup(parentWindow);
    dlg->setAttribute(Qt::WA_DeleteOnClose);

    dlg->setUiText(title, formatHintText(rawText, cfg), dontShow, closeText);
    dlg->adjustSize();

    const QRect area = preferredCenterRectFor(parentWindow);
    dlg->move(area.center() - QPoint(dlg->width() / 2, dlg->height() / 2));

    QObject::connect(dlg, &QDialog::finished, dlg, [dlg, kind]()
                     {
        if (!dlg)
            return;
        if (dlg->m_dontShowAgain && dlg->m_dontShowAgain->isChecked())
        {
            QSettings settings(ConfigManager::settingsIniPath(), QSettings::IniFormat);
            settings.setValue(settingsKeyForKind(kind), true);
        } });

    dlg->show();
    dlg->raise();
    dlg->activateWindow();
}
