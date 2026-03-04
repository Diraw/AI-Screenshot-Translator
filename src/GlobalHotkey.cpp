#include "GlobalHotkey.h"
#include <QCoreApplication>
#include <QDebug>

#ifdef Q_OS_WIN
#include <windows.h>
#ifndef MOD_NOREPEAT
#define MOD_NOREPEAT 0x4000
#endif
#endif

GlobalHotkey::GlobalHotkey(int id, QObject *parent)
    : QObject(parent), m_hotkeyId(id), m_isRegistered(false) {
    QCoreApplication::instance()->installNativeEventFilter(this);
}

GlobalHotkey::~GlobalHotkey() {
    unregisterHotkey();
    // QCoreApplication::instance()->removeNativeEventFilter(this); 
    // Note: It's often safer to not remove it in destructor if app is closing, 
    // but good practice if lifecycle is shorter than app.
}

bool GlobalHotkey::registerHotkey(const QString &keySequence) {
    unregisterHotkey(); // Clear any existing

    if (keySequence.isEmpty()) return false;

    UINT modifiers = 0;
    UINT vk = 0;

    if (!parseKeySequence(keySequence, modifiers, vk)) {
        qWarning() << "Failed to parse hotkey:" << keySequence;
        return false;
    }

#ifdef Q_OS_WIN
    const UINT noRepeatModifiers = modifiers | MOD_NOREPEAT;
    if (RegisterHotKey(NULL, m_hotkeyId, noRepeatModifiers, vk)) {
        m_isRegistered = true;
        qDebug() << "Registered global hotkey:" << keySequence;
        return true;
    }

    // Fallback for environments that do not support MOD_NOREPEAT.
    if (RegisterHotKey(NULL, m_hotkeyId, modifiers, vk)) {
        m_isRegistered = true;
        qDebug() << "Registered global hotkey without MOD_NOREPEAT:" << keySequence;
        return true;
    } else {
        qWarning() << "Failed to register global hotkey:" << keySequence << "Error:" << GetLastError();
        return false;
    }
#else
    qWarning() << "Global hotkeys only implemented for Windows.";
    return false;
#endif
}

void GlobalHotkey::unregisterHotkey() {
    if (m_isRegistered) {
#ifdef Q_OS_WIN
        UnregisterHotKey(NULL, m_hotkeyId);
#endif
        m_isRegistered = false;
        qDebug() << "Unregistered global hotkey";
    }
}

bool GlobalHotkey::nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) {
    Q_UNUSED(eventType);
    Q_UNUSED(result);

#ifdef Q_OS_WIN
    if (eventType == "windows_generic_MSG") {
        MSG *msg = static_cast<MSG *>(message);
        if (msg->message == WM_HOTKEY) {
            if (msg->wParam == m_hotkeyId) {
                emit activated();
                return true;
            }
        }
    }
#endif
    return false;
}

bool GlobalHotkey::parseKeySequence(const QString &sequence, unsigned int &modifiers, unsigned int &vk) {
    QKeySequence ks(sequence);
    if (ks.isEmpty()) return false;

    int keyInt = ks[0].toCombined(); // Qt 6 returns QKeyCombination which we can cast or use toCombined for int

    modifiers = 0;
    if (keyInt & Qt::ShiftModifier) modifiers |= MOD_SHIFT;
    if (keyInt & Qt::ControlModifier) modifiers |= MOD_CONTROL;
    if (keyInt & Qt::AltModifier)   modifiers |= MOD_ALT;
    if (keyInt & Qt::MetaModifier)  modifiers |= MOD_WIN;

    int key = keyInt & ~Qt::KeyboardModifierMask; // Strip modifiers

    // Mapping Qt keys to Windows VK codes is complex for all keys, 
    // here we handle common alphanumeric and function keys.
    // For a robust implementation, a full map is needed.
    
    if (key >= Qt::Key_A && key <= Qt::Key_Z) {
        vk = 'A' + (key - Qt::Key_A);
    } else if (key >= Qt::Key_0 && key <= Qt::Key_9) {
        vk = '0' + (key - Qt::Key_0);
    } else if (key >= Qt::Key_F1 && key <= Qt::Key_F12) {
        vk = VK_F1 + (key - Qt::Key_F1);
    } else {
        // Fallback or specific handling
        switch (key) {
        case Qt::Key_Space: vk = VK_SPACE; break;
        case Qt::Key_Escape: vk = VK_ESCAPE; break;
        case Qt::Key_Print: vk = VK_SNAPSHOT; break;
        case Qt::Key_Tab: vk = VK_TAB; break;
        case Qt::Key_Enter: vk = VK_RETURN; break;
        case Qt::Key_Return: vk = VK_RETURN; break;
        // Add more as needed
        case Qt::Key_S: vk = 'S'; break; // Explicitly safe
        default: 
            qWarning() << "Unsupported key code for hotkey:" << key;
            return false;
        }
    }
    
    return true;
}
