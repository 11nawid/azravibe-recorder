#include "RecordButton.h"
#include "Style.h"

#include <QPainter>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QtMath>
#include <QMouseEvent>

RecordButton::RecordButton(QWidget* parent) : QWidget(parent) {
    setFixedSize(88, 88);
    setCursor(Qt::PointingHandCursor);
    m_anim.setInterval(33);
    connect(&m_anim, &QTimer::timeout, this, [this] {
        ++m_phase;
        const qreal target = m_pressed ? 0.88 : 1.0;
        m_pressScale += (target - m_pressScale) * 0.35;
        update();
    });
}

void RecordButton::setVisual(Visual v) {
    if (v == m_visual) return;
    m_visual = v;
    if (v == Visual::Idle) m_anim.stop();
    else if (!m_anim.isActive()) m_anim.start();
    update();
}

void RecordButton::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF r = rect();
    const QPointF c = r.center();
    const qreal outerR = r.width() / 2.0 - 3.0;
    const bool live = m_visual != Visual::Idle;

    // ---- breathing halo while recording (slate wash) ----
    if (live) {
        const qreal pulse = 0.5 + 0.5 * qSin(m_phase * 0.16);
        QRadialGradient halo(c, outerR + 10);
        QColor edge(0x9A, 0xA3, 0xB2);
        edge.setAlpha(int(36 + 46 * pulse));
        halo.setColorAt(0.55, QColor(154, 163, 178, 0));
        halo.setColorAt(0.8, edge);
        halo.setColorAt(1.0, QColor(154, 163, 178, 0));
        p.fillRect(r, halo);
    }

    // ---- ring: monochrome steel gradient (light top → mid gray bottom) ----
    QLinearGradient ring(r.topLeft(), r.bottomRight());
    if (live) {
        ring.setColorAt(0.0, QColor(0xD9, 0xDE, 0xE6));
        ring.setColorAt(1.0, QColor(0x8B, 0x92, 0xA2));
    } else {
        ring.setColorAt(0.0, QColor(0xE9, 0xEC, 0xF1));
        ring.setColorAt(1.0, QColor(0x6E, 0x76, 0x86));
    }

    QPen ringPen(QBrush(ring), 4.6);
    p.setPen(ringPen);
    p.setBrush(QColor(0x1D, 0x20, 0x27));
    p.drawEllipse(c, outerR, outerR);

    if (m_hover) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 255, 12));
        p.drawEllipse(c, outerR, outerR);
    }

    // ---- inner glyph (scale animated) ----
    const qreal inner = 22.0 * m_pressScale;
    p.save();
    p.translate(c);
    p.scale(m_pressScale, m_pressScale);
    if (!live) {
        // Idle: record dot (the one red cue)
        QRadialGradient dot(QPointF(-inner * 0.25, -inner * 0.25), inner * 1.6);
        dot.setColorAt(0.0, QColor(0xFF, 0x6B, 0x7A));
        dot.setColorAt(1.0, QColor(az::RED));
        p.setPen(Qt::NoPen);
        p.setBrush(dot);
        p.drawEllipse(QPointF(0, 0), inner, inner);
    } else {
        // Recording: stop square (rounded)
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(az::RED));
        const qreal half = inner * 0.78;
        p.drawRoundedRect(QRectF(-half, -half, half * 2, half * 2), 5.5, 5.5);
    }
    p.restore();
}

void RecordButton::mousePressEvent(QMouseEvent* ev) {
    if (ev->button() == Qt::LeftButton) {
        m_pressed = true;
        if (!m_anim.isActive()) m_anim.start();
        update();
    }
}

void RecordButton::mouseReleaseEvent(QMouseEvent* ev) {
    if (ev->button() == Qt::LeftButton && m_pressed) {
        m_pressed = false;
        update();
        if (rect().contains(ev->position().toPoint()) && onClicked)
            onClicked();
    }
}

void RecordButton::enterEvent(QEnterEvent*) { m_hover = true;  update(); }
void RecordButton::leaveEvent(QEvent*)      { m_hover = false; m_pressed = false; update(); }
