#include "PauseButton.h"
#include "Style.h"

#include <QPainter>
#include <QMouseEvent>
#include <QPainterPath>

PauseButton::PauseButton(QWidget* parent) : QWidget(parent) {
    setFixedSize(56, 56);
    setCursor(Qt::PointingHandCursor);
}

void PauseButton::setVisual(Visual v) {
    if (v == m_visual) return;
    m_visual = v;
    setCursor(v == Visual::Disabled ? Qt::ArrowCursor : Qt::PointingHandCursor);
    update();
}

void PauseButton::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF r = rect().adjusted(2, 2, -2, -2);
    const QPointF c = r.center();
    const bool enabled = m_visual != Visual::Disabled;

    // Body
    p.setPen(QPen(enabled ? QColor(0x2B, 0x2B, 0x38) : QColor(0x1E, 0x1E, 0x27), 1.2));
    p.setBrush(enabled
               ? (m_pressed ? QColor(0x1F, 0x1F, 0x2A) : QColor(0x1A, 0x1A, 0x23))
               : QColor(0x12, 0x12, 0x18));
    p.drawEllipse(r);

    if (m_hover && enabled) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 255, 10));
        p.drawEllipse(r);
    }

    const QColor glyph = enabled ? QColor(az::TEXT) : QColor(0x45, 0x45, 0x54);
    p.setPen(Qt::NoPen);
    p.setBrush(glyph);

    if (m_visual == Visual::Paused) {
        // Resume: play triangle
        QPainterPath tri;
        tri.moveTo(c.x() - 5.5, c.y() - 8.0);
        tri.lineTo(c.x() + 8.0, c.y());
        tri.lineTo(c.x() - 5.5, c.y() + 8.0);
        tri.closeSubpath();
        p.drawPath(tri);
    } else {
        // Recording: two pause bars (dimmed when disabled)
        const qreal bw = 5.0, bh = 17.0, gap = 6.0;
        p.drawRoundedRect(QRectF(c.x() - gap / 2 - bw, c.y() - bh / 2, bw, bh), 1.6, 1.6);
        p.drawRoundedRect(QRectF(c.x() + gap / 2,      c.y() - bh / 2, bw, bh), 1.6, 1.6);
    }
}

void PauseButton::mousePressEvent(QMouseEvent* ev) {
    if (ev->button() == Qt::LeftButton && m_visual != Visual::Disabled) {
        m_pressed = true;
        update();
    }
}

void PauseButton::mouseReleaseEvent(QMouseEvent* ev) {
    if (ev->button() == Qt::LeftButton) {
        const bool wasPressed = m_pressed;
        m_pressed = false;
        update();
        if (wasPressed && m_visual != Visual::Disabled
            && rect().contains(ev->position().toPoint()) && onClicked)
            onClicked();
    }
}

void PauseButton::enterEvent(QEnterEvent*) { m_hover = true;  update(); }
void PauseButton::leaveEvent(QEvent*)      { m_hover = false; m_pressed = false; update(); }
