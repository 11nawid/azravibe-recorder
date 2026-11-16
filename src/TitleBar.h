// Frameless-window title bar: painted logo, app name, minimize / close buttons.
// Draggable. Emits intents via std::function callbacks.
#pragma once

#include <QWidget>
#include <functional>

class LogoMark : public QWidget {
public:
    explicit LogoMark(QWidget* parent = nullptr);
protected:
    void paintEvent(QPaintEvent*) override;
};

class TitleBar : public QWidget {
public:
    explicit TitleBar(QWidget* parent = nullptr);

    std::function<void()> onMinimize;
    std::function<void()> onClose;

protected:
    void mousePressEvent(QMouseEvent*) override;

private:
    static QIcon makeMinimizeIcon();
    static QIcon makeCloseIcon(bool hover);   // color baked; we tint via two icons

    LogoMark*    m_logo   = nullptr;
};
