#include "HotkeyEdit.h"
#include <QDebug>

HotkeyEdit::HotkeyEdit(QWidget *parent) : QLineEdit(parent) {
    setPlaceholderText("Press key combination (or Del to clear)");
    setReadOnly(true); // Don't allow normal typing
}

void HotkeyEdit::keyPressEvent(QKeyEvent *event) {
    int key = event->key();
    
    // Ignore standalone modifiers
    if (key == Qt::Key_Control || key == Qt::Key_Shift || 
        key == Qt::Key_Alt || key == Qt::Key_Meta) {
        return;
    }
    
    // Clear on Delete or Backspace
    if (key == Qt::Key_Delete || key == Qt::Key_Backspace) {
        clear();
        return;
    }
    
    // Construct key sequence
    Qt::KeyboardModifiers modifiers = event->modifiers();
    
    // Qt 6 standard way or fallback to int combination
    QKeySequence seq(modifiers | static_cast<Qt::Key>(key));
    
    setText(seq.toString(QKeySequence::PortableText).toLower());
}
