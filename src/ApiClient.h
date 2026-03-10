#ifndef APICLIENT_H
#define APICLIENT_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QString>
#include <QByteArray>
#include <QHash>
#include <QJsonValue>
#include <QSet>

enum class ApiProvider
{
    OpenAI,
    Gemini,
    Claude
};

class ApiClient : public QObject
{
    Q_OBJECT
public:
    explicit ApiClient(QObject *parent = nullptr);
    ~ApiClient();

    // Setup connection details
    void configure(const QString &apiKey, const QString &baseUrl, const QString &modelName,
                   ApiProvider provider, bool useProxy, const QString &proxyUrl = QString(), const QString &endpointPath = QString(),
                   bool useAdvancedApi = false, const QString &advancedApiTemplate = QString());

    // Main action
    void processImage(const QByteArray &base64Image, const QString &promptText, void *context = nullptr);

signals:
    void success(const QString &text, const QString &originalBase64, const QString &originalPrompt, void *context);
    void error(const QString &errorMessage, void *context);

private slots:
    void onReplyFinished(QNetworkReply *reply);

private:
    QNetworkAccessManager *m_manager;
    QString m_apiKey;
    QString m_baseUrl;
    QString m_endpointPath;
    QString m_modelName;
    ApiProvider m_provider = ApiProvider::OpenAI;
    bool m_useProxy = false;
    QString m_proxyUrl;
    bool m_useAdvancedApi = false;
    QString m_advancedApiTemplate;

    // Retry logic tracking (to prevent infinite loops)
    // We store the context in a set to track if a specific request has already retried
    QSet<void *> m_retriedContexts;

    // Provider-specific request formatters
    QByteArray formatOpenAIRequest(const QByteArray &base64Image, const QString &prompt);
    QByteArray formatGeminiRequest(const QByteArray &base64Image, const QString &prompt);
    QByteArray formatClaudeRequest(const QByteArray &base64Image, const QString &prompt);

    // Provider-specific response parsers
    QString parseOpenAIResponse(const QJsonObject &root);
    QString parseGeminiResponse(const QJsonObject &root);
    QString parseClaudeResponse(const QJsonObject &root);

    // Provider-specific endpoint and header helpers
    QString getEndpoint() const;
    void setProviderHeaders(QNetworkRequest &request) const;
    bool buildAdvancedRequest(const QByteArray &base64Image, const QString &promptText,
                              QNetworkRequest &request, QByteArray &payload,
                              QString &outError) const;
    QJsonValue applyTemplateTokens(const QJsonValue &value, const QHash<QString, QString> &tokens) const;
    QString extractGenericText(const QJsonObject &root) const;
};

#endif // APICLIENT_H
