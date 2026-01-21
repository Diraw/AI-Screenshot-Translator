#include "ScreenshotTool.h"
#include <QApplication>
#include <QScreen>
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QDebug>

ScreenshotTool::ScreenshotTool(int targetScreenIndex, QWidget *parent) : QWidget(parent), m_isSelecting(false) {
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_DeleteOnClose);
    setCursor(Qt::CrossCursor);
    
    // Default to primary screen if index is invalid
    QList<QScreen*> allScreens = QGuiApplication::screens();
    QScreen *targetScreen = QGuiApplication::primaryScreen();
    if (targetScreenIndex >= 0 && targetScreenIndex < allScreens.size()) {
        targetScreen = allScreens[targetScreenIndex];
        qDebug() << "Screenshot tool: Single screen mode, index" << targetScreenIndex << "(" << targetScreen->name() << ")";
    } else {
        qDebug() << "Screenshot tool: Defaulting to primary screen";
    }
    
    m_screensToCapture.clear();
    m_screensToCapture.append(targetScreen);
    
    // Geometry is exactly the screen logical geometry
    QRect virtualGeometry = targetScreen->geometry();
    m_globalOrigin = virtualGeometry.topLeft();
    
    // Set widget to cover this screen
    setGeometry(virtualGeometry);
    
    qDebug() << "Capture Geometry:" << virtualGeometry;
    qDebug() << "Global Origin:" << m_globalOrigin;
    qDebug() << "DPR:" << targetScreen->devicePixelRatio();

    captureScreens();
}

void ScreenshotTool::captureScreens() {
    if (m_screensToCapture.isEmpty()) return;
    
    QScreen *screen = m_screensToCapture.first();
    qreal dpr = screen->devicePixelRatio();
    
    // Grab the screen window. This returns a pixmap in physical pixels 
    // BUT WITH the DPR property set to the screen's DPR.
    m_fullCapture = screen->grabWindow(0);
    
    // CRITICAL: Ensure the DPR is set on the pixmap. Qt's grabWindow usually sets it,
    // but we'll force it just to be safe. This ensures painter.drawPixmap works correctly.
    m_fullCapture.setDevicePixelRatio(dpr);
    
    qDebug() << "Grabbed screen:" << screen->name() 
             << "Size:" << m_fullCapture.size() 
             << "DPR:" << m_fullCapture.devicePixelRatio();
}

void ScreenshotTool::showEvent(QShowEvent *event) {
    QWidget::showEvent(event);
    activateWindow();
    setFocus();
}

void ScreenshotTool::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QPainter painter(this);
    
    // Draw the full desktop capture
    // Since the widget starts at global virtualGeometry.topLeft(), 
    // local(0,0) matches the start of our m_fullCapture.
    painter.drawPixmap(0, 0, m_fullCapture);
    
    // Draw semi-transparent dim layer
    painter.fillRect(rect(), QColor(0, 0, 0, 100));
    
    // Draw selection
    if (m_isSelecting) {
        QRect r = getNormalizedRect();
        
        // Clear the selection area (redraw the background pixmap inside the clip rect)
        painter.save();
        painter.setClipRect(r);
        painter.drawPixmap(0, 0, m_fullCapture);
        painter.restore();
        
        // Draw border
        painter.setPen(QPen(Qt::white, 2));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(r);
    }
}

void ScreenshotTool::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_startPoint = event->pos();
        m_endPoint = event->pos();
        m_isSelecting = true;
        update();
    }
}

void ScreenshotTool::mouseMoveEvent(QMouseEvent *event) {
    if (m_isSelecting) {
        m_endPoint = event->pos();
        update();
    }
}

void ScreenshotTool::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton && m_isSelecting) {
        qDebug() << "ScreenshotTool: Mouse released. Selection finished.";
        m_isSelecting = false;
        m_endPoint = event->pos();
        
        QRect r = getNormalizedRect();
        if (r.width() > 10 && r.height() > 10) {
            QPixmap result = getResultPixmap(r);
            if (!result.isNull()) {
                qDebug() << "ScreenshotTool: Emitting screenshotTaken signal.";
                emit screenshotTaken(result, r);
                qDebug() << "ScreenshotTool: Signal emitted. Closing.";
                close();
            } else {
                update();
            }
        } else {
             update();
        }
    }
}

QPixmap ScreenshotTool::getResultPixmap(const QRect &selectionRect) {
    QScreen *targetScreen = QApplication::screenAt(mapToGlobal(selectionRect.center()));
    if (!targetScreen) targetScreen = QApplication::primaryScreen();
    qreal dpr = targetScreen->devicePixelRatio();
    
    QPixmap result(selectionRect.size() * dpr);
    result.setDevicePixelRatio(dpr);
    result.fill(Qt::transparent);
    
    QPainter p(&result);
    // Translate so that selectionRect.topLeft maps to (0,0) in the result
    p.translate(-selectionRect.topLeft());
    
    // Draw the full capture; it correctly covers the selectionRect because they share the same local coordinate space
    p.drawPixmap(0, 0, m_fullCapture);
    p.end();
    
    return result;
}

void ScreenshotTool::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape) {
        emit cancelled();
        close();
    }
}

QRect ScreenshotTool::getNormalizedRect() const {
    return QRect(m_startPoint, m_endPoint).normalized();
}
