#include "ConfigDialog.h"

#include <QColor>
#include <QRegularExpression>

bool ConfigDialog::tryParseColorText(QString text, QColor &out) const
{
    text = text.trimmed();
    if (text.isEmpty())
        return false;

    if (text.startsWith('#'))
    {
        QString hex = text.mid(1).trimmed();
        if (hex.size() == 3)
        {
            auto h = [&](int i)
            { return QString(hex[i]) + QString(hex[i]); };
            bool okR = false, okG = false, okB = false;
            int r = h(0).toInt(&okR, 16);
            int g = h(1).toInt(&okG, 16);
            int b = h(2).toInt(&okB, 16);
            if (!okR || !okG || !okB)
                return false;
            out = QColor(r, g, b);
            return out.isValid();
        }
        if (hex.size() == 6 || hex.size() == 8)
        {
            bool ok = false;
            int r = hex.mid(0, 2).toInt(&ok, 16);
            if (!ok)
                return false;
            int g = hex.mid(2, 2).toInt(&ok, 16);
            if (!ok)
                return false;
            int b = hex.mid(4, 2).toInt(&ok, 16);
            if (!ok)
                return false;
            int a = 255;
            if (hex.size() == 8)
            {
                a = hex.mid(6, 2).toInt(&ok, 16);
                if (!ok)
                    return false;
            }
            out = QColor(r, g, b, a);
            return out.isValid();
        }
        return false;
    }

    {
        static const QRegularExpression reRgb(
            R"(^\s*rgba?\s*\(\s*([^\)]*)\s*\)\s*$)",
            QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch m = reRgb.match(text);
        if (m.hasMatch())
        {
            const QString inside = m.captured(1);
            QStringList parts = inside.split(',', Qt::SkipEmptyParts);
            for (auto &p : parts)
                p = p.trimmed();

            if (parts.size() < 3)
                return false;
            bool okR = false, okG = false, okB = false;
            int r = parts[0].toInt(&okR);
            int g = parts[1].toInt(&okG);
            int b = parts[2].toInt(&okB);
            if (!okR || !okG || !okB)
                return false;

            int a = 255;
            if (parts.size() >= 4)
            {
                bool okAInt = false;
                int aInt = parts[3].toInt(&okAInt);
                if (okAInt)
                {
                    a = qBound(0, aInt, 255);
                }
                else
                {
                    bool okAF = false;
                    double af = parts[3].toDouble(&okAF);
                    if (!okAF)
                        return false;
                    if (af <= 1.0)
                        a = qBound(0, (int)qRound(af * 255.0), 255);
                    else
                        a = qBound(0, (int)qRound(af), 255);
                }
            }
            out = QColor(qBound(0, r, 255), qBound(0, g, 255), qBound(0, b, 255), a);
            return out.isValid();
        }
    }

    if (text.contains(','))
    {
        QStringList parts = text.split(',', Qt::SkipEmptyParts);
        for (auto &p : parts)
            p = p.trimmed();

        if (parts.size() < 3)
            return false;
        bool okR = false, okG = false, okB = false;
        int r = parts[0].toInt(&okR);
        int g = parts[1].toInt(&okG);
        int b = parts[2].toInt(&okB);
        if (!okR || !okG || !okB)
            return false;

        int a = 255;
        if (parts.size() >= 4)
        {
            bool okAInt = false;
            int aInt = parts[3].toInt(&okAInt);
            if (okAInt)
            {
                a = qBound(0, aInt, 255);
            }
            else
            {
                bool okAF = false;
                double af = parts[3].toDouble(&okAF);
                if (!okAF)
                    return false;
                if (af <= 1.0)
                    a = qBound(0, (int)qRound(af * 255.0), 255);
                else
                    a = qBound(0, (int)qRound(af), 255);
            }
        }
        out = QColor(qBound(0, r, 255), qBound(0, g, 255), qBound(0, b, 255), a);
        return out.isValid();
    }

    QColor c;
    c.setNamedColor(text);
    if (!c.isValid())
        return false;
    out = c;
    return true;
}

void ConfigDialog::updateColorPreviewLabel(QLabel *label, const QString &text) const
{
    if (!label)
        return;

    QColor c;
    if (!tryParseColorText(text, c))
    {
        label->setToolTip(QString());
        label->setStyleSheet("QLabel{background: transparent; border: 1px solid rgba(127,127,127,120); border-radius: 3px;}");
        return;
    }

    label->setToolTip(QString("rgba(%1,%2,%3,%4)").arg(c.red()).arg(c.green()).arg(c.blue()).arg(c.alpha()));
    label->setStyleSheet(QString(
                             "QLabel{background-color: rgba(%1,%2,%3,%4); border: 1px solid rgba(127,127,127,160); border-radius: 3px;}")
                             .arg(c.red())
                             .arg(c.green())
                             .arg(c.blue())
                             .arg(c.alpha()));
}
