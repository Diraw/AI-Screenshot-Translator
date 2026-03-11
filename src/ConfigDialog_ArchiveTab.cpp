#include "ConfigDialog.h"

#include "TranslationManager.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QPushButton>

void ConfigDialog::setupArchiveTab()
{
    TranslationManager &tm = TranslationManager::instance();

    m_archiveTab = new QWidget();
    auto *archiveMainLayout = new QVBoxLayout(m_archiveTab);

    auto *grpView = new QGroupBox("View Toggle", m_archiveTab);
    grpView->setObjectName("grpView");
    auto *viewLayout = new QFormLayout(grpView);

    m_viewToggleHotkeyEdit = new QLineEdit(this);
    viewLayout->addRow(tm.tr("lbl_view_hotkey"), m_viewToggleHotkeyEdit);

    m_screenshotToggleHotkeyEdit = new QLineEdit(this);
    viewLayout->addRow(tm.tr("lbl_shot_toggle_hotkey"), m_screenshotToggleHotkeyEdit);

    m_selectionToggleHotkeyEdit = new QLineEdit(this);
    viewLayout->addRow(tm.tr("lbl_selection_toggle_hotkey"), m_selectionToggleHotkeyEdit);

    m_archiveLoadModeCombo = new QComboBox(this);
    m_archiveLoadModeCombo->addItem(tm.tr("opt_archive_load_all"), false);
    m_archiveLoadModeCombo->addItem(tm.tr("opt_archive_load_paged"), true);
    viewLayout->addRow(tm.tr("lbl_archive_load_mode"), m_archiveLoadModeCombo);

    m_archivePageSizeSpin = new QSpinBox(this);
    m_archivePageSizeSpin->setRange(1, 1000);
    m_archivePageSizeSpin->setValue(50);
    viewLayout->addRow(tm.tr("lbl_archive_page_size"), m_archivePageSizeSpin);

    connect(m_archiveLoadModeCombo, &QComboBox::currentIndexChanged, this, [this](int)
            {
                if (!m_archiveLoadModeCombo || !m_archivePageSizeSpin)
                    return;
                m_archivePageSizeSpin->setEnabled(m_archiveLoadModeCombo->currentData().toBool());
            });
    m_archivePageSizeSpin->setEnabled(false);

    archiveMainLayout->addWidget(grpView);

    auto *grpEdit = new QGroupBox("Edit Mode", m_archiveTab);
    grpEdit->setObjectName("grpEdit");
    auto *editLayout = new QFormLayout(grpEdit);

    m_editHotkeyEdit = new QLineEdit(this);
    editLayout->addRow(tm.tr("lbl_edit_hotkey"), m_editHotkeyEdit);

    m_boldHotkeyEdit = new QLineEdit(this);
    editLayout->addRow(tm.tr("lbl_bold"), m_boldHotkeyEdit);

    m_underlineHotkeyEdit = new QLineEdit(this);
    editLayout->addRow(tm.tr("lbl_underline"), m_underlineHotkeyEdit);

    m_highlightHotkeyEdit = new QLineEdit(this);
    editLayout->addRow(tm.tr("lbl_highlight"), m_highlightHotkeyEdit);

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
    editLayout->addRow(tm.tr("lbl_highlight_color"), hl1w);

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
    editLayout->addRow(tm.tr("lbl_highlight_color_dark"), hl2w);

    archiveMainLayout->addWidget(grpEdit);

    auto *grpData = new QGroupBox(tm.tr("grp_data_migration"), m_archiveTab);
    grpData->setObjectName("grpDataMigration");
    auto *dataLayout = new QVBoxLayout(grpData);
    m_importLegacyHistoryBtn = new QPushButton(tm.tr("btn_import_legacy_history"), this);
    connect(m_importLegacyHistoryBtn, &QPushButton::clicked, this, &ConfigDialog::onImportLegacyHistory);
    m_exportHistoryBtn = new QPushButton(tm.tr("btn_export_history"), this);
    connect(m_exportHistoryBtn, &QPushButton::clicked, this, &ConfigDialog::onExportHistory);
    m_importLegacyHistoryBtn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_exportHistoryBtn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_importLegacyHistoryBtn->setFixedWidth(96);
    m_exportHistoryBtn->setFixedWidth(96);
    auto *dataBtnRow = new QHBoxLayout();
    dataBtnRow->setContentsMargins(0, 0, 0, 0);
    dataBtnRow->setSpacing(8);
    dataBtnRow->addWidget(m_importLegacyHistoryBtn);
    dataBtnRow->addWidget(m_exportHistoryBtn);
    dataBtnRow->addStretch();
    dataLayout->addLayout(dataBtnRow);
    archiveMainLayout->addWidget(grpData);

    archiveMainLayout->addStretch();

    m_tabWidget->addTab(m_archiveTab, tm.tr("tab_archive_interface"));
}
