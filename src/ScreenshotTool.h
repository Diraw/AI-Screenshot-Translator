#ifndef SCREENSHOTTOOL_H
#define SCREENSHOTTOOL_H

#include <QWidget>
#include <QPixmap>
#include <QPoint>
#include <QRect>
#include <QList>

struct ScreenCapture {
    QPixmap pixmap;
    QPoint logicalPos;
};

class ScreenshotTool : public QWidget {
    Q_OBJECT
public:
    explicit ScreenshotTool(int targetScreenIndex = -1, QWidget *parent = nullptr);

signals:
    void screenshotTaken(const QPixmap &pixmap, const QRect &rect);
    void cancelled();

protected:
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    QPixmap m_fullCapture;
    QPoint m_globalOrigin; // To offset global coordinates to local widgets coordinates
    
    QPoint m_startPoint;
    QPoint m_endPoint;
    bool m_isSelecting;
    QList<QScreen*> m_screensToCapture;  // Stores which screens to capture
    // QList<ScreenCapture> m_captures; // Removed in favor of single composite capture
    // QPixmap m_compositePixmap; // Removed
    
    void captureScreens();
    QRect getNormalizedRect() const;
    QPixmap getResultPixmap(const QRect &selectionRect);
};

#endif // SCREENSHOTTOOL_H
