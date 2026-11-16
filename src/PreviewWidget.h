// Rounded, letterboxed preview with REC/PAUSED badge and a transient toast.
#pragma once

#include <QImage>
#include <QWidget>
#include <QTimer>
#include <functional>

class PreviewWidget : public QWidget {
public:
    enum class Badge { None, Recording, Paused };

    explicit PreviewWidget(QWidget* parent = nullptr);

    void setFrame(const QImage& frame);
    void setBadge(Badge badge);
    void setElapsedUs(int64_t us);   // media clock for the file being recorded
    void clearFrame();
    void showToast(const QString& text);   // fades out automatically

    bool hasFrame() const { return !m_frame.isNull(); }

protected:
    void paintEvent(QPaintEvent*) override;

private:
    static QString formatTime(int64_t us);

    QImage  m_frame;
    Badge   m_badge = Badge::None;
    int64_t m_elapsedUs = 0;

    QString m_toast;
    qreal   m_toastAlpha = 0.0;
    QTimer  m_phaseTimer;       // blink / fade animation, only ticks when needed
    int     m_phase = 0;
};
