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
         QStringLiteral("\u4f8b\u5982 http://127.0.0.1:1080 \u6216 socks5://127.0.0.1:1080")},
        {QStringLiteral("hotkey_conflict_title"), QStringLiteral("\u5168\u5c40\u5feb\u6377\u952e\u51b2\u7a81")},
        {QStringLiteral("msg_hotkey_conflict_item"), QStringLiteral("- %1 (%2)")},
        {QStringLiteral("msg_hotkey_conflict_body"),
         QStringLiteral("\u4ee5\u4e0b\u5168\u5c40\u5feb\u6377\u952e\u6ce8\u518c\u5931\u8d25\uff1a\n%1\n\n\u8bf7\u5728\u8bbe\u7f6e\u4e2d\u4fee\u6539\u5feb\u6377\u952e\uff0c\u6216\u5173\u95ed\u51b2\u7a81\u7684\u7a0b\u5e8f\u3002")},
        {QStringLiteral("shottool_batch_mode"), QStringLiteral("\u6279\u91cf\u6a21\u5f0f")},
        {QStringLiteral("shottool_single_mode"), QStringLiteral("\u5355\u5f20\u6a21\u5f0f")},
        {QStringLiteral("shottool_stashed_count"), QStringLiteral("\u5df2\u6682\u5b58 %1 \u5f20")},
        {QStringLiteral("shottool_current_is_last"), QStringLiteral("\u672c\u5f20\u4e3a\u6700\u540e\u4e00\u5f20")},
        {QStringLiteral("shottool_mark_last"), QStringLiteral("\u6807\u8bb0\u6700\u540e\u4e00\u5f20")},
        {QStringLiteral("shottool_toggle_batch"), QStringLiteral("\u5207\u6362\u6279\u91cf\u6a21\u5f0f")},
        {QStringLiteral("shottool_clear_pending"), QStringLiteral("\u6e05\u7a7a\u5df2\u6682\u5b58\u6279\u91cf")},
        {QStringLiteral("shottool_cancel_current"), QStringLiteral("\u53d6\u6d88\u5f53\u524d\u622a\u56fe")},
        {QStringLiteral("shottool_hold_preview_last"), QStringLiteral("\u6309\u4f4f\u663e\u793a\u4e0a\u4e00\u5f20\u622a\u56fe")},
        {QStringLiteral("shottool_enter_translate"), QStringLiteral("\u76f4\u63a5\u53d1\u8d77\u7ffb\u8bd1")},
    };

    static const QHash<QString, QString> enTranslations = {
        {QStringLiteral("proxy_placeholder"),
         QStringLiteral("For example: http://127.0.0.1:1080 or socks5://127.0.0.1:1080")},
        {QStringLiteral("hotkey_conflict_title"), QStringLiteral("Global Hotkey Conflict")},
        {QStringLiteral("msg_hotkey_conflict_item"), QStringLiteral("- %1 (%2)")},
        {QStringLiteral("msg_hotkey_conflict_body"),
         QStringLiteral("The following global hotkeys could not be registered:\n%1\n\nChange the shortcut in Settings or close the conflicting app.")},
        {QStringLiteral("shottool_batch_mode"), QStringLiteral("Batch Mode")},
        {QStringLiteral("shottool_single_mode"), QStringLiteral("Single Mode")},
        {QStringLiteral("shottool_stashed_count"), QStringLiteral("%1 shots stashed")},
        {QStringLiteral("shottool_current_is_last"), QStringLiteral("This shot is the last one")},
        {QStringLiteral("shottool_mark_last"), QStringLiteral("Mark as last shot")},
        {QStringLiteral("shottool_toggle_batch"), QStringLiteral("Toggle batch mode")},
        {QStringLiteral("shottool_clear_pending"), QStringLiteral("Clear pending batch")},
        {QStringLiteral("shottool_cancel_current"), QStringLiteral("Cancel current screenshot")},
        {QStringLiteral("shottool_hold_preview_last"), QStringLiteral("Hold to preview last shot")},
        {QStringLiteral("shottool_enter_translate"), QStringLiteral("Translate immediately")},
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
