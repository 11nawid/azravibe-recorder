// Small painted icon button used for the output-folder affordance.
#pragma once

#include <QWidget>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QLinearGradient>
#include <functional>

class FolderButton : public QWidget {
public:
    explicit FolderButton(QWidget* parent = nullptr) : QWidget(parent) {
        setFixedSize(34, 34);
        setCursor(Qt::PointingHandCursor);
        setToolTip(QStringLiteral("Choose output folder"));
    }

    std::function<void()> onClicked;

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        const QRectF r = rect().adjusted(1, 1, -1, -1);
        p.setPen(QPen(m_hover ? QColor(0x33, 0x33, 0x42) : QColor(0x23, 0x23, 0x2F), 1));
        p.setBrush(m_pressed ? QColor(0x1F, 0x1F, 0x2A) : (m_hover ? QColor(0x1E, 0x1E, 0x28) : QColor(0x1A, 0x1A, 0x23)));
        p.drawRoundedRect(r, 10, 10);

        // Folder glyph: matte slate strokes
        QLinearGradient g(0, 0, width(), 0);
        g.setColorAt(0.0, QColor(0xC7, 0xCB, 0xD4));
        g.setColorAt(1.0, QColor(0x8A, 0x91, 0xA1));
        QPen pen(QBrush(g), 1.7, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);

        const QRectF f(rect().center().x() - 8.5, rect().center().y() - 6.0, 17, 12.5);
        // Tab
        QPainterPath tab;
        tab.moveTo(f.left(), f.top() + 1.2);
        tab.lineTo(f.left() + 4.6, f.top() + 1.2);
        tab.lineTo(f.left() + 6.6, f.top() - 1.6);
        tab.lineTo(f.left() + 11.5, f.top() - 1.6);
        tab.lineTo(f.left() + 13.2, f.top() + 1.2);
        tab.lineTo(f.right(), f.top() + 1.2);
        p.drawPath(tab);
        // Body
        p.drawRoundedRect(f.translated(0, 3.2), 2.6, 2.6);
    }

    void enterEvent(QEnterEvent*) override { m_hover = true; update(); }
    void leaveEvent(QEvent*) override      { m_hover = false; m_pressed = false; update(); }

    void mousePressEvent(QMouseEvent* ev) override {
        if (ev->button() == Qt::LeftButton) { m_pressed = true; update(); }
    }
    void mouseReleaseEvent(QMouseEvent* ev) override {
        if (ev->button() == Qt::LeftButton) {
            const bool was = m_pressed;
            m_pressed = false;
            update();
            if (was && rect().contains(ev->position().toPoint()) && onClicked)
                onClicked();
        }
    }

private:
    bool m_hover = false, m_pressed = false;
};
