#include "ScreenshotTool.h"
#include <QApplication>
#include <QScreen>
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QDebug>
#include <QFont>
#include <QFontMetrics>
#include <QPainterPath>

namespace
{
QString normalizeHotkey(QString key)
{
    key = key.trimmed().toLower();
    key.replace(" ", "");
    return key;
}
}

ScreenshotTool::ScreenshotTool(int targetScreenIndex, bool batchModeActive, int pendingBatchCount,
                               const QString &batchToggleHotkey, QWidget *parent)
    : QWidget(parent), m_isSelecting(false), m_batchModeActive(batchModeActive),
      m_pendingBatchCount(qMax(0, pendingBatchCount)),
      m_batchToggleHotkey(normalizeHotkey(batchToggleHotkey.isEmpty() ? QStringLiteral("d") : batchToggleHotkey)) {
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

    painter.setRenderHint(QPainter::Antialiasing, true);
    QFont font = painter.font();
    font.setPointSize(12);
    font.setBold(true);
    painter.setFont(font);

    QStringList lines;
    lines << (m_batchModeActive ? QString::fromUtf8("批量模式") : QString::fromUtf8("单张模式"));
    if (m_batchModeActive)
        lines << QString::fromUtf8("已暂存 %1 张").arg(m_pendingBatchCount);
    if (m_finalizeBatch)
        lines << QString::fromUtf8("本张为最后一张");
    lines << QString::fromUtf8("%1: %2  Esc: %3")
                 .arg(batchHotkeyLabel(),
                      m_pendingBatchCount > 0 ? QString::fromUtf8("标记最后一张") : QString::fromUtf8("切换批量模式"),
                      (m_finalizeBatch && m_pendingBatchCount > 0) ? QString::fromUtf8("清空已暂存批量") : QString::fromUtf8("取消当前截图"));

    const int padding = 12;
    const int spacing = 6;
    QFontMetrics fm(font);
    int width = 0;
    int height = padding * 2 - spacing;
    for (const QString &line : lines)
    {
        width = qMax(width, fm.horizontalAdvance(line));
        height += fm.height() + spacing;
    }
    QRect box(24, 24, width + padding * 2, height);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(20, 20, 20, 190));
    painter.drawRoundedRect(box, 10, 10);

    painter.setPen(Qt::white);
    int y = box.top() + padding + fm.ascent();
    for (const QString &line : lines)
    {
        painter.drawText(box.left() + padding, y, line);
        y += fm.height() + spacing;
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
                emit screenshotTaken(result, r, m_batchModeActive, m_finalizeBatch);
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
    if (matchesBatchToggleHotkey(event)) {
        if (m_pendingBatchCount > 0 || m_batchModeActive)
        {
            if (m_pendingBatchCount > 0)
                m_finalizeBatch = !m_finalizeBatch;
            else
                m_batchModeActive = !m_batchModeActive;
        }
        else
        {
            m_batchModeActive = !m_batchModeActive;
        }
        if (!m_batchModeActive)
            m_finalizeBatch = false;
        update();
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Escape) {
        emit cancelled(m_finalizeBatch && m_pendingBatchCount > 0);
        close();
        event->accept();
        return;
    }

    QWidget::keyPressEvent(event);
}

QRect ScreenshotTool::getNormalizedRect() const {
    return QRect(m_startPoint, m_endPoint).normalized();
}

bool ScreenshotTool::matchesBatchToggleHotkey(QKeyEvent *event) const
{
    if (!event)
        return false;

    const QString normalizedConfig = normalizeHotkey(m_batchToggleHotkey);
    if (normalizedConfig.isEmpty())
        return false;

    QString eventHotkey = normalizeHotkey(QKeySequence(event->modifiers() | event->key()).toString(QKeySequence::NativeText));
    if (eventHotkey == normalizedConfig)
        return true;

    const QString eventText = normalizeHotkey(event->text());
    return !eventText.isEmpty() && eventText == normalizedConfig;
}

QString ScreenshotTool::batchHotkeyLabel() const
{
    if (m_batchToggleHotkey.isEmpty())
        return QStringLiteral("D");
    return m_batchToggleHotkey.toUpper();
}
