#include "SummaryWindow.h"

#include "EmbedWebView.h"
#include "HistoryManager.h"
#include "ThemeUtils.h"
#include "TranslationManager.h"

#include <QComboBox>
#include <QDate>
#include <QDateEdit>
#include <QFile>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QPushButton>
#include <QToolBar>
#include <QVBoxLayout>

#include <algorithm>

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

    const int controlHeight = 22;
    const int dateWidth = 128;

    // Left group: filters
    m_filtersGroup = new QWidget(this);
    m_filtersGroup->setObjectName("archiveFiltersGroup");
    // Keep filters compact so the right-side batch actions don't get pushed into the overflow.
    m_filtersGroup->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    QHBoxLayout *filtersLayout = new QHBoxLayout(m_filtersGroup);
    filtersLayout->setContentsMargins(3, 0, 3, 0);
    filtersLayout->setSpacing(3);

    QLabel *fromLabel = new QLabel(TranslationManager::instance().tr("filter_from_date") + ":", this);
    fromLabel->setProperty("role", "caption");
    filtersLayout->addWidget(fromLabel);

    m_fromDateEdit = new QDateEdit(this);
    m_fromDateEdit->setObjectName("fromDateEdit");
    m_fromDateEdit->setCalendarPopup(true);
    m_fromDateEdit->setDate(QDate::currentDate().addMonths(-1));
    m_fromDateEdit->setDisplayFormat("yyyy-MM-dd");
    m_fromDateEdit->setFixedHeight(controlHeight);
    m_fromDateEdit->setFixedWidth(dateWidth);
    filtersLayout->addWidget(m_fromDateEdit);

    QLabel *toLabel = new QLabel(TranslationManager::instance().tr("filter_to_date") + ":", this);
    toLabel->setProperty("role", "caption");
    filtersLayout->addWidget(toLabel);

    m_toDateEdit = new QDateEdit(this);
    m_toDateEdit->setObjectName("toDateEdit");
    m_toDateEdit->setCalendarPopup(true);
    m_toDateEdit->setDate(QDate::currentDate());
    m_toDateEdit->setDisplayFormat("yyyy-MM-dd");
    m_toDateEdit->setFixedHeight(controlHeight);
    m_toDateEdit->setFixedWidth(dateWidth);
    filtersLayout->addWidget(m_toDateEdit);

    QLabel *tagLabel = new QLabel(TranslationManager::instance().tr("filter_tags") + ":", this);
    tagLabel->setProperty("role", "caption");
    filtersLayout->addWidget(tagLabel);

    m_tagFilterCombo = new QComboBox(this);
    m_tagFilterCombo->setObjectName("tagFilterCombo");
    m_tagFilterCombo->setEditable(false);
    m_tagFilterCombo->addItem(TranslationManager::instance().tr("filter_all_tags"), "");
    m_tagFilterCombo->setFixedHeight(controlHeight);
    m_tagFilterCombo->setFixedWidth(96);
    m_tagFilterCombo->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    filtersLayout->addWidget(m_tagFilterCombo);

    m_clearFilterBtn = new QPushButton(TranslationManager::instance().tr("filter_clear"), this);
    m_clearFilterBtn->setObjectName("clearFilterBtn");
    m_clearFilterBtn->setProperty("variant", "ghost");
    m_clearFilterBtn->setFixedHeight(controlHeight);
    filtersLayout->addWidget(m_clearFilterBtn);

    m_filterToolbar->addWidget(m_filtersGroup);

    // Push batch actions to the right
    QWidget *spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_filterToolbar->addWidget(spacer);

    // Right group: batch actions
    m_actionsGroup = new QWidget(this);
    m_actionsGroup->setObjectName("archiveActionsGroup");
    m_actionsGroup->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    QHBoxLayout *actionsLayout = new QHBoxLayout(m_actionsGroup);
    actionsLayout->setContentsMargins(3, 0, 3, 0);
    actionsLayout->setSpacing(2);

    m_selectionModeBtn = new QPushButton(TranslationManager::instance().tr("btn_selection_mode"), this);
    m_selectionModeBtn->setObjectName("selectionModeBtn");
    m_selectionModeBtn->setCheckable(true);
    m_selectionModeBtn->setProperty("variant", "toggle");
    m_selectionModeBtn->setFixedHeight(controlHeight);
    connect(m_selectionModeBtn, &QPushButton::toggled, this, &SummaryWindow::toggleSelectionMode);
    actionsLayout->addWidget(m_selectionModeBtn);

    m_selectAllBtn = new QPushButton(TranslationManager::instance().tr("btn_select_all"), this);
    m_selectAllBtn->setObjectName("selectAllBtn");
    m_selectAllBtn->setProperty("variant", "ghost");
    m_selectAllBtn->setFixedHeight(controlHeight);
    connect(m_selectAllBtn, &QPushButton::clicked, this, &SummaryWindow::onBatchSelectAll);
    actionsLayout->addWidget(m_selectAllBtn);
    m_selectAllBtn->setVisible(false);

    m_batchDeleteBtn = new QPushButton(TranslationManager::instance().tr("btn_batch_delete"), this);
    m_batchDeleteBtn->setObjectName("batchDeleteBtn");
    m_batchDeleteBtn->setProperty("variant", "danger");
    m_batchDeleteBtn->setFixedHeight(controlHeight);
    connect(m_batchDeleteBtn, &QPushButton::clicked, this, &SummaryWindow::onBatchDelete);
    actionsLayout->addWidget(m_batchDeleteBtn);
    m_batchDeleteBtn->setVisible(false);

    m_batchAddTagBtn = new QPushButton(TranslationManager::instance().tr("btn_batch_add_tag"), this);
    m_batchAddTagBtn->setObjectName("batchAddTagBtn");
    m_batchAddTagBtn->setProperty("variant", "ghost");
    m_batchAddTagBtn->setFixedHeight(controlHeight);
    connect(m_batchAddTagBtn, &QPushButton::clicked, this, &SummaryWindow::onBatchAddTags);
    actionsLayout->addWidget(m_batchAddTagBtn);
    m_batchAddTagBtn->setVisible(false);

    m_batchRemoveTagBtn = new QPushButton(TranslationManager::instance().tr("btn_batch_remove_tag"), this);
    m_batchRemoveTagBtn->setObjectName("batchRemoveTagBtn");
    m_batchRemoveTagBtn->setProperty("variant", "ghost");
    m_batchRemoveTagBtn->setFixedHeight(controlHeight);
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
