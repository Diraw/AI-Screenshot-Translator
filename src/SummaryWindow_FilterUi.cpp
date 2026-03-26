#include "SummaryWindow.h"

#include "EmbedWebView.h"
#include "HistoryManager.h"
#include "TagDialog.h"
#include "TranslationManager.h"

#include <QApplication>
#include <QDate>
#include <QDateEdit>
#include <QFontMetrics>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>

#include <functional>

namespace
{
QString overflowHandleToolTipText()
{
    if (TranslationManager::instance().getLanguage() == QStringLiteral("zh"))
        return QStringLiteral("\u6a2a\u5411\u62d6\u52a8\u8c03\u6574\u7a97\u53e3\u5bbd\u5ea6\uff0c\u53cc\u51fb\u81ea\u52a8\u9002\u914d");

    return QStringLiteral("Drag horizontally to resize, double-click to auto-fit");
}

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
} // namespace

void SummaryWindow::setupFilterUI()
{
    m_filterToolbarShell = new QWidget(this);
    m_filterToolbarShell->setObjectName("archiveFilterToolbarShell");
    QHBoxLayout *toolbarShellLayout = new QHBoxLayout(m_filterToolbarShell);
    toolbarShellLayout->setContentsMargins(0, 0, 0, 0);
    toolbarShellLayout->setSpacing(0);

    m_filterToolbar = new QToolBar(m_filterToolbarShell);
    m_filterToolbar->setObjectName("archiveFilterToolbar");
    m_filterToolbar->setAttribute(Qt::WA_StyledBackground, true);
    m_filterToolbar->setAutoFillBackground(true);
    m_filterToolbar->setMovable(false);
    m_filterToolbar->setFloatable(false);
    m_filterToolbar->setContextMenuPolicy(Qt::PreventContextMenu);
    m_filterToolbar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    toolbarShellLayout->addWidget(m_filterToolbar, 1);

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

    const int controlHeight = qMax(26, QFontMetrics(m_filterToolbar->font()).lineSpacing() + 8);
    const int dateWidth = 117;

    m_filtersGroup = new QWidget(this);
    m_filtersGroup->setObjectName("archiveFiltersGroup");
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

    QWidget *spacer = new ClickToClearFocusWidget(clearToolbarFocus, this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_filterToolbar->addWidget(spacer);

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

    m_batchSelectAllAction = nullptr;
    m_batchDeleteAction = nullptr;
    m_batchAddTagAction = nullptr;
    m_batchRemoveTagAction = nullptr;

    m_filterToolbar->addWidget(m_actionsGroup);

    m_toolbarOverflowButton = new QToolButton(m_filterToolbarShell);
    m_toolbarOverflowButton->setObjectName("archiveOverflowHandle");
    m_toolbarOverflowButton->setText(QStringLiteral(">>"));
    m_toolbarOverflowButton->setToolTip(QStringLiteral("Drag horizontally to resize / æ¨ªå‘æ‹–åŠ¨è°ƒæ•´çª—å£å®½åº¦"));
    m_toolbarOverflowButton->setFocusPolicy(Qt::NoFocus);
    m_toolbarOverflowButton->setCursor(Qt::SizeHorCursor);
    m_toolbarOverflowButton->setMouseTracking(true);
    m_toolbarOverflowButton->setAutoRaise(false);
    m_toolbarOverflowButton->setFixedHeight(controlHeight);
    m_toolbarOverflowButton->setFixedWidth(12);
    m_toolbarOverflowButton->setToolTip(overflowHandleToolTipText());
    m_toolbarOverflowButton->setProperty("toolbarOverflowHandle", true);
    m_toolbarOverflowButton->setProperty("overflowHintActive", false);
    auto *overflowOpacity = new QGraphicsOpacityEffect(m_toolbarOverflowButton);
    overflowOpacity->setOpacity(0.5);
    m_toolbarOverflowButton->setGraphicsEffect(overflowOpacity);
    m_toolbarOverflowButton->installEventFilter(this);
    toolbarShellLayout->addWidget(m_toolbarOverflowButton, 0, Qt::AlignVCenter);

    QTimer::singleShot(0, this, [this]()
                       { refreshToolbarOverflowHint(); });

    QVBoxLayout *mainLayout = qobject_cast<QVBoxLayout *>(layout());
    if (mainLayout)
        mainLayout->insertWidget(0, m_filterToolbarShell);

    clearToolbarFocus();

    connect(m_fromDateEdit, &QDateEdit::dateChanged, this, &SummaryWindow::applyFilters);
    connect(m_toDateEdit, &QDateEdit::dateChanged, this, &SummaryWindow::applyFilters);

    connect(m_tagFilterBtn, &QPushButton::clicked, this, [this]()
            {
                if (!m_historyManager)
                    return;
                QStringList allTags = m_historyManager->getAllTags();
                TagDialog *dialog = new TagDialog(allTags, m_selectedTags, this, [this]() -> bool {
                    if (m_selectionModeBtn && m_selectionModeBtn->isChecked())
                    {
                        m_selectionModeBtn->setChecked(false);
                        return true;
                    }
                    return false;
                });
                connect(dialog, &TagDialog::tagsUpdated, this, [this](const QStringList &tags) {
                    m_selectedTags = tags;
                    if (m_tagFilterBtn)
                    {
                        if (m_selectedTags.isEmpty())
                        {
                            m_tagFilterBtn->setText(TranslationManager::instance().tr("filter_all_tags"));
                        }
                        else
                        {
                            QString text = m_selectedTags.join(", ");
                            if (text.size() > 28)
                                text = text.left(28) + "...";
                            m_tagFilterBtn->setText(text);
                        }
                    }
                    applyFilters();
                });
                dialog->exec();
                dialog->deleteLater();
            });

    if (!m_searchDebounceTimer)
    {
        m_searchDebounceTimer = new QTimer(this);
        m_searchDebounceTimer->setSingleShot(true);
        m_searchDebounceTimer->setInterval(180);
        connect(m_searchDebounceTimer, &QTimer::timeout, this, [this]()
                { applyFilters(); });
    }

    connect(m_searchEdit, &QLineEdit::textChanged, this, [this]()
            {
                if (m_searchDebounceTimer)
                    m_searchDebounceTimer->start();
                else
                    applyFilters();
            });

    connect(m_clearFilterBtn, &QPushButton::clicked, this, [this]()
            {
                m_fromDateEdit->setDate(QDate::currentDate().addMonths(-1));
                m_toDateEdit->setDate(QDate::currentDate());
                m_selectedTags.clear();
                if (m_tagFilterBtn)
                    m_tagFilterBtn->setText(TranslationManager::instance().tr("filter_all_tags"));
                if (m_searchEdit)
                    m_searchEdit->clear();
                if (m_searchDebounceTimer)
                    m_searchDebounceTimer->stop();
                applyFilters();
            });

    updatePaginationUi();
}
