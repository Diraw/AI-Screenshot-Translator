#include "TagDialog.h"
#include "TranslationManager.h"
#include <QMessageBox>
#include <QEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QStyle>
#include <QStyleOptionViewItem>

TagDialog::TagDialog(const QStringList &allTags,
                     const QStringList &currentTags,
                     QWidget *parent,
                     std::function<bool()> escapeInterceptor)
    : QDialog(parent), m_allTags(allTags), m_currentTags(currentTags), m_escapeInterceptor(std::move(escapeInterceptor))
{
    setWindowTitle(TranslationManager::instance().tr("tag_dialog_title"));
    setModal(true);
    resize(400, 300);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Title label
    QLabel *label = new QLabel(TranslationManager::instance().tr("tag_dialog_label"), this);
    mainLayout->addWidget(label);

    // Tag list (checkable)
    m_tagList = new QListWidget(this);
    m_tagList->setSelectionMode(QAbstractItemView::NoSelection);
    for (const QString &tag : m_allTags)
    {
        QListWidgetItem *item = new QListWidgetItem(tag, m_tagList);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(m_currentTags.contains(tag) ? Qt::Checked : Qt::Unchecked);
    }
    if (m_tagList->viewport())
        m_tagList->viewport()->installEventFilter(this);
    mainLayout->addWidget(m_tagList);

    // New tag input
    QLabel *newTagLabel = new QLabel(TranslationManager::instance().tr("tag_dialog_new"), this);
    mainLayout->addWidget(newTagLabel);

    m_newTagInput = new QLineEdit(this);
    m_newTagInput->setPlaceholderText(TranslationManager::instance().tr("tag_dialog_placeholder"));
    mainLayout->addWidget(m_newTagInput);

    // Buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    m_okButton = new QPushButton(TranslationManager::instance().tr("ok"), this);
    m_cancelButton = new QPushButton(TranslationManager::instance().tr("cancel"), this);

    buttonLayout->addWidget(m_okButton);
    buttonLayout->addWidget(m_cancelButton);
    mainLayout->addLayout(buttonLayout);

    connect(m_okButton, &QPushButton::clicked, this, &TagDialog::onOkClicked);
    connect(m_cancelButton, &QPushButton::clicked, this, &TagDialog::onCancelClicked);
}

bool TagDialog::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == (m_tagList ? m_tagList->viewport() : nullptr) && event && event->type() == QEvent::MouseButtonRelease)
    {
        auto *me = static_cast<QMouseEvent *>(event);
        if (me->button() != Qt::LeftButton)
            return QDialog::eventFilter(watched, event);

        const QPoint pos = me->pos();
        QListWidgetItem *item = m_tagList->itemAt(pos);
        if (!item)
            return QDialog::eventFilter(watched, event);

        // If user clicked the native checkbox indicator, let default handling toggle it.
        QStyleOptionViewItem option;
        option.initFrom(m_tagList->viewport());
        option.rect = m_tagList->visualItemRect(item);
        option.features |= QStyleOptionViewItem::HasCheckIndicator;
        option.checkState = item->checkState();

        const QRect checkRect = m_tagList->style()->subElementRect(QStyle::SE_ItemViewItemCheckIndicator, &option, m_tagList->viewport());
        if (checkRect.contains(pos))
            return QDialog::eventFilter(watched, event);

        // Row click toggles the check state.
        item->setCheckState(item->checkState() == Qt::Checked ? Qt::Unchecked : Qt::Checked);
        return true;
    }
    return QDialog::eventFilter(watched, event);
}

void TagDialog::keyPressEvent(QKeyEvent *event)
{
    if (event && event->key() == Qt::Key_Escape)
    {
        if (m_escapeInterceptor && m_escapeInterceptor())
        {
            event->accept();
            return;
        }
    }
    QDialog::keyPressEvent(event);
}

QStringList TagDialog::getSelectedTags() const
{
    QStringList selectedTags;

    // Get checked tags from list
    for (int i = 0; i < m_tagList->count(); ++i)
    {
        QListWidgetItem *item = m_tagList->item(i);
        if (item->checkState() == Qt::Checked)
        {
            selectedTags.append(item->text());
        }
    }

    // Add new tags from input (comma-separated)
    QString newTagsText = m_newTagInput->text().trimmed();
    if (!newTagsText.isEmpty())
    {
        QStringList newTags = newTagsText.split(',', Qt::SkipEmptyParts);
        for (QString &tag : newTags)
        {
            tag = tag.trimmed();
            if (!tag.isEmpty() && !selectedTags.contains(tag))
            {
                selectedTags.append(tag);
            }
        }
    }

    return selectedTags;
}

void TagDialog::onOkClicked()
{
    QStringList tags = getSelectedTags();
    emit tagsUpdated(tags);
    accept();
}

void TagDialog::onCancelClicked()
{
    reject();
}
