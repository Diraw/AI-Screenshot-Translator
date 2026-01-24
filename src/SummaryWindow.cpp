#include "SummaryWindow.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QCloseEvent>
#include <QUuid>

#include <QIcon>
#include <QMenu>
#include <QAction>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QTimer>
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QRegularExpression>
#include <QJsonArray>
#include <QJsonDocument>
#include <QColor>
#include <algorithm>

#include "TranslationManager.h"
#include "HistoryManager.h"

#include "EmbedWebView.h"
#include "TagDialog.h"
#include "ThemeUtils.h"
#include <QMessageBox>

static QString normalizeCssColor(const QString &input, const QString &fallback)
{
    QString s = input.trimmed();
    if (s.isEmpty())
        s = fallback;

    auto clamp255 = [](int v)
    { return std::max(0, std::min(255, v)); };
    auto normalizeAlpha01 = [](double a)
    {
        if (a <= 1.0)
            return std::max(0.0, std::min(1.0, a));
        if (a <= 255.0)
            return std::max(0.0, std::min(1.0, a / 255.0));
        return 1.0;
    };

    // Accept rgba(r,g,b,a) where a is 0..1 or 0..255
    {
        QRegularExpression re(
            "^rgba\\(\\s*(\\d{1,3})\\s*,\\s*(\\d{1,3})\\s*,\\s*(\\d{1,3})\\s*,\\s*([0-9]*\\.?[0-9]+)\\s*\\)$",
            QRegularExpression::CaseInsensitiveOption);
        auto m = re.match(s);
        if (m.hasMatch())
        {
            bool ok1 = false, ok2 = false, ok3 = false, ok4 = false;
            int r = m.captured(1).toInt(&ok1);
            int g = m.captured(2).toInt(&ok2);
            int b = m.captured(3).toInt(&ok3);
            double a = m.captured(4).toDouble(&ok4);
            if (ok1 && ok2 && ok3 && ok4 && r >= 0 && r <= 255 && g >= 0 && g <= 255 && b >= 0 && b <= 255)
            {
                const double a01 = normalizeAlpha01(a);
                return QString("rgba(%1,%2,%3,%4)")
                    .arg(r)
                    .arg(g)
                    .arg(b)
                    .arg(a01, 0, 'f', 3);
            }
        }
    }

    {
        QRegularExpression re(
            "^rgb\\(\\s*(\\d{1,3})\\s*,\\s*(\\d{1,3})\\s*,\\s*(\\d{1,3})\\s*\\)$",
            QRegularExpression::CaseInsensitiveOption);
        auto m = re.match(s);
        if (m.hasMatch())
        {
            bool ok1 = false, ok2 = false, ok3 = false;
            int r = m.captured(1).toInt(&ok1);
            int g = m.captured(2).toInt(&ok2);
            int b = m.captured(3).toInt(&ok3);
            if (ok1 && ok2 && ok3 && r >= 0 && r <= 255 && g >= 0 && g <= 255 && b >= 0 && b <= 255)
                return QString("rgb(%1,%2,%3)").arg(r).arg(g).arg(b);
        }
    }

    {
        QStringList parts = s.split(',', Qt::SkipEmptyParts);
        if (parts.size() == 3)
        {
            bool ok1 = false, ok2 = false, ok3 = false;
            int r = parts[0].trimmed().toInt(&ok1);
            int g = parts[1].trimmed().toInt(&ok2);
            int b = parts[2].trimmed().toInt(&ok3);
            if (ok1 && ok2 && ok3 && r >= 0 && r <= 255 && g >= 0 && g <= 255 && b >= 0 && b <= 255)
                return QString("rgb(%1,%2,%3)").arg(r).arg(g).arg(b);
        }

        if (parts.size() == 4)
        {
            bool ok1 = false, ok2 = false, ok3 = false, ok4 = false;
            int r = parts[0].trimmed().toInt(&ok1);
            int g = parts[1].trimmed().toInt(&ok2);
            int b = parts[2].trimmed().toInt(&ok3);
            double a = parts[3].trimmed().toDouble(&ok4);
            if (ok1 && ok2 && ok3 && ok4 && r >= 0 && r <= 255 && g >= 0 && g <= 255 && b >= 0 && b <= 255)
            {
                const double a01 = normalizeAlpha01(a);
                return QString("rgba(%1,%2,%3,%4)")
                    .arg(r)
                    .arg(g)
                    .arg(b)
                    .arg(a01, 0, 'f', 3);
            }
        }
    }

    // Accept #RGBA / #RRGGBBAA (normalize to rgba for broad compatibility)
    {
        QRegularExpression re4("^#([0-9a-f]{4})$", QRegularExpression::CaseInsensitiveOption);
        auto m = re4.match(s);
        if (m.hasMatch())
        {
            const QString hex = m.captured(1);
            bool ok = false;
            const int r = QString(hex[0]).repeated(2).toInt(&ok, 16);
            if (!ok)
                return fallback;
            const int g = QString(hex[1]).repeated(2).toInt(&ok, 16);
            if (!ok)
                return fallback;
            const int b = QString(hex[2]).repeated(2).toInt(&ok, 16);
            if (!ok)
                return fallback;
            const int a255 = QString(hex[3]).repeated(2).toInt(&ok, 16);
            if (!ok)
                return fallback;
            const double a01 = normalizeAlpha01((double)clamp255(a255));
            return QString("rgba(%1,%2,%3,%4)")
                .arg(clamp255(r))
                .arg(clamp255(g))
                .arg(clamp255(b))
                .arg(a01, 0, 'f', 3);
        }
    }

    {
        QRegularExpression re8("^#([0-9a-f]{8})$", QRegularExpression::CaseInsensitiveOption);
        auto m = re8.match(s);
        if (m.hasMatch())
        {
            const QString hex = m.captured(1);
            bool ok = false;
            const int r = hex.mid(0, 2).toInt(&ok, 16);
            if (!ok)
                return fallback;
            const int g = hex.mid(2, 2).toInt(&ok, 16);
            if (!ok)
                return fallback;
            const int b = hex.mid(4, 2).toInt(&ok, 16);
            if (!ok)
                return fallback;
            const int a255 = hex.mid(6, 2).toInt(&ok, 16);
            if (!ok)
                return fallback;
            const double a01 = normalizeAlpha01((double)clamp255(a255));
            return QString("rgba(%1,%2,%3,%4)")
                .arg(clamp255(r))
                .arg(clamp255(g))
                .arg(clamp255(b))
                .arg(a01, 0, 'f', 3);
        }
    }

    {
        QRegularExpression re("^#([0-9a-f]{3}|[0-9a-f]{6})$", QRegularExpression::CaseInsensitiveOption);
        if (re.match(s).hasMatch())
            return s;
    }

    return fallback;
}

void SummaryWindow::setConfig(const AppConfig &config)
{
    m_config = config;

    if (m_webView)
    {
        const QString mark = normalizeCssColor(m_config.highlightMarkColor, "#ffeb3b");
        const QString markDark = normalizeCssColor(m_config.highlightMarkColorDark, "#d4af37");
        const QString js = QString(
                               "(()=>{try{document.documentElement.style.setProperty('--mark-bg', '%1');"
                               "document.documentElement.style.setProperty('--mark-bg-dark', '%2');}catch(e){}})();")
                               .arg(mark, markDark);
        m_webView->eval(js.toStdString());
    }
}

SummaryWindow::SummaryWindow(QWidget *parent) : QWidget(parent)
{
    setWindowTitle(TranslationManager::instance().tr("summary_title"));
    setWindowIcon(QIcon(":/assets/icon.ico"));
    resize(1000, 750); // Landscape default as requested

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    // Create container for webview
    m_webContainer = new QWidget(this);
    m_webContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(m_webContainer);

    // EmbedWebView handles native window attachment
    m_webView = std::make_unique<EmbedWebView>(m_webContainer);
    qApp->installEventFilter(this);

    // DevTools shortcut for debugging WebView content
    QShortcut *devToolsShortcut = new QShortcut(QKeySequence(Qt::Key_F12), this);
    devToolsShortcut->setContext(Qt::ApplicationShortcut);
    connect(devToolsShortcut, &QShortcut::activated, this, [this]()
            {
        if (m_webView) m_webView->openDevTools(); });

    // Bind 'cmd_restore'
    m_webView->bind("cmd_restore", [this](std::string seq, std::string req, void *arg)
                    {
        QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(req));
        if (doc.isArray() && !doc.array().isEmpty()) {
            QString id = doc.array().at(0).toString();
            emit restorePreviewRequested(id);
        } });

    // Bind 'cmd_delete'
    m_webView->bind("cmd_delete", [this](std::string seq, std::string req, void *arg)
                    {
        QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(req));
        if (doc.isArray() && !doc.array().isEmpty()) {
            QString id = doc.array().at(0).toString();
            // Remove from entries list
            for (int i = 0; i < m_entries.size(); ++i) {
                if (m_entries[i].id == id) {
                    m_entries.removeAt(i);
                    break;
                }
            }
            emit requestDeleteEntry(id);
        } });

    // Bind 'cmd_updateEntry'
    m_webView->bind("cmd_updateEntry", [this](std::string seq, std::string req, void *arg)
                    {
        // req is JSON array [id, content]
        QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(req));
        if (doc.isArray()) {
            QJsonArray arr = doc.array();
            if (arr.size() >= 2) {
                QString id = arr.at(0).toString();
                QString content = arr.at(1).toString();
                emit entryEdited(id, content);
            }
        } });

    // JS log bridge
    m_webView->bind("cmd_log", [this](std::string seq, std::string req, void *arg)
                    {
        QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(req));
        if (doc.isArray() && !doc.array().isEmpty()) {
            qDebug() << "[JS]" << doc.array().at(0).toString();
        } else {
            qDebug() << "[JS]" << QString::fromStdString(req);
        } });

    // Scroll position updates from JS
    m_webView->bind("cmd_scroll", [this](std::string seq, std::string req, void *arg)
                    {
        QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(req));
        if (doc.isArray() && !doc.array().isEmpty()) {
            m_lastScrollY = doc.array().at(0).toDouble();
        } });
    m_webView->bind("cmd_exitSelectionMode", [this](std::string, std::string, void *)
                    {
        if (m_selectionModeBtn) m_selectionModeBtn->setChecked(false); });

    // Bind DevTools opener for JS-triggered hotkey inside WebView
    m_webView->bind("cmd_openDevTools", [this](std::string, std::string, void *)
                    {
        if (m_webView) {
            qDebug() << "[DevTools] Request from JS in SummaryWindow";
            m_webView->openDevTools();
        } });

    connect(m_webView.get(), &EmbedWebView::ready, this, [this]()
            {
        if (m_webContainer && m_webView) {
            m_webView->setSize(m_webContainer->width(), m_webContainer->height());
            m_webView->focusNative();
        } });

    auto parseIdsFromReq = [](const std::string &req) -> QStringList
    {
        QStringList ids;
        QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(req));
        if (!doc.isArray())
            return ids;

        QJsonArray arr = doc.array();
        // Case A: ["id1", "id2"] - multiple args passed from JSspread
        // Case B: ["[\"id1\", \"id2\"]"] - user's reported "double encoded" case

        for (const auto &v : arr)
        {
            QString s = v.toString();
            if (s.startsWith("[") && s.endsWith("]"))
            {
                // Try to parse inner array
                QJsonDocument innerDoc = QJsonDocument::fromJson(s.toUtf8());
                if (innerDoc.isArray())
                {
                    for (const auto &iv : innerDoc.array())
                        ids << iv.toString();
                    continue;
                }
            }
            if (v.isString())
                ids << s;
            else if (v.isArray())
            {
                for (const auto &iv : v.toArray())
                    ids << iv.toString();
            }
        }
        return ids;
    };

    // Bind batch operations
    m_webView->bind("cmd_batchDelete", [this, parseIdsFromReq](std::string seq, std::string req, void *arg)
                    {
        qDebug() << "cmd_batchDelete triggered. Raw request:" << QString::fromStdString(req);
        QStringList ids = parseIdsFromReq(req);
        qDebug() << "Extracted IDs count:" << ids.size() << "IDs:" << ids;
        if (ids.isEmpty()) return;
        
        auto reply = QMessageBox::question(this, tr("Delete"), tr("Are you sure you want to delete %1 selected entries?").arg(ids.size()));
        if (reply == QMessageBox::Yes) {
            if (m_historyManager && m_historyManager->deleteEntries(ids)) {
                qDebug() << "Batch delete successful. Updating local list.";
                QSet<QString> idSet = QSet<QString>(ids.begin(), ids.end());
                for (int i = m_entries.size() - 1; i >= 0; --i) {
                    if (idSet.contains(m_entries[i].id)) m_entries.removeAt(i);
                }
                applyFilters(); // Refresh UI
            } else {
                qDebug() << "Batch delete failed in HistoryManager.";
            }
        } });

    m_webView->bind("cmd_batchAddTags", [this, parseIdsFromReq](std::string seq, std::string req, void *arg)
                    {
        qDebug() << "cmd_batchAddTags triggered. Raw request:" << QString::fromStdString(req);
        QStringList ids = parseIdsFromReq(req);
        qDebug() << "Extracted IDs count:" << ids.size() << "IDs:" << ids;
        if (ids.isEmpty() || !m_historyManager) return;

        QStringList allTags = m_historyManager->getAllTags();
        TagDialog *dialog = new TagDialog(allTags, QStringList(), this);
        connect(dialog, &TagDialog::tagsUpdated, this, [this, ids](const QStringList& tags) {
            qDebug() << "Batch add tags - user selected tags:" << tags;
            if (m_historyManager->addTagsToEntries(ids, tags)) {
                qDebug() << "Batch add tags successful. Reloading entries.";
                m_entries = m_historyManager->loadEntries();
                applyFilters();
                loadAvailableTags();
            } else {
                qDebug() << "Batch add tags returned false (maybe no changes needed or failed).";
            }
        });
        dialog->exec();
        dialog->deleteLater(); });

    m_webView->bind("cmd_batchRemoveTags", [this, parseIdsFromReq](std::string seq, std::string req, void *arg)
                    {
        qDebug() << "cmd_batchRemoveTags triggered. Raw request:" << QString::fromStdString(req);
        QStringList ids = parseIdsFromReq(req);
        qDebug() << "Extracted IDs count:" << ids.size() << "IDs:" << ids;
        if (ids.isEmpty() || !m_historyManager) return;

        QStringList allTags = m_historyManager->getAllTags();
        TagDialog *dialog = new TagDialog(allTags, QStringList(), this);
        connect(dialog, &TagDialog::tagsUpdated, this, [this, ids](const QStringList& tags) {
            qDebug() << "Batch remove tags - user selected tags:" << tags;
            if (m_historyManager->removeTagsFromEntries(ids, tags)) {
                qDebug() << "Batch remove tags successful. Reloading entries.";
                m_entries = m_historyManager->loadEntries();
                applyFilters();
                loadAvailableTags();
            } else {
                qDebug() << "Batch remove tags returned false (maybe no changes needed or failed).";
            }
        });
        dialog->exec();
        dialog->deleteLater(); });

    // We need to initialize HTML after webview is ready.
    QTimer::singleShot(0, this, [this]()
                       {
        initHtml();
        // Force an initial resize to ensure WebView matches container
        QTimer::singleShot(100, this, [this](){
            if (m_webView && m_webContainer) {
                qDebug() << "Forcing initial WebView size:" << m_webContainer->width() << "x" << m_webContainer->height();
                m_webView->setSize(m_webContainer->width(), m_webContainer->height());
                // Restore last scroll position
                m_webView->eval(QString("window.scrollTo(0,%1);").arg(m_lastScrollY).toStdString());
            }
        }); });

    // Setup filter toolbar
    setupFilterUI();

    // Restore window state from previous session
    restoreState();

    // Initial Theme Update
    updateTheme(ThemeUtils::isSystemDark());
}

SummaryWindow::~SummaryWindow()
{
    // unique_ptr handles cleanup
}

void SummaryWindow::resizeEvent(QResizeEvent *event)
{
    qDebug() << "SummaryWindow::resizeEvent - Window:" << width() << "x" << height()
             << "Container:" << (m_webContainer ? m_webContainer->size() : QSize(0, 0));
    QWidget::resizeEvent(event);
}

bool SummaryWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::KeyPress)
    {
        QKeyEvent *ke = static_cast<QKeyEvent *>(event);
        qDebug() << "[KeyEvent]" << ke->key() << ke->text() << ke->modifiers() << "obj" << (watched ? watched->metaObject()->className() : "null");
    }
    return QWidget::eventFilter(watched, event);
}

// closeEvent restored
void SummaryWindow::closeEvent(QCloseEvent *event)
{
    saveState();
    emit closed();
    hide();          // Just hide instead of close
    event->ignore(); // Don't actually close, just hide
}

void SummaryWindow::setInitialHistory(const QList<TranslationEntry> &history)
{
    captureScrollPosition();
    m_entries = history;
    refreshHtml();
}

void SummaryWindow::addEntry(const TranslationEntry &entry)
{
    m_entries.append(entry);
    appendEntryHtml(entry);
}

void SummaryWindow::clearEntries()
{
    captureScrollPosition();
    m_entries.clear();
    refreshHtml();
}

const TranslationEntry *SummaryWindow::getEntry(const QString &id) const
{
    for (const auto &entry : m_entries)
    {
        if (entry.id == id)
            return &entry;
    }
    return nullptr;
}

void SummaryWindow::updateEntryGeometry(const QString &id, const QPoint &pos, const QSize &size)
{
    for (auto &entry : m_entries)
    {
        if (entry.id == id)
        {
            entry.lastPosition = pos;
            entry.lastSize = size;
            entry.hasLastPosition = true;
            break;
        }
    }
}

void SummaryWindow::updateEntry(const QString &id, const QString &markdown)
{
    // Alias for updateEntryContent to match the API calls from App.cpp
    updateEntryContent(id, markdown);
}

void SummaryWindow::updateEntryContent(const QString &id, const QString &markdown)
{
    for (auto &entry : m_entries)
    {
        if (entry.id == id)
        {
            entry.translatedMarkdown = markdown;
            break;
        }
    }
    if (m_webView)
    {
        // Simple unescape for JS injection
        QString safeMd = markdown;
        safeMd.replace("\\", "\\\\").replace("'", "\\'").replace("\n", "\\n").replace("\r", "");
        QString js = QString("updateEntryInDom('%1', '%2')").arg(id, safeMd);
        m_webView->eval(js.toStdString());
    }
}

void SummaryWindow::setZoomFactor(qreal factor)
{
    m_currentZoom = factor;
    if (m_webView)
    {
        m_webView->eval("document.body.style.zoom = '" + std::to_string(factor) + "'");
    }
}

qreal SummaryWindow::getZoomFactor() const
{
    return m_currentZoom;
}

void SummaryWindow::captureScrollPosition()
{
    if (m_webView)
    {
        m_webView->eval("if(window.cmd_scroll){window.cmd_scroll(window.scrollY);}");
    }
}

// Duplicate closeEvent removed

// Helper struct for math protection
struct ProtectedContent
{
    QString text;
    QStringList mathBlocks;
};

static ProtectedContent protectMath(const QString &markdown)
{
    ProtectedContent result;
    result.text = markdown;

    QRegularExpression finalRegex(R"((\$\$[\s\S]*?\$\$)|(\\\[[\s\S]*?\\\])|(\\\([\s\S]*?\\\))|(\$[^\$\n]+\$))");

    int counter = 0;
    QString newText;
    int lastPos = 0;

    QRegularExpressionMatchIterator i = finalRegex.globalMatch(result.text);
    while (i.hasNext())
    {
        QRegularExpressionMatch match = i.next();
        newText.append(result.text.mid(lastPos, match.capturedStart() - lastPos));

        QString matchStr = match.captured();
        result.mathBlocks.append(matchStr);

        QString placeholder = QString("MATHBLOCKPH%1").arg(counter++);
        newText.append(placeholder);

        lastPos = match.capturedEnd();
    }
    newText.append(result.text.mid(lastPos));
    result.text = newText;
    return result;
}

void SummaryWindow::saveState()
{
    QSettings settings("YourCompany", "AIScreenshotTranslator");
    settings.setValue("summaryWindow/geometry", saveGeometry());
    settings.setValue("summaryWindow/zoom", m_currentZoom);
    captureScrollPosition();
    settings.setValue("summaryWindow/scrollY", m_lastScrollY);

    // Save scroll position via JavaScript
    if (m_webView)
    {
        QString jsGetScroll = "window.scrollY;";
        // Note: eval doesn't return values directly, would need binding for this
        // For now just save geometry
    }
}

void SummaryWindow::restoreState()
{
    QSettings settings("YourCompany", "AIScreenshotTranslator");

    // Restore geometry
    QByteArray geom = settings.value("summaryWindow/geometry").toByteArray();
    if (!geom.isEmpty())
    {
        restoreGeometry(geom);
    }

    // Restore zoom
    qreal zoom = settings.value("summaryWindow/zoom", 1.0).toReal();
    setZoomFactor(zoom);
    m_lastScrollY = settings.value("summaryWindow/scrollY", 0.0).toDouble();
}

void SummaryWindow::configureHotkeys(const QString &editKey, const QString &viewKey, const QString &screenshotKey,
                                     const QString &boldKey, const QString &underlineKey, const QString &highlightKey)
{
    auto normalizeHotkey = [](QString key)
    {
        key = key.trimmed().toLower();
        key.replace(" ", ""); // remove inner blanks like "ctrl + e"
        return key;
    };

    m_editKey = normalizeHotkey(editKey);
    m_viewKey = normalizeHotkey(viewKey);
    m_screenshotKey = normalizeHotkey(screenshotKey);
    m_boldKey = normalizeHotkey(boldKey);
    m_underlineKey = normalizeHotkey(underlineKey);
    m_highlightKey = normalizeHotkey(highlightKey);

    qDeleteAll(m_shortcuts);
    m_shortcuts.clear();
    auto addShortcut = [this](const QString &key, const QString &js)
    {
        if (key.isEmpty())
            return;
        QShortcut *sc = new QShortcut(QKeySequence(key), this);
        sc->setContext(Qt::ApplicationShortcut);
        connect(sc, &QShortcut::activated, this, [this, js, key]()
                {
            qDebug() << "[QShortcut activated]" << key;
            if (m_webView) m_webView->eval(js.toStdString()); });
        m_shortcuts.append(sc);
    };
    addShortcut(m_editKey, "if(window.currentEntry){var e=currentEntry(); if(e) toggleEdit(e);}");
    addShortcut(m_viewKey, "if(window.currentEntry){var e=currentEntry(); if(e) toggleView(e.getAttribute('data-id'));}");
    addShortcut(m_screenshotKey, "if(window.currentEntry && window.cmd_restore){var e=currentEntry(); window.cmd_restore(e.getAttribute('data-id'));}");

    qDebug() << "[Hotkeys]"
             << "edit=" << m_editKey
             << "view=" << m_viewKey
             << "shot=" << m_screenshotKey
             << "bold=" << m_boldKey
             << "underline=" << m_underlineKey
             << "highlight=" << m_highlightKey;
    refreshHtml();
}

void SummaryWindow::refreshHtml()
{
    captureScrollPosition();
    initHtml();
}

QString SummaryWindow::getAddEntryJs(const TranslationEntry &entry)
{
    QString originalMarkdown = entry.translatedMarkdown;
    ProtectedContent protectedData = protectMath(originalMarkdown);

    QString safeContent = protectedData.text;
    safeContent.replace("\\", "\\\\").replace("\"", "\\\"").replace("\n", "\\n").replace("\r", "");

    QString mathBlocksJs = "[";
    for (int i = 0; i < protectedData.mathBlocks.size(); ++i)
    {
        QString b = protectedData.mathBlocks[i];
        b.replace("\\", "\\\\").replace("\"", "\\\"").replace("\n", "\\n").replace("\r", "");
        if (i > 0)
            mathBlocksJs += ",";
        mathBlocksJs += "\"" + b + "\"";
    }
    mathBlocksJs += "]";

    QString originalSafe = originalMarkdown;
    originalSafe.replace("\\", "\\\\").replace("\"", "\\\"").replace("\n", "\\n").replace("\r", "");

    QString tagsJs = "[";
    for (int i = 0; i < entry.tags.size(); ++i)
    {
        if (i > 0)
            tagsJs += ",";
        QString t = entry.tags[i];
        t.replace("\\", "\\\\").replace("\"", "\\\"");
        tagsJs += "\"" + t + "\"";
    }
    tagsJs += "]";

    return QString("addEntryToDom('%1', '%2', \"%3\", %4, \"%5\", %6, %7);")
        .arg(entry.id)
        .arg(entry.timestamp.toString("yyyy-MM-dd HH:mm:ss"))
        .arg(safeContent)
        .arg(mathBlocksJs)
        .arg(originalSafe)
        .arg(m_selectionMode ? "true" : "false")
        .arg(tagsJs);
}

void SummaryWindow::initHtml()
{
    if (!m_webView)
        return;

    bool isDark = ThemeUtils::isSystemDark();

    // If the HTML shell has already been loaded once, avoid calling setHtml() again (it is intentionally guarded
    // in EmbedWebView to prevent reload-induced focus issues). Instead, update entries via JS.
    if (m_htmlLoaded)
    {
        QList<TranslationEntry> filteredEntries = getFilteredEntries();

        QString js;
        js += "(()=>{";
        js += QString("try{applyDarkMode(%1);}catch(e){};").arg(isDark ? "true" : "false");
        js += QString("try{SELECTION_MODE=%1;}catch(e){};").arg(m_selectionMode ? "true" : "false");
        js += "try{document.querySelectorAll('.entry').forEach(function(n){n.remove();});}catch(e){};";
        for (const auto &entry : filteredEntries)
        {
            js += getAddEntryJs(entry) + ";";
        }
        if (m_currentZoom != 1.0)
        {
            js += QString("try{document.body.style.zoom='%1';}catch(e){};").arg(m_currentZoom);
        }
        js += "})();";

        m_webView->eval(js.toStdString());
        if (m_lastScrollY > 0.0)
            m_webView->eval(QString("window.scrollTo(0,%1);").arg(m_lastScrollY).toStdString());
        return;
    }

    QString html = R"RAW_HTML(<!DOCTYPE html>
<html class="__HTML_CLASS__">
<head>
<meta charset='UTF-8'>
<meta name='viewport' content='width=device-width, initial-scale=1.0'>
<script src='%1'></script>
<link rel='stylesheet' href='%2'>
<script src='%3'></script>
<script src='%4'></script>
<style>
html, body {
  background: #ffffff;
  color: #111111;
}

:root {
            --mark-bg: __MARK_BG__;
            --mark-bg-dark: __MARK_BG_DARK__;
    --mark-fg: #000;
}

mark { background: var(--mark-bg); color: var(--mark-fg); }
body.dark-mode mark { background: var(--mark-bg-dark); color: var(--mark-fg); }
body { font-family: sans-serif; padding: 8px; }
#status-indicator {
  position: fixed;
  top: 8px;
  right: 12px;
  font-size: 12px;
  background: rgba(0,0,0,0.05);
  padding: 4px 8px;
  border-radius: 4px;
  z-index: 9999;
}
.entry { margin-bottom: 12px; padding: 8px; background: #f7f7f7; color: #111111; border: 1px solid #ddd; border-radius: 4px; }
.entry.mode-edit { outline: 1px solid #777; }
.entry-header { display: flex; align-items: center; gap: 6px; margin-bottom: 6px; }
.selection-checkbox { margin-right: 6px; }
.raw-text { width: 100%; min-height: 80px; white-space: pre-wrap; background: #ffffff; color: #111111; border: 1px solid #ccc; border-radius: 4px; padding: 8px; }
.rendered-html { color: #111111; }
html.dark-mode, body.dark-mode { background: #1e1e1e !important; color: #e0e0e0 !important; }
body.dark-mode .entry { background: #2a2a2a !important; color: #e0e0e0 !important; border-color: #444 !important; }
body.dark-mode .entry-header { color: #e0e0e0 !important; }
body.dark-mode .raw-text { background: #1f1f1f !important; color: #e0e0e0 !important; border-color: #555 !important; }
body.dark-mode .rendered-html { color: #e0e0e0 !important; }
body.dark-mode .katex,
body.dark-mode .katex * { color: #e0e0e0 !important; }
.entry.selected { outline: 2px solid #3d8bfd; }
.selection-rect {
  position: fixed;
  border: 1px dashed #3d8bfd;
  background: rgba(61,139,253,0.1);
  pointer-events: none;
  z-index: 9998;
}
</style>
<script>
const IS_DARK = __IS_DARK__;
function applyDarkMode(d) {
  document.documentElement.classList.toggle('dark-mode', d);
  if (document.body) {
    document.body.classList.toggle('dark-mode', d);
  } else {
    document.addEventListener('DOMContentLoaded', () => applyDarkMode(d), { once: true });
  }
}
applyDarkMode(IS_DARK);
try {
        document.documentElement.style.setProperty('--mark-bg', '__MARK_BG__');
        document.documentElement.style.setProperty('--mark-bg-dark', '__MARK_BG_DARK__');
} catch(e) {}
function log(msg){
  try { if (window.cmd_log) window.cmd_log(JSON.stringify([msg])); }
  catch(e){}
}
let SELECTION_MODE = false;
let lastScrollReport = 0;
window.addEventListener('scroll', function() {
  if (!window.cmd_scroll) return;
  const now = Date.now();
  if (now - lastScrollReport < 50) return;
  lastScrollReport = now;
  window.cmd_scroll(window.scrollY);
}, { passive: true });
</script>
)RAW_HTML";

    auto loadAsset = [](const QString &name) -> QString
    {
        QString path = QCoreApplication::applicationDirPath() + "/assets/libs/" + name;
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            return "";
        }
        return QString::fromUtf8(file.readAll());
    };

    QString jsMarked = loadAsset("marked.min.js");
    QString cssKatex = loadAsset("katex.min.css");
    QString jsKatex = loadAsset("katex.min.js");
    QString jsKatexAuto = loadAsset("auto-render.min.js");

    auto embedFonts = [](QString css) -> QString
    {
        QString fontDir = QCoreApplication::applicationDirPath() + "/assets/libs/fonts/";
        QRegularExpression regex("url\\(fonts/([^)]+)\\)");
        QRegularExpressionMatchIterator it = regex.globalMatch(css);
        QString result = css;
        while (it.hasNext())
        {
            QRegularExpressionMatch match = it.next();
            QString filename = match.captured(1);
            QString extension = QFileInfo(filename).suffix().toLower();
            if (extension != "woff2")
            {
                result.replace(match.captured(0), "url(data:application/x-font-placeholder;base64,AAA=)");
                continue;
            }
            QString filePath = fontDir + filename;
            QFile fontFile(filePath);
            if (fontFile.open(QIODevice::ReadOnly))
            {
                QString base64 = fontFile.readAll().toBase64();
                QString dataUri = QString("url(data:font/woff2;base64,%1)").arg(base64);
                result.replace(match.captured(0), dataUri);
            }
        }
        return result;
    };

    cssKatex = embedFonts(cssKatex);

    html = html.replace("<script src='%1'></script>", "<script>\n" + jsMarked + "\n</script>");
    html = html.replace("<link rel='stylesheet' href='%2'>", "<style>\n" + cssKatex + "\n</style>");
    html = html.replace("<script src='%3'></script>", "<script>\n" + jsKatex + "\n</script>");
    html = html.replace("<script src='%4'></script>", "<script>\n" + jsKatexAuto + "\n</script>");
    html = html.replace("__IS_DARK__", isDark ? "true" : "false");
    html = html.replace("__HTML_CLASS__", isDark ? "dark-mode" : "");

    const QString mark = normalizeCssColor(m_config.highlightMarkColor, "#ffeb3b");
    const QString markDark = normalizeCssColor(m_config.highlightMarkColorDark, "#d4af37");
    html = html.replace("__MARK_BG__", mark);
    html = html.replace("__MARK_BG_DARK__", markDark);

    // Static JS Logic
    html += "<script>";
    html += "if (typeof marked !== 'undefined') { marked.setOptions({ breaks: true, gfm: true }); }\n";
    html += QString("var KEY_EDIT = '%1';\n").arg(m_editKey);
    html += QString("var KEY_VIEW = '%1';\n").arg(m_viewKey);
    html += QString("var KEY_SHOT = '%1';\n").arg(m_screenshotKey);
    html += QString("var KEY_BOLD = '%1';\n").arg(m_boldKey);
    html += QString("var KEY_UNDERLINE = '%1';\n").arg(m_underlineKey);
    html += QString("var KEY_HIGHLIGHT = '%1';\n").arg(m_highlightKey);
    html += QString("var RESTORE_SCROLL = %1;\n").arg(m_lastScrollY);
    html += R"JSCODE(
if (document.body) { applyDarkMode(IS_DARK); } else { document.addEventListener('DOMContentLoaded', () => applyDarkMode(IS_DARK), {once:true}); }
var LAST_ENTRY = null;
document.addEventListener('DOMContentLoaded', function() {
    if (document.body) {
        document.body.tabIndex = -1;
        document.body.focus();
    }
    if (typeof RESTORE_SCROLL !== 'undefined') {
        window.scrollTo(0, RESTORE_SCROLL);
        setTimeout(() => window.scrollTo(0, RESTORE_SCROLL), 50);
    }
    log('DOMContentLoaded; KEY_VIEW='+KEY_VIEW+' KEY_SHOT='+KEY_SHOT+' KEY_EDIT='+KEY_EDIT);
});
function currentEntry() {
    var active = document.activeElement;
    if (active) {
        var e = active.closest('.entry');
        if (e) {
            LAST_ENTRY = e;
            return e;
        }
    }
    if (LAST_ENTRY) return LAST_ENTRY;
    return document.querySelector('.entry');
}
function updateStatus(mode) {
   var ind = document.getElementById('status-indicator');
   if (ind) {
       if (mode === 'edit') ind.innerText = 'MODE: EDIT';
       else if (mode === 'raw') ind.innerText = 'MODE: RAW';
       else ind.innerText = 'MODE: VIEW';
   }
}

function protectMathJs(text) {
   var blocks = [];
   var counter = 0;
   var regex = /(\$\$[\s\S]*?\$\$)|(\\\[[\s\S]*?\\\])|(\\\([\s\S]*?\\\))|(\$[^\$\n]+\$)/g;
   var protectedText = text.replace(regex, function(match) {
       var ph = 'MATHBLOCKPH' + counter++;
       blocks.push(match);
       return ph;
   });
   return {text: protectedText, blocks: blocks};
}

function extractMarkdown(rawEl) {
    if (!rawEl) return '';
    var html = rawEl.innerHTML || '';
    html = html.replace(/<div><br\s*\/?>\s*<\/div>/gi, '\n');
    html = html.replace(/<div>/gi, '\n').replace(/<\/div>/gi, '');
    html = html.replace(/<p>/gi, '').replace(/<\/p>/gi, '\n');
    html = html.replace(/<br\s*\/?>/gi, '\n');
    html = html.replace(/&nbsp;/gi, ' ');
    var tmp = document.createElement('div');
    tmp.style.whiteSpace = 'pre-wrap';
    tmp.innerHTML = html;
    return (tmp.textContent || '').replace(/\r\n?/g, '\n');
}

function ensureMarkedOptions() {
    if (typeof marked !== 'undefined') {
         marked.setOptions({ breaks: true, gfm: true });
    }
}

function renderContent(id, markdownOverride) {
    ensureMarkedOptions();
    var raw = document.getElementById('raw_' + id);
    var rendered = document.getElementById('rendered_' + id);
    var markdown = (typeof markdownOverride === 'string') ? markdownOverride : (raw ? raw.textContent : '');
    var p = protectMathJs(markdown);
    var html = marked.parse(p.text);
    p.blocks.forEach(function(block, index) {
         html = html.replace('MATHBLOCKPH' + index, block);
    });
    rendered.innerHTML = html;
    requestAnimationFrame(function() {
      renderMathInElement(rendered, {delimiters: [{left: '$$', right: '$$', display: true}, {left: '$', right: '$', display: false}, {left: '\\(', right: '\\)', display: false}, {left: '\\[', right: '\\]', display: true}], throwOnError : false});
    });
}

function insertMarkdown(startTag, endTag) {
   var sel = window.getSelection();
   if (!sel.rangeCount) return;
   var range = sel.getRangeAt(0);
   var text = range.toString();
   document.execCommand('insertText', false, startTag + text + endTag);
}

function matchHotkey(e, hotkeyStr) {
   if (!hotkeyStr) return false;
   var parts = hotkeyStr.toLowerCase().split('+').map(function(p){ return p.trim(); }).filter(Boolean);
   if (!parts.length) return false;
   var key = parts.pop();
   var ctrl = parts.includes('ctrl') || parts.includes('control');
   var alt = parts.includes('alt');
   var shift = parts.includes('shift');
   return (e.key.toLowerCase() === key && e.ctrlKey === ctrl && e.altKey === alt && e.shiftKey === shift);
}

function toggleView(id) {
  var raw = document.getElementById('raw_' + id);
  var rendered = document.getElementById('rendered_' + id);
  if (raw.style.display === 'none') {
    raw.style.display = 'block';
    rendered.style.display = 'none';
    updateStatus('raw');
  } else {
    raw.style.display = 'none';
    rendered.style.display = 'block';
    updateStatus('view');
  }
}

function toggleEdit(entry) {
   var id = entry.getAttribute('data-id');
   var rendered = document.getElementById('rendered_' + id);
   var raw = document.getElementById('raw_' + id);
   
   if (entry.classList.contains('mode-edit')) {
       entry.classList.remove('mode-edit');
       entry.classList.add('mode-view');
       updateStatus('view');
       raw.contentEditable = 'false';

       var markdown = extractMarkdown(raw);
       raw.innerText = markdown;
       renderContent(id, markdown);

       raw.style.display = 'none';
       rendered.style.display = 'block';
       
       // Notify C++ of edit
       window.cmd_updateEntry(id, markdown);

       entry.focus();
   } else {
       entry.classList.remove('mode-view');
       entry.classList.add('mode-edit');
       updateStatus('edit');
       rendered.style.display = 'none';
       raw.style.display = 'block';
       raw.contentEditable = 'true';
       raw.focus();
   }
}

document.addEventListener('focusin', function(e) {
   var entry = e.target.closest('.entry');
   if (entry) {
       var id = entry.getAttribute('data-id');
       var raw = document.getElementById('raw_' + id);
       var isRaw = raw && raw.style.display !== 'none';
       if (entry.classList.contains('mode-edit')) updateStatus('edit');
       else if (isRaw) updateStatus('raw');
       else updateStatus('view');
       LAST_ENTRY = entry;
   } else { updateStatus('view'); }
});

function handleKey(e) {
   log(`handleKey enter key=${e.key} ctrl=${e.ctrlKey} alt=${e.altKey} shift=${e.shiftKey}`);
   var keyLower = e.key ? e.key.toLowerCase() : '';
   if (keyLower === 'f12' || (e.ctrlKey && e.shiftKey && keyLower === 'i')) {
       if (window.cmd_openDevTools) {
           log('Trigger openDevTools from JS');
           window.cmd_openDevTools();
       } else {
           log('cmd_openDevTools not bound');
       }
       e.preventDefault();
       return;
   }
   if (SELECTION_MODE && e.key === 'Escape') {
       if (window.cmd_exitSelectionMode) window.cmd_exitSelectionMode();
       e.preventDefault();
       return;
   }
   var entry = currentEntry();
   if (!entry) { log('handleKey: no entry'); return; }
   log(`handleKey currentEntry id=${entry.getAttribute('data-id')}`);
   
   var isEditing = entry.classList.contains('mode-edit');
   var k = keyLower;
   log(`KEY_VIEW=${KEY_VIEW} KEY_SHOT=${KEY_SHOT} k=${k} isEditing=${isEditing}`);

   if (matchHotkey(e, KEY_EDIT)) {
       e.preventDefault(); toggleEdit(entry); return; 
   }
   
   if (isEditing) {
      if (e.key === 'Escape') {
          e.preventDefault(); toggleEdit(entry); return;
      }
      if (matchHotkey(e, KEY_BOLD)) {
          e.preventDefault(); insertMarkdown('**', '**'); return;
      }
      if (matchHotkey(e, KEY_UNDERLINE)) {
          e.preventDefault(); insertMarkdown('<u>', '</u>'); return;
      }
      if (matchHotkey(e, KEY_HIGHLIGHT)) {
          e.preventDefault(); insertMarkdown('<mark>', '</mark>'); return;
      }
      return; 
    }
    if (!isEditing) {
        if (matchHotkey(e, KEY_VIEW)) { toggleView(entry.getAttribute('data-id')); e.preventDefault(); return; }
        if (matchHotkey(e, KEY_SHOT)) { 
            // Call native webview binding
            window.cmd_restore(entry.getAttribute('data-id'));
            e.preventDefault(); return; 
        }
        
        // Handle 'dd' for deletion
        if (k === 'd') {
            e.preventDefault();
            var now = Date.now();
            if (entry.lastDTime && (now - entry.lastDTime < 500)) {
                 // Double press detected
                 var id = entry.getAttribute('data-id');
                 // No confirm dialog as requested
                 window.cmd_delete(id);
                 entry.remove();
                 entry.lastDTime = 0;
            } else {
                 entry.lastDTime = now;
            }
            return;
        }
    }
}

if (!window.__INIT_ONCE__) {
    window.__INIT_ONCE__ = true;
    window.__FOCUS_GUARD_ID__ = (Math.random().toString(16).slice(2) + Date.now().toString(16));
    log('init_guard gid=' + window.__FOCUS_GUARD_ID__);
    window.addEventListener('keydown', handleKey, true);
    window.addEventListener('keydown', function(e){
        log(`keydown key=${e.key} ctrl=${e.ctrlKey} alt=${e.altKey} shift=${e.shiftKey} target=${(e.target && e.target.tagName)}`);
    }, true);
    // 非 capture：避免把元素级 focus/blur 误记为 window focus/blur。
    window.addEventListener('focus', ()=>log('window focus gid=' + window.__FOCUS_GUARD_ID__));
    window.addEventListener('blur', ()=>log('window blur gid=' + window.__FOCUS_GUARD_ID__));
    document.addEventListener('visibilitychange', ()=>log('visibility='+document.visibilityState), true);
}

function updateEntryInDom(id, newMarkdown) {
   var raw = document.getElementById('raw_' + id);
   if (raw) {
       raw.innerText = newMarkdown;
       var entry = raw.closest('.entry');
       if (entry && !entry.classList.contains('mode-edit')) {
           renderContent(id, newMarkdown);
       }
   }
}

function addEntryToDom(id, time, markdown, mathBlocks, originalRaw, isSelectionMode, tags) {
  var div = document.createElement('div');
  div.className = 'entry mode-view';
  div.id = 'entry_' + id;
  div.setAttribute('data-id', id);
  div.tabIndex = 0;

  var checkboxDisplay = isSelectionMode ? 'block' : 'none';

  var tagText = tags.length ? `Tags: ${tags.join(', ')}` : '';

  div.innerHTML = `
    <div class='entry-header'>
        <input type='checkbox' class='selection-checkbox' style='display: ${checkboxDisplay}' data-id='${id}'>
        <div class='entry-info'>
            <div>${time}</div>
            ${tagText ? `<div>${tagText}</div>` : ''}
        </div>
    </div>
    <div class='content-area'>
        <div id='rendered_${id}' class='rendered-html'></div>
        <div id='raw_${id}' class='raw-text' style='display:none;' spellcheck='false'></div>
    </div>
  `;
  document.body.appendChild(div);
  
  var rawContainer = document.getElementById('raw_' + id);
  if (rawContainer) {
      rawContainer.textContent = originalRaw; 
      renderContent(id);
  }
}

function toggleSelectionMode(show) {
    SELECTION_MODE = !!show;
    var checkboxes = document.querySelectorAll('.selection-checkbox');
    checkboxes.forEach(cb => {
        cb.style.display = show ? 'block' : 'none';
        if (!show) {
            cb.checked = false;
            var entry = cb.closest('.entry');
            if (entry) entry.classList.remove('selected');
        }
    });
}

function getSelectedIds() {
    var checkboxes = document.querySelectorAll('.selection-checkbox:checked');
    return Array.from(checkboxes).map(cb => cb.getAttribute('data-id'));
}

function selectAllEntries(select) {
    var checkboxes = document.querySelectorAll('.selection-checkbox');
    checkboxes.forEach(cb => {
        cb.checked = select;
        var entry = cb.closest('.entry');
        if (entry) entry.classList.toggle('selected', select);
    });
}

// Click-to-select in selection mode
document.addEventListener('click', function(e) {
    if (!SELECTION_MODE) return;
    var entry = e.target.closest('.entry');
    if (!entry) return;
    var cb = entry.querySelector('.selection-checkbox');
    if (!cb) return;
    cb.checked = !cb.checked;
    entry.classList.toggle('selected', cb.checked);
    e.preventDefault();
});

// Drag rectangle selection
let dragSelect = false;
let dragStart = {x:0, y:0};
let selectionRectEl = null;
document.addEventListener('mousedown', function(e){
    if (!SELECTION_MODE || e.button !== 0) return;
    dragSelect = true;
    dragStart = {x: e.clientX, y: e.clientY};
    selectionRectEl = document.createElement('div');
    selectionRectEl.className = 'selection-rect';
    document.body.appendChild(selectionRectEl);
});
document.addEventListener('mousemove', function(e){
    if (!dragSelect || !selectionRectEl) return;
    var x1 = Math.min(dragStart.x, e.clientX);
    var y1 = Math.min(dragStart.y, e.clientY);
    var x2 = Math.max(dragStart.x, e.clientX);
    var y2 = Math.max(dragStart.y, e.clientY);
    selectionRectEl.style.left = x1 + 'px';
    selectionRectEl.style.top = y1 + 'px';
    selectionRectEl.style.width = (x2 - x1) + 'px';
    selectionRectEl.style.height = (y2 - y1) + 'px';
    var checkboxes = document.querySelectorAll('.selection-checkbox');
    checkboxes.forEach(cb => {
        var entry = cb.closest('.entry');
        if (!entry) return;
        var rect = entry.getBoundingClientRect();
        var overlap = rect.left < x2 && rect.right > x1 && rect.top < y2 && rect.bottom > y1;
        cb.checked = overlap;
        entry.classList.toggle('selected', cb.checked);
    });
});
document.addEventListener('mouseup', function(e){
    if (!dragSelect) return;
    dragSelect = false;
    if (selectionRectEl) {
        selectionRectEl.remove();
        selectionRectEl = null;
    }
});
 </script>
)JSCODE";

    html += "</head><body class=\"__BODY_CLASS__\">";
    html = html.replace("__BODY_CLASS__", isDark ? "dark-mode" : "");
    html += "<div id='status-indicator'>MODE: VIEW</div>";
    html += "<script>";
    QList<TranslationEntry> filteredEntries = getFilteredEntries();
    for (const auto &entry : filteredEntries)
    {
        html += getAddEntryJs(entry);
    }
    // Apply zoom
    if (m_currentZoom != 1.0)
    {
        html += QString("document.body.style.zoom = '%1';").arg(m_currentZoom);
    }
    html += "</script>";

    html += "</body></html>";

    m_webView->setHtml(html.toStdString());
    m_htmlLoaded = true;
    m_webView->focusNative();
    QString toggleJs = QString("applyDarkMode(%1);").arg(isDark ? "true" : "false");
    m_webView->eval(toggleJs.toStdString());
    if (m_lastScrollY > 0.0)
    {
        m_webView->eval(QString("window.scrollTo(0,%1);").arg(m_lastScrollY).toStdString());
    }
}

void SummaryWindow::appendEntryHtml(const TranslationEntry &entry)
{
    if (!m_webView)
        return;
    QString js = getAddEntryJs(entry);
    m_webView->eval(js.toStdString());
}

void SummaryWindow::setupFilterUI()
{
    m_filterToolbar = new QToolBar(this);
    m_filterToolbar->setAttribute(Qt::WA_StyledBackground, true);
    m_filterToolbar->setAutoFillBackground(true);
    // Dark style to match application and prevent white flash
    // Style will be set by updateTheme

    m_filterToolbar->addWidget(new QLabel(TranslationManager::instance().tr("filter_from_date") + ": ", this));
    m_fromDateEdit = new QDateEdit(this);
    m_fromDateEdit->setCalendarPopup(true);
    m_fromDateEdit->setDate(QDate::currentDate().addMonths(-1));
    m_fromDateEdit->setDisplayFormat("yyyy-MM-dd");
    m_filterToolbar->addWidget(m_fromDateEdit);

    m_filterToolbar->addSeparator();

    m_filterToolbar->addWidget(new QLabel(TranslationManager::instance().tr("filter_to_date") + ": ", this));
    m_toDateEdit = new QDateEdit(this);
    m_toDateEdit->setCalendarPopup(true);
    m_toDateEdit->setDate(QDate::currentDate());
    m_toDateEdit->setDisplayFormat("yyyy-MM-dd");
    m_filterToolbar->addWidget(m_toDateEdit);

    m_filterToolbar->addSeparator();

    m_filterToolbar->addWidget(new QLabel(TranslationManager::instance().tr("filter_tags") + ": ", this));
    m_tagFilterCombo = new QComboBox(this);
    m_tagFilterCombo->setEditable(false);
    m_tagFilterCombo->addItem(TranslationManager::instance().tr("filter_all_tags"), "");
    m_filterToolbar->addWidget(m_tagFilterCombo);

    m_filterToolbar->addSeparator();

    m_clearFilterBtn = new QPushButton(TranslationManager::instance().tr("filter_clear"), this);
    m_filterToolbar->addWidget(m_clearFilterBtn);

    m_filterToolbar->addSeparator();

    m_selectionModeBtn = new QPushButton(TranslationManager::instance().tr("btn_selection_mode"), this);
    m_selectionModeBtn->setCheckable(true);
    // Fix dark mode text visibility issues
    // Style will be set by updateTheme

    connect(m_selectionModeBtn, &QPushButton::toggled, this, &SummaryWindow::toggleSelectionMode);
    m_filterToolbar->addWidget(m_selectionModeBtn);

    m_selectAllBtn = new QPushButton(TranslationManager::instance().tr("btn_select_all"), this);
    connect(m_selectAllBtn, &QPushButton::clicked, this, &SummaryWindow::onBatchSelectAll);
    m_batchSelectAllAction = m_filterToolbar->addWidget(m_selectAllBtn);
    m_batchSelectAllAction->setVisible(false);

    m_batchDeleteBtn = new QPushButton(TranslationManager::instance().tr("btn_batch_delete"), this);
    // Red color for delete - special case, handled in updateTheme or kept here if constant?
    // Let's move to updateTheme to ensure contrast.
    connect(m_batchDeleteBtn, &QPushButton::clicked, this, &SummaryWindow::onBatchDelete);
    m_batchDeleteAction = m_filterToolbar->addWidget(m_batchDeleteBtn);
    m_batchDeleteAction->setVisible(false);

    m_batchAddTagBtn = new QPushButton(TranslationManager::instance().tr("btn_batch_add_tag"), this);
    connect(m_batchAddTagBtn, &QPushButton::clicked, this, &SummaryWindow::onBatchAddTags);
    m_batchAddTagAction = m_filterToolbar->addWidget(m_batchAddTagBtn);
    m_batchAddTagAction->setVisible(false);

    m_batchRemoveTagBtn = new QPushButton(TranslationManager::instance().tr("btn_batch_remove_tag"), this);
    connect(m_batchRemoveTagBtn, &QPushButton::clicked, this, &SummaryWindow::onBatchRemoveTags);
    m_batchRemoveTagAction = m_filterToolbar->addWidget(m_batchRemoveTagBtn);
    m_batchRemoveTagAction->setVisible(false);

    QVBoxLayout *mainLayout = qobject_cast<QVBoxLayout *>(layout());
    if (mainLayout)
    {
        mainLayout->insertWidget(0, m_filterToolbar);
    }

    connect(m_fromDateEdit, &QDateEdit::dateChanged, this, &SummaryWindow::applyFilters);
    connect(m_toDateEdit, &QDateEdit::dateChanged, this, &SummaryWindow::applyFilters);
    connect(m_tagFilterCombo, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &SummaryWindow::applyFilters);
    connect(m_clearFilterBtn, &QPushButton::clicked, this, [this]()
            {
        m_fromDateEdit->setDate(QDate::currentDate().addMonths(-1));
        m_toDateEdit->setDate(QDate::currentDate());
        m_tagFilterCombo->setCurrentIndex(0);
        applyFilters(); });
}

void SummaryWindow::setHistoryManager(HistoryManager *historyManager)
{
    m_historyManager = historyManager;
    loadAvailableTags();
}

void SummaryWindow::loadAvailableTags()
{
    if (!m_historyManager || !m_tagFilterCombo)
        return;

    QStringList allTags = m_historyManager->getAllTags();

    while (m_tagFilterCombo->count() > 1)
    {
        m_tagFilterCombo->removeItem(1);
    }

    for (const QString &tag : allTags)
    {
        m_tagFilterCombo->addItem(tag, tag);
    }
}

void SummaryWindow::applyFilters()
{
    captureScrollPosition();
    refreshHtml();
}

QList<TranslationEntry> SummaryWindow::getFilteredEntries() const
{
    QList<TranslationEntry> filtered;

    QDate fromDate = m_fromDateEdit ? m_fromDateEdit->date() : QDate();
    QDate toDate = m_toDateEdit ? m_toDateEdit->date() : QDate();
    QString selectedTag = m_tagFilterCombo ? m_tagFilterCombo->currentData().toString() : "";

    for (const TranslationEntry &entry : m_entries)
    {
        QDate entryDate = entry.timestamp.date();
        if (m_fromDateEdit && entryDate < fromDate)
            continue;
        if (m_toDateEdit && entryDate > toDate)
            continue;

        if (!selectedTag.isEmpty() && !entry.tags.contains(selectedTag))
            continue;

        filtered.append(entry);
    }

    std::sort(filtered.begin(), filtered.end(), [](const TranslationEntry &a, const TranslationEntry &b)
              {
                  return a.timestamp > b.timestamp; // newer first
              });

    return filtered;
}

// Toggle Selection Mode Logic
void SummaryWindow::toggleSelectionMode()
{
    m_selectionMode = m_selectionModeBtn->isChecked();
    if (m_batchDeleteAction)
        m_batchDeleteAction->setVisible(m_selectionMode);
    if (m_batchAddTagAction)
        m_batchAddTagAction->setVisible(m_selectionMode);
    if (m_batchRemoveTagAction)
        m_batchRemoveTagAction->setVisible(m_selectionMode);
    if (m_batchSelectAllAction)
        m_batchSelectAllAction->setVisible(m_selectionMode);

    // Reset Select All state when toggling mode
    m_allSelected = false;
    if (m_selectAllBtn)
    {
        m_selectAllBtn->setText(TranslationManager::instance().tr("btn_select_all"));
    }

    // Notify JS to show/hide checkboxes
    m_webView->eval(QString("if(window.toggleSelectionMode) window.toggleSelectionMode(%1);").arg(m_selectionMode ? "true" : "false").toStdString());
}

void SummaryWindow::onBatchSelectAll()
{
    m_allSelected = !m_allSelected;

    // Update button text
    if (m_selectAllBtn)
    {
        QString key = m_allSelected ? "btn_deselect_all" : "btn_select_all";
        m_selectAllBtn->setText(TranslationManager::instance().tr(key));
    }

    // execute JS
    QString js = QString("if(window.selectAllEntries) window.selectAllEntries(%1);").arg(m_allSelected ? "true" : "false");
    m_webView->eval(js.toStdString());
}

void SummaryWindow::onBatchDelete()
{
    m_webView->eval("if(window.getSelectedIds) { let ids = window.getSelectedIds(); if(ids.length > 0) window.cmd_batchDelete(...ids); }");
    if (m_selectionModeBtn)
        m_selectionModeBtn->setChecked(false);
}

void SummaryWindow::onBatchAddTags()
{
    m_webView->eval("if(window.getSelectedIds) { let ids = window.getSelectedIds(); if(ids.length > 0) window.cmd_batchAddTags(...ids); }");
    if (m_selectionModeBtn)
        m_selectionModeBtn->setChecked(false);
}

void SummaryWindow::onBatchRemoveTags()
{
    m_webView->eval("if(window.getSelectedIds) { let ids = window.getSelectedIds(); if(ids.length > 0) window.cmd_batchRemoveTags(...ids); }");
    if (m_selectionModeBtn)
        m_selectionModeBtn->setChecked(false);
}
void SummaryWindow::updateTheme(bool isDark)
{
    if (m_webView)
    {
        QColor bg = isDark ? QColor(30, 30, 30) : QColor(255, 255, 255);
        m_webView->setBackgroundColor(bg.red(), bg.green(), bg.blue(), 255);
        QString js = QString("if (typeof applyDarkMode === 'function') { applyDarkMode(%1); } else { document.documentElement.classList.toggle('dark-mode', %1); document.body.classList.toggle('dark-mode', %1); }").arg(isDark ? "true" : "false");
        m_webView->eval(js.toStdString());
    }

    QString toolbarBg = isDark ? "#2a2a2a" : "#f5f5f5";
    QString fg = isDark ? "#e0e0e0" : "#111111";
    QString controlBg = isDark ? "#3a3a3a" : "#ffffff";
    QString border = isDark ? "#555555" : "#cccccc";

    if (m_filterToolbar)
        m_filterToolbar->setStyleSheet(QString("QToolBar { background:%1; color:%2; } QLabel { color:%2; }").arg(toolbarBg, fg));

    QString btnStyle = QString("color:%1; background:%2; border:1px solid %3;").arg(fg, controlBg, border);
    if (m_selectionModeBtn)
        m_selectionModeBtn->setStyleSheet(btnStyle);
    if (m_batchDeleteBtn)
        m_batchDeleteBtn->setStyleSheet(btnStyle);
    if (m_selectAllBtn)
        m_selectAllBtn->setStyleSheet(btnStyle);
    if (m_clearFilterBtn)
        m_clearFilterBtn->setStyleSheet(btnStyle);
    if (m_batchAddTagBtn)
        m_batchAddTagBtn->setStyleSheet(btnStyle);
    if (m_batchRemoveTagBtn)
        m_batchRemoveTagBtn->setStyleSheet(btnStyle);

    QString inputStyle = QString("color:%1; background:%2; border:1px solid %3; selection-background-color:%2;").arg(fg, controlBg, border);
    if (m_tagFilterCombo)
        m_tagFilterCombo->setStyleSheet(inputStyle);
    if (m_fromDateEdit)
        m_fromDateEdit->setStyleSheet(inputStyle);
    if (m_toDateEdit)
        m_toDateEdit->setStyleSheet(inputStyle);
}
