#include "ConfigDialog.h"

#include <QGridLayout>

void ConfigDialog::setupProfileSection(QVBoxLayout *mainLayout)
{
    m_profileGroup = new QGroupBox("Profiles", this);
    auto *profileMainLayout = new QHBoxLayout(m_profileGroup);

    m_profileList = new QListWidget(this);
    m_profileList->setFixedHeight(60);
    m_profileList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    connect(m_profileList, &QListWidget::currentTextChanged, this, &ConfigDialog::onProfileChanged);

    m_newProfileBtn = new QPushButton("New", this);
    connect(m_newProfileBtn, &QPushButton::clicked, this, &ConfigDialog::newProfile);

    m_deleteProfileBtn = new QPushButton("Delete", this);
    connect(m_deleteProfileBtn, &QPushButton::clicked, this, &ConfigDialog::deleteProfile);

    m_renameProfileBtn = new QPushButton("Rename", this);
    connect(m_renameProfileBtn, &QPushButton::clicked, this, &ConfigDialog::renameProfile);

    m_copyProfileBtn = new QPushButton("Copy", this);
    connect(m_copyProfileBtn, &QPushButton::clicked, this, &ConfigDialog::copyProfile);

    m_importProfileBtn = new QPushButton("Import", this);
    m_importProfileBtn->setToolTip("Import Profile");
    connect(m_importProfileBtn, &QPushButton::clicked, this, &ConfigDialog::importProfile);

    m_exportProfileBtn = new QPushButton("Export", this);
    m_exportProfileBtn->setToolTip("Export Profile");
    connect(m_exportProfileBtn, &QPushButton::clicked, this, &ConfigDialog::exportProfile);

    auto *buttonGrid = new QGridLayout();
    buttonGrid->addWidget(m_newProfileBtn, 0, 0);
    buttonGrid->addWidget(m_deleteProfileBtn, 0, 1);
    buttonGrid->addWidget(m_importProfileBtn, 0, 2);
    buttonGrid->addWidget(m_renameProfileBtn, 1, 0);
    buttonGrid->addWidget(m_copyProfileBtn, 1, 1);
    buttonGrid->addWidget(m_exportProfileBtn, 1, 2);

    profileMainLayout->addWidget(m_profileList, 1);
    profileMainLayout->addLayout(buttonGrid);
    profileMainLayout->setStretch(0, 1);
    profileMainLayout->setStretch(1, 0);

    m_profileGroup->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    mainLayout->addWidget(m_profileGroup);
}

void ConfigDialog::setupActionButtons(QVBoxLayout *mainLayout)
{
    auto *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    auto *saveBtn = new QPushButton("Save and Apply", this);
    saveBtn->setProperty("isSaveBtn", true);
    saveBtn->setDefault(true);
    connect(saveBtn, &QPushButton::clicked, this, &ConfigDialog::save);
    btnLayout->addWidget(saveBtn);

    mainLayout->addLayout(btnLayout);
}
