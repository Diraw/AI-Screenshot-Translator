#include "ScreenshotTool.h"

#include <QApplication>
#include <QDebug>
#include <QFont>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>

#include "TranslationManager.h"

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
                               const QList<QPixmap> &previousCaptures,
                               const QString &batchToggleHotkey, QWidget *parent)
    : QWidget(parent), m_previousCaptures(previousCaptures), m_isSelecting(false), m_batchModeActive(batchModeActive),
      m_pendingBatchCount(qMax(0, pendingBatchCount)),
      m_batchToggleHotkey(normalizeHotkey(batchToggleHotkey.isEmpty() ? QStringLiteral("d") : batchToggleHotkey))
{
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_DeleteOnClose);
    setCursor(Qt::CrossCursor);

    QList<QScreen *> allScreens = QGuiApplication::screens();
    QScreen *targetScreen = QGuiApplication::primaryScreen();
    if (targetScreenIndex >= 0 && targetScreenIndex < allScreens.size())
    {
        targetScreen = allScreens[targetScreenIndex];
        qDebug() << "Screenshot tool: Single screen mode, index" << targetScreenIndex << "(" << targetScreen->name() << ")";
    }
    else
    {
        qDebug() << "Screenshot tool: Defaulting to primary screen";
    }

    m_screensToCapture.clear();
    m_screensToCapture.append(targetScreen);

    QRect virtualGeometry = targetScreen->geometry();
    m_globalOrigin = virtualGeometry.topLeft();

    setGeometry(virtualGeometry);

    qDebug() << "Capture Geometry:" << virtualGeometry;
    qDebug() << "Global Origin:" << m_globalOrigin;
    qDebug() << "DPR:" << targetScreen->devicePixelRatio();

    captureScreens();
}

void ScreenshotTool::captureScreens()
{
    if (m_screensToCapture.isEmpty())
        return;

    QScreen *screen = m_screensToCapture.first();
    qreal dpr = screen->devicePixelRatio();

    m_fullCapture = screen->grabWindow(0);
    m_fullCapture.setDevicePixelRatio(dpr);

    qDebug() << "Grabbed screen:" << screen->name()
             << "Size:" << m_fullCapture.size()
             << "DPR:" << m_fullCapture.devicePixelRatio();
}

void ScreenshotTool::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    activateWindow();
    setFocus();
}

void ScreenshotTool::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    TranslationManager &tm = TranslationManager::instance();

    painter.drawPixmap(0, 0, m_fullCapture);
    painter.fillRect(rect(), QColor(0, 0, 0, 100));

    if (m_isSelecting)
    {
        QRect r = getNormalizedRect();

        painter.save();
        painter.setClipRect(r);
        painter.drawPixmap(0, 0, m_fullCapture);
        painter.restore();

        painter.setPen(QPen(Qt::white, 2));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(r);
    }

    if (m_showPreviousCapture)
    {
        const QPixmap previousCapture = previousCapturePixmap();
        if (!previousCapture.isNull())
        {
            QSize previewSize = previousCapture.deviceIndependentSize().toSize();
            if (previewSize.isEmpty())
                previewSize = previousCapture.size();

            const QPoint topLeft((width() - previewSize.width()) / 2,
                                 (height() - previewSize.height()) / 2);
            const QRect previewRect(topLeft, previewSize);

            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(0, 0, 0, 160));
            painter.drawRoundedRect(previewRect.adjusted(-8, -8, 8, 8), 10, 10);

            painter.drawPixmap(topLeft, previousCapture);

            painter.setPen(QPen(Qt::white, 1));
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(previewRect.adjusted(0, 0, -1, -1));
        }
    }

    painter.setRenderHint(QPainter::Antialiasing, true);
    QFont font = painter.font();
    font.setPointSize(12);
    font.setBold(true);
    painter.setFont(font);

    QStringList lines;
    lines << (m_batchModeActive ? tm.tr("shottool_batch_mode") : tm.tr("shottool_single_mode"));
    if (m_batchModeActive)
        lines << tm.tr("shottool_stashed_count").arg(m_pendingBatchCount);
    if (m_finalizeBatch)
        lines << tm.tr("shottool_current_is_last");
    lines << QStringLiteral("%1: %2  Esc: %3")
                 .arg(batchHotkeyLabel(),
                      m_pendingBatchCount > 0 ? tm.tr("shottool_mark_last") : tm.tr("shottool_toggle_batch"),
                      (m_finalizeBatch && m_pendingBatchCount > 0) ? tm.tr("shottool_clear_pending") : tm.tr("shottool_cancel_current"));

    if (!previousCapturePixmap().isNull())
        lines << QStringLiteral("S: %1").arg(tm.tr("shottool_hold_preview_last"));

    if (m_pendingBatchCount > 0)
        lines << QStringLiteral("Enter: %1").arg(tm.tr("shottool_enter_translate"));

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

void ScreenshotTool::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        m_startPoint = event->pos();
        m_endPoint = event->pos();
        m_isSelecting = true;
        update();
    }
}

void ScreenshotTool::mouseMoveEvent(QMouseEvent *event)
{
    if (m_isSelecting)
    {
        m_endPoint = event->pos();
        update();
    }
}

void ScreenshotTool::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_isSelecting)
    {
        qDebug() << "ScreenshotTool: Mouse released. Selection finished.";
        m_isSelecting = false;
        m_endPoint = event->pos();

        QRect r = getNormalizedRect();
        if (r.width() > 10 && r.height() > 10)
        {
            QPixmap result = getResultPixmap(r);
            if (!result.isNull())
            {
                qDebug() << "ScreenshotTool: Emitting screenshotTaken signal.";
                emit screenshotTaken(result, r, m_batchModeActive, m_finalizeBatch);
                qDebug() << "ScreenshotTool: Signal emitted. Closing.";
                close();
            }
            else
            {
                update();
            }
        }
        else
        {
            update();
        }
    }
}

QPixmap ScreenshotTool::getResultPixmap(const QRect &selectionRect)
{
    QScreen *targetScreen = QApplication::screenAt(mapToGlobal(selectionRect.center()));
    if (!targetScreen)
        targetScreen = QApplication::primaryScreen();
    qreal dpr = targetScreen->devicePixelRatio();

    QPixmap result(selectionRect.size() * dpr);
    result.setDevicePixelRatio(dpr);
    result.fill(Qt::transparent);

    QPainter p(&result);
    p.translate(-selectionRect.topLeft());
    p.drawPixmap(0, 0, m_fullCapture);
    p.end();

    return result;
}

QPixmap ScreenshotTool::previousCapturePixmap() const
{
    for (auto it = m_previousCaptures.crbegin(); it != m_previousCaptures.crend(); ++it)
    {
        if (!it->isNull())
            return *it;
    }
    return QPixmap();
}

bool ScreenshotTool::matchesPreviousCaptureHotkey(QKeyEvent *event) const
{
    if (!event)
        return false;

    const Qt::KeyboardModifiers blockedModifiers =
        event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier);
    if (blockedModifiers != Qt::NoModifier)
        return false;

    return event->key() == Qt::Key_S || normalizeHotkey(event->text()) == QStringLiteral("s");
}

void ScreenshotTool::keyPressEvent(QKeyEvent *event)
{
    if (matchesPreviousCaptureHotkey(event))
    {
        if (!event->isAutoRepeat() && !m_showPreviousCapture)
        {
            m_showPreviousCapture = true;
            update();
        }
        event->accept();
        return;
    }

    if (matchesBatchToggleHotkey(event))
    {
        if (event->isAutoRepeat())
        {
            event->accept();
            return;
        }
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

    if (event->key() == Qt::Key_Escape)
    {
        emit cancelled(m_finalizeBatch && m_pendingBatchCount > 0);
        event->accept();
        return;
    }

    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) &&
        m_pendingBatchCount > 0)
    {
        emit batchFinalizeRequested();
        close();
        event->accept();
        return;
    }

    QWidget::keyPressEvent(event);
}

void ScreenshotTool::keyReleaseEvent(QKeyEvent *event)
{
    if (matchesPreviousCaptureHotkey(event))
    {
        if (!event->isAutoRepeat() && m_showPreviousCapture)
        {
            m_showPreviousCapture = false;
            update();
        }
        event->accept();
        return;
    }

    QWidget::keyReleaseEvent(event);
}

QRect ScreenshotTool::getNormalizedRect() const
{
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
