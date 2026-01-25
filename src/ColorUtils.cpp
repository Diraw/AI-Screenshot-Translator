#include "ColorUtils.h"

#include <QRegularExpression>
#include <QStringList>
#include <algorithm>

namespace ColorUtils
{

    static int clamp255(int v)
    {
        return std::max(0, std::min(255, v));
    }

    static double normalizeAlpha01(double a)
    {
        // Accept a as 0..1 or 0..255.
        if (a <= 1.0)
            return std::max(0.0, std::min(1.0, a));
        if (a <= 255.0)
            return std::max(0.0, std::min(1.0, a / 255.0));
        return 1.0;
    }

    QString normalizeCssColor(const QString &input, const QString &fallback)
    {
        QString s = input.trimmed();
        if (s.isEmpty())
            return fallback;

        // rgba(r,g,b,a) where a is 0..1 or 0..255
        {
            QRegularExpression re(
                "^rgba\\(\\s*(\\d{1,3})\\s*,\\s*(\\d{1,3})\\s*,\\s*(\\d{1,3})\\s*,\\s*([0-9]*\\.?[0-9]+)\\s*\\)$",
                QRegularExpression::CaseInsensitiveOption);
            auto m = re.match(s);
            if (m.hasMatch())
            {
                bool ok1 = false, ok2 = false, ok3 = false, ok4 = false;
                int r = m.captured(1).toInt(&ok1);
                int g = m.captured(2).toInt(&ok2);
                int b = m.captured(3).toInt(&ok3);
                double a = m.captured(4).toDouble(&ok4);
                if (!(ok1 && ok2 && ok3 && ok4))
                    return fallback;
                const double a01 = normalizeAlpha01(a);
                return QString("rgba(%1,%2,%3,%4)")
                    .arg(clamp255(r))
                    .arg(clamp255(g))
                    .arg(clamp255(b))
                    .arg(a01, 0, 'f', 3);
            }
        }

        // rgb(r,g,b)
        {
            QRegularExpression re(
                "^rgb\\(\\s*(\\d{1,3})\\s*,\\s*(\\d{1,3})\\s*,\\s*(\\d{1,3})\\s*\\)$",
                QRegularExpression::CaseInsensitiveOption);
            auto m = re.match(s);
            if (m.hasMatch())
            {
                bool ok1 = false, ok2 = false, ok3 = false;
                int r = m.captured(1).toInt(&ok1);
                int g = m.captured(2).toInt(&ok2);
                int b = m.captured(3).toInt(&ok3);
                if (!(ok1 && ok2 && ok3))
                    return fallback;
                return QString("rgb(%1,%2,%3)")
                    .arg(clamp255(r))
                    .arg(clamp255(g))
                    .arg(clamp255(b));
            }
        }

        // r,g,b or r,g,b,a (a is 0..1 or 0..255)
        {
            QStringList parts = s.split(',', Qt::SkipEmptyParts);
            if (parts.size() == 3 || parts.size() == 4)
            {
                bool ok1 = false, ok2 = false, ok3 = false;
                int r = parts[0].trimmed().toInt(&ok1);
                int g = parts[1].trimmed().toInt(&ok2);
                int b = parts[2].trimmed().toInt(&ok3);
                if (!(ok1 && ok2 && ok3))
                    return fallback;

                if (parts.size() == 3)
                {
                    return QString("rgb(%1,%2,%3)")
                        .arg(clamp255(r))
                        .arg(clamp255(g))
                        .arg(clamp255(b));
                }

                bool ok4 = false;
                double a = parts[3].trimmed().toDouble(&ok4);
                if (!ok4)
                    return fallback;
                const double a01 = normalizeAlpha01(a);
                return QString("rgba(%1,%2,%3,%4)")
                    .arg(clamp255(r))
                    .arg(clamp255(g))
                    .arg(clamp255(b))
                    .arg(a01, 0, 'f', 3);
            }
        }

        // #RGB or #RRGGBB
        {
            QRegularExpression re("^#([0-9a-f]{3}|[0-9a-f]{6})$", QRegularExpression::CaseInsensitiveOption);
            if (re.match(s).hasMatch())
                return s;
        }

        // #RRGGBBAA -> convert to rgba(...)
        {
            QRegularExpression re("^#([0-9a-f]{8})$", QRegularExpression::CaseInsensitiveOption);
            auto m = re.match(s);
            if (m.hasMatch())
            {
                const QString hex = m.captured(1);
                bool ok = false;
                const int r = hex.mid(0, 2).toInt(&ok, 16);
                if (!ok)
                    return fallback;
                const int g = hex.mid(2, 2).toInt(&ok, 16);
                if (!ok)
                    return fallback;
                const int b = hex.mid(4, 2).toInt(&ok, 16);
                if (!ok)
                    return fallback;
                const int a255 = hex.mid(6, 2).toInt(&ok, 16);
                if (!ok)
                    return fallback;

                const double a01 = normalizeAlpha01(static_cast<double>(a255));
                return QString("rgba(%1,%2,%3,%4)")
                    .arg(clamp255(r))
                    .arg(clamp255(g))
                    .arg(clamp255(b))
                    .arg(a01, 0, 'f', 3);
            }
        }

        return fallback;
    }

} // namespace ColorUtils
