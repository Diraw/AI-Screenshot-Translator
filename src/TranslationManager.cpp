#include "TranslationManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

namespace
{

QStringList translationSearchPaths()
{
    const QString baseDir = QCoreApplication::applicationDirPath();
    return {
        QDir::cleanPath(baseDir + "/assets/translations"),
        QDir::cleanPath(baseDir + "/../assets/translations"),
        QDir::cleanPath(baseDir + "/../../assets/translations"),
    };
}

} // namespace

TranslationManager &TranslationManager::instance()
{
    static TranslationManager instance;
    return instance;
}

TranslationManager::TranslationManager()
    : m_currentLang(QStringLiteral("zh"))
{
    for (const QString &dirPath : translationSearchPaths())
    {
        QDir dir(dirPath);
        if (!dir.exists())
            continue;

        const QFileInfoList files = dir.entryInfoList(QStringList() << "*.json", QDir::Files, QDir::Name);
        for (const QFileInfo &fileInfo : files)
        {
            QFile file(fileInfo.absoluteFilePath());
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
                continue;

            QJsonParseError error{};
            const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
            if (error.error != QJsonParseError::NoError || !doc.isObject())
                continue;

            const QJsonObject root = doc.object();
            const QString displayName = root.value(QStringLiteral("display_name")).toString(fileInfo.completeBaseName());
            const QJsonObject translationsObject = root.value(QStringLiteral("translations")).toObject();
            if (translationsObject.isEmpty())
                continue;

            TranslationMap translations;
            for (auto it = translationsObject.begin(); it != translationsObject.end(); ++it)
            {
                if (it.value().isString())
                    translations.insert(it.key(), it.value().toString());
            }

            if (!translations.isEmpty())
                registerLanguage(fileInfo.completeBaseName(), displayName, translations);
        }

        if (!m_languageOrder.isEmpty())
            break;
    }

    if (!m_languages.contains(m_currentLang) && !m_languageOrder.isEmpty())
        m_currentLang = m_languageOrder.first();
}

void TranslationManager::setLanguage(const QString &lang)
{
    if (m_languages.contains(lang))
        m_currentLang = lang;
}

QString TranslationManager::getLanguage() const
{
    return m_currentLang;
}

QString TranslationManager::tr(const QString &key) const
{
    const auto currentIt = m_languages.constFind(m_currentLang);
    if (currentIt != m_languages.cend())
    {
        const auto translationIt = currentIt->translations.constFind(key);
        if (translationIt != currentIt->translations.cend())
            return *translationIt;
    }

    const auto enIt = m_languages.constFind(QStringLiteral("en"));
    if (enIt != m_languages.cend())
    {
        const auto translationIt = enIt->translations.constFind(key);
        if (translationIt != enIt->translations.cend())
            return *translationIt;
    }

    return key;
}

QStringList TranslationManager::availableLanguages() const
{
    return m_languageOrder;
}

QString TranslationManager::languageDisplayName(const QString &lang) const
{
    const auto it = m_languages.constFind(lang);
    if (it == m_languages.cend() || it->displayName.isEmpty())
        return lang;
    return it->displayName;
}

void TranslationManager::registerLanguage(const QString &lang, const QString &displayName, const TranslationMap &translations)
{
    if (!m_languages.contains(lang))
        m_languageOrder << lang;

    LanguagePack pack;
    pack.displayName = displayName;
    pack.translations = translations;
    m_languages.insert(lang, pack);
}
