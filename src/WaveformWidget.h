// Real-time audio waveform: scrolling peak history, mirrored bars with a
// violet→cyan gradient. Expects one 10 ms peak bin per update.
#pragma once

#include <QWidget>
#include <deque>
#include <vector>

class WaveformWidget : public QWidget {
public:
    explicit WaveformWidget(QWidget* parent = nullptr);

    void pushPeak(float peak);        // append bins (called from the poll timer)
    void pushPeaks(const std::vector<float>& peaks);   // batched; single repaint
    void clear();                     // back to flat line (e.g. on stop)
    void setActive(bool active);      // flat idle vs live look
    bool isActive() const { return m_active; }

protected:
    void paintEvent(QPaintEvent*) override;

private:
    std::deque<float> m_bins;         // newest at the back
    bool              m_active = false;
    int               m_phase  = 0;
};
