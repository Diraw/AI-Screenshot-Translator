#include "ConfigDialog.h"

#include "TranslationManager.h"

#include <QFormLayout>

void ConfigDialog::setupArchiveTab()
{
    m_archiveTab = new QWidget();
    auto *archiveMainLayout = new QVBoxLayout(m_archiveTab);

    auto *grpView = new QGroupBox("View Toggle", m_archiveTab);
    grpView->setObjectName("grpView");
    auto *viewLayout = new QFormLayout(grpView);

    m_viewToggleHotkeyEdit = new QLineEdit(this);
    viewLayout->addRow("Summary View Toggle:", m_viewToggleHotkeyEdit);

    m_screenshotToggleHotkeyEdit = new QLineEdit(this);
    viewLayout->addRow("Summary Screenshot Toggle:", m_screenshotToggleHotkeyEdit);

    m_selectionToggleHotkeyEdit = new QLineEdit(this);
    viewLayout->addRow(TranslationManager::instance().tr("lbl_selection_toggle_hotkey"), m_selectionToggleHotkeyEdit);

    archiveMainLayout->addWidget(grpView);

    auto *grpEdit = new QGroupBox("Edit Mode", m_archiveTab);
    grpEdit->setObjectName("grpEdit");
    auto *editLayout = new QFormLayout(grpEdit);

    m_editHotkeyEdit = new QLineEdit(this);
    editLayout->addRow("Summary Edit Toggle:", m_editHotkeyEdit);

    m_boldHotkeyEdit = new QLineEdit(this);
    editLayout->addRow("Bold Shortcut:", m_boldHotkeyEdit);

    m_underlineHotkeyEdit = new QLineEdit(this);
    editLayout->addRow("Underline Shortcut:", m_underlineHotkeyEdit);

    m_highlightHotkeyEdit = new QLineEdit(this);
    editLayout->addRow("Highlight Shortcut:", m_highlightHotkeyEdit);

    m_highlightMarkColorEdit = new QLineEdit(this);
    m_highlightMarkColorEdit->setPlaceholderText("#ffeb3b80 or rgba(255,235,59,0.5) or 255,235,59,0.5");
    m_highlightMarkColorPreview = new QLabel(this);
    m_highlightMarkColorPreview->setFixedSize(18, 18);
    updateColorPreviewLabel(m_highlightMarkColorPreview, m_highlightMarkColorEdit->text());
    connect(m_highlightMarkColorEdit, &QLineEdit::textChanged, this, [this](const QString &t)
            { updateColorPreviewLabel(m_highlightMarkColorPreview, t); });
    auto *hl1 = new QHBoxLayout();
    hl1->setContentsMargins(0, 0, 0, 0);
    hl1->addWidget(m_highlightMarkColorEdit);
    hl1->addWidget(m_highlightMarkColorPreview);
    auto *hl1w = new QWidget(this);
    hl1w->setLayout(hl1);
    editLayout->addRow("Highlight Color:", hl1w);

    m_highlightMarkColorDarkEdit = new QLineEdit(this);
    m_highlightMarkColorDarkEdit->setPlaceholderText("#d4af3780 or rgba(212,175,55,0.5) or 212,175,55,0.5");
    m_highlightMarkColorDarkPreview = new QLabel(this);
    m_highlightMarkColorDarkPreview->setFixedSize(18, 18);
    updateColorPreviewLabel(m_highlightMarkColorDarkPreview, m_highlightMarkColorDarkEdit->text());
    connect(m_highlightMarkColorDarkEdit, &QLineEdit::textChanged, this, [this](const QString &t)
            { updateColorPreviewLabel(m_highlightMarkColorDarkPreview, t); });
    auto *hl2 = new QHBoxLayout();
    hl2->setContentsMargins(0, 0, 0, 0);
    hl2->addWidget(m_highlightMarkColorDarkEdit);
    hl2->addWidget(m_highlightMarkColorDarkPreview);
    auto *hl2w = new QWidget(this);
    hl2w->setLayout(hl2);
    editLayout->addRow("Highlight Color (Dark):", hl2w);

    archiveMainLayout->addWidget(grpEdit);
    archiveMainLayout->addStretch();

    m_tabWidget->addTab(m_archiveTab, "Archive Interface");
}
