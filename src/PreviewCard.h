#ifndef PREVIEWCARD_H
#define PREVIEWCARD_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QShortcut>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QList>
#include <QKeyEvent>
#include <QPixmap>
#include <QMouseEvent>
#include <QPoint>

class PreviewCard : public QWidget
{
    Q_OBJECT
public:
    explicit PreviewCard(const QList<QPixmap> &pixmaps, QWidget *parent = nullptr);
    explicit PreviewCard(const QPixmap &pixmap, QWidget *parent = nullptr);

    void setZoomSensitivity(float sensitivity);
    void setBorderColor(const QString &colorStr);
    void setUseBorder(bool use);
    void setImage(const QPixmap &pixmap);
    void setImages(const QList<QPixmap> &pixmaps);
    void setNavigationHotkeys(const QString &prevKey, const QString &nextKey);
    void updateLanguage() {}

signals:
    void closed();
    void closedWithGeometry(QPoint pos, QSize size);
    void restoreRequest();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    QString navigationButtonStyle(const QString &backgroundColor, const QString &hoverColor,
                                  const QString &pressedColor, const QString &disabledColor) const;
    QPixmap m_pixmap;
    QList<QPixmap> m_pixmaps;
    float m_zoomSensitivity = 500.0f;
    QColor m_borderColor = QColor(100, 100, 100);
    bool m_useBorder = true;

    QLabel *m_imageLabel = nullptr;
    QPoint m_dragPosition;
    bool m_isDragging = false;

    QPushButton *m_closeBtn = nullptr;
    QPushButton *m_restoreBtn = nullptr;
    QPushButton *m_zoomBtn = nullptr;
    QPushButton *m_prevBtn = nullptr;
    QPushButton *m_nextBtn = nullptr;
    QLabel *m_indexLabel = nullptr;
    QShortcut *m_prevShortcut = nullptr;
    QShortcut *m_nextShortcut = nullptr;

    bool m_isResizing = false;
    QPoint m_resizeDragPosition;

    QWidget *m_controlsWidget{nullptr};
    int m_controlsH{32};
    QSize m_baseSize;
    double m_scale{1.0};
    int m_currentImageIndex = 0;
    QString m_prevHotkey = QStringLiteral("z");
    QString m_nextHotkey = QStringLiteral("x");

    void applyScale();
    void updateImageIndexLabel();
    void showPreviousImage();
    void showNextImage();
    bool matchesHotkey(QKeyEvent *event, const QString &hotkey) const;

    bool eventFilter(QObject *watched, QEvent *event) override;
};

#endif // PREVIEWCARD_H
