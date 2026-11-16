#include "WaveformWidget.h"
#include "Style.h"

#include <QPainter>
#include <QPainterPath>
#include <QSizePolicy>
#include <QLinearGradient>
#include <QtMath>
#include <cmath>

namespace {
static float shapePeak(float peak) {
    // Lift the noise floor a touch so quiet passages still have texture.
    return qBound(0.0f, (float)std::pow(qMax(peak, 0.0f), 0.72f) * 1.12f, 1.0f);
}
} // namespace

WaveformWidget::WaveformWidget(QWidget* parent) : QWidget(parent) {
    setMinimumHeight(56);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void WaveformWidget::pushPeak(float peak) {
    pushPeaks({peak});
}

void WaveformWidget::pushPeaks(const std::vector<float>& peaks) {
    for (float p : peaks) {
        // Fast attack, slow decay envelope on each bin.
        const float shaped = shapePeak(p);
        if (shaped > m_envelope) m_envelope = shaped;
        else                     m_envelope += (shaped - m_envelope) * 0.22f;
        m_bins.push_back(m_envelope);
        while ((int)m_bins.size() > 240) m_bins.pop_front();
    }
    if (!peaks.empty()) update();
}

void WaveformWidget::clear() {
    m_bins.clear();
    m_envelope = 0.0f;
    update();
}

void WaveformWidget::setActive(bool active) {
    if (m_active == active) return;
    m_active = active;
    if (!m_active) m_envelope = 0.0f;
    update();
}

void WaveformWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF r = rect();
    const qreal cy = r.height() / 2.0;
    const qreal maxHalf = (r.height() - 14.0) / 2.0;

    // Panel
    QPainterPath panel;
    panel.addRoundedRect(r, 12, 12);
    QLinearGradient panelBg(0, 0, 0, r.height());
    panelBg.setColorAt(0.0, QColor(0x1E, 0x20, 0x26));
    panelBg.setColorAt(1.0, QColor(0x18, 0x1A, 0x20));
    p.fillPath(panel, panelBg);

    p.setPen(QPen(QColor(255, 255, 255, 12), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), 12, 12);

    // dB gridlines at roughly -12 dB and -24 dB (faint)
    auto drawGrid = [&](qreal level, int alpha) {
        const qreal half = maxHalf * (1.0 - level);
        p.setPen(QPen(QColor(0x3A, 0x3F, 0x4B, alpha), 1, Qt::SolidLine));
        p.drawLine(QPointF(14, cy - half), QPointF(r.width() - 14, cy - half));
        p.drawLine(QPointF(14, cy + half), QPointF(r.width() - 14, cy + half));
    };
    drawGrid(0.25, 90);   // ~ -12 dB
    drawGrid(0.50, 45);   // ~ -24 dB

    // Center line
    p.setPen(QPen(QColor(0x39, 0x3F, 0x4C), 1, Qt::DashLine));
    p.drawLine(QPointF(14, cy), QPointF(r.width() - 14, cy));

    const int barCount = 96;
    const qreal slot = (r.width() - 28.0) / barCount;
    const qreal barW = qMax(2.2, slot * 0.52);

    if (m_bins.empty()) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0x2B, 0x2E, 0x38));
        p.drawRoundedRect(QRectF(14, cy - 1.5, r.width() - 28, 3), 1.5, 1.5);
        return;
    }

    const int n = (int)m_bins.size();
    for (int i = 0; i < barCount; ++i) {
        const int bin = (int)((qint64)i * n / barCount);
        const float v = qBound(0.0f, m_bins[(size_t)bin], 1.0f);
        const qreal half = qMax<qreal>(1.6, v * maxHalf);

        // Newest bars are brighter; shape each bar with a vertical white gradient.
        const qreal age = (n <= 1) ? 1.0 : (qreal)bin / (n - 1);
        const int alpha = int(105 + 150 * age);

        QLinearGradient g(0, cy - half, 0, cy + half);
        g.setColorAt(0.0, QColor(0xF2, 0xF4, 0xF7, alpha));   // light near tops
        g.setColorAt(0.45, QColor(0xC7, 0xCB, 0xD4, alpha));
        g.setColorAt(1.0, QColor(0x6E, 0x74, 0x82, alpha));

        p.setPen(Qt::NoPen);
        p.setBrush(g);
        const qreal x = 14 + i * slot + (slot - barW) / 2.0;
        p.drawRoundedRect(QRectF(x, cy - half, barW, half * 2.0), barW / 2.0, barW / 2.0);
    }

    // Slight panel glow on hot signal (soft white rim)
    const float recent = m_bins.back();
    if (recent > 0.55f) {
        QColor glow(0xE9, 0xEC, 0xF1);
        glow.setAlpha((int)((recent - 0.55f) * 70));
        p.setPen(QPen(glow, 1.6));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(r.adjusted(0.8, 0.8, -0.8, -0.8), 12, 12);
    }
}
