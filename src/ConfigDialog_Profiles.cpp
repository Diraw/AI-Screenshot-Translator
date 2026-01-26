#include "ConfigDialog.h"

#include "TranslationManager.h"

#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QFileSystemWatcher>
#include <QTimer>
#include <QDir>
#include <QFileInfo>

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

    refreshProfilesWatcherPaths();
}

void ConfigDialog::setupProfilesWatcher()
{
    if (m_profilesWatcher)
        return;

    m_profilesWatcher = new QFileSystemWatcher(this);
    connect(m_profilesWatcher, &QFileSystemWatcher::directoryChanged,
            this, &ConfigDialog::onProfilesDirChanged);
    connect(m_profilesWatcher, &QFileSystemWatcher::fileChanged,
            this, &ConfigDialog::onProfileFileChanged);

    m_profilesWatchTimer = new QTimer(this);
    m_profilesWatchTimer->setSingleShot(true);
    m_profilesWatchTimer->setInterval(250);
    connect(m_profilesWatchTimer, &QTimer::timeout,
            this, &ConfigDialog::onProfilesWatcherTimeout);

    refreshProfilesWatcherPaths();
}

void ConfigDialog::refreshProfilesWatcherPaths()
{
    if (!m_profilesWatcher || !m_configManager)
        return;

    const QString profilesDir = m_configManager->profilesDirPath();
    if (!profilesDir.isEmpty() && !m_profilesWatcher->directories().contains(profilesDir))
    {
        // Watch directory for add/remove/rename
        m_profilesWatcher->addPath(profilesDir);
    }

    QDir dir(profilesDir);
    QStringList desiredFiles;
    if (dir.exists())
    {
        const QStringList files = dir.entryList(QStringList() << "*.json", QDir::Files);
        desiredFiles.reserve(files.size());
        for (const auto &f : files)
            desiredFiles << QDir::cleanPath(dir.filePath(f));
    }

    const QStringList currentFiles = m_profilesWatcher->files();
    QStringList toRemove;
    for (const auto &p : currentFiles)
    {
        if (!desiredFiles.contains(QDir::cleanPath(p)))
            toRemove << p;
    }
    if (!toRemove.isEmpty())
        m_profilesWatcher->removePaths(toRemove);

    QStringList toAdd;
    for (const auto &p : desiredFiles)
    {
        if (!currentFiles.contains(p))
            toAdd << p;
    }
    if (!toAdd.isEmpty())
        m_profilesWatcher->addPaths(toAdd);
}

void ConfigDialog::onProfilesDirChanged(const QString & /*path*/)
{
    m_profilesDirDirty = true;
    if (m_profilesWatchTimer)
        m_profilesWatchTimer->start();
}

void ConfigDialog::onProfileFileChanged(const QString &path)
{
    m_profilesChangedFile = QDir::cleanPath(path);
    if (m_profilesWatchTimer)
        m_profilesWatchTimer->start();
}

void ConfigDialog::onProfilesWatcherTimeout()
{
    if (!m_configManager)
        return;

    // Always refresh list (covers add/remove/rename)
    updateProfileList();

    // If current profile disappeared, fall back to Default or first
    const QStringList profiles = m_configManager->listProfiles();
    const QString currentName = m_configManager->currentProfileName();
    if (!profiles.contains(currentName))
    {
        const QString fallback = profiles.contains("Default") ? QStringLiteral("Default") : (profiles.isEmpty() ? QString() : profiles.first());
        if (!fallback.isEmpty() && m_configManager->loadProfile(fallback))
        {
            loadFromConfig();
            updateProfileList();
        }
        m_profilesDirDirty = false;
        m_profilesChangedFile.clear();
        return;
    }

    // If the currently selected profile file was modified externally, reload UI
    bool shouldReloadCurrent = false;
    if (!m_profilesChangedFile.isEmpty())
    {
        const QString currentPath = QDir::cleanPath(QFileInfo(m_configManager->configFilePath()).absoluteFilePath());
        const QString changedPath = QDir::cleanPath(QFileInfo(m_profilesChangedFile).absoluteFilePath());
        shouldReloadCurrent = (currentPath == changedPath);
    }

    if (shouldReloadCurrent)
    {
        if (m_configManager->loadProfile(currentName))
            loadFromConfig();
    }

    m_profilesDirDirty = false;
    m_profilesChangedFile.clear();
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
