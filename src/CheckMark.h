// "Cursor" checkmark — painted compound control (box + label), no QSS hacks.
#pragma once

#include <QWidget>
#include <QPainter>
#include <QLinearGradient>
#include <QFont>
#include <QMouseEvent>
#include <functional>

class CheckMarkBox : public QWidget {
public:
    explicit CheckMarkBox(QWidget* parent = nullptr) : QWidget(parent) {
        setFixedSize(96, 30);
        setCursor(Qt::PointingHandCursor);
        setToolTip(QStringLiteral("Bake the mouse cursor into the recording"));
    }

    void setChecked(bool checked) {
        if (m_checked == checked) return;
        m_checked = checked;
        update();
    }
    bool isChecked() const { return m_checked; }

    std::function<void(bool)> onToggled;

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        const QRectF box(0, (height() - 18) / 2.0, 18, 18);
        if (m_checked) {
            QLinearGradient g(0, 0, box.width(), box.height());
            g.setColorAt(0.0, QColor(0x8B, 0x5C, 0xF6));
            g.setColorAt(1.0, QColor(0x22, 0xD3, 0xEE));
            p.setPen(Qt::NoPen);
            p.setBrush(g);
            p.drawRoundedRect(box, 5.5, 5.5);
            // White checkmark stroke
            QPen pen(QColor(0xFF, 0xFF, 0xFF), 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            p.setPen(pen);
            p.setBrush(Qt::NoBrush);
            QPainterPath check;
            check.moveTo(box.left() + 4.2, box.center().y() + 0.4);
            check.lineTo(box.left() + 8.0, box.center().y() + 4.0);
            check.lineTo(box.left() + 14.0, box.center().y() - 4.4);
            p.drawPath(check);
        } else {
            p.setPen(QPen(m_hover ? QColor(0x39, 0x39, 0x4A) : QColor(0x2A, 0x2A, 0x36), 1.4));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(box.adjusted(0.7, 0.7, -0.7, -0.7), 5.5, 5.5);
        }

        QFont f = p.font();
        f.setPixelSize(11);
        p.setFont(f);
        p.setPen(QColor(0xC9, 0xC9, 0xD6));
        p.drawText(QRectF(box.right() + 8, 0, width() - box.right() - 8, height()),
                   Qt::AlignVCenter | Qt::AlignLeft,
                   QStringLiteral("Cursor"));
    }

    void enterEvent(QEnterEvent*) override { m_hover = true; update(); }
    void leaveEvent(QEvent*) override      { m_hover = false; update(); }

    void mousePressEvent(QMouseEvent* ev) override {
        if (ev->button() == Qt::LeftButton) {
            m_checked = !m_checked;
            update();
            if (onToggled) onToggled(m_checked);
        }
    }

private:
    bool m_checked = true;
    bool m_hover = false;
};
