#include "ConfigDialog.h"

#include "TranslationManager.h"

#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>

void ConfigDialog::updateProfileList()
{ // kept
    m_profileList->blockSignals(true);
    m_profileList->clear();
    m_profileList->addItems(m_configManager->listProfiles());

    // Select current
    QString current = m_configManager->currentProfileName();
    auto items = m_profileList->findItems(current, Qt::MatchExactly);
    if (!items.isEmpty())
    {
        m_profileList->setCurrentItem(items.first());
    }
    m_profileList->blockSignals(false);
}

void ConfigDialog::onProfileChanged(const QString &name)
{
    if (name.isEmpty())
        return;
    m_configManager->loadProfile(name);
    loadFromConfig();
}

void ConfigDialog::newProfile()
{
    TranslationManager &tm = TranslationManager::instance();
    bool ok;
    QString text = QInputDialog::getText(this, tm.tr("new_profile_title"),
                                         tm.tr("new_profile_label"), QLineEdit::Normal,
                                         "", &ok);
    if (ok && !text.isEmpty())
    {
        if (m_configManager->createProfile(text))
        {
            updateProfileList();
            auto items = m_profileList->findItems(text, Qt::MatchExactly);
            if (!items.isEmpty())
                m_profileList->setCurrentItem(items.first());

            // Force reload to clear UI
            onProfileChanged(text);
        }
        else
        {
            QMessageBox::warning(this, tm.tr("new_profile_title"), tm.tr("msg_profile_exists"));
        }
    }
}

void ConfigDialog::deleteProfile()
{
    TranslationManager &tm = TranslationManager::instance();
    if (!m_profileList->currentItem())
        return;
    QString current = m_profileList->currentItem()->text();
    if (current == "Default")
    {
        QMessageBox::warning(this, tm.tr("delete_profile_title"), tm.tr("msg_cannot_delete_default"));
        return;
    }

    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, tm.tr("delete_profile_title"),
                                  tm.tr("delete_profile_msg").arg(current),
                                  QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::Yes)
    {
        m_configManager->deleteProfile(current);
        updateProfileList();
        if (m_profileList->currentItem())
            onProfileChanged(m_profileList->currentItem()->text());
    }
}

void ConfigDialog::renameProfile()
{
    TranslationManager &tm = TranslationManager::instance();
    if (!m_profileList->currentItem())
        return;
    QString current = m_profileList->currentItem()->text();
    if (current == "Default")
    {
        QMessageBox::warning(this, tm.tr("rename_profile_title"), tm.tr("msg_cannot_rename_default"));
        return;
    }

    bool ok;
    QString newName = QInputDialog::getText(this, tm.tr("rename_profile_title"),
                                            tm.tr("rename_profile_label"), QLineEdit::Normal,
                                            current, &ok);
    if (ok && !newName.isEmpty() && newName != current)
    {
        if (m_configManager->renameProfile(current, newName))
        {
            updateProfileList();
            auto items = m_profileList->findItems(newName, Qt::MatchExactly);
            if (!items.isEmpty())
                m_profileList->setCurrentItem(items.first());

            QMessageBox::information(this, tm.tr("rename_profile_title"),
                                     tm.tr("msg_rename_success"));
        }
        else
        {
            QMessageBox::warning(this, tm.tr("rename_profile_title"),
                                 tm.tr("msg_rename_error"));
        }
    }
}

void ConfigDialog::copyProfile()
{
    if (!m_profileList->currentItem())
        return;
    QString current = m_profileList->currentItem()->text();
    if (current.isEmpty())
        return;

    bool ok;
    QString newName = QInputDialog::getText(this,
                                            TranslationManager::instance().tr("new_profile_title"),
                                            TranslationManager::instance().tr("new_profile_label"),
                                            QLineEdit::Normal, current + "_Copy", &ok);

    if (ok && !newName.isEmpty())
    {
        if (m_configManager->copyProfile(current, newName))
        {
            updateProfileList();
            auto items = m_profileList->findItems(newName, Qt::MatchExactly);
            if (!items.isEmpty())
                m_profileList->setCurrentItem(items.first());
        }
        else
        {
            QMessageBox::warning(this, "Error", TranslationManager::instance().tr("msg_profile_exists"));
        }
    }
}

void ConfigDialog::importProfile()
{
    TranslationManager &tm = TranslationManager::instance();
    QString fileName = QFileDialog::getOpenFileName(this, tm.tr("import_profile_title"), "", tm.tr("json_files"));
    if (fileName.isEmpty())
        return;

    if (m_configManager->importProfile(fileName))
    {
        updateProfileList();
        QMessageBox::information(this, tm.tr("import_profile_title"), tm.tr("msg_import_success"));
    }
    else
    {
        QMessageBox::warning(this, tm.tr("import_profile_title"), tm.tr("msg_import_error"));
    }
}

void ConfigDialog::exportProfile()
{
    TranslationManager &tm = TranslationManager::instance();
    QString fileName = QFileDialog::getSaveFileName(this, tm.tr("export_profile_title"), "profile.json", tm.tr("json_files"));
    if (fileName.isEmpty())
        return;

    if (m_configManager->exportProfile(m_configManager->currentProfileName(), fileName))
    {
        QMessageBox::information(this, tm.tr("export_profile_title"), tm.tr("msg_export_success"));
    }
    else
    {
        QMessageBox::warning(this, tm.tr("export_profile_title"), tm.tr("msg_export_error"));
    }
}
