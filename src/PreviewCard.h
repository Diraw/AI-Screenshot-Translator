#ifndef PREVIEWCARD_H
#define PREVIEWCARD_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPixmap>
#include <QMouseEvent>
#include <QPoint>

class PreviewCard : public QWidget {
    Q_OBJECT
public:
    explicit PreviewCard(const QPixmap &pixmap, QWidget *parent = nullptr);
    
    void setZoomSensitivity(float sensitivity);
    void setBorderColor(const QString &colorStr);
    void setUseBorder(bool use);
    void setImage(const QPixmap &pixmap);

signals:
    void closed();
    void closedWithGeometry(QPoint pos, QSize size);
    void restoreRequest(); // "R" button

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    QPixmap m_pixmap;
    float m_zoomSensitivity = 500.0f;
    QColor m_borderColor = QColor(100, 100, 100);
    bool m_useBorder = true;
    
    QLabel *m_imageLabel;
    QPoint m_dragPosition;
    bool m_isDragging;

    QPushButton *m_closeBtn;
    QPushButton *m_restoreBtn;
    QPushButton *m_zoomBtn;
    
    bool m_isResizing;
    QPoint m_resizeDragPosition;
    
    // 1:1 Scale Logic
    QWidget *m_controlsWidget{nullptr};
    int m_controlsH{20}; // Button height
    QSize m_baseSize;    // Original logical size
    double m_scale{1.0}; // Current zoom scale

    void applyScale();   // Update UI based on scale

    bool eventFilter(QObject *watched, QEvent *event) override;
};

#endif // PREVIEWCARD_H

