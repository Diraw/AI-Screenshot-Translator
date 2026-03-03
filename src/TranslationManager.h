#ifndef TRANSLATIONMANAGER_H
#define TRANSLATIONMANAGER_H

#include <QMap>
#include <QString>
#include <QStringList>

class TranslationManager
{
public:
    using TranslationMap = QMap<QString, QString>;

    static TranslationManager &instance();

    void setLanguage(const QString &lang);
    QString getLanguage() const;
    QString tr(const QString &key) const;

    QStringList availableLanguages() const;
    QString languageDisplayName(const QString &lang) const;

private:
    struct LanguagePack
    {
        QString displayName;
        TranslationMap translations;
    };

    TranslationManager();

    void registerLanguage(const QString &lang, const QString &displayName, const TranslationMap &translations);

    QString m_currentLang;
    QMap<QString, LanguagePack> m_languages;
    QStringList m_languageOrder;
};

#endif // TRANSLATIONMANAGER_H
