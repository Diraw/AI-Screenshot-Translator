#include "ConfigManager.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFileInfo>
#include <QJsonParseError>

namespace
{
QString findDefaultProfileTemplatePath()
{
    const QString baseDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        QDir::cleanPath(baseDir + "/assets/default_profile.json"),
        QDir::cleanPath(baseDir + "/../assets/default_profile.json"),
        QDir::cleanPath(baseDir + "/../../assets/default_profile.json"),
    };
    for (const auto &p : candidates)
    {
        if (QFile::exists(p))
            return p;
    }

    if (QFile::exists(QStringLiteral(":/assets/default_profile.json")))
        return QStringLiteral(":/assets/default_profile.json");

    return QString();
}

bool tryLoadDefaultProfileTemplate(QJsonObject &outRoot)
{
    const QString path = findDefaultProfileTemplatePath();
    if (path.isEmpty())
    {
        qWarning() << "[ConfigManager] default_profile.json not found. Looked in:"
                   << "<exe>/assets, <exe>/../assets, <exe>/../../assets, :/assets/default_profile.json";
        return false;
    }

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return false;
    const QByteArray data = f.readAll();
    f.close();

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (doc.isNull() || !doc.isObject() || err.error != QJsonParseError::NoError)
    {
        qWarning() << "[ConfigManager] default_profile.json parse failed:" << path
                   << "error=" << err.errorString() << "offset=" << err.offset;
        return false;
    }

    qInfo() << "[ConfigManager] Loaded default profile template:" << path
            << "source=" << (path.startsWith(":/") ? "resource" : "disk");
    outRoot = doc.object();
    return true;
}

QString normalizedAbsolutePath(const QString &path)
{
    return QDir::cleanPath(QDir::fromNativeSeparators(QFileInfo(path).absoluteFilePath()));
}
} // namespace

bool ConfigManager::tryGetProfileFilePath(const QString &name, QString *outPath) const
{
    if (outPath)
        outPath->clear();

    const QString trimmed = name.trimmed();
    if (!validateProfileName(trimmed).isEmpty())
        return false;

    const QString profilesRoot = normalizedAbsolutePath(m_profilesDir);
    const QString candidate = normalizedAbsolutePath(QDir(m_profilesDir).absoluteFilePath(trimmed + ".json"));
    const QString prefix = profilesRoot.endsWith('/') ? profilesRoot : (profilesRoot + '/');
    if (!candidate.startsWith(prefix, Qt::CaseInsensitive))
        return false;

    if (outPath)
        *outPath = candidate;
    return true;
}

ConfigManager::ConfigManager()
{
    m_appDataDir = appDataDirPath();
    m_profilesDir = m_appDataDir + "/profiles";

    QDir dir(m_profilesDir);
    if (!dir.exists())
        dir.mkpath(".");

    loadMeta();

    if (m_currentProfileName.isEmpty() || !listProfiles().contains(m_currentProfileName))
    {
        if (listProfiles().isEmpty())
        {
            m_currentProfileName = "Default";
            if (!loadProfile("Default"))
            {
                QJsonObject root;
                if (tryLoadDefaultProfileTemplate(root))
                {
                    m_config = AppConfig();
                    parseJson(root);

                    qInfo() << "[ConfigManager] default_profile.json prompt_prefix="
                            << m_config.promptText.left(80).replace("\n", "\\n");
                }
                saveConfig();
            }
        }
        else
        {
            m_currentProfileName = listProfiles().first();
            loadProfile(m_currentProfileName);
        }
    }
    else
    {
        loadProfile(m_currentProfileName);
    }
}

QStringList ConfigManager::listProfiles() const
{
    QDir dir(m_profilesDir);
    QStringList filters;
    filters << "*.json";
    QStringList files = dir.entryList(filters, QDir::Files);
    QStringList profiles;
    for (const QString &f : files)
        profiles << f.section(".json", 0, 0);
    return profiles;
}

bool ConfigManager::createProfile(const QString &name)
{
    const QString trimmedName = name.trimmed();
    if (!validateProfileName(trimmedName).isEmpty())
        return false;
    QString path;
    if (!tryGetProfileFilePath(trimmedName, &path))
        return false;
    if (QFile::exists(path))
        return false;

    if (trimmedName == "Default")
    {
        QJsonObject root;
        if (tryLoadDefaultProfileTemplate(root))
        {
            m_config = AppConfig();
            parseJson(root);
        }
        else
        {
            m_config = AppConfig();
        }
    }
    else
    {
        AppConfig defaultCfg;
        m_config = defaultCfg;
    }
    m_currentProfileName = trimmedName;
    saveConfig();
    saveMeta();
    return true;
}

bool ConfigManager::loadProfile(const QString &name)
{
    QString path;
    if (!tryGetProfileFilePath(name, &path))
        return false;
    QFile file(path);
    if (!file.exists())
        return false;

    if (!file.open(QIODevice::ReadOnly))
        return false;
    const QByteArray data = file.readAll();
    file.close();

    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull())
        return false;

    parseJson(doc.object());
    m_currentProfileName = name.trimmed();
    saveMeta();
    return true;
}

bool ConfigManager::deleteProfile(const QString &name)
{
    const QString trimmedName = name.trimmed();
    if (trimmedName == "Default")
        return false;
    QString path;
    if (!tryGetProfileFilePath(trimmedName, &path))
        return false;
    if (QFile::remove(path))
    {
        if (m_currentProfileName == trimmedName)
            loadProfile("Default");
        return true;
    }
    return false;
}

bool ConfigManager::renameProfile(const QString &oldName, const QString &newName)
{
    const QString trimmedOldName = oldName.trimmed();
    const QString trimmedNewName = newName.trimmed();
    if (trimmedOldName.isEmpty() || trimmedNewName.isEmpty() || trimmedOldName == "Default")
        return false;

    QString oldPath;
    QString newPath;
    if (!tryGetProfileFilePath(trimmedOldName, &oldPath) || !tryGetProfileFilePath(trimmedNewName, &newPath))
        return false;

    if (!QFile::exists(oldPath))
        return false;
    if (QFile::exists(newPath))
        return false;

    if (QFile::rename(oldPath, newPath))
    {
        if (m_currentProfileName == trimmedOldName)
        {
            m_currentProfileName = trimmedNewName;
            saveMeta();
        }
        return true;
    }
    return false;
}

bool ConfigManager::copyProfile(const QString &sourceName, const QString &newName)
{
    const QString trimmedSourceName = sourceName.trimmed();
    const QString trimmedNewName = newName.trimmed();
    if (trimmedSourceName.isEmpty() || trimmedNewName.isEmpty())
        return false;

    QString srcPath;
    QString destPath;
    if (!tryGetProfileFilePath(trimmedSourceName, &srcPath) || !tryGetProfileFilePath(trimmedNewName, &destPath))
        return false;

    if (!QFile::exists(srcPath))
        return false;
    if (QFile::exists(destPath))
        return false;

    return QFile::copy(srcPath, destPath);
}

bool ConfigManager::importProfile(const QString &path)
{
    QFile src(path);
    if (!src.open(QIODevice::ReadOnly))
        return false;
    const QByteArray data = src.readAll();
    src.close();

    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject())
        return false;

    const QString name = QFileInfo(path).baseName();
    QString dest = m_profilesDir + "/" + name + ".json";

    int counter = 1;
    while (QFile::exists(dest))
        dest = m_profilesDir + "/" + name + QString("_%1").arg(counter++) + ".json";

    QFile destFile(dest);
    if (!destFile.open(QIODevice::WriteOnly))
        return false;
    destFile.write(data);
    destFile.close();

    return true;
}

bool ConfigManager::exportProfile(const QString &name, const QString &destPath)
{
    QString srcPath;
    if (!tryGetProfileFilePath(name, &srcPath))
        return false;
    return QFile::copy(srcPath, destPath);
}

QString ConfigManager::currentProfileName() const
{
    return m_currentProfileName;
}

AppConfig ConfigManager::getConfig() const
{
    return m_config;
}

void ConfigManager::setConfig(const AppConfig &config)
{
    m_config = config;
    qInfo() << "[ConfigManager] setConfig received:";
    qInfo() << "  Prev Shortcut:" << m_config.prevResultShortcut;
    qInfo() << "  Next Shortcut:" << m_config.nextResultShortcut;
    saveConfig();
}

void ConfigManager::saveConfig()
{
    QString path;
    if (!tryGetProfileFilePath(m_currentProfileName, &path))
    {
        qWarning() << "Failed to save config profile: invalid profile name" << m_currentProfileName;
        return;
    }
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
    {
        qWarning() << "Failed to save config profile:" << path;
        return;
    }
    const QJsonDocument doc(toJson());
    file.write(doc.toJson());
    file.close();
}

QString ConfigManager::configFilePath() const
{
    QString path;
    return tryGetProfileFilePath(m_currentProfileName, &path) ? path : QString();
}

QString ConfigManager::profilesDirPath() const
{
    return QDir::cleanPath(m_profilesDir);
}

void ConfigManager::loadMeta()
{
    QFile file(settingsJsonPath());
    if (file.open(QIODevice::ReadOnly))
    {
        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        m_currentProfileName = doc.object()["last_profile"].toString();
    }
}

void ConfigManager::saveMeta()
{
    QFile file(settingsJsonPath());
    if (file.open(QIODevice::WriteOnly))
    {
        QJsonObject root;
        root["last_profile"] = m_currentProfileName;
        file.write(QJsonDocument(root).toJson());
    }
}
