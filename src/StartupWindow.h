#ifndef STARTUPWINDOW_H
#define STARTUPWINDOW_H

#include <QDialog>
#include <QPointer>
#include <QJsonObject>

#include "ConfigManager.h"

class QLabel;
class QPushButton;
class QNetworkAccessManager;
class QNetworkReply;
class QEvent;
class QKeyEvent;

class StartupWindow : public QDialog
{
    Q_OBJECT
public:
    explicit StartupWindow(const AppConfig &cfg, QWidget *parent = nullptr);

private slots:
    void startUpdateCheck(bool forceNetwork = false);
    void onUpdateReplyFinished();
    void openReleasesPage();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    void closeIfNonInteractive(QObject *eventTarget);
    void setUpdateStatus(const QString &text);
    void updateHintColor();

    bool applyCachedUpdateStatusIfFresh();
    void saveUpdateCache(const QString &status, const QString &latestVer = QString(), const QString &latestUrl = QString());

    void loadUiConfig();
    QString formatText(QString text) const;
    QString uiString(const QString &path, const QString &fallback) const;

    AppConfig m_cfg;

    // UI config (loaded from assets/startup_window.json)
    QJsonObject m_uiConfig;
    QString m_currentVersion;
    QString m_updateApiUrl;
    QString m_repoUrl;
    QString m_updateNotChecked;
    QString m_updateChecking;
    QString m_updateNetworkError;
    QString m_updateParseError;
    QString m_updateNoVersion;
    QString m_updateLatestTpl;
    QString m_updateNewTpl;

    QLabel *m_titleLabel = nullptr;
    QLabel *m_versionLabel = nullptr;
    QLabel *m_copyrightLabel = nullptr;
    QLabel *m_hotkeysLabel = nullptr;
    QLabel *m_hintLabel = nullptr;

    bool m_updatingHintColor = false;

    QLabel *m_updateStatusLabel = nullptr;
    QPushButton *m_checkUpdateBtn = nullptr;
    QPushButton *m_openReleasesBtn = nullptr;

    QPointer<QNetworkAccessManager> m_nam;
    QPointer<QNetworkReply> m_reply;

    QString m_latestTag;
    QString m_latestUrl;
};

#endif // STARTUPWINDOW_H
