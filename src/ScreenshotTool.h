#ifndef SCREENSHOTTOOL_H
#define SCREENSHOTTOOL_H

#include <QWidget>
#include <QPixmap>
#include <QPoint>
#include <QRect>
#include <QList>
#include <QString>

struct ScreenCapture {
    QPixmap pixmap;
    QPoint logicalPos;
};

class ScreenshotTool : public QWidget {
    Q_OBJECT
public:
    explicit ScreenshotTool(int targetScreenIndex = -1,
                            bool batchModeActive = false,
                            int pendingBatchCount = 0,
                            const QList<QPixmap> &previousCaptures = {},
                            const QString &batchToggleHotkey = QStringLiteral("d"),
                            QWidget *parent = nullptr);

signals:
    void screenshotTaken(const QPixmap &pixmap, const QRect &rect, bool batchMode, bool finalizeBatch);
    void batchFinalizeRequested();
    void cancelled(bool clearPendingBatch);

protected:
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;

private:
    QPixmap m_fullCapture;
    QList<QPixmap> m_previousCaptures;
    QPoint m_globalOrigin; // To offset global coordinates to local widgets coordinates
    
    QPoint m_startPoint;
    QPoint m_endPoint;
    bool m_isSelecting;
    bool m_showPreviousCapture = false;
    bool m_batchModeActive = false;
    bool m_finalizeBatch = false;
    int m_pendingBatchCount = 0;
    QString m_batchToggleHotkey = QStringLiteral("d");
    QList<QScreen*> m_screensToCapture;  // Stores which screens to capture
    // QList<ScreenCapture> m_captures; // Removed in favor of single composite capture
    // QPixmap m_compositePixmap; // Removed
    
    void captureScreens();
    QRect getNormalizedRect() const;
    QPixmap getResultPixmap(const QRect &selectionRect);
    QPixmap previousCapturePixmap() const;
    bool matchesPreviousCaptureHotkey(QKeyEvent *event) const;
    bool matchesBatchToggleHotkey(QKeyEvent *event) const;
    QString batchHotkeyLabel() const;
};

#endif // SCREENSHOTTOOL_H
