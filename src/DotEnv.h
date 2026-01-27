#ifndef DOTENV_H
#define DOTENV_H

#include <QMap>
#include <QString>

class DotEnv
{
public:
    static QMap<QString, QString> loadFile(const QString &filePath);

    // Find .env starting from startDir, walking up parent directories.
    // Returns empty string if not found.
    static QString findEnvFileUpwards(const QString &startDir, int maxDepth = 6);

    static QString getValue(const QMap<QString, QString> &env, const QString &key, const QString &defaultValue = QString());
};

#endif // DOTENV_H
