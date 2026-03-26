#include "SummaryWindow.h"

#include "EmbedWebView.h"
#include "HistoryManager.h"
#include "ThemeUtils.h"
#include "TranslationManager.h"

#include <QDate>
#include <QFile>
#include <QLayout>
#include <QStyle>
#include <QToolButton>
#include <QTimer>

#include <algorithm>

namespace
{
QString overflowHandleToolTipText()
{
    if (TranslationManager::instance().getLanguage() == QStringLiteral("zh"))
        return QStringLiteral("\u6a2a\u5411\u62d6\u52a8\u8c03\u6574\u7a97\u53e3\u5bbd\u5ea6\uff0c\u53cc\u51fb\u81ea\u52a8\u9002\u914d");

    return QStringLiteral("Drag horizontally to resize, double-click to auto-fit");
}
} // namespace

void SummaryWindow::refreshToolbarOverflowHint()
{
    if (!m_filterToolbar || !m_toolbarOverflowButton)
        return;

    if (!isVisible() || !m_filterToolbar->isVisible() ||
        m_filterToolbar->width() <= 0 || m_filterToolbar->height() <= 0)
    {
        m_toolbarOverflowButton->setProperty("overflowHintActive", false);
        m_toolbarOverflowButton->style()->unpolish(m_toolbarOverflowButton);
        m_toolbarOverflowButton->style()->polish(m_toolbarOverflowButton);
        m_toolbarOverflowButton->update();
        return;
    }

    bool foundInternalExtension = false;
    const QList<QToolButton *> buttons = m_filterToolbar->findChildren<QToolButton *>();
    for (QToolButton *candidate : buttons)
    {
        if (!candidate)
            continue;

        const QString className = QString::fromLatin1(candidate->metaObject()->className());
        if (className.contains(QStringLiteral("QToolBarExtension")))
        {
            foundInternalExtension = true;
            if (candidate != m_internalToolbarExtensionButton)
            {
                if (m_internalToolbarExtensionButton)
                    m_internalToolbarExtensionButton->removeEventFilter(this);
                m_internalToolbarExtensionButton = candidate;
                m_internalToolbarExtensionButton->installEventFilter(this);
                m_internalToolbarExtensionButton->setFocusPolicy(Qt::NoFocus);
                m_internalToolbarExtensionButton->setEnabled(false);
                m_internalToolbarExtensionButton->setAttribute(Qt::WA_TransparentForMouseEvents, true);
                m_internalToolbarExtensionButton->setStyleSheet(QStringLiteral(
                    "QToolButton { min-width: 0px; max-width: 0px; width: 0px; "
                    "padding: 0px; margin: 0px; border: none; }"));
            }
            candidate->setMinimumWidth(0);
            candidate->setMaximumWidth(0);
            candidate->resize(0, candidate->height());
            candidate->hide();
            break;
        }
    }

    if (!foundInternalExtension)
    {
        if (m_internalToolbarExtensionButton)
            m_internalToolbarExtensionButton->removeEventFilter(this);
        m_internalToolbarExtensionButton = nullptr;
    }

    bool overflowActive = false;
    if (m_filterToolbar->layout())
    {
        overflowActive = m_filterToolbar->layout()->minimumSize().width() > m_filterToolbar->width();
    }

    const QList<QWidget *> groups = {m_filtersGroup, m_paginationGroup, m_actionsGroup};
    for (QWidget *group : groups)
    {
        if (!group)
            continue;

        if (!group->isVisible())
        {
            overflowActive = true;
            break;
        }

        const QRect groupRect = group->geometry();
        if (groupRect.isEmpty() ||
            groupRect.left() < 0 ||
            groupRect.top() < 0 ||
            groupRect.right() > m_filterToolbar->rect().right() ||
            groupRect.bottom() > m_filterToolbar->rect().bottom())
        {
            overflowActive = true;
            break;
        }
    }

    if (m_internalToolbarExtensionButton)
        m_internalToolbarExtensionButton->hide();

    m_toolbarOverflowButton->setProperty("overflowHintActive", overflowActive);
    m_toolbarOverflowButton->style()->unpolish(m_toolbarOverflowButton);
    m_toolbarOverflowButton->style()->polish(m_toolbarOverflowButton);
    m_toolbarOverflowButton->update();
}

void SummaryWindow::setHistoryManager(HistoryManager *historyManager)
{
    m_historyManager = historyManager;
    loadAvailableTags();
}

void SummaryWindow::loadAvailableTags()
{
    if (!m_historyManager)
        return;
    // Tag list is fetched on-demand for the dialog; keep selected button text up-to-date.
    if (m_tagFilterBtn)
    {
        if (m_selectedTags.isEmpty())
            m_tagFilterBtn->setText(TranslationManager::instance().tr("filter_all_tags"));
        else
        {
            QString text = m_selectedTags.join(", ");
            if (text.size() > 28)
                text = text.left(28) + "...";
            m_tagFilterBtn->setText(text);
        }
    }
}

void SummaryWindow::applyFilters()
{
    m_currentPage = 1;
    refreshHtml(false);
}

QList<TranslationEntry> SummaryWindow::getFilteredEntries() const
{
    QList<TranslationEntry> filtered;

    QDate fromDate = m_fromDateEdit ? m_fromDateEdit->date() : QDate();
    QDate toDate = m_toDateEdit ? m_toDateEdit->date() : QDate();
    const QStringList selectedTags = m_selectedTags;
    const QString searchText = m_searchEdit ? m_searchEdit->text().trimmed().toLower() : QString();

    for (const TranslationEntry &entry : m_entries)
    {
        QDate entryDate = entry.timestamp.date();
        if (m_fromDateEdit && entryDate < fromDate)
            continue;
        if (m_toDateEdit && entryDate > toDate)
            continue;

        if (!selectedTags.isEmpty())
        {
            bool anyMatch = false;
            for (const QString &t : selectedTags)
            {
                if (entry.tags.contains(t))
                {
                    anyMatch = true;
                    break;
                }
            }
            if (!anyMatch)
                continue;
        }

        // Search by translation content
        if (!searchText.isEmpty())
        {
            const QString content = entry.translatedMarkdown.toLower();
            if (!content.contains(searchText))
                continue;
        }

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
    if (m_selectionModeBtn)
    {
        const QString key = m_selectionMode ? "btn_cancel_selection_mode" : "btn_selection_mode";
        m_selectionModeBtn->setText(TranslationManager::instance().tr(key));
    }
    if (m_batchDeleteBtn)
        m_batchDeleteBtn->setVisible(m_selectionMode);
    if (m_batchAddTagBtn)
        m_batchAddTagBtn->setVisible(m_selectionMode);
    if (m_batchRemoveTagBtn)
        m_batchRemoveTagBtn->setVisible(m_selectionMode);
    if (m_selectAllBtn)
        m_selectAllBtn->setVisible(m_selectionMode);

    // QToolBar may overflow widgets when width is tight; force a relayout when toggling.
    if (m_actionsGroup)
        m_actionsGroup->adjustSize();
    if (m_filterToolbar)
    {
        m_filterToolbar->updateGeometry();
        if (m_filterToolbar->layout())
            m_filterToolbar->layout()->invalidate();
    }
    refreshToolbarOverflowHint();
    QTimer::singleShot(0, this, [this]()
                       { refreshToolbarOverflowHint(); });

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
    ThemeUtils::applyThemeToWindow(this, isDark);

    if (m_webView)
    {
        QColor bg = isDark ? QColor(30, 30, 30) : QColor(255, 255, 255);
        m_webView->setBackgroundColor(bg.red(), bg.green(), bg.blue(), 255);
        QString js = QString("if (typeof applyDarkMode === 'function') { applyDarkMode(%1); } else { document.documentElement.classList.toggle('dark-mode', %1); document.body.classList.toggle('dark-mode', %1); }").arg(isDark ? "true" : "false");
        m_webView->eval(js.toStdString());
    }

    if (m_filterToolbar)
    {
        const QString qssPath = isDark ? ":/assets/qss/archive_toolbar_dark.qss" : ":/assets/qss/archive_toolbar_light.qss";
        QFile f(qssPath);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            const QString qss = QString::fromUtf8(f.readAll());
            if (m_filterToolbarShell)
                m_filterToolbarShell->setStyleSheet(qss);
            else
                m_filterToolbar->setStyleSheet(qss);
        }
        else
        {
            // Fallback: keep it readable even if resource is missing.
            const QString bg = isDark ? "#1f1f1f" : "#f6f7f9";
            const QString border = isDark ? "#343434" : "#d8dde6";
            const QString fallbackQss = QString(
                                            "QToolBar#archiveFilterToolbar{background:%1;border:none;border-bottom:1px solid %2;}"
                                            "QToolButton#archiveOverflowHandle{background:#2563eb;color:#eff6ff;border:1px solid #1d4ed8;}")
                                            .arg(bg, border);
            if (m_filterToolbarShell)
                m_filterToolbarShell->setStyleSheet(fallbackQss);
            else
                m_filterToolbar->setStyleSheet(fallbackQss);
        }
        refreshToolbarOverflowHint();
    }
}

void SummaryWindow::updateLanguage()
{
    // Update filter labels
    if (m_fromLabel)
        m_fromLabel->setText(TranslationManager::instance().tr("filter_from_date") + ":");
    if (m_toLabel)
        m_toLabel->setText(TranslationManager::instance().tr("filter_to_date") + ":");
    if (m_searchLabel)
        m_searchLabel->setText(TranslationManager::instance().tr("filter_search") + ":");
    
    // Update buttons
    if (m_tagFilterBtn)
    {
        if (m_selectedTags.isEmpty())
            m_tagFilterBtn->setText(TranslationManager::instance().tr("filter_all_tags"));
        else
        {
            QString text = m_selectedTags.join(", ");
            if (text.size() > 28) text = text.left(28) + "...";
            m_tagFilterBtn->setText(text);
        }
    }
    if (m_clearFilterBtn)
        m_clearFilterBtn->setText(TranslationManager::instance().tr("filter_clear"));
    
    // Update batch buttons
    if (m_selectionModeBtn)
    {
        const QString key = m_selectionMode ? "btn_cancel_selection_mode" : "btn_selection_mode";
        m_selectionModeBtn->setText(TranslationManager::instance().tr(key));
    }
    if (m_selectAllBtn)
    {
        QString key = m_allSelected ? "btn_deselect_all" : "btn_select_all";
        m_selectAllBtn->setText(TranslationManager::instance().tr(key));
    }
    if (m_batchDeleteBtn)
        m_batchDeleteBtn->setText(TranslationManager::instance().tr("btn_batch_delete"));
    if (m_batchAddTagBtn)
        m_batchAddTagBtn->setText(TranslationManager::instance().tr("btn_batch_add_tag"));
    if (m_batchRemoveTagBtn)
        m_batchRemoveTagBtn->setText(TranslationManager::instance().tr("btn_batch_remove_tag"));
    if (m_prevPageBtn)
        m_prevPageBtn->setText(TranslationManager::instance().tr("btn_page_prev"));
    if (m_nextPageBtn)
        m_nextPageBtn->setText(TranslationManager::instance().tr("btn_page_next"));
    updatePaginationUi();

    // Update search placeholder
    if (m_searchEdit)
        m_searchEdit->setPlaceholderText(TranslationManager::instance().tr("filter_search_placeholder"));

    if (m_toolbarOverflowButton)
        m_toolbarOverflowButton->setToolTip(overflowHandleToolTipText());
    
    // Refresh window title
    setWindowTitle(TranslationManager::instance().tr("summary_title"));
}

