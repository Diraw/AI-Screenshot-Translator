#include "TranslationManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
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
    const QString currentBuiltin = builtinTranslation(m_currentLang, key);
    if (!currentBuiltin.isEmpty())
        return currentBuiltin;

    const auto currentIt = m_languages.constFind(m_currentLang);
    if (currentIt != m_languages.cend())
    {
        const auto translationIt = currentIt->translations.constFind(key);
        if (translationIt != currentIt->translations.cend())
            return *translationIt;
    }

    const QString enBuiltin = builtinTranslation(QStringLiteral("en"), key);
    if (!enBuiltin.isEmpty())
        return enBuiltin;

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

QString TranslationManager::builtinTranslation(const QString &lang, const QString &key) const
{
static const QHash<QString, QString> zhTranslations = {
        {QStringLiteral("proxy_placeholder"),
         QStringLiteral("例如 http://127.0.0.1:1080 或 socks5://127.0.0.1:1080")},
        {QStringLiteral("hotkey_conflict_title"), QStringLiteral("全局快捷键冲突")},
        {QStringLiteral("msg_hotkey_conflict_item"), QStringLiteral("- %1 (%2)")},
        {QStringLiteral("msg_hotkey_conflict_body"),
         QStringLiteral("以下全局快捷键注册失败：\n%1\n\n请在设置中修改快捷键，或关闭冲突的程序。")},
    };

static const QHash<QString, QString> enTranslations = {
        {QStringLiteral("proxy_placeholder"),
         QStringLiteral("For example: http://127.0.0.1:1080 or socks5://127.0.0.1:1080")},
        {QStringLiteral("hotkey_conflict_title"), QStringLiteral("Global Hotkey Conflict")},
        {QStringLiteral("msg_hotkey_conflict_item"), QStringLiteral("- %1 (%2)")},
        {QStringLiteral("msg_hotkey_conflict_body"),
         QStringLiteral("The following global hotkeys could not be registered:\n%1\n\nChange the shortcut in Settings or close the conflicting app.")},
    };

    const QHash<QString, QString> *translations = nullptr;
    if (lang == QStringLiteral("zh"))
        translations = &zhTranslations;
    else if (lang == QStringLiteral("en"))
        translations = &enTranslations;

    if (!translations)
        return QString();

    const auto it = translations->constFind(key);
    if (it == translations->cend())
        return QString();

    return *it;
}
