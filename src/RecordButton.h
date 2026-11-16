// Big circular record/stop button: gradient ring, morphing inner glyph,
// breathing glow while recording. Fully custom-painted, no icon fonts.
#pragma once

#include <QWidget>
#include <QTimer>
#include <functional>

class RecordButton : public QWidget {
public:
    enum class Visual { Idle, Recording, Paused };   // Paused renders like Recording

    explicit RecordButton(QWidget* parent = nullptr);

    void setVisual(Visual v);
    Visual visual() const { return m_visual; }
    std::function<void()> onClicked;

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void enterEvent(QEnterEvent*) override;
    void leaveEvent(QEvent*) override;

private:
    Visual m_visual = Visual::Idle;
    bool   m_hover  = false;
    bool   m_pressed = false;
    qreal  m_pressScale = 1.0;   // eased towards 0.9 on press, back to 1 on release
    QTimer m_anim;
    int    m_phase = 0;
};
