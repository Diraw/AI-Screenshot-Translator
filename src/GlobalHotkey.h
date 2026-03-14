#ifndef GLOBALHOTKEY_H
#define GLOBALHOTKEY_H

#include <QObject>
#include <QAbstractNativeEventFilter>
#include <QString>
#include <QKeySequence>

class GlobalHotkey : public QObject, public QAbstractNativeEventFilter {
    Q_OBJECT
public:
    explicit GlobalHotkey(int id, QObject *parent = nullptr);
    ~GlobalHotkey();

    bool registerHotkey(const QString &keySequence);
    void unregisterHotkey();

signals:
    void activated();

protected:
    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;

private:
    int m_hotkeyId;
    bool m_isRegistered;
    bool m_eventFilterInstalled = false;

    // Helper to parse key sequence to Windows modifiers and VK code
    bool parseKeySequence(const QString &sequence, unsigned int &modifiers, unsigned int &vk);
};

#endif // GLOBALHOTKEY_H
