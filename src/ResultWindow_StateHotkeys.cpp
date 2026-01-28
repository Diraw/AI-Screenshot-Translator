#include "ResultWindow.h"

#include "EmbedWebView.h"
#include "HistoryManager.h"
#include "TagDialog.h"

#include <QKeySequence>
#include <QShortcut>

void ResultWindow::configureHotkeys(const QString &v, const QString &e, const QString &s, const QString &b, const QString &u, const QString &h, const QString &p, const QString &n, const QString &t)
{
    m_viewToggleKey = v;
    m_editToggleKey = e;
    m_screenshotToggleKey = s;
    m_boldKey = b;
    m_underlineKey = u;
    m_highlightKey = h;
    m_prevKey = p;
    m_nextKey = n;
    m_tagKey = t;

    qDeleteAll(m_navShortcuts);
    m_navShortcuts.clear();

    auto add = [this](const QString &k, std::function<void()> f, Qt::ShortcutContext c = Qt::WindowShortcut)
    {
        if (k.isEmpty())
            return;
        QShortcut *sc = new QShortcut(QKeySequence(k), this);
        sc->setContext(c);
        connect(sc, &QShortcut::activated, this, f);
        m_navShortcuts.append(sc);
    };

    add(m_viewToggleKey, [this]()
        {
            if (m_webView)
                m_webView->eval("toggleSource();"); }, Qt::ApplicationShortcut);

    add(m_editToggleKey, [this]()
        {
            if (m_webView)
                m_webView->eval("toggleEdit();"); }, Qt::ApplicationShortcut);

    add(m_screenshotToggleKey, [this]()
        {
            if (m_webView)
                m_webView->eval("toggleScreenshot();"); }, Qt::ApplicationShortcut);

    add(m_boldKey, [this]()
        {
            if (m_webView)
                m_webView->eval("applyFormat('bold');"); });

    add(m_underlineKey, [this]()
        {
            if (m_webView)
                m_webView->eval("applyFormat('underline');"); });

    add(m_highlightKey, [this]()
        {
            if (m_webView)
                m_webView->eval("applyFormat('highlight');"); });

    add(m_prevKey, [this]()
        { showPrevious(); }, Qt::ApplicationShortcut);
    add(m_nextKey, [this]()
        { showNext(); }, Qt::ApplicationShortcut);
    add(m_tagKey, [this]()
        { openTagDialog(); }, Qt::ApplicationShortcut);
}

void ResultWindow::addEntry(const TranslationEntry &entry)
{
    m_history.append(entry);
    m_currentIndex = m_history.size() - 1;
    updateNavigation();
    setContent(entry.translatedMarkdown, entry.originalBase64, entry.prompt, entry.id);
}

void ResultWindow::showPrevious()
{
    if (m_currentIndex > 0)
    {
        m_currentIndex--;
        TranslationEntry e = m_history[m_currentIndex];
        // Refresh from history manager to avoid stale cached content (e.g. "Processing...").
        if (m_historyManager && !e.id.isEmpty())
        {
            TranslationEntry fresh = m_historyManager->getEntryById(e.id);
            if (!fresh.id.isEmpty())
            {
                m_history[m_currentIndex] = fresh;
                e = fresh;
            }
        }
        setContent(e.translatedMarkdown, e.originalBase64, e.prompt, e.id);
        updateNavigation();
    }
}

void ResultWindow::showNext()
{
    if (m_currentIndex < m_history.size() - 1)
    {
        m_currentIndex++;
        TranslationEntry e = m_history[m_currentIndex];
        // Refresh from history manager to avoid stale cached content (e.g. "Processing...").
        if (m_historyManager && !e.id.isEmpty())
        {
            TranslationEntry fresh = m_historyManager->getEntryById(e.id);
            if (!fresh.id.isEmpty())
            {
                m_history[m_currentIndex] = fresh;
                e = fresh;
            }
        }
        setContent(e.translatedMarkdown, e.originalBase64, e.prompt, e.id);
        updateNavigation();
    }
}

void ResultWindow::updateNavigation()
{
    m_prevAction->setEnabled(m_currentIndex > 0);
    m_nextAction->setEnabled(m_currentIndex < m_history.size() - 1);
    m_pageLabel->setText(QString(" %1 / %2 ").arg(m_currentIndex + 1).arg(m_history.size()));
    updateToolbarResponsive();
}

void ResultWindow::setHistoryManager(HistoryManager *h)
{
    m_historyManager = h;
}

void ResultWindow::openTagDialog()
{
    if (!m_historyManager || m_entryId.isEmpty())
        return;
    QStringList allTags = m_historyManager->getAllTags();
    TagDialog *dialog = new TagDialog(allTags, m_currentTags, this);
    connect(dialog, &TagDialog::tagsUpdated, this, [this](const QStringList &tags)
            {
                m_currentTags = tags;
                emit tagsUpdated(m_entryId, tags); });
    dialog->exec();
    dialog->deleteLater();
}

void ResultWindow::updateShortcuts() {}
