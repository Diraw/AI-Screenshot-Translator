#include "TagDialog.h"
#include "TranslationManager.h"
#include <QMessageBox>

TagDialog::TagDialog(const QStringList& allTags, const QStringList& currentTags, QWidget *parent)
    : QDialog(parent), m_allTags(allTags), m_currentTags(currentTags)
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
    for (const QString& tag : m_allTags) {
        QListWidgetItem *item = new QListWidgetItem(tag, m_tagList);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(m_currentTags.contains(tag) ? Qt::Checked : Qt::Unchecked);
    }
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

QStringList TagDialog::getSelectedTags() const {
    QStringList selectedTags;
    
    // Get checked tags from list
    for (int i = 0; i < m_tagList->count(); ++i) {
        QListWidgetItem *item = m_tagList->item(i);
        if (item->checkState() == Qt::Checked) {
            selectedTags.append(item->text());
        }
    }
    
    // Add new tags from input (comma-separated)
    QString newTagsText = m_newTagInput->text().trimmed();
    if (!newTagsText.isEmpty()) {
        QStringList newTags = newTagsText.split(',', Qt::SkipEmptyParts);
        for (QString& tag : newTags) {
            tag = tag.trimmed();
            if (!tag.isEmpty() && !selectedTags.contains(tag)) {
                selectedTags.append(tag);
            }
        }
    }
    
    return selectedTags;
}

void TagDialog::onOkClicked() {
    QStringList tags = getSelectedTags();
    emit tagsUpdated(tags);
    accept();
}

void TagDialog::onCancelClicked() {
    reject();
}
