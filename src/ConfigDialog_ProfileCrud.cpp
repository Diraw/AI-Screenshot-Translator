#include "ConfigDialog.h"

#include "TranslationManager.h"

#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>

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
    bool ok = false;
    const QString text = QInputDialog::getText(this, tm.tr("new_profile_title"),
                                               tm.tr("new_profile_label"), QLineEdit::Normal,
                                               QString(), &ok);
    if (!ok || text.isEmpty())
        return;

    if (m_configManager->createProfile(text))
    {
        updateProfileList();
        const QList<QListWidgetItem *> items = m_profileList->findItems(text, Qt::MatchExactly);
        if (!items.isEmpty())
            m_profileList->setCurrentItem(items.first());

        onProfileChanged(text);
        return;
    }

    QMessageBox::warning(this, tm.tr("new_profile_title"), tm.tr("msg_profile_exists"));
}

void ConfigDialog::deleteProfile()
{
    TranslationManager &tm = TranslationManager::instance();
    if (!m_profileList->currentItem())
        return;

    const QString current = m_profileList->currentItem()->text();
    if (current == "Default")
    {
        QMessageBox::warning(this, tm.tr("delete_profile_title"), tm.tr("msg_cannot_delete_default"));
        return;
    }

    const QMessageBox::StandardButton reply = QMessageBox::question(
        this, tm.tr("delete_profile_title"),
        tm.tr("delete_profile_msg").arg(current),
        QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes)
        return;

    m_configManager->deleteProfile(current);
    updateProfileList();
    if (m_profileList->currentItem())
        onProfileChanged(m_profileList->currentItem()->text());
}

void ConfigDialog::renameProfile()
{
    TranslationManager &tm = TranslationManager::instance();
    if (!m_profileList->currentItem())
        return;

    const QString current = m_profileList->currentItem()->text();
    if (current == "Default")
    {
        QMessageBox::warning(this, tm.tr("rename_profile_title"), tm.tr("msg_cannot_rename_default"));
        return;
    }

    bool ok = false;
    const QString newName = QInputDialog::getText(this, tm.tr("rename_profile_title"),
                                                  tm.tr("rename_profile_label"), QLineEdit::Normal,
                                                  current, &ok);
    if (!ok || newName.isEmpty() || newName == current)
        return;

    if (m_configManager->renameProfile(current, newName))
    {
        updateProfileList();
        const QList<QListWidgetItem *> items = m_profileList->findItems(newName, Qt::MatchExactly);
        if (!items.isEmpty())
            m_profileList->setCurrentItem(items.first());

        QMessageBox::information(this, tm.tr("rename_profile_title"), tm.tr("msg_rename_success"));
        return;
    }

    QMessageBox::warning(this, tm.tr("rename_profile_title"), tm.tr("msg_rename_error"));
}

void ConfigDialog::copyProfile()
{
    if (!m_profileList->currentItem())
        return;

    const QString current = m_profileList->currentItem()->text();
    if (current.isEmpty())
        return;

    TranslationManager &tm = TranslationManager::instance();
    bool ok = false;
    const QString newName = QInputDialog::getText(this,
                                                  tm.tr("new_profile_title"),
                                                  tm.tr("new_profile_label"),
                                                  QLineEdit::Normal,
                                                  current + "_Copy", &ok);
    if (!ok || newName.isEmpty())
        return;

    if (m_configManager->copyProfile(current, newName))
    {
        updateProfileList();
        const QList<QListWidgetItem *> items = m_profileList->findItems(newName, Qt::MatchExactly);
        if (!items.isEmpty())
            m_profileList->setCurrentItem(items.first());
        return;
    }

    QMessageBox::warning(this, "Error", tm.tr("msg_profile_exists"));
}

void ConfigDialog::importProfile()
{
    TranslationManager &tm = TranslationManager::instance();
    const QString fileName = QFileDialog::getOpenFileName(this, tm.tr("import_profile_title"), QString(), tm.tr("json_files"));
    if (fileName.isEmpty())
        return;

    if (m_configManager->importProfile(fileName))
    {
        updateProfileList();
        QMessageBox::information(this, tm.tr("import_profile_title"), tm.tr("msg_import_success"));
        return;
    }

    QMessageBox::warning(this, tm.tr("import_profile_title"), tm.tr("msg_import_error"));
}

void ConfigDialog::exportProfile()
{
    TranslationManager &tm = TranslationManager::instance();
    const QString fileName = QFileDialog::getSaveFileName(this, tm.tr("export_profile_title"), "profile.json", tm.tr("json_files"));
    if (fileName.isEmpty())
        return;

    if (m_configManager->exportProfile(m_configManager->currentProfileName(), fileName))
    {
        QMessageBox::information(this, tm.tr("export_profile_title"), tm.tr("msg_export_success"));
        return;
    }

    QMessageBox::warning(this, tm.tr("export_profile_title"), tm.tr("msg_export_error"));
}
