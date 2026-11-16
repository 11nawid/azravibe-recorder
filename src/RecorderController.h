// Orchestrates capture, encoding, the shared media clock and the lock-free
// handoffs to the GUI. The GUI polls: nothing here needs Q_OBJECT / signals.
#pragma once

#include <QImage>
#include <QRect>
#include <QString>
#include <QVector>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "capture/Capture.h"
#include "audio/AudioCapturer.h"
#include "media/Encoder.h"

extern "C" {
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

class RecorderController {
public:
    enum class State { Idle, Recording, Paused };

    RecorderController();
    ~RecorderController();

    // target: from Capture.h's listCaptureTargets(); audioSourceName: platform source (may be empty).
    bool startRecording(const TargetItem& target, const QString& audioSourceName);
    void togglePause();          // Recording <-> Paused (no-op when Idle)
    void stopRecording();        // blocks briefly; finalizes the file
    // Swap the audio input mid-recording (mic ↔ system audio, pick another device, …).
    // Only valid while recording; returns empty string on success else an error message.
    QString setAudioSource(const QString& sourceName);

    State    state() const { return m_state.load(std::memory_order_acquire); }
    bool     isBusy() const { return state() != State::Idle; }
    int64_t  mediaUs() const { return m_mediaUs.load(std::memory_order_acquire); }
    QString  lastSavedPath() const;
    QString  error() const;

    QImage              previewImage() const;      // latest preview (small), GUI thread
    std::vector<float>  consumePeaks();            // 0..1 waveform bins, oldest first
    QVector<QString>    consumeNotices();          // one-shot toast messages, oldest first
    bool                autoStopRequested() const { return m_autoStopRequested.load(std::memory_order_acquire); }
    // GUI hint: render the preview pipeline (skipped while the app is minimized).
    void                setPreviewWanted(bool wanted) { m_previewWanted.store(wanted, std::memory_order_relaxed); }
    // Stream the live preview chosen by the user (every target is previewed).
    // Target specs: same shape as startRecording; safe while idle.
    // Windows changes keep capturing at 60 Hz; failures close the preview.
    bool setTarget(const TargetItem& target);
    // Where files are written (empty → MoviesLocation/Azravibe Recorder).
    void                setOutputDir(const QString& dir);
    QString             outputDir() const { return m_outputDir; }
    // Draw the live cursor into captured frames.
    void                setCursorEnabled(bool enabled) { m_cursorEnabled.store(enabled, std::memory_order_relaxed); }
    bool                cursorEnabled() const { return m_cursorEnabled.load(std::memory_order_relaxed); }
    int                 sourceWidth()  const { return m_sourceW; }
    int                 sourceHeight() const { return m_sourceH; }

private:
    void videoLoop();
    void audioLoop();
    void tickMediaClock(std::chrono::steady_clock::time_point now);
    void pushNotice(const QString& text);

    std::atomic<State>   m_state{State::Idle};
    std::atomic<int64_t> m_mediaUs{0};
    std::atomic<bool>    m_stopRequested{false};
    std::atomic<bool>    m_videoStopRequested{false};  // video thread lives for app lifetime
    std::atomic<bool>    m_audioStopRequested{false};  // hot-swap: only stop the audio thread
    std::atomic<bool>    m_autoStopRequested{false};
    std::atomic<bool>    m_previewWanted{true};
    std::atomic<bool>    m_cursorEnabled{true};
    std::atomic<int>     m_captureFailures{0};

    mutable std::mutex       m_capturerMutex;   // guards m_capturer swap vs. grab
    std::thread m_videoThread;
    std::thread m_audioThread;

    std::unique_ptr<CaptureBackend> m_capturer;  // created in startRecording() / setTarget()
    CaptureSpec      m_capturerSpec{};   // last spec opened (avoid re-open churn)
    AudioCapturer  m_audio;
    Encoder        m_encoder;

    QString          m_audioSourceName;
    int  m_fps       = 30;
    int  m_sourceW   = 0;
    int  m_sourceH   = 0;
    int  m_previewW  = 0;   // scaled size for the preview pipeline
    int  m_previewH  = 0;
    bool m_audioActive = false;

    // media clock bookkeeping (video thread is the owner)
    std::chrono::steady_clock::time_point m_lastTick;

    // preview pipeline (video thread produces; GUI consumes a copy)
    mutable std::mutex       m_previewMutex;
    QImage                   m_preview;
    SwsContext*              m_swsPreview = nullptr;
    AVFrame*                 m_previewFrame = nullptr;
    std::vector<uint8_t>     m_previewBuf;

    mutable std::mutex       m_peakMutex;
    std::deque<float>        m_peaks;              // bounded

    mutable std::mutex       m_noticeMutex;
    QVector<QString>         m_notices;

    mutable std::mutex       m_pathMutex;
    QString                  m_savedPath;
    QString                  m_outputDir;         // empty = default location
    mutable std::mutex       m_errorMutex;
    QString                  m_error;
};
