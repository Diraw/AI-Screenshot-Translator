#include "DotEnv.h"

#include <QDir>
#include <QFile>
#include <QTextStream>

static QString stripQuotes(const QString &value)
{
    QString v = value.trimmed();
    if ((v.startsWith('"') && v.endsWith('"')) || (v.startsWith('\'') && v.endsWith('\'')))
    {
        v = v.mid(1, v.size() - 2);
    }
    return v;
}

QMap<QString, QString> DotEnv::loadFile(const QString &filePath)
{
    QMap<QString, QString> result;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return result;

    QTextStream ts(&file);
    while (!ts.atEnd())
    {
        QString line = ts.readLine().trimmed();
        if (line.isEmpty())
            continue;
        if (line.startsWith('#'))
            continue;

        // Allow inline comments: KEY=VALUE # comment
        // But keep '#' inside quotes; keep it simple: split at first unquoted '#'.
        bool inSingle = false;
        bool inDouble = false;
        int commentPos = -1;
        for (int i = 0; i < line.size(); i++)
        {
            const QChar c = line.at(i);
            if (c == '\'' && !inDouble)
                inSingle = !inSingle;
            else if (c == '"' && !inSingle)
                inDouble = !inDouble;
            else if (c == '#' && !inSingle && !inDouble)
            {
                commentPos = i;
                break;
            }
        }
        if (commentPos >= 0)
        {
            line = line.left(commentPos).trimmed();
            if (line.isEmpty())
                continue;
        }

        const int eq = line.indexOf('=');
        if (eq <= 0)
            continue;

        QString key = line.left(eq).trimmed();
        QString value = line.mid(eq + 1).trimmed();
        result.insert(key, stripQuotes(value));
    }

    return result;
}

QString DotEnv::findEnvFileUpwards(const QString &startDir, int maxDepth)
{
    QDir dir(startDir);
    for (int i = 0; i <= maxDepth; i++)
    {
        const QString candidate = QDir::cleanPath(dir.absolutePath() + "/.env");
        if (QFile::exists(candidate))
            return candidate;

        if (!dir.cdUp())
            break;
    }
    return QString();
}

QString DotEnv::getValue(const QMap<QString, QString> &env, const QString &key, const QString &defaultValue)
{
    auto it = env.constFind(key);
    if (it == env.constEnd())
        return defaultValue;
    return it.value();
}
