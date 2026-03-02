#ifndef ANALYTICSMANAGER_H
#define ANALYTICSMANAGER_H

#include <QObject>
#include <QElapsedTimer>
#include <QTimer>

#include "UmamiClient.h"

class AnalyticsManager : public QObject
{
    Q_OBJECT
public:
    explicit AnalyticsManager(QObject *parent = nullptr);

    bool isUserEnabled() const { return m_enabled; }
    void setEnabled(bool enabled);
    // Starts after delayMs to avoid startup stalls.
    void startDelayed(int delayMs);
    void stop();

private:
    void startNow();
    void sendStart();
    void sendHeartbeat();
    void sendEnd();

    UmamiConfig loadConfig();
    QString loadOrCreateDistinctId();

    UmamiClient *m_client = nullptr;

    QTimer m_startDelayTimer;
    QTimer m_heartbeatTimer;
    QElapsedTimer m_uptime;

    bool m_started = false;
    bool m_enabled = true;
};

#endif // ANALYTICSMANAGER_H
