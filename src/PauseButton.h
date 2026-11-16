// Small circular pause/resume button. Disabled (dimmed) while idle.
#pragma once

#include <QWidget>
#include <functional>

class PauseButton : public QWidget {
public:
    enum class Visual { Disabled, Recording, Paused };

    explicit PauseButton(QWidget* parent = nullptr);

    void setVisual(Visual v);
    std::function<void()> onClicked;

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void enterEvent(QEnterEvent*) override;
    void leaveEvent(QEvent*) override;

private:
    Visual m_visual = Visual::Disabled;
    bool   m_hover  = false;
    bool   m_pressed = false;
};
