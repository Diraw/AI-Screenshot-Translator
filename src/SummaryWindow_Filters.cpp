#include "SummaryWindow.h"

#include "EmbedWebView.h"
#include "HistoryManager.h"
#include "ThemeUtils.h"
#include "TranslationManager.h"
#include "TagDialog.h"

#include <QDate>
#include <QDateEdit>
#include <QApplication>
#include <QMouseEvent>
#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QPushButton>
#include <QToolBar>
#include <QVBoxLayout>
#include <QFontMetrics>

#include <algorithm>
#include <functional>

namespace
{
    class ClickToClearFocusWidget final : public QWidget
    {
    public:
        explicit ClickToClearFocusWidget(std::function<void()> onClick, QWidget *parent = nullptr)
            : QWidget(parent), m_onClick(std::move(onClick))
        {
            setFocusPolicy(Qt::NoFocus);
        }

    protected:
        void mousePressEvent(QMouseEvent *event) override
        {
            if (m_onClick)
                m_onClick();
            event->accept();
        }

    private:
        std::function<void()> m_onClick;
    };
}

void SummaryWindow::setupFilterUI()
{
    m_filterToolbar = new QToolBar(this);
    m_filterToolbar->setObjectName("archiveFilterToolbar");
    m_filterToolbar->setAttribute(Qt::WA_StyledBackground, true);
    m_filterToolbar->setAutoFillBackground(true);
    m_filterToolbar->setMovable(false);
    m_filterToolbar->setFloatable(false);
    m_filterToolbar->setContextMenuPolicy(Qt::PreventContextMenu);
    m_filterToolbar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    const auto clearToolbarFocus = [this]()
    {
        QWidget *fw = QApplication::focusWidget();
        if (fw && m_filterToolbar && m_filterToolbar->isAncestorOf(fw))
            fw->clearFocus();
        if (m_webView)
            m_webView->focusNative();
        else
            setFocus(Qt::OtherFocusReason);
    };

    // Avoid glyph clipping on high-DPI / larger system fonts (but keep the toolbar compact).
    const int controlHeight = qMax(26, QFontMetrics(m_filterToolbar->font()).lineSpacing() + 8);
    const int dateWidth = 117;

    // Left group: filters
    m_filtersGroup = new QWidget(this);
    m_filtersGroup->setObjectName("archiveFiltersGroup");
    // Keep filters compact so the right-side batch actions don't get pushed into the overflow.
    m_filtersGroup->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    QHBoxLayout *filtersLayout = new QHBoxLayout(m_filtersGroup);
    filtersLayout->setContentsMargins(3, 0, 3, 0);
    filtersLayout->setSpacing(2);
    filtersLayout->setAlignment(Qt::AlignVCenter);

    m_fromLabel = new QLabel(TranslationManager::instance().tr("filter_from_date") + ":", this);
    m_fromLabel->setProperty("role", "caption");
    m_fromLabel->setAlignment(Qt::AlignVCenter);
    m_fromLabel->setMinimumHeight(controlHeight);
    filtersLayout->addWidget(m_fromLabel);

    m_fromDateEdit = new QDateEdit(this);
    m_fromDateEdit->setObjectName("fromDateEdit");
    m_fromDateEdit->setCalendarPopup(true);
    m_fromDateEdit->setDate(QDate::currentDate().addMonths(-1));
    m_fromDateEdit->setDisplayFormat("yyyy-MM-dd");
    m_fromDateEdit->setMinimumHeight(controlHeight);
    m_fromDateEdit->setFixedWidth(dateWidth);
    m_fromDateEdit->setFocusPolicy(Qt::ClickFocus);
    filtersLayout->addWidget(m_fromDateEdit);

    m_toLabel = new QLabel(TranslationManager::instance().tr("filter_to_date") + ":", this);
    m_toLabel->setProperty("role", "caption");
    m_toLabel->setAlignment(Qt::AlignVCenter);
    m_toLabel->setMinimumHeight(controlHeight);
    filtersLayout->addWidget(m_toLabel);

    m_toDateEdit = new QDateEdit(this);
    m_toDateEdit->setObjectName("toDateEdit");
    m_toDateEdit->setCalendarPopup(true);
    m_toDateEdit->setDate(QDate::currentDate());
    m_toDateEdit->setDisplayFormat("yyyy-MM-dd");
    m_toDateEdit->setMinimumHeight(controlHeight);
    m_toDateEdit->setFixedWidth(dateWidth);
    m_toDateEdit->setFocusPolicy(Qt::ClickFocus);
    filtersLayout->addWidget(m_toDateEdit);

    m_tagFilterBtn = new QPushButton(TranslationManager::instance().tr("filter_all_tags"), this);
    m_tagFilterBtn->setObjectName("tagFilterButton");
    m_tagFilterBtn->setProperty("variant", "ghost");
    m_tagFilterBtn->setMinimumHeight(controlHeight);
    m_tagFilterBtn->setFixedWidth(100);
    m_tagFilterBtn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    filtersLayout->addWidget(m_tagFilterBtn);

    m_clearFilterBtn = new QPushButton(TranslationManager::instance().tr("filter_clear"), this);
    m_clearFilterBtn->setObjectName("clearFilterBtn");
    m_clearFilterBtn->setProperty("variant", "ghost");
    m_clearFilterBtn->setMinimumHeight(controlHeight);
    m_clearFilterBtn->setFixedWidth(100);
    m_clearFilterBtn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    filtersLayout->addWidget(m_clearFilterBtn);

    // Search by translation content
    m_searchLabel = new QLabel(TranslationManager::instance().tr("filter_search") + ":", this);
    m_searchLabel->setProperty("role", "caption");
    m_searchLabel->setAlignment(Qt::AlignVCenter);
    m_searchLabel->setMinimumHeight(controlHeight);
    filtersLayout->addWidget(m_searchLabel);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setObjectName("searchEdit");
    m_searchEdit->setPlaceholderText(TranslationManager::instance().tr("filter_search_placeholder"));
    m_searchEdit->setMinimumHeight(controlHeight);
    m_searchEdit->setFixedWidth(175);
    m_searchEdit->setFocusPolicy(Qt::ClickFocus);
    filtersLayout->addWidget(m_searchEdit);

    m_filterToolbar->addWidget(m_filtersGroup);

    // Push batch actions to the right
    QWidget *spacer = new ClickToClearFocusWidget(clearToolbarFocus, this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_filterToolbar->addWidget(spacer);

    // Middle-right group: pagination controls
    m_paginationGroup = new QWidget(this);
    m_paginationGroup->setObjectName("archivePaginationGroup");
    m_paginationGroup->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    QHBoxLayout *paginationLayout = new QHBoxLayout(m_paginationGroup);
    paginationLayout->setContentsMargins(3, 0, 3, 0);
    paginationLayout->setSpacing(2);
    paginationLayout->setAlignment(Qt::AlignVCenter);

    m_prevPageBtn = new QPushButton(TranslationManager::instance().tr("btn_page_prev"), this);
    m_prevPageBtn->setObjectName("prevPageBtn");
    m_prevPageBtn->setProperty("variant", "ghost");
    m_prevPageBtn->setMinimumHeight(controlHeight);
    m_prevPageBtn->setMinimumWidth(58);
    paginationLayout->addWidget(m_prevPageBtn);

    m_pageInfoLabel = new QLabel(this);
    m_pageInfoLabel->setObjectName("pageInfoLabel");
    m_pageInfoLabel->setProperty("role", "caption");
    m_pageInfoLabel->setMinimumHeight(controlHeight);
    m_pageInfoLabel->setAlignment(Qt::AlignCenter);
    m_pageInfoLabel->setMinimumWidth(140);
    paginationLayout->addWidget(m_pageInfoLabel);

    m_nextPageBtn = new QPushButton(TranslationManager::instance().tr("btn_page_next"), this);
    m_nextPageBtn->setObjectName("nextPageBtn");
    m_nextPageBtn->setProperty("variant", "ghost");
    m_nextPageBtn->setMinimumHeight(controlHeight);
    m_nextPageBtn->setMinimumWidth(58);
    paginationLayout->addWidget(m_nextPageBtn);

    connect(m_prevPageBtn, &QPushButton::clicked, this, [this]()
            {
                if (!m_archiveUsePagination || m_currentPage <= 1)
                    return;
                --m_currentPage;
                refreshHtml(false);
            });
    connect(m_nextPageBtn, &QPushButton::clicked, this, [this]()
            {
                if (!m_archiveUsePagination || m_currentPage >= m_totalPages)
                    return;
                ++m_currentPage;
                refreshHtml(false);
            });

    m_filterToolbar->addWidget(m_paginationGroup);

    // Right group: batch actions
    m_actionsGroup = new QWidget(this);
    m_actionsGroup->setObjectName("archiveActionsGroup");
    m_actionsGroup->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    QHBoxLayout *actionsLayout = new QHBoxLayout(m_actionsGroup);
    actionsLayout->setContentsMargins(3, 0, 3, 0);
    actionsLayout->setSpacing(2);
    actionsLayout->setAlignment(Qt::AlignVCenter);

    m_selectionModeBtn = new QPushButton(TranslationManager::instance().tr("btn_selection_mode"), this);
    m_selectionModeBtn->setObjectName("selectionModeBtn");
    m_selectionModeBtn->setCheckable(true);
    m_selectionModeBtn->setProperty("variant", "toggle");
    m_selectionModeBtn->setMinimumHeight(controlHeight);
    connect(m_selectionModeBtn, &QPushButton::toggled, this, &SummaryWindow::toggleSelectionMode);
    actionsLayout->addWidget(m_selectionModeBtn);

    m_selectAllBtn = new QPushButton(TranslationManager::instance().tr("btn_select_all"), this);
    m_selectAllBtn->setObjectName("selectAllBtn");
    m_selectAllBtn->setProperty("variant", "ghost");
    m_selectAllBtn->setMinimumHeight(controlHeight);
    connect(m_selectAllBtn, &QPushButton::clicked, this, &SummaryWindow::onBatchSelectAll);
    actionsLayout->addWidget(m_selectAllBtn);
    m_selectAllBtn->setVisible(false);

    m_batchDeleteBtn = new QPushButton(TranslationManager::instance().tr("btn_batch_delete"), this);
    m_batchDeleteBtn->setObjectName("batchDeleteBtn");
    m_batchDeleteBtn->setProperty("variant", "danger");
    m_batchDeleteBtn->setMinimumHeight(controlHeight);
    connect(m_batchDeleteBtn, &QPushButton::clicked, this, &SummaryWindow::onBatchDelete);
    actionsLayout->addWidget(m_batchDeleteBtn);
    m_batchDeleteBtn->setVisible(false);

    m_batchAddTagBtn = new QPushButton(TranslationManager::instance().tr("btn_batch_add_tag"), this);
    m_batchAddTagBtn->setObjectName("batchAddTagBtn");
    m_batchAddTagBtn->setProperty("variant", "ghost");
    m_batchAddTagBtn->setMinimumHeight(controlHeight);
    connect(m_batchAddTagBtn, &QPushButton::clicked, this, &SummaryWindow::onBatchAddTags);
    actionsLayout->addWidget(m_batchAddTagBtn);
    m_batchAddTagBtn->setVisible(false);

    m_batchRemoveTagBtn = new QPushButton(TranslationManager::instance().tr("btn_batch_remove_tag"), this);
    m_batchRemoveTagBtn->setObjectName("batchRemoveTagBtn");
    m_batchRemoveTagBtn->setProperty("variant", "ghost");
    m_batchRemoveTagBtn->setMinimumHeight(controlHeight);
    connect(m_batchRemoveTagBtn, &QPushButton::clicked, this, &SummaryWindow::onBatchRemoveTags);
    actionsLayout->addWidget(m_batchRemoveTagBtn);
    m_batchRemoveTagBtn->setVisible(false);

    // These actions used to come from QToolBar::addWidget; keep them null and use button visibility instead.
    m_batchSelectAllAction = nullptr;
    m_batchDeleteAction = nullptr;
    m_batchAddTagAction = nullptr;
    m_batchRemoveTagAction = nullptr;

    m_filterToolbar->addWidget(m_actionsGroup);

    QVBoxLayout *mainLayout = qobject_cast<QVBoxLayout *>(layout());
    if (mainLayout)
    {
        mainLayout->insertWidget(0, m_filterToolbar);
    }

    // Prefer the web view as the default focus target.
    clearToolbarFocus();

    connect(m_fromDateEdit, &QDateEdit::dateChanged, this, &SummaryWindow::applyFilters);
    connect(m_toDateEdit, &QDateEdit::dateChanged, this, &SummaryWindow::applyFilters);

    connect(m_tagFilterBtn, &QPushButton::clicked, this, [this]()
            {
        if (!m_historyManager) return;
        QStringList allTags = m_historyManager->getAllTags();
        TagDialog *dialog = new TagDialog(allTags, m_selectedTags, this, [this]() -> bool {
            if (m_selectionModeBtn && m_selectionModeBtn->isChecked()) {
                m_selectionModeBtn->setChecked(false);
                return true;
            }
            return false;
        });
        connect(dialog, &TagDialog::tagsUpdated, this, [this](const QStringList &tags) {
            m_selectedTags = tags;
            if (m_tagFilterBtn) {
                if (m_selectedTags.isEmpty()) {
                    m_tagFilterBtn->setText(TranslationManager::instance().tr("filter_all_tags"));
                } else {
                    QString text = m_selectedTags.join(", ");
                    // Keep button compact
                    if (text.size() > 28) text = text.left(28) + "...";
                    m_tagFilterBtn->setText(text);
                }
            }
            applyFilters();
        });
        dialog->exec();
        dialog->deleteLater(); });

    // Search text changed
    connect(m_searchEdit, &QLineEdit::textChanged, this, [this]() {
        applyFilters();
    });

    connect(m_clearFilterBtn, &QPushButton::clicked, this, [this]()
            {
        m_fromDateEdit->setDate(QDate::currentDate().addMonths(-1));
        m_toDateEdit->setDate(QDate::currentDate());
        m_selectedTags.clear();
        if (m_tagFilterBtn) m_tagFilterBtn->setText(TranslationManager::instance().tr("filter_all_tags"));
        if (m_searchEdit) m_searchEdit->clear();
        applyFilters(); });

    updatePaginationUi();
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
            m_filterToolbar->setStyleSheet(qss);
        }
        else
        {
            // Fallback: keep it readable even if resource is missing.
            const QString bg = isDark ? "#1f1f1f" : "#f6f7f9";
            const QString border = isDark ? "#343434" : "#d8dde6";
            m_filterToolbar->setStyleSheet(QString("QToolBar#archiveFilterToolbar{background:%1;border:none;border-bottom:1px solid %2;}").arg(bg, border));
        }
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
    
    // Refresh window title
    setWindowTitle(TranslationManager::instance().tr("summary_title"));
}
