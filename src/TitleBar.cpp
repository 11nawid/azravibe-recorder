#include "TitleBar.h"
#include "Style.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QMouseEvent>
#include <QWindow>
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QFont>

// ---------------- LogoMark ----------------

LogoMark::LogoMark(QWidget* parent) : QWidget(parent) {
    setFixedSize(24, 24);
}

void LogoMark::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QRectF r = rect();
    QLinearGradient g(0, 0, r.width(), r.height());
    g.setColorAt(0.0, QColor(0x2A2E37));
    g.setColorAt(1.0, QColor(0x1D2027));

    QPainterPath path;
    path.addRoundedRect(r, 7, 7);
    p.fillPath(path, g);
    p.setPen(QPen(QColor(255, 255, 255, 22), 1));
    p.drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), 7, 7);

    // Waveform bars (light gray) + red record dot
    const qreal cx = r.center().x();
    const qreal cy = r.center().y();
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0xE9, 0xEC, 0xF1));
    const qreal heights[5] = {6, 11, 15, 9, 5};
    const qreal barW = 2.6, gap = 2.2;
    const qreal totalW = 5 * barW + 4 * gap;
    qreal x = cx - totalW / 2.0;
    for (int i = 0; i < 5; ++i) {
        const qreal h = heights[i];
        p.drawRoundedRect(QRectF(x, cy - h / 2.0, barW, h), barW / 2.0, barW / 2.0);
        x += barW + gap;
    }

    // Small red record dot top-right
    p.setBrush(QColor(az::RED));
    p.drawEllipse(QPointF(r.width() - 5.5, 5.5), 3.0, 3.0);
}

// ---------------- TitleBar ----------------

QIcon TitleBar::makeMinimizeIcon() {
    QPixmap pm(20, 20);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(QColor(0xC9, 0xC9, 0xD6), 1.6, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(QPointF(5.5, 10), QPointF(14.5, 10));
    return QIcon(pm);
}

QIcon TitleBar::makeCloseIcon(bool hover) {
    QPixmap pm(20, 20);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(hover ? QColor(0xFF, 0x8A, 0x96) : QColor(0xC9, 0xC9, 0xD6),
                  1.6, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(QPointF(6, 6),  QPointF(14, 14));
    p.drawLine(QPointF(14, 6), QPointF(6, 14));
    return QIcon(pm);
}

TitleBar::TitleBar(QWidget* parent) : QWidget(parent) {
    setObjectName(QStringLiteral("titlebar"));
    setFixedHeight(46);

    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(16, 0, 10, 0);
    row->setSpacing(9);

    m_logo = new LogoMark(this);
    row->addWidget(m_logo, 0, Qt::AlignVCenter);

    auto* name = new QLabel(this);
    name->setObjectName(QStringLiteral("appName"));
    QFont f = name->font();
    f.setPixelSize(12);
    f.setWeight(QFont::DemiBold);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 2.4);
    name->setFont(f);
    name->setText(QStringLiteral("AZRAVIBE"));
    row->addWidget(name, 0, Qt::AlignVCenter);

    auto* nameFaint = new QLabel(this);
    nameFaint->setObjectName(QStringLiteral("appNameFaint"));
    QFont f2 = f;
    f2.setWeight(QFont::Normal);
    nameFaint->setFont(f2);
    nameFaint->setText(QStringLiteral("RECORDER"));
    row->addWidget(nameFaint, 0, Qt::AlignVCenter);

    row->addStretch(1);

    auto* minBtn = new QToolButton(this);
    minBtn->setObjectName(QStringLiteral("iconButton"));
    minBtn->setIcon(makeMinimizeIcon());
    minBtn->setIconSize(QSize(20, 20));
    minBtn->setFixedSize(40, 30);
    minBtn->setToolTip(QStringLiteral("Minimize"));
    connect(minBtn, &QToolButton::clicked, this, [this] { if (onMinimize) onMinimize(); });
    row->addWidget(minBtn, 0, Qt::AlignVCenter);

    auto* closeBtn = new QToolButton(this);
    closeBtn->setObjectName(QStringLiteral("closeButton"));
    closeBtn->setIcon(makeCloseIcon(false));
    closeBtn->setIconSize(QSize(20, 20));
    closeBtn->setFixedSize(40, 30);
    closeBtn->setToolTip(QStringLiteral("Close"));
    connect(closeBtn, &QToolButton::clicked, this, [this] { if (onClose) onClose(); });
    row->addWidget(closeBtn, 0, Qt::AlignVCenter);
}

void TitleBar::mousePressEvent(QMouseEvent* ev) {
    if (ev->button() == Qt::LeftButton) {
        if (QWidget* top = window()) {
            if (QWindow* handle = top->windowHandle())
                handle->startSystemMove();
        }
        return;
    }
    QWidget::mousePressEvent(ev);
}
