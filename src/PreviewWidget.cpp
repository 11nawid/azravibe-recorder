#include "PreviewWidget.h"
#include "Style.h"

#include <QPainter>
#include <QPainterPath>
#include <QFontMetrics>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QtMath>

PreviewWidget::PreviewWidget(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_OpaquePaintEvent, false);
    setMinimumSize(120, 90);
    m_phaseTimer.setInterval(33);   // ~30 fps blink/fade while visible
    connect(&m_phaseTimer, &QTimer::timeout, this, [this] {
        ++m_phase;
        if (m_toastAlpha > 0.0) {
            m_toastAlpha = qMax(0.0, m_toastAlpha - 0.028);
            if (m_toastAlpha == 0.0) m_toast.clear();
        }
        update();
    });
}

void PreviewWidget::setFrame(const QImage& frame) {
    m_frame = frame;
    update();
}

void PreviewWidget::setBadge(Badge badge) {
    if (badge == m_badge) return;
    m_badge = badge;
    if (badge != Badge::None || !m_toast.isEmpty()) m_phaseTimer.start();
    else if (m_toast.isEmpty()) m_phaseTimer.stop();
    update();
}

void PreviewWidget::setElapsedUs(int64_t us) {
    m_elapsedUs = us;
    // No explicit repaint here; the badge repaint cadence follows the phase timer.
}

void PreviewWidget::clearFrame() {
    m_frame = QImage{};
    m_badge = Badge::None;
    m_elapsedUs = 0;
    update();
}

void PreviewWidget::showToast(const QString& text) {
    m_toast = text;
    m_toastAlpha = 1.0;
    m_phaseTimer.start();
    update();
}

QString PreviewWidget::formatTime(int64_t us) {
    const int64_t totalSeconds = us / 1000000;
    const int h = (int)(totalSeconds / 3600);
    const int m = (int)((totalSeconds % 3600) / 60);
    const int s = (int)(totalSeconds % 60);
    if (h > 0)
        return QStringLiteral("%1:%2:%3")
            .arg(h)
            .arg(m, 2, 10, QChar('0'))
            .arg(s, 2, 10, QChar('0'));
    return QStringLiteral("%1:%2").arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'));
}

void PreviewWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    const QRectF outer = rect();
    QPainterPath clip;
    clip.addRoundedRect(outer, 16, 16);
    p.setClipPath(clip);

    // ---- background (flat near-black) ----
    QLinearGradient bg(0, 0, 0, height());
    bg.setColorAt(0.0, QColor(0x17, 0x19, 0x1F));
    bg.setColorAt(1.0, QColor(0x11, 0x12, 0x17));
    p.fillRect(outer, bg);

    // ---- content ----
    if (!m_frame.isNull()) {
        const double scale = qMin(width()  / (double)m_frame.width(),
                                  height() / (double)m_frame.height());
        const int iw = (int)(m_frame.width()  * scale);
        const int ih = (int)(m_frame.height() * scale);
        const int ix = (width()  - iw) / 2;
        const int iy = (height() - ih) / 2;
        p.drawImage(QRect(ix, iy, iw, ih), m_frame);
    } else {
        // Idle artwork: flat slate monogram + monitor outline
        p.save();
        p.translate(width() / 2.0, height() / 2.0);

        QLinearGradient glyph(QPointF(-80, 0), QPointF(80, 0));
        glyph.setColorAt(0.0, QColor(0xC9, 0xCD, 0xD6));
        glyph.setColorAt(1.0, QColor(0x7B, 0x82, 0x92));

        QPen pen(QBrush(glyph), 3.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);

        const QRectF screen(-84, -52, 168, 104);
        p.drawRoundedRect(screen, 10, 10);
        p.drawLine(QPointF(0, 52), QPointF(0, 70));
        p.drawLine(QPointF(-34, 70), QPointF(34, 70));

        QPainterPath play;
        play.moveTo(-12, -14);
        play.lineTo(16, 0);
        play.lineTo(-12, 14);
        play.closeSubpath();
        p.fillPath(play, QBrush(glyph));
        p.restore();
    }

    // ---- inner hairline ----
    p.setPen(QPen(QColor(255, 255, 255, 18), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(outer.adjusted(0.5, 0.5, -0.5, -0.5), 16, 16);

    // ---- top-left badge (REC / PAUSED + time) ----
    if (m_badge != Badge::None) {
        const bool rec = m_badge == Badge::Recording;
        const QString timeText = formatTime(m_elapsedUs);

        p.setPen(Qt::NoPen);
        QFont font = p.font();
        font.setPixelSize(11);
        font.setBold(true);
        font.setLetterSpacing(QFont::AbsoluteSpacing, 1.1);
        p.setFont(font);
        const QFontMetrics fm(font);

        const QString label = rec ? QStringLiteral("REC") : QStringLiteral("PAUSED");
        const int dotD   = 9;
        const int padX   = 12, padY = 7, gap = 8;
        const int labelW = fm.horizontalAdvance(label);
        const int timeW  = fm.horizontalAdvance(timeText);
        const int chipW  = padX + dotD + gap + labelW + 10 + timeW + padX;
        const int chipH  = 30;

        const QRectF chip(12, 12, chipW, chipH);
        QPainterPath chipPath;
        chipPath.addRoundedRect(chip, 15, 15);
        p.setBrush(QColor(10, 10, 16, 178));
        p.drawPath(chipPath);

        // Blinking dot (paused = solid amber)
        const qreal blink = rec ? (0.55 + 0.45 * qSin(m_phase * 0.22)) : 1.0;
        QColor dot = rec ? QColor(az::RED) : QColor(az::AMBER);
        dot.setAlphaF(blink);
        p.setBrush(dot);
        p.drawEllipse(QPointF(chip.left() + padX + dotD / 2.0, chip.center().y()), dotD / 2.0, dotD / 2.0);

        p.setPen(QColor(az::TEXT));
        p.drawText(QPointF(chip.left() + padX + dotD + gap, chip.center().y() + fm.ascent() / 2.0 - 1), label);

        p.setPen(QColor(0xC9, 0xC9, 0xD6));
        p.drawText(QPointF(chip.left() + padX + dotD + gap + labelW + 10, chip.center().y() + fm.ascent() / 2.0 - 1), timeText);
    }

    // ---- bottom toast (saved / info) ----
    if (m_toastAlpha > 0.01 && !m_toast.isEmpty()) {
        QFont font = p.font();
        font.setPixelSize(11);
        font.setWeight(QFont::Medium);
        p.setFont(font);
        const QFontMetrics fm(font);

        const QString elided = fm.elidedText(m_toast, Qt::ElideMiddle, qMax(200, width() - 80));
        const int padX = 14, padY = 8;
        const int chipW = fm.horizontalAdvance(elided) + padX * 2 + 20;
        const int chipH = 30;
        const QRectF chip((width() - chipW) / 2.0, height() - chipH - 14, chipW, chipH);

        QColor bg(13, 13, 19, int(210 * m_toastAlpha));
        QColor fg(az::TEXT); fg.setAlphaF(m_toastAlpha);

        QPainterPath chipPath;
        chipPath.addRoundedRect(chip, 15, 15);
        p.setPen(Qt::NoPen);
        p.setBrush(bg);
        p.drawPath(chipPath);

        // Check glyph
        QPen checkPen(QColor(0xE9, 0xEC, 0xF1), 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(checkPen);
        p.setBrush(Qt::NoBrush);
        const qreal cy = chip.center().y();
        const QPointF c0(chip.left() + padX, cy), c1(chip.left() + padX + 4.5, cy + 4.5), c2(chip.left() + padX + 12, cy - 5);
        QPainterPath check;
        check.moveTo(c0); check.lineTo(c1); check.lineTo(c2);
        p.drawPath(check);

        p.setPen(fg);
        p.drawText(QPointF(chip.left() + padX + 20, cy + fm.ascent() / 2.0 - 1), elided);
    }
}
