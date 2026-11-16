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

    // ---- breathing halo while recording ----
    if (live) {
        const qreal pulse = 0.5 + 0.5 * qSin(m_phase * 0.16);
        QRadialGradient halo(c, outerR + 10);
        QColor edge(az::RED);
        edge.setAlpha(int(40 + 50 * pulse));
        halo.setColorAt(0.55, QColor(255, 77, 94, 0));
        halo.setColorAt(0.8, edge);
        halo.setColorAt(1.0, QColor(255, 77, 94, 0));
        p.fillRect(r, halo);
    }

    // ---- ring ----
    QLinearGradient ring(r.topLeft(), r.bottomRight());
    if (live) {
        QColor a(az::RED);
        QColor b(QStringLiteral("#FF8A5C"));
        ring.setColorAt(0.0, a);
        ring.setColorAt(1.0, b);
    } else {
        ring.setColorAt(0.0, QColor(az::VIOLET));
        ring.setColorAt(1.0, QColor(az::CYAN));
    }

    QPen ringPen(QBrush(ring), 4.6);
    p.setPen(ringPen);
    p.setBrush(QColor(0x17, 0x17, 0x20));
    p.drawEllipse(c, outerR, outerR);

    // Hover: soft lighten
    if (m_hover) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 255, 14));
        p.drawEllipse(c, outerR, outerR);
    }

    // ---- inner glyph (scale animated) ----
    const qreal inner = 22.0 * m_pressScale;
    p.save();
    p.translate(c);
    p.scale(m_pressScale, m_pressScale);
    if (!live) {
        // Idle: record dot
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
