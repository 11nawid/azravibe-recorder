// QComboBox that paints its own chevron and lets the owner refresh its
// model right before the popup appears (target/window lists change often).
#pragma once

#include <QComboBox>
#include <QPainter>
#include <QPaintEvent>
#include <QPen>
#include <functional>

class RefreshComboBox : public QComboBox {
public:
    std::function<void()> onAboutToShow;   // called from showPopup()

    explicit RefreshComboBox(QWidget* parent = nullptr) : QComboBox(parent) {}

protected:
    void showPopup() override {
        if (onAboutToShow) onAboutToShow();
        QComboBox::showPopup();
    }

    void paintEvent(QPaintEvent* ev) override {
        QComboBox::paintEvent(ev);
        // Chevron: two short strokes.
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QColor c = isEnabled() ? QColor(0xA9, 0xA9, 0xBC) : QColor(0x55, 0x55, 0x66);
        QPen pen(c, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(pen);
        const qreal cx = width() - 18.0;
        const qreal cy = height() / 2.0;
        p.drawLine(QPointF(cx - 4.5, cy - 2.5), QPointF(cx, cy + 2.5));
        p.drawLine(QPointF(cx, cy + 2.5),       QPointF(cx + 4.5, cy - 2.5));
    }
};
