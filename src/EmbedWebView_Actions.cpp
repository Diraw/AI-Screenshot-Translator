#include "EmbedWebView.h"

#include <QDebug>
#include <QString>
#include <QWidget>

void EmbedWebView::setHtml(const std::string &html)
{
    static int setHtmlCount = 0;
    QWidget *p = m_parentWidget.data();
    QWidget *top = p ? p->window() : nullptr;
    qDebug() << "[Native] setHtml called" << ++setHtmlCount
             << "EmbedWebView=" << (void *)this
             << "parentWidget=" << (void *)p
             << "topWindow=" << (void *)top
             << (top ? top->windowTitle() : QString("<null>"))
             << "len=" << static_cast<int>(html.size());

    if (m_hasEverSetHtml)
    {
        qWarning() << "[Native] setHtml ignored (already loaded once)" << (void *)this;
        return;
    }
    m_hasEverSetHtml = true;

    if (!m_impl)
        return;
    if (!m_isReady)
    {
        m_pendingActions.push_back([this, html]()
                                   { implSetHtml(html); });
        return;
    }
    implSetHtml(html);
}

void EmbedWebView::eval(const std::string &js)
{
    if (!m_impl)
        return;
    if (!m_isReady)
    {
        m_pendingActions.push_back([this, js]()
                                   { implEval(js); });
        return;
    }
    implEval(js);
}

void EmbedWebView::resolve(const std::string &seq, int status, const std::string &result)
{
    if (!m_impl)
        return;
    if (!m_isReady)
    {
        m_pendingActions.push_back([this, seq, status, result]()
                                   { implResolve(seq, status, result); });
        return;
    }
    implResolve(seq, status, result);
}

void EmbedWebView::setSize(int width, int height)
{
    if (!m_impl)
        return;

    if (!m_isReady)
    {
        m_pendingActions.push_back([this, width, height]()
                                   { implSetSize(width, height); });
        return;
    }
    implSetSize(width, height);
}

void EmbedWebView::focus()
{
    if (!m_impl)
        return;
    if (!m_isReady)
    {
        m_pendingActions.push_back([this]()
                                   { implFocus(); });
        return;
    }
    implFocus();
}

void EmbedWebView::focusNative()
{
    if (!m_impl)
        return;
    if (!m_isReady)
    {
        m_pendingActions.push_back([this]()
                                   { implFocusNative(); });
        return;
    }
    implFocusNative();
}

void EmbedWebView::setBackgroundColor(int r, int g, int b, int a)
{
    if (!m_impl)
        return;
    if (!m_isReady)
    {
        m_pendingActions.push_back([this, r, g, b, a]()
                                   { implSetBackgroundColor(r, g, b, a); });
        return;
    }
    implSetBackgroundColor(r, g, b, a);
}

void EmbedWebView::bind(const std::string &name, BindCallback fn)
{
    if (!m_isReady)
    {
        m_pendingActions.push_back([this, name, fn]()
                                   { implBind(name, fn); });
        return;
    }
    implBind(name, fn);
}

void EmbedWebView::setVisible(bool visible)
{
    if (m_isReady && m_impl)
    {
        implSetVisible(visible);
    }
    else if (!m_isReady)
    {
        m_pendingActions.push_back([this, visible]()
                                   {
            if (m_impl)
                implSetVisible(visible); });
    }
}

void EmbedWebView::openDevTools()
{
#ifdef _WIN32
    if (!m_isReady)
    {
        m_pendingActions.push_back([this]()
                                   { implOpenDevTools(); });
        return;
    }
    implOpenDevTools();
#endif
}
