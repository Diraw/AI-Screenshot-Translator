#include "ConfigDialog.h"

#include <QFormLayout>

void ConfigDialog::setupTranslationTab()
{
    m_transTab = new QWidget();
    auto *transMainLayout = new QVBoxLayout(m_transTab);

    auto *grpCard = new QGroupBox("Screenshot Card Settings", m_transTab);
    grpCard->setObjectName("grpCard");
    auto *cardLayout = new QFormLayout(grpCard);

    m_zoomSensitivitySpin = new QDoubleSpinBox(this);
    m_zoomSensitivitySpin->setRange(10.0, 2000.0);
    m_zoomSensitivitySpin->setSingleStep(10.0);
    cardLayout->addRow("Zoom Sensitivity:", m_zoomSensitivitySpin);

    m_cardBorderColorEdit = new QLineEdit(this);
    m_cardBorderColorEdit->setPlaceholderText("R,G,B,A (e.g., 100,100,100,128) æˆ– rgba(100,100,100,0.5)");
    m_useBorderCheck = new QCheckBox(this);
    m_lblCardBorderColor = new QLabel(this);

    m_cardBorderColorPreview = new QLabel(this);
    m_cardBorderColorPreview->setFixedSize(18, 18);
    updateColorPreviewLabel(m_cardBorderColorPreview, m_cardBorderColorEdit->text());
    connect(m_cardBorderColorEdit, &QLineEdit::textChanged, this, [this](const QString &t)
            { updateColorPreviewLabel(m_cardBorderColorPreview, t); });

    auto *borderColorLayout = new QHBoxLayout();
    borderColorLayout->addWidget(m_cardBorderColorEdit);
    borderColorLayout->addWidget(m_cardBorderColorPreview);
    borderColorLayout->addWidget(m_useBorderCheck);
    cardLayout->addRow(m_lblCardBorderColor, borderColorLayout);

    transMainLayout->addWidget(grpCard);

    auto *grpTrans = new QGroupBox("Translation Window Settings", m_transTab);
    grpTrans->setObjectName("grpTrans");
    auto *transWinLayout = new QFormLayout(grpTrans);

    m_initialFontSizeSpin = new QSpinBox(this);
    m_initialFontSizeSpin->setRange(8, 72);
    m_lblInitialFontSize = new QLabel(this);
    transWinLayout->addRow(m_lblInitialFontSize, m_initialFontSizeSpin);

    m_defaultLookCheck = new QCheckBox("Default Window State is Locked", this);
    transWinLayout->addRow(m_defaultLookCheck);

    m_lockBehaviorCombo = new QComboBox(this);
    m_lockBehaviorCombo->addItem("Reset to Unlocked", 0);
    m_lockBehaviorCombo->addItem("Maintain Previous State", 1);
    transWinLayout->addRow("After Closing Locked Windows:", m_lockBehaviorCombo);

    m_prevPageHotkeyEdit = new QLineEdit(this);
    transWinLayout->addRow("Prev Page Hotkey:", m_prevPageHotkeyEdit);

    m_nextPageHotkeyEdit = new QLineEdit(this);
    transWinLayout->addRow("Next Page Hotkey:", m_nextPageHotkeyEdit);

    m_tagHotkeyEdit = new QLineEdit(this);
    transWinLayout->addRow("Tag Dialog Hotkey:", m_tagHotkeyEdit);

    m_retranslateHotkeyEdit = new QLineEdit(this);
    transWinLayout->addRow("Retranslate Hotkey:", m_retranslateHotkeyEdit);

    transMainLayout->addWidget(grpTrans);
    transMainLayout->addStretch();

    m_tabWidget->addTab(m_transTab, "Translation");
}
