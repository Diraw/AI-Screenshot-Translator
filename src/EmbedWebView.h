#pragma once
#include <memory>
#include <string>
#include <functional>
#include <QObject>
#include <QTimer>
#include <QPointer>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif

class EmbedWebView : public QObject {
    Q_OBJECT
public:
    explicit EmbedWebView(QWidget* parent);
    ~EmbedWebView();

    void setHtml(const std::string& html);
    void eval(const std::string& js);
    void setSize(int width, int height); 
    void focus(); // Add explicit focus method
    void setBackgroundColor(int r, int g, int b, int a = 255);

    // Support for async return values
    void resolve(const std::string& seq, int status, const std::string& result); 
    
    using BindCallback = std::function<void(std::string, std::string, void*)>;
    void bind(const std::string& name, BindCallback fn);
    
    void setVisible(bool visible);
    void openDevTools();

signals:
    void ready();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void checkReady();

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
    QPointer<QWidget> m_parentWidget;
#ifdef _WIN32
    HWND m_hwnd = nullptr; // Stable HWND value; pass address to WebView2
#endif
    
    QTimer* m_initTimer;
    QTimer* m_resizeTimer;
    std::vector<std::function<void()>> m_pendingActions;
    bool m_isReady = false;
};
