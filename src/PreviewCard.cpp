#include "PreviewCard.h"
#include <QPainter>
#include <QStyle>
#include <QDebug>
#include <QMouseEvent>
#include <QCloseEvent>

// Constants for window chrome/frame overhead
static const int EXTRA_W = 0;
static const int EXTRA_H = 22; // 0 margins + 2 spacing + 20 button height
static const int MIN_IMG_SIDE = 50;

PreviewCard::PreviewCard(const QPixmap &pixmap, QWidget *parent)
    : QWidget(parent), m_pixmap(pixmap), m_isDragging(false), m_isResizing(false) {
    
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool | Qt::WindowDoesNotAcceptFocus);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose); // Ensure widget is deleted so QPointer becomes null
    setAttribute(Qt::WA_ShowWithoutActivating);

    // 1. Initialize Base Size (Strict 1:1 Logical Size)
    m_baseSize = m_pixmap.deviceIndependentSize().toSize();
    if (m_baseSize.isEmpty()) m_baseSize = QSize(1, 1);

    // 2. Image Label (Absolute Positioning)
    m_imageLabel = new QLabel(this);
    m_imageLabel->setScaledContents(false);
    m_imageLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    // Transparent background to avoid white borders
    m_imageLabel->setStyleSheet("QLabel{background:transparent;border:none;}");

    // 3. Controls Widget (Absolute Positioning)
    m_controlsWidget = new QWidget(this);
    m_controlsWidget->setStyleSheet("background:transparent;");

    QHBoxLayout *controlsLayout = new QHBoxLayout(m_controlsWidget);
    controlsLayout->setContentsMargins(0, 0, 0, 0);
    controlsLayout->setSpacing(0);
    controlsLayout->addStretch();
    
    // Zoom Button (Yellow, ? U+2921)
    m_zoomBtn = new QPushButton(QString::fromUtf8("\u2921"), m_controlsWidget);
    m_zoomBtn->setFixedSize(20, 20);
    m_zoomBtn->setStyleSheet("background-color:#ffc107;color:black;border-radius:10px;font-weight:bold;font-size:14px;");
    m_zoomBtn->installEventFilter(this); // Handle drag to zoom
    controlsLayout->addWidget(m_zoomBtn);

    m_closeBtn = new QPushButton("X", m_controlsWidget);
    m_closeBtn->setFixedSize(20, 20);
    m_closeBtn->setStyleSheet("background-color:red;color:white;border-radius:10px;font-weight:bold;");
    connect(m_closeBtn, &QPushButton::clicked, this, &QWidget::close);
    controlsLayout->addWidget(m_closeBtn);

    // Initial State
    m_scale = 1.0;
    applyScale();
}

void PreviewCard::closeEvent(QCloseEvent *event) {
    emit closedWithGeometry(this->pos(), this->size());
    emit closed();
    QWidget::closeEvent(event);
}

bool PreviewCard::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_zoomBtn) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto *me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                m_isResizing = true;
                m_resizeDragPosition = me->globalPosition().toPoint();
                return true; 
            }
        } else if (event->type() == QEvent::MouseMove) {
            if (m_isResizing) {
                auto *me = static_cast<QMouseEvent*>(event);
                QPoint globalPos = me->globalPosition().toPoint();
                int dy = globalPos.y() - m_resizeDragPosition.y();

                // Sensitivity
                double factor = m_zoomSensitivity / 500.0;
                int effectiveDy = (int)qRound(dy * factor);

                // Drive scale by HEIGHT change: newScale = (currentHeight + dy) / baseHeight
                int currentImgH = (int)qRound(m_baseSize.height() * m_scale);
                int newImgH = currentImgH + effectiveDy;
                
                // Clamp
                int minH = qMax(MIN_IMG_SIDE, 1);
                if (newImgH < minH) newImgH = minH;

                // Update Scale
                m_scale = (double)newImgH / (double)m_baseSize.height();
                if (m_scale < 0.01) m_scale = 0.01;

                applyScale();

                m_resizeDragPosition = globalPos;
                return true;
            }
        } else if (event->type() == QEvent::MouseButtonRelease) {
            if (m_isResizing) {
                m_isResizing = false;
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void PreviewCard::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    // 1:1 Scale Logic - No aspect locking or adjustment needed here
    // Window size is strictly controlled by applyScale()
}

void PreviewCard::applyScale() {
    // 1. Calculate Target Dimenions
    int imgW = qMax(MIN_IMG_SIDE, (int)qRound(m_baseSize.width()  * m_scale));
    int imgH = qMax(MIN_IMG_SIDE, (int)qRound(m_baseSize.height() * m_scale));

    // 2. Set Label Size & Position (Absolute)
    m_imageLabel->setFixedSize(imgW, imgH);
    m_imageLabel->move(0, 0);

    // 3. Set Controls Size & Position (Absolute - below image)
    m_controlsWidget->setFixedSize(imgW, m_controlsH);
    m_controlsWidget->move(0, imgH);

    // 4. Set Window Size
    setFixedSize(imgW, imgH + m_controlsH);

    // 5. Update Content
    if (qAbs(m_scale - 1.0) < 0.001) {
        // Strict 1:1 - No re-sampling
        m_imageLabel->setPixmap(m_pixmap);
    } else {
        // Scaled Preview - Handle HiDPI: Scale in physical pixels + Restore DPR
        const qreal dpr = m_pixmap.devicePixelRatio();

        const int physW = qRound(imgW * dpr);
        const int physH = qRound(imgH * dpr);

        if (physW <= 0 || physH <= 0) {
             qWarning() << "PreviewCard: Invalid scaled size" << physW << "x" << physH;
             return; 
        }

        QPixmap scaled = m_pixmap.scaled(
            physW, physH,
            Qt::IgnoreAspectRatio,
            Qt::SmoothTransformation
        );
        scaled.setDevicePixelRatio(dpr); // Critical: ensure logical size matches label

        m_imageLabel->setPixmap(scaled);
    }
}

void PreviewCard::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Border is now drawn via QLabel stylesheet in setUseBorder()
    // No need to draw anything here
}

void PreviewCard::setUseBorder(bool use) {
    m_useBorder = use;
    if (m_imageLabel) {
        if (use) {
            // L-shaped border using stylesheet (top and left only)
            QString colorStr = QString("%1,%2,%3")
                .arg(m_borderColor.red())
                .arg(m_borderColor.green())
                .arg(m_borderColor.blue());
            m_imageLabel->setStyleSheet(
                QString("QLabel { background: transparent; border-top: 2px solid rgb(%1); border-left: 2px solid rgb(%1); }")
                .arg(colorStr)
            );
        } else {
            // No border
            m_imageLabel->setStyleSheet("QLabel { background: transparent; border: none; }");
        }
    }
    update();
}

void PreviewCard::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
        m_isDragging = true;
        event->accept();
    }
}

void PreviewCard::mouseMoveEvent(QMouseEvent *event) {
    if (event->buttons() & Qt::LeftButton && m_isDragging) {
        move(event->globalPosition().toPoint() - m_dragPosition);
        event->accept();
    }
}

void PreviewCard::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        m_isDragging = false;
        event->accept();
    }
}

void PreviewCard::setZoomSensitivity(float sensitivity) {
    qInfo() << "[PreviewCard] setZoomSensitivity:" << sensitivity;
    m_zoomSensitivity = sensitivity;
}

void PreviewCard::setBorderColor(const QString &colorStr) {
    QStringList rgb = colorStr.split(',');
    if (rgb.size() == 3) {
        m_borderColor = QColor(rgb[0].toInt(), rgb[1].toInt(), rgb[2].toInt());
        // Re-apply the border with new color
        setUseBorder(m_useBorder);
    }
}

// Fix for High DPI: Pixmap size is in physical pixels, but widget resize expects logical pixels.
void PreviewCard::setImage(const QPixmap &pixmap) {
    m_pixmap = pixmap;
    
    // Reset to new 1:1 base
    m_baseSize = m_pixmap.deviceIndependentSize().toSize();
    if (m_baseSize.isEmpty()) m_baseSize = QSize(1,1);
    
    m_imageLabel->setPixmap(m_pixmap); // Update internal pixmap
    
    // Reset Scale
    m_scale = 1.0;
    applyScale(); 
}

void PreviewCard::wheelEvent(QWheelEvent *event) {
    int delta = event->angleDelta().y();
    if (delta == 0) return;
    
    double factor = (m_zoomSensitivity / 2000.0);
    if (factor < 0.01) factor = 0.01;
    
    double mul = (delta > 0) ? (1.0 + factor) : (1.0 / (1.0 + factor));
    m_scale *= mul;

    // Clamp Scale
    double minScaleW = (double)MIN_IMG_SIDE / (double)m_baseSize.width();
    double minScaleH = (double)MIN_IMG_SIDE / (double)m_baseSize.height();
    double minScale = qMax(minScaleW, minScaleH);
    
    if (m_scale < minScale) m_scale = minScale;
    
    applyScale();
    event->accept();
}
