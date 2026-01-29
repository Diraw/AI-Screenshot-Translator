#ifndef HINTPOPUP_H
#define HINTPOPUP_H

#include <QDialog>
#include <QJsonObject>

class QLabel;
class QCheckBox;
class QPushButton;

struct AppConfig;

class HintPopup : public QDialog
{
    Q_OBJECT
public:
    enum class Kind
    {
        ResultWindow,
        ArchiveWindow,
    };

    static void maybeShow(Kind kind, QWidget *parentWindow, const AppConfig &cfg);

private:
    explicit HintPopup(QWidget *parent = nullptr);

    static QJsonObject loadHintsJson();
    static QString jsonString(const QJsonObject &root, const QString &path, const QString &fallback);
    static QString prettyHotkey(QString s);
    static QString formatHintText(QString text, const AppConfig &cfg);

    static QString settingsKeyForKind(Kind kind);
    static QString jsonPathForKind(Kind kind);

    void setUiText(const QString &title, const QString &body, const QString &dontShowAgainText, const QString &closeText);
    void closeIfNonInteractive(QObject *eventTarget);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    QLabel *m_titleLabel = nullptr;
    QLabel *m_bodyLabel = nullptr;
    QCheckBox *m_dontShowAgain = nullptr;
    QPushButton *m_closeBtn = nullptr;
};

#endif // HINTPOPUP_H
