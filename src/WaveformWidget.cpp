#include "WaveformWidget.h"
#include "Style.h"

#include <QPainter>
#include <QPainterPath>
#include <QSizePolicy>
#include <QLinearGradient>
#include <QtMath>
#include <cmath>

WaveformWidget::WaveformWidget(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(56);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

static float shapePeak(float peak) {
    return qBound(0.0f, (float)std::pow(qMax(peak, 0.0f), 0.72f) * 1.12f, 1.0f);
}

void WaveformWidget::pushPeak(float peak) {
    m_bins.push_back(shapePeak(peak));
    while ((int)m_bins.size() > 240) m_bins.pop_front();
    update();
}

void WaveformWidget::pushPeaks(const std::vector<float>& peaks) {
    for (float p : peaks) {
        m_bins.push_back(shapePeak(p));
        while ((int)m_bins.size() > 240) m_bins.pop_front();
    }
    if (!peaks.empty()) update();
}

void WaveformWidget::clear() {
    m_bins.clear();
    update();
}

void WaveformWidget::setActive(bool active) {
    m_active = active;
    update();
}

void WaveformWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF r = rect();
    const qreal cy = r.height() / 2.0;
    const qreal maxHalf = (r.height() - 14.0) / 2.0;   // bar half-height at full scale

    // Panel
    QPainterPath panel;
    panel.addRoundedRect(r, 12, 12);
    QLinearGradient panelBg(0, 0, 0, r.height());
    panelBg.setColorAt(0.0, QColor(0x16, 0x16, 0x1F));
    panelBg.setColorAt(1.0, QColor(0x11, 0x11, 0x17));
    p.fillPath(panel, panelBg);

    // Hairline border
    p.setPen(QPen(QColor(255, 255, 255, 13), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), 12, 12);

    // Center guide line
    p.setPen(QPen(QColor(0x2A, 0x2A, 0x36), 1, Qt::DashLine));
    p.drawLine(QPointF(14, cy), QPointF(r.width() - 14, cy));

    const int barCount = 96;                  // bars to render (bins are resampled)
    const qreal slot = (r.width() - 28.0) / barCount;
    const qreal barW = qMax(2.2, slot * 0.52);

    if (m_bins.empty()) {
        // Idle: dim flat pill
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0x2B, 0x2B, 0x38));
        p.drawRoundedRect(QRectF(14, cy - 1.5, r.width() - 28, 3), 1.5, 1.5);
        return;
    }

    // Resample the rolling history to barCount bars (nearest, newest at right).
    const int n = (int)m_bins.size();
    for (int i = 0; i < barCount; ++i) {
        const int bin = (int)((qint64)i * n / barCount);
        const float v = qBound(0.0f, m_bins[(size_t)bin], 1.0f);
        const qreal half = qMax<qreal>(1.6, v * maxHalf);

        // Newest bars brighter.
        const qreal age = (n <= 1) ? 1.0 : (qreal)(bin) / (n - 1);
        const int alpha = int(110 + 145 * age);

        QLinearGradient g(0, cy - half, 0, cy + half);
        QColor top(az::CYAN);
        QColor mid(az::VIOLET);
        QColor bot(az::VIOLET);
        top.setAlpha(alpha);
        mid.setAlpha(alpha);
        bot.setAlpha(alpha);
        g.setColorAt(0.0, top);
        g.setColorAt(0.45, mid);
        g.setColorAt(1.0, QColor(0x4C, 0x2F, 0xB8, alpha));

        p.setPen(Qt::NoPen);
        p.setBrush(g);
        const qreal x = 14 + i * slot + (slot - barW) / 2.0;
        p.drawRoundedRect(QRectF(x, cy - half, barW, half * 2.0), barW / 2.0, barW / 2.0);
    }

    // Subtle outer glow on strong signal (cheap: translucent ellipse at right edge).
    const float recent = m_bins.back();
    if (recent > 0.55f) {
        QColor glow(az::VIOLET);
        glow.setAlpha((int)((recent - 0.55f) * 90));
        p.setBrush(glow);
        p.drawRoundedRect(r, 12, 12);
    }
}
