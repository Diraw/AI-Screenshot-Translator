#include "GlobalHotkey.h"
#include <QCoreApplication>
#include <QDebug>

#ifdef Q_OS_WIN
#include <windows.h>
#ifndef MOD_NOREPEAT
#define MOD_NOREPEAT 0x4000
#endif

namespace
{
bool mapQtKeyToVirtualKey(int key, bool keypad, UINT &vk)
{
    if (keypad)
    {
        if (key >= Qt::Key_0 && key <= Qt::Key_9)
        {
            vk = VK_NUMPAD0 + (key - Qt::Key_0);
            return true;
        }

        switch (key)
        {
        case Qt::Key_Plus:
            vk = VK_ADD;
            return true;
        case Qt::Key_Minus:
            vk = VK_SUBTRACT;
            return true;
        case Qt::Key_Asterisk:
            vk = VK_MULTIPLY;
            return true;
        case Qt::Key_Slash:
            vk = VK_DIVIDE;
            return true;
        case Qt::Key_Period:
        case Qt::Key_Delete:
            vk = VK_DECIMAL;
            return true;
        default:
            break;
        }
    }

    if (key >= Qt::Key_A && key <= Qt::Key_Z)
    {
        vk = 'A' + (key - Qt::Key_A);
        return true;
    }
    if (key >= Qt::Key_0 && key <= Qt::Key_9)
    {
        vk = '0' + (key - Qt::Key_0);
        return true;
    }
    if (key >= Qt::Key_F1 && key <= Qt::Key_F24)
    {
        vk = VK_F1 + (key - Qt::Key_F1);
        return true;
    }

    switch (key)
    {
    case Qt::Key_Space:
        vk = VK_SPACE;
        return true;
    case Qt::Key_Escape:
        vk = VK_ESCAPE;
        return true;
    case Qt::Key_Print:
        vk = VK_SNAPSHOT;
        return true;
    case Qt::Key_Tab:
        vk = VK_TAB;
        return true;
    case Qt::Key_Enter:
    case Qt::Key_Return:
        vk = VK_RETURN;
        return true;
    case Qt::Key_Backspace:
        vk = VK_BACK;
        return true;
    case Qt::Key_Insert:
        vk = VK_INSERT;
        return true;
    case Qt::Key_Delete:
        vk = VK_DELETE;
        return true;
    case Qt::Key_Home:
        vk = VK_HOME;
        return true;
    case Qt::Key_End:
        vk = VK_END;
        return true;
    case Qt::Key_PageUp:
        vk = VK_PRIOR;
        return true;
    case Qt::Key_PageDown:
        vk = VK_NEXT;
        return true;
    case Qt::Key_Left:
        vk = VK_LEFT;
        return true;
    case Qt::Key_Right:
        vk = VK_RIGHT;
        return true;
    case Qt::Key_Up:
        vk = VK_UP;
        return true;
    case Qt::Key_Down:
        vk = VK_DOWN;
        return true;
    case Qt::Key_Pause:
        vk = VK_PAUSE;
        return true;
    case Qt::Key_CapsLock:
        vk = VK_CAPITAL;
        return true;
    case Qt::Key_Minus:
    case Qt::Key_Underscore:
        vk = VK_OEM_MINUS;
        return true;
    case Qt::Key_Equal:
    case Qt::Key_Plus:
        vk = VK_OEM_PLUS;
        return true;
    case Qt::Key_BracketLeft:
    case Qt::Key_BraceLeft:
        vk = VK_OEM_4;
        return true;
    case Qt::Key_BracketRight:
    case Qt::Key_BraceRight:
        vk = VK_OEM_6;
        return true;
    case Qt::Key_Backslash:
    case Qt::Key_Bar:
        vk = VK_OEM_5;
        return true;
    case Qt::Key_Semicolon:
    case Qt::Key_Colon:
        vk = VK_OEM_1;
        return true;
    case Qt::Key_Apostrophe:
    case Qt::Key_QuoteDbl:
        vk = VK_OEM_7;
        return true;
    case Qt::Key_Comma:
    case Qt::Key_Less:
        vk = VK_OEM_COMMA;
        return true;
    case Qt::Key_Period:
    case Qt::Key_Greater:
        vk = VK_OEM_PERIOD;
        return true;
    case Qt::Key_Slash:
    case Qt::Key_Question:
        vk = VK_OEM_2;
        return true;
    case Qt::Key_QuoteLeft:
    case Qt::Key_AsciiTilde:
        vk = VK_OEM_3;
        return true;
    default:
        return false;
    }
}
} // namespace
#endif

GlobalHotkey::GlobalHotkey(int id, QObject *parent)
    : QObject(parent), m_hotkeyId(id), m_isRegistered(false) {
    if (QCoreApplication *app = QCoreApplication::instance()) {
        app->installNativeEventFilter(this);
        m_eventFilterInstalled = true;
    }
}

GlobalHotkey::~GlobalHotkey() {
    unregisterHotkey();
    if (m_eventFilterInstalled) {
        if (QCoreApplication *app = QCoreApplication::instance())
            app->removeNativeEventFilter(this);
        m_eventFilterInstalled = false;
    }
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

    const int keyInt = ks[0].toCombined(); // Qt 6 returns QKeyCombination which we can cast or use toCombined for int

    modifiers = 0;
    if (keyInt & Qt::ShiftModifier) modifiers |= MOD_SHIFT;
    if (keyInt & Qt::ControlModifier) modifiers |= MOD_CONTROL;
    if (keyInt & Qt::AltModifier)   modifiers |= MOD_ALT;
    if (keyInt & Qt::MetaModifier)  modifiers |= MOD_WIN;

    const bool keypad = (keyInt & Qt::KeypadModifier);
    const int key = keyInt & ~Qt::KeyboardModifierMask; // Strip modifiers

    if (!mapQtKeyToVirtualKey(key, keypad, vk)) {
        qWarning() << "Unsupported key code for hotkey:" << key;
        return false;
    }

    return true;
}
