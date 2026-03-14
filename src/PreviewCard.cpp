#include "PreviewCard.h"

#include <QCloseEvent>
#include <QColor>
#include <QDebug>
#include <QKeyEvent>
#include <QKeySequence>
#include <QPainter>
#include <QRegularExpression>
#include <QStyle>
#include <algorithm>

namespace
{
QString normalizeHotkey(QString key)
{
    key = key.trimmed().toLower();
    key.replace(" ", "");
    return key;
}

class BorderLabel final : public QLabel
{
public:
    explicit BorderLabel(QWidget *parent = nullptr)
        : QLabel(parent)
    {
        setAttribute(Qt::WA_TranslucentBackground);
    }

    void setBorderEnabled(bool enabled)
    {
        if (m_borderEnabled == enabled)
            return;
        m_borderEnabled = enabled;
        update();
    }

    void setBorderColor(const QColor &color)
    {
        if (m_borderColor == color)
            return;
        m_borderColor = color;
        update();
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        QLabel::paintEvent(event);

        if (!m_borderEnabled)
            return;

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, false);

        const int borderW = 2;
        QColor c = m_borderColor;
        if (!c.isValid())
            c = QColor(100, 100, 100, 255);

        QPixmap pm = pixmap(Qt::ReturnByValue);
        int drawW = width();
        int drawH = height();
        if (!pm.isNull())
        {
            const qreal dpr = pm.devicePixelRatio();
            const QSize logicalSize = (dpr > 0.0) ? QSize(qRound(pm.width() / dpr), qRound(pm.height() / dpr)) : pm.size();
            drawW = qMin(drawW, logicalSize.width());
            drawH = qMin(drawH, logicalSize.height());
        }

        if (drawW <= 0 || drawH <= 0)
            return;

        painter.fillRect(QRect(0, 0, drawW, borderW), c);
        painter.fillRect(QRect(0, 0, borderW, drawH), c);
    }

private:
    bool m_borderEnabled{false};
    QColor m_borderColor;
};
} // namespace

static const int MIN_IMG_SIDE = 50;
static const int CONTROL_BUTTON_SIZE = 22;
static const int CONTROL_BAR_HEIGHT = 32;

QString PreviewCard::navigationButtonStyle(const QString &backgroundColor, const QString &hoverColor,
                                           const QString &pressedColor, const QString &disabledColor) const
{
    return QString(
               "QPushButton{background-color:%1;color:white;border:none;border-radius:%2px;font-weight:bold;font-size:14px;}"
               "QPushButton:hover{background-color:%3;}"
               "QPushButton:pressed{background-color:%4;}"
               "QPushButton:disabled{background-color:%5;color:rgba(255,255,255,210);}")
        .arg(backgroundColor)
        .arg(CONTROL_BUTTON_SIZE / 2)
        .arg(hoverColor)
        .arg(pressedColor)
        .arg(disabledColor);
}

PreviewCard::PreviewCard(const QList<QPixmap> &pixmaps, QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose);
    setFocusPolicy(Qt::StrongFocus);
    m_controlsH = CONTROL_BAR_HEIGHT;

    m_imageLabel = new BorderLabel(this);
    m_imageLabel->setScaledContents(false);
    m_imageLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_imageLabel->setStyleSheet("QLabel{background:transparent;border:none;}");

    m_controlsWidget = new QWidget(this);
    m_controlsWidget->setStyleSheet("background:transparent;");

    QHBoxLayout *controlsLayout = new QHBoxLayout(m_controlsWidget);
    controlsLayout->setContentsMargins(0, 5, 0, 5);
    controlsLayout->setSpacing(6);

    m_prevBtn = new QPushButton(QString::fromUtf8("\u2190"), m_controlsWidget);
    m_prevBtn->setFixedSize(CONTROL_BUTTON_SIZE, CONTROL_BUTTON_SIZE);
    m_prevBtn->setStyleSheet(navigationButtonStyle("#2faa60", "#37ba6d", "#1d8c4c", "#5f5f5f"));
    connect(m_prevBtn, &QPushButton::clicked, this, &PreviewCard::showPreviousImage);
    controlsLayout->addWidget(m_prevBtn);

    m_nextBtn = new QPushButton(QString::fromUtf8("\u2192"), m_controlsWidget);
    m_nextBtn->setFixedSize(CONTROL_BUTTON_SIZE, CONTROL_BUTTON_SIZE);
    m_nextBtn->setStyleSheet(navigationButtonStyle("#2f7ff7", "#4590ff", "#2464c3", "#5f5f5f"));
    connect(m_nextBtn, &QPushButton::clicked, this, &PreviewCard::showNextImage);
    controlsLayout->addWidget(m_nextBtn);

    controlsLayout->addStretch();

    m_zoomBtn = new QPushButton(QString::fromUtf8("\u2921"), m_controlsWidget);
    m_zoomBtn->setFixedSize(CONTROL_BUTTON_SIZE, CONTROL_BUTTON_SIZE);
    m_zoomBtn->setStyleSheet("background-color:#ffc107;color:black;border-radius:11px;font-weight:bold;font-size:14px;");
    m_zoomBtn->installEventFilter(this);

    m_indexLabel = new QLabel(m_controlsWidget);
    m_indexLabel->setAlignment(Qt::AlignCenter);
    m_indexLabel->setStyleSheet(
        "color:white;"
        "background-color:rgba(22,22,22,155);"
        "border:1px solid rgba(255,255,255,55);"
        "border-radius:8px;"
        "padding:2px 10px;");
    m_indexLabel->setMinimumWidth(56);
    m_indexLabel->setMinimumHeight(CONTROL_BUTTON_SIZE);
    controlsLayout->addWidget(m_indexLabel);

    controlsLayout->addStretch();
    controlsLayout->addWidget(m_zoomBtn);

    m_closeBtn = new QPushButton("X", m_controlsWidget);
    m_closeBtn->setFixedSize(CONTROL_BUTTON_SIZE, CONTROL_BUTTON_SIZE);
    m_closeBtn->setStyleSheet("background-color:red;color:white;border-radius:11px;font-weight:bold;");
    connect(m_closeBtn, &QPushButton::clicked, this, &QWidget::close);
    controlsLayout->addWidget(m_closeBtn);

    setImages(pixmaps);
}

PreviewCard::PreviewCard(const QPixmap &pixmap, QWidget *parent)
    : PreviewCard(QList<QPixmap>{pixmap}, parent)
{
}

void PreviewCard::closeEvent(QCloseEvent *event)
{
    emit closedWithGeometry(this->pos(), this->size());
    emit closed();
    QWidget::closeEvent(event);
}

bool PreviewCard::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_zoomBtn)
    {
        if (event->type() == QEvent::MouseButtonPress)
        {
            auto *me = static_cast<QMouseEvent *>(event);
            if (me->button() == Qt::LeftButton)
            {
                m_isResizing = true;
                m_resizeDragPosition = me->globalPosition().toPoint();
                return true;
            }
        }
        else if (event->type() == QEvent::MouseMove)
        {
            if (m_isResizing)
            {
                auto *me = static_cast<QMouseEvent *>(event);
                QPoint globalPos = me->globalPosition().toPoint();
                int dy = globalPos.y() - m_resizeDragPosition.y();

                double factor = m_zoomSensitivity / 500.0;
                int effectiveDy = (int)qRound(dy * factor);

                int currentImgH = (int)qRound(m_baseSize.height() * m_scale);
                int newImgH = currentImgH + effectiveDy;
                int minH = qMax(MIN_IMG_SIDE, 1);
                if (newImgH < minH)
                    newImgH = minH;

                m_scale = (double)newImgH / (double)m_baseSize.height();
                if (m_scale < 0.01)
                    m_scale = 0.01;

                applyScale();
                m_resizeDragPosition = globalPos;
                return true;
            }
        }
        else if (event->type() == QEvent::MouseButtonRelease)
        {
            if (m_isResizing)
            {
                m_isResizing = false;
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void PreviewCard::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
}

void PreviewCard::applyScale()
{
    int imgW = qMax(MIN_IMG_SIDE, (int)qRound(m_baseSize.width() * m_scale));
    int imgH = qMax(MIN_IMG_SIDE, (int)qRound(m_baseSize.height() * m_scale));

    m_imageLabel->setFixedSize(imgW, imgH);
    m_imageLabel->move(0, 0);

    m_controlsWidget->setFixedSize(imgW, m_controlsH);
    m_controlsWidget->move(0, imgH);

    setFixedSize(imgW, imgH + m_controlsH);

    if (qAbs(m_scale - 1.0) < 0.001)
    {
        m_imageLabel->setPixmap(m_pixmap);
    }
    else
    {
        const qreal dpr = m_pixmap.devicePixelRatio();
        const int physW = qRound(imgW * dpr);
        const int physH = qRound(imgH * dpr);

        if (physW <= 0 || physH <= 0)
        {
            qWarning() << "PreviewCard: Invalid scaled size" << physW << "x" << physH;
            return;
        }

        QPixmap scaled = m_pixmap.scaled(physW, physH, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        scaled.setDevicePixelRatio(dpr);
        m_imageLabel->setPixmap(scaled);
    }

    updateImageIndexLabel();
}

void PreviewCard::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
}

void PreviewCard::setUseBorder(bool use)
{
    m_useBorder = use;
    if (m_imageLabel)
    {
        m_imageLabel->setStyleSheet("QLabel { background: transparent; border: none; }");
        auto *bl = static_cast<BorderLabel *>(m_imageLabel);
        bl->setBorderColor(m_borderColor);
        bl->setBorderEnabled(use);
    }
    update();
}

void PreviewCard::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        activateWindow();
        raise();
        setFocus(Qt::MouseFocusReason);
        m_dragPosition = event->globalPosition().toPoint() - frameGeometry().topLeft();
        m_isDragging = true;
        event->accept();
    }
}

void PreviewCard::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton && m_isDragging)
    {
        move(event->globalPosition().toPoint() - m_dragPosition);
        event->accept();
    }
}

void PreviewCard::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        m_isDragging = false;
        event->accept();
    }
}

void PreviewCard::setZoomSensitivity(float sensitivity)
{
    qInfo() << "[PreviewCard] setZoomSensitivity:" << sensitivity;
    m_zoomSensitivity = sensitivity;
}

void PreviewCard::setBorderColor(const QString &colorStr)
{
    const QString s = colorStr.trimmed();
    if (s.isEmpty())
        return;

    auto clamp255 = [](int v)
    { return std::max(0, std::min(255, v)); };
    auto parseAlpha255 = [clamp255](double a) -> int
    {
        if (a <= 1.0)
            return clamp255((int)qRound(a * 255.0));
        return clamp255((int)qRound(a));
    };

    QColor parsed;

    {
        QRegularExpression re(
            "^rgba\\(\\s*(\\d{1,3})\\s*,\\s*(\\d{1,3})\\s*,\\s*(\\d{1,3})\\s*,\\s*([0-9]*\\.?[0-9]+)\\s*\\)$",
            QRegularExpression::CaseInsensitiveOption);
        auto m = re.match(s);
        if (m.hasMatch())
        {
            bool ok1 = false, ok2 = false, ok3 = false, ok4 = false;
            int r = m.captured(1).toInt(&ok1);
            int g = m.captured(2).toInt(&ok2);
            int b = m.captured(3).toInt(&ok3);
            double a = m.captured(4).toDouble(&ok4);
            if (ok1 && ok2 && ok3 && ok4)
                parsed = QColor(clamp255(r), clamp255(g), clamp255(b), parseAlpha255(a));
        }
    }

    if (!parsed.isValid())
    {
        QRegularExpression re(
            "^rgb\\(\\s*(\\d{1,3})\\s*,\\s*(\\d{1,3})\\s*,\\s*(\\d{1,3})\\s*\\)$",
            QRegularExpression::CaseInsensitiveOption);
        auto m = re.match(s);
        if (m.hasMatch())
        {
            bool ok1 = false, ok2 = false, ok3 = false;
            int r = m.captured(1).toInt(&ok1);
            int g = m.captured(2).toInt(&ok2);
            int b = m.captured(3).toInt(&ok3);
            if (ok1 && ok2 && ok3)
                parsed = QColor(clamp255(r), clamp255(g), clamp255(b), 255);
        }
    }

    if (!parsed.isValid())
    {
        QStringList parts = s.split(',', Qt::SkipEmptyParts);
        if (parts.size() == 3)
        {
            bool ok1 = false, ok2 = false, ok3 = false;
            int r = parts[0].trimmed().toInt(&ok1);
            int g = parts[1].trimmed().toInt(&ok2);
            int b = parts[2].trimmed().toInt(&ok3);
            if (ok1 && ok2 && ok3)
                parsed = QColor(clamp255(r), clamp255(g), clamp255(b), 255);
        }
        else if (parts.size() == 4)
        {
            bool ok1 = false, ok2 = false, ok3 = false, ok4 = false;
            int r = parts[0].trimmed().toInt(&ok1);
            int g = parts[1].trimmed().toInt(&ok2);
            int b = parts[2].trimmed().toInt(&ok3);
            double a = parts[3].trimmed().toDouble(&ok4);
            if (ok1 && ok2 && ok3 && ok4)
                parsed = QColor(clamp255(r), clamp255(g), clamp255(b), parseAlpha255(a));
        }
    }

    if (!parsed.isValid() && s.startsWith('#'))
    {
        const QString hex = s.mid(1);
        bool ok = false;
        if (hex.size() == 3)
        {
            const int r = QString(hex[0]).repeated(2).toInt(&ok, 16);
            if (!ok)
                return;
            const int g = QString(hex[1]).repeated(2).toInt(&ok, 16);
            if (!ok)
                return;
            const int b = QString(hex[2]).repeated(2).toInt(&ok, 16);
            if (!ok)
                return;
            parsed = QColor(clamp255(r), clamp255(g), clamp255(b), 255);
        }
        else if (hex.size() == 4)
        {
            const int r = QString(hex[0]).repeated(2).toInt(&ok, 16);
            if (!ok)
                return;
            const int g = QString(hex[1]).repeated(2).toInt(&ok, 16);
            if (!ok)
                return;
            const int b = QString(hex[2]).repeated(2).toInt(&ok, 16);
            if (!ok)
                return;
            const int a = QString(hex[3]).repeated(2).toInt(&ok, 16);
            if (!ok)
                return;
            parsed = QColor(clamp255(r), clamp255(g), clamp255(b), clamp255(a));
        }
        else if (hex.size() == 6)
        {
            const int r = hex.mid(0, 2).toInt(&ok, 16);
            if (!ok)
                return;
            const int g = hex.mid(2, 2).toInt(&ok, 16);
            if (!ok)
                return;
            const int b = hex.mid(4, 2).toInt(&ok, 16);
            if (!ok)
                return;
            parsed = QColor(clamp255(r), clamp255(g), clamp255(b), 255);
        }
        else if (hex.size() == 8)
        {
            const int r = hex.mid(0, 2).toInt(&ok, 16);
            if (!ok)
                return;
            const int g = hex.mid(2, 2).toInt(&ok, 16);
            if (!ok)
                return;
            const int b = hex.mid(4, 2).toInt(&ok, 16);
            if (!ok)
                return;
            const int a = hex.mid(6, 2).toInt(&ok, 16);
            if (!ok)
                return;
            parsed = QColor(clamp255(r), clamp255(g), clamp255(b), clamp255(a));
        }
    }

    if (!parsed.isValid())
        return;

    m_borderColor = parsed;
    setUseBorder(m_useBorder);
}

void PreviewCard::setImage(const QPixmap &pixmap)
{
    m_pixmap = pixmap;
    m_baseSize = m_pixmap.deviceIndependentSize().toSize();
    if (m_baseSize.isEmpty())
        m_baseSize = QSize(1, 1);
    if (m_imageLabel)
        m_imageLabel->setPixmap(m_pixmap);
    m_scale = 1.0;
    applyScale();
}

void PreviewCard::setImages(const QList<QPixmap> &pixmaps)
{
    m_pixmaps = pixmaps;
    if (m_pixmaps.isEmpty())
        m_pixmaps.append(QPixmap());

    m_currentImageIndex = qBound(0, m_currentImageIndex, m_pixmaps.size() - 1);
    setImage(m_pixmaps.value(m_currentImageIndex));
}

void PreviewCard::setNavigationHotkeys(const QString &prevKey, const QString &nextKey)
{
    m_prevHotkey = normalizeHotkey(prevKey.isEmpty() ? QStringLiteral("z") : prevKey);
    m_nextHotkey = normalizeHotkey(nextKey.isEmpty() ? QStringLiteral("x") : nextKey);

    if (m_prevShortcut)
    {
        m_prevShortcut->deleteLater();
        m_prevShortcut = nullptr;
    }
    if (m_nextShortcut)
    {
        m_nextShortcut->deleteLater();
        m_nextShortcut = nullptr;
    }

    if (!m_prevHotkey.isEmpty())
    {
        m_prevShortcut = new QShortcut(QKeySequence(m_prevHotkey), this);
        m_prevShortcut->setContext(Qt::WindowShortcut);
        connect(m_prevShortcut, &QShortcut::activated, this, &PreviewCard::showPreviousImage);
    }

    if (!m_nextHotkey.isEmpty())
    {
        m_nextShortcut = new QShortcut(QKeySequence(m_nextHotkey), this);
        m_nextShortcut->setContext(Qt::WindowShortcut);
        connect(m_nextShortcut, &QShortcut::activated, this, &PreviewCard::showNextImage);
    }

    updateImageIndexLabel();
}

void PreviewCard::updateImageIndexLabel()
{
    if (!m_indexLabel)
        return;
    if (m_pixmaps.size() <= 1)
    {
        m_indexLabel->hide();
        m_indexLabel->clear();
        if (m_prevBtn)
            m_prevBtn->setEnabled(false);
        if (m_nextBtn)
            m_nextBtn->setEnabled(false);
        return;
    }
    m_indexLabel->show();
    m_indexLabel->setText(QString("%1/%2").arg(m_currentImageIndex + 1).arg(m_pixmaps.size()));
    if (m_prevBtn)
        m_prevBtn->setEnabled(m_currentImageIndex > 0);
    if (m_nextBtn)
        m_nextBtn->setEnabled(m_currentImageIndex < m_pixmaps.size() - 1);
}

void PreviewCard::showPreviousImage()
{
    if (m_pixmaps.size() <= 1 || m_currentImageIndex <= 0)
        return;
    --m_currentImageIndex;
    setImage(m_pixmaps.value(m_currentImageIndex));
}

void PreviewCard::showNextImage()
{
    if (m_pixmaps.size() <= 1 || m_currentImageIndex >= m_pixmaps.size() - 1)
        return;
    ++m_currentImageIndex;
    setImage(m_pixmaps.value(m_currentImageIndex));
}

bool PreviewCard::matchesHotkey(QKeyEvent *event, const QString &hotkey) const
{
    const QString normalized = normalizeHotkey(hotkey);
    if (normalized.isEmpty())
        return false;

    const QString eventHotkey = normalizeHotkey(QKeySequence(event->modifiers() | event->key()).toString(QKeySequence::NativeText));
    if (eventHotkey == normalized)
        return true;

    return normalizeHotkey(event->text()) == normalized;
}

void PreviewCard::keyPressEvent(QKeyEvent *event)
{
    if (m_pixmaps.size() > 1 && matchesHotkey(event, m_prevHotkey))
    {
        showPreviousImage();
        event->accept();
        return;
    }

    if (m_pixmaps.size() > 1 && matchesHotkey(event, m_nextHotkey))
    {
        showNextImage();
        event->accept();
        return;
    }

    QWidget::keyPressEvent(event);
}

void PreviewCard::wheelEvent(QWheelEvent *event)
{
    int delta = event->angleDelta().y();
    if (delta == 0)
        return;

    double factor = (m_zoomSensitivity / 2000.0);
    if (factor < 0.01)
        factor = 0.01;

    double mul = (delta > 0) ? (1.0 + factor) : (1.0 / (1.0 + factor));
    m_scale *= mul;

    double minScaleW = (double)MIN_IMG_SIDE / (double)m_baseSize.width();
    double minScaleH = (double)MIN_IMG_SIDE / (double)m_baseSize.height();
    double minScale = qMax(minScaleW, minScaleH);

    if (m_scale < minScale)
        m_scale = minScale;

    applyScale();
    event->accept();
}
