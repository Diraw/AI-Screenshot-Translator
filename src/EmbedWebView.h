#pragma once
#include <memory>
#include <string>
#include <functional>
#include <QObject>
#include <QElapsedTimer>
#include <QTimer>
#include <QPointer>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif

class EmbedWebView : public QObject
{
    Q_OBJECT
public:
    explicit EmbedWebView(QWidget *parent);
    ~EmbedWebView();

    void setHtml(const std::string &html);
    void eval(const std::string &js);
    void setSize(int width, int height);
    void focus();       // Add explicit focus method
    void focusNative(); // Force OS focus to underlying WebView HWND
    void setBackgroundColor(int r, int g, int b, int a = 255);

    // Support for async return values
    void resolve(const std::string &seq, int status, const std::string &result);

    using BindCallback = std::function<void(std::string, std::string, void *)>;
    void bind(const std::string &name, BindCallback fn);

    void setVisible(bool visible);
    void openDevTools();

signals:
    void ready();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void checkReady();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
    QPointer<QWidget> m_parentWidget;
#ifdef _WIN32
    HWND m_hwnd = nullptr; // Stable HWND value; pass address to WebView2
#endif

    QTimer *m_initTimer;
    QTimer *m_resizeTimer;
    std::vector<std::function<void()>> m_pendingActions;
    bool m_isReady = false;
    bool m_hasEverSetHtml = false;

    // WebView2 LostFocus 时的兜底抢回（需要节流，避免陷入 GotFocus/LostFocus 回环）
    QElapsedTimer m_lastWv2Refocus;
    bool m_wv2RefocusPending = false;

    // Impl wrappers (implemented in EmbedWebView.cpp where Impl is complete)
    void implSetHtml(const std::string &html);
    void implEval(const std::string &js);
    void implResolve(const std::string &seq, int status, const std::string &result);
    void implSetSize(int width, int height);
    void implFocus();
    void implFocusNative();
    void implSetBackgroundColor(int r, int g, int b, int a);
    void implBind(const std::string &name, BindCallback fn);
    void implSetVisible(bool visible);
    void implOpenDevTools();
};
