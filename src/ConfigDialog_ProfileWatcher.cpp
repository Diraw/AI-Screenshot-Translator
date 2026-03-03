#include "ConfigDialog.h"

#include <QDir>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QTimer>

void ConfigDialog::updateProfileList()
{
    m_profileList->blockSignals(true);
    m_profileList->clear();
    m_profileList->addItems(m_configManager->listProfiles());

    const QString current = m_configManager->currentProfileName();
    const QList<QListWidgetItem *> items = m_profileList->findItems(current, Qt::MatchExactly);
    if (!items.isEmpty())
        m_profileList->setCurrentItem(items.first());

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
        m_profilesWatcher->addPath(profilesDir);

    QDir dir(profilesDir);
    QStringList desiredFiles;
    if (dir.exists())
    {
        const QStringList files = dir.entryList(QStringList() << "*.json", QDir::Files);
        desiredFiles.reserve(files.size());
        for (const QString &fileName : files)
            desiredFiles << QDir::cleanPath(dir.filePath(fileName));
    }

    const QStringList currentFiles = m_profilesWatcher->files();
    QStringList toRemove;
    for (const QString &path : currentFiles)
    {
        if (!desiredFiles.contains(QDir::cleanPath(path)))
            toRemove << path;
    }
    if (!toRemove.isEmpty())
        m_profilesWatcher->removePaths(toRemove);

    QStringList toAdd;
    for (const QString &path : desiredFiles)
    {
        if (!currentFiles.contains(path))
            toAdd << path;
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

    updateProfileList();

    const QStringList profiles = m_configManager->listProfiles();
    const QString currentName = m_configManager->currentProfileName();
    if (!profiles.contains(currentName))
    {
        const QString fallback = profiles.contains("Default")
                                     ? QStringLiteral("Default")
                                     : (profiles.isEmpty() ? QString() : profiles.first());
        if (!fallback.isEmpty() && m_configManager->loadProfile(fallback))
        {
            loadFromConfig();
            updateProfileList();
        }
        m_profilesDirDirty = false;
        m_profilesChangedFile.clear();
        return;
    }

    bool shouldReloadCurrent = false;
    if (!m_profilesChangedFile.isEmpty())
    {
        const QString currentPath = QDir::cleanPath(QFileInfo(m_configManager->configFilePath()).absoluteFilePath());
        const QString changedPath = QDir::cleanPath(QFileInfo(m_profilesChangedFile).absoluteFilePath());
        shouldReloadCurrent = (currentPath == changedPath);
    }

    if (shouldReloadCurrent && m_configManager->loadProfile(currentName))
        loadFromConfig();

    m_profilesDirDirty = false;
    m_profilesChangedFile.clear();
}
