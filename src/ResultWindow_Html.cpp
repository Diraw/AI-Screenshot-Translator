#include "ResultWindow.h"

#include "EmbedWebView.h"
#include "ThemeUtils.h"
#include "ColorUtils.h"

#include <QBuffer>
#include <QByteArray>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QRegularExpressionMatchIterator>

static QString makeHtmlSafeJson(const QJsonDocument &doc)
{
    QString json = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
    json.replace("<", "\\u003C");
    json.replace(">", "\\u003E");
    json.replace("&", "\\u0026");
    json.replace(QChar(0x2028), "\\u2028");
    json.replace(QChar(0x2029), "\\u2029");
    return json;
}

static QString readTextFileUtf8(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    return QString::fromUtf8(f.readAll());
}

static QByteArray readBinaryFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return f.readAll();
}

QString ResultWindow::loadTemplate()
{
    const QString path = QCoreApplication::applicationDirPath() + "/assets/templates/result_window.html";
    return readTextFileUtf8(path);
}

ResultWindow::ProtectedContent ResultWindow::protectMath(const QString &m)
{
    ProtectedContent p;
    p.text = m;
    return p;
}

void ResultWindow::setContent(const QString &markdown, const QString &originalBase64, const QStringList &originalBase64List,
                              const QString &prompt, const QString &entryId)
{
    bool sameContent = m_htmlLoaded && (entryId == m_entryId) && (markdown == m_currentMarkdown);

    m_originalBase64 = originalBase64;
    m_originalBase64List = originalBase64List;
    m_originalPrompt = prompt;
    m_entryId = entryId;
    m_currentMarkdown = markdown;

    if (sameContent)
    {
        // Avoid reloading identical content; still ensure theme is in sync
        bool isDark = ThemeUtils::isSystemDark();
        QString toggleJs = QString("document.documentElement.classList.toggle('dark-mode', %1); document.body.classList.toggle('dark-mode', %1);")
                               .arg(isDark ? "true" : "false");
        if (m_webView)
            m_webView->eval(toggleJs.toStdString());
        return;
    }

    if (m_history.isEmpty() && !entryId.isEmpty())
    {
        TranslationEntry e;
        e.id = entryId;
        e.translatedMarkdown = markdown;
        e.originalBase64 = originalBase64;
        e.originalBase64List = originalBase64List;
        e.prompt = prompt;
        m_history.append(e);
        m_currentIndex = 0;
        updateNavigation();
    }

    QJsonObject initData;
    initData["raw_md"] = markdown;
    // Provide fallbacks so empty shortcut strings won't disable key handling.
    initData["key_view"] = m_viewToggleKey.isEmpty() ? "r" : m_viewToggleKey;
    initData["key_edit"] = m_editToggleKey.isEmpty() ? "e" : m_editToggleKey;
    initData["key_screenshot"] = m_screenshotToggleKey.isEmpty() ? "s" : m_screenshotToggleKey;
    initData["key_prev"] = m_prevKey.isEmpty() ? "z" : m_prevKey;
    initData["key_next"] = m_nextKey.isEmpty() ? "x" : m_nextKey;
    initData["key_tag"] = m_tagKey.isEmpty() ? "t" : m_tagKey;
    initData["key_bold"] = m_boldKey;
    initData["key_underline"] = m_underlineKey;
    initData["key_highlight"] = m_highlightKey;
    initData["is_dark"] = ThemeUtils::isSystemDark();
    initData["show_adv_debug"] = m_config.showAdvancedDebugInResultWindow;
    initData["win_id"] = QString::number(reinterpret_cast<quintptr>(this), 16);
    initData["mark_bg"] = ColorUtils::normalizeCssColor(m_config.highlightMarkColor, "#ffeb3b");
    initData["mark_bg_dark"] = ColorUtils::normalizeCssColor(m_config.highlightMarkColorDark, "#d4af37");

    const QString initJson = makeHtmlSafeJson(QJsonDocument(initData));

    auto loadLib = [](const QString &name)
    {
        return readBinaryFile(QCoreApplication::applicationDirPath() + "/assets/libs/" + name);
    };

    auto embedFonts = [](QString css) -> QString
    {
        QString fontDir = QCoreApplication::applicationDirPath() + "/assets/libs/fonts/";
        QRegularExpression regex("url\\(fonts/([^)]+)\\)");
        QRegularExpressionMatchIterator it = regex.globalMatch(css);
        QString result = css;
        while (it.hasNext())
        {
            QRegularExpressionMatch match = it.next();
            QString filename = match.captured(1);
            QString extension = QFileInfo(filename).suffix().toLower();
            if (extension != "woff2")
            {
                result.replace(match.captured(0), "url(data:application/x-font-placeholder;base64,AAA=)");
                continue;
            }
            QString filePath = fontDir + filename;
            QByteArray fontBytes = readBinaryFile(filePath);
            if (!fontBytes.isEmpty())
            {
                QString base64 = fontBytes.toBase64();
                QString dataUri = QString("url(data:font/woff2;base64,%1)").arg(base64);
                result.replace(match.captured(0), dataUri);
            }
        }
        return result;
    };

    auto toUri = [](const QByteArray &content, const char *mime)
    {
        return QString("data:%1;charset=utf-8;base64,%2").arg(mime).arg(QString(content.toBase64()));
    };

    const QString htmlTemplate = loadTemplate();
    const QString logicPath = QCoreApplication::applicationDirPath() + "/assets/templates/result_logic.js";
    QString logic = readTextFileUtf8(logicPath);
    if (logic.isEmpty())
        logic = "/* missing assets/templates/result_logic.js */";

    QString katexCss = QString::fromUtf8(loadLib("katex.min.css"));
    katexCss = embedFonts(katexCss);

    QString out = htmlTemplate;
    if (out.isEmpty())
    {
        // If template is missing, keep behavior predictable instead of crashing.
        out = "<!DOCTYPE html><html><body><pre>Missing template: assets/templates/result_window.html</pre></body></html>";
    }

    out.replace("__CSS_HL__", toUri(loadLib("highlight.default.min.css"), "text/css"));
    out.replace("__CSS_KATEX__", toUri(katexCss.toUtf8(), "text/css"));
    out.replace("__JS_MARKED__", toUri(loadLib("marked.min.js"), "text/javascript"));
    out.replace("__JS_HL__", toUri(loadLib("highlight.min.js"), "text/javascript"));
    out.replace("__JS_KATEX__", toUri(loadLib("katex.min.js"), "text/javascript"));
    out.replace("__JS_KATEX_AUTO__", toUri(loadLib("auto-render.min.js"), "text/javascript"));
    out.replace("__FONT_SIZE__", QString::number(m_config.initialFontSize));
    out.replace("__INIT_JSON__", initJson);
    out.replace("__RESULT_LOGIC__", logic);

    if (m_webView)
    {
        if (!m_htmlLoaded)
        {
            m_webView->setHtml(out.toStdString());
            m_htmlLoaded = true;
        }
        else
        {
            externalContentUpdate(markdown);
        }
        bool isDark = ThemeUtils::isSystemDark();
        QString toggleJs = QString("document.documentElement.classList.toggle('dark-mode', %1); document.body.classList.toggle('dark-mode', %1);")
                               .arg(isDark ? "true" : "false");
        m_webView->eval(toggleJs.toStdString());
    }
}

void ResultWindow::externalContentUpdate(const QString &m)
{
    m_currentMarkdown = m;

    // Keep cached history entry in sync so paging won't resurrect stale content.
    if (!m_entryId.isEmpty())
    {
        for (int i = 0; i < m_history.size(); ++i)
        {
            if (m_history[i].id == m_entryId)
            {
                m_history[i].translatedMarkdown = m;
                break;
            }
        }
    }

    if (m_webView)
    {
        QJsonObject payload;
        payload["raw_md"] = m;
        const QString safeJson = makeHtmlSafeJson(QJsonDocument(payload));
        m_webView->eval(QString("updateContentFromNative(%1);").arg(safeJson).toStdString());
    }
}
