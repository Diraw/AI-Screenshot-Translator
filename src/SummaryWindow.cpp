
#include "SummaryWindow.h"

#include <QHBoxLayout>
#include <QFrame>
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
#include <QApplication>

#include <algorithm>

#include "TranslationManager.h"
#include "HistoryManager.h"
#include "EmbedWebView.h"
#include "TagDialog.h"
#include "ThemeUtils.h"
#include "ColorUtils.h"

#include <QMessageBox>

void SummaryWindow::setConfig(const AppConfig &config)
{
    m_config = config;

    auto normalizeHotkey = [](QString key)
    {
        key = key.trimmed().toLower();
        key.replace(" ", "");
        return key;
    };

    m_selectionToggleKey = normalizeHotkey(m_config.selectionToggleHotkey);
    if (m_selectionToggleKey.isEmpty())
        m_selectionToggleKey = QStringLiteral("ctrl+s");
    if (m_selectionToggleShortcut)
        m_selectionToggleShortcut->setKey(QKeySequence(m_selectionToggleKey));

    if (m_webView)
    {
        const QString mark = ColorUtils::normalizeCssColor(m_config.highlightMarkColor, "#ffeb3b");
        const QString markDark = ColorUtils::normalizeCssColor(m_config.highlightMarkColorDark, "#d4af37");
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
    layout->setSpacing(0);

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

    // Local shortcuts for archive window UX
    // - Ctrl+S toggles batch selection mode (keep 's' for screenshot card)
    // - Esc exits RAW (if active), selection mode, or clears active filters
    m_selectionToggleShortcut = new QShortcut(QKeySequence(QStringLiteral("Ctrl+S")), this);
    m_selectionToggleShortcut->setContext(Qt::WindowShortcut);
    connect(m_selectionToggleShortcut, &QShortcut::activated, this, [this]()
            {
        if (m_selectionModeBtn) m_selectionModeBtn->setChecked(!m_selectionModeBtn->isChecked()); });

    QShortcut *escapeShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    escapeShortcut->setContext(Qt::WindowShortcut);
    connect(escapeShortcut, &QShortcut::activated, this, [this]()
            {
        // 1) Exit RAW mode back to VIEW (if currently in RAW)
        if (m_webView)
        {
            const QString js = "(()=>{try{var e=currentEntry&&currentEntry(); if(!e) return; var id=e.getAttribute('data-id'); var raw=document.getElementById('raw_'+id); if(raw && raw.style.display!=='none' && !e.classList.contains('mode-edit')){ toggleView(id); }}catch(_e){}})();";
            m_webView->eval(js.toStdString());
        }

        // 2) Exit selection mode
        if (m_selectionModeBtn && m_selectionModeBtn->isChecked())
        {
            m_selectionModeBtn->setChecked(false);
            return;
        }

        // 3) Clear active filters (date and/or tags)
        const QDate defaultFrom = QDate::currentDate().addMonths(-1);
        const QDate defaultTo = QDate::currentDate();
        const bool dateActive = (m_fromDateEdit && m_fromDateEdit->date() != defaultFrom) || (m_toDateEdit && m_toDateEdit->date() != defaultTo);
        const bool tagsActive = !m_selectedTags.isEmpty();
        if (dateActive || tagsActive)
        {
            if (m_fromDateEdit) m_fromDateEdit->setDate(defaultFrom);
            if (m_toDateEdit) m_toDateEdit->setDate(defaultTo);
            m_selectedTags.clear();
            if (m_tagFilterBtn) m_tagFilterBtn->setText(TranslationManager::instance().tr("filter_all_tags"));
            applyFilters();
            return;
        }

        // 4) Clear focus from the toolbar (e.g. date edits) and return focus to the web view
        QWidget *fw = QApplication::focusWidget();
        if (fw && m_filterToolbar && m_filterToolbar->isAncestorOf(fw))
        {
            fw->clearFocus();
            if (m_webView)
                m_webView->focusNative();
            else
                setFocus(Qt::OtherFocusReason);
            return;
        } });

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

    // Content-area click helper: WebView notifies us to clear toolbar focus (if any)
    m_webView->bind("cmd_clearToolbarFocus", [this](std::string, std::string, void *)
                    {
        QWidget *fw = QApplication::focusWidget();
        if (fw && m_filterToolbar && m_filterToolbar->isAncestorOf(fw))
        {
            fw->clearFocus();
            if (m_webView)
                m_webView->focusNative();
            else
                setFocus(Qt::OtherFocusReason);
        } });
    m_webView->bind("cmd_exitSelectionMode", [this](std::string, std::string, void *)
                    {
        if (m_selectionModeBtn) m_selectionModeBtn->setChecked(false); });

    m_webView->bind("cmd_toggleSelectionMode", [this](std::string, std::string, void *)
                    {
        if (m_selectionModeBtn) m_selectionModeBtn->setChecked(!m_selectionModeBtn->isChecked()); });

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
        TagDialog *dialog = new TagDialog(allTags, QStringList(), this, [this]() -> bool {
            if (m_selectionModeBtn && m_selectionModeBtn->isChecked()) {
                m_selectionModeBtn->setChecked(false);
                return true;
            }
            return false;
        });
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
        TagDialog *dialog = new TagDialog(allTags, QStringList(), this, [this]() -> bool {
            if (m_selectionModeBtn && m_selectionModeBtn->isChecked()) {
                m_selectionModeBtn->setChecked(false);
                return true;
            }
            return false;
        });
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
        auto *ke = static_cast<QKeyEvent *>(event);

        // Some widgets (notably QDateEdit) consume Esc, preventing QShortcut from firing.
        // Use an application-level filter to ensure Esc can always clear toolbar focus.
        if (ke->key() == Qt::Key_Escape)
        {
            QWidget *fw = QApplication::focusWidget();
            if (fw && m_filterToolbar && m_filterToolbar->isAncestorOf(fw))
            {
                fw->clearFocus();
                if (m_webView)
                    m_webView->focusNative();
                else
                    setFocus(Qt::OtherFocusReason);
                return true;
            }
        }
    }

    if (event->type() == QEvent::MouseButtonPress)
    {
        QWidget *fw = QApplication::focusWidget();
        if (fw && m_filterToolbar && m_filterToolbar->isAncestorOf(fw))
        {
            auto *target = qobject_cast<QWidget *>(watched);
            auto *me = static_cast<QMouseEvent *>(event);

            // Clicked a Qt widget outside the toolbar: clear toolbar focus.
            if (target && !m_filterToolbar->isAncestorOf(target))
            {
                fw->clearFocus();
                if (m_webView)
                    m_webView->focusNative();
                else
                    setFocus(Qt::OtherFocusReason);
            }
            // Clicked inside the toolbar: clear focus only when clicking on non-interactive/blank area.
            else if (target == m_filterToolbar)
            {
                const QPoint p = me->position().toPoint();
                QWidget *child = m_filterToolbar->childAt(p);
                if (!child || child->focusPolicy() == Qt::NoFocus)
                {
                    fw->clearFocus();
                    if (m_webView)
                        m_webView->focusNative();
                    else
                        setFocus(Qt::OtherFocusReason);
                }
            }
            else if (target && target->focusPolicy() == Qt::NoFocus)
            {
                // e.g. clicking group background / spacer
                fw->clearFocus();
                if (m_webView)
                    m_webView->focusNative();
                else
                    setFocus(Qt::OtherFocusReason);
            }
        }
    }

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
