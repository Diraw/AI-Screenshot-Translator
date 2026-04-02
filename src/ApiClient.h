#ifndef APICLIENT_H
#define APICLIENT_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QString>
#include <QByteArray>
#include <QList>
#include <QHash>
#include <QJsonValue>
#include <QPointer>
#include <QtGlobal>

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
    void processImage(const QByteArray &base64Image, const QString &promptText, const QString &requestId = QString());
    void processImages(const QList<QByteArray> &base64Images, const QString &promptText, const QString &requestId = QString());

signals:
    void success(const QString &text, const QString &originalBase64, const QString &originalPrompt,
                 const QString &requestId, qint64 elapsedMs);
    void error(const QString &errorMessage, const QString &requestId, qint64 elapsedMs);

private:
    struct RequestSettings
    {
        QString apiKey;
        QString baseUrl;
        QString endpointPath;
        QString modelName;
        ApiProvider provider = ApiProvider::OpenAI;
        bool useProxy = false;
        QString proxyUrl;
        bool useAdvancedApi = false;
        QString advancedApiTemplate;
        int requestTimeoutMs = 30000;
    };

    QString m_apiKey;
    QString m_baseUrl;
    QString m_endpointPath;
    QString m_modelName;
    ApiProvider m_provider = ApiProvider::OpenAI;
    bool m_useProxy = false;
    QString m_proxyUrl;
    bool m_useAdvancedApi = false;
    QString m_advancedApiTemplate;

    static constexpr int kDefaultRequestTimeoutMs = 30000;
    static constexpr int kMaxNetworkRetries = 1;

    RequestSettings currentRequestSettings() const;
    QNetworkAccessManager *createRequestManager(const RequestSettings &settings) const;

    // Provider-specific request formatters
    QByteArray formatOpenAIRequest(const RequestSettings &settings, const QList<QByteArray> &base64Images,
                                   const QString &prompt);
    QByteArray formatGeminiRequest(const QByteArray &base64Image, const QString &prompt);
    QByteArray formatGeminiRequest(const QList<QByteArray> &base64Images, const QString &prompt);
    QByteArray formatClaudeRequest(const RequestSettings &settings, const QList<QByteArray> &base64Images,
                                   const QString &prompt);

    // Provider-specific response parsers
    QString parseOpenAIResponse(const QJsonObject &root);
    QString parseGeminiResponse(const QJsonObject &root);
    QString parseClaudeResponse(const QJsonObject &root);

    // Provider-specific endpoint and header helpers
    QString getEndpoint(const RequestSettings &settings) const;
    void setProviderHeaders(QNetworkRequest &request, const RequestSettings &settings) const;
    void processImagesInternal(const QList<QByteArray> &base64Images, const QString &promptText,
                               const QString &requestId, int retryCount, const RequestSettings &settings,
                               qint64 requestStartMs);
    bool buildAdvancedRequest(const RequestSettings &settings, const QList<QByteArray> &base64Images,
                              const QString &promptText,
                              QNetworkRequest &request, QByteArray &payload,
                              QString &outError) const;
    void onReplyFinished(QNetworkReply *reply, const RequestSettings &settings);
    QJsonValue applyTemplateTokens(const QJsonValue &value, const QHash<QString, QString> &tokens) const;
    QString extractGenericText(const QJsonObject &root) const;
};

#endif // APICLIENT_H
