#include "RecorderController.h"

#include <QStandardPaths>
#include <QDateTime>
#include <QDir>
#include <QtMath>

#include <chrono>
#include <cstring>
#include <cmath>
#include <algorithm>

namespace {
constexpr int64_t kUsPerSecond = 1000000;

double peakAbs(const int16_t* pcm, int samples) {
    int32_t maxv = 0;
    for (int i = 0; i < samples; ++i) {
        const int32_t v = std::abs((int32_t)pcm[i]);
        if (v > maxv) maxv = v;
    }
    return (double)maxv / 32768.0;
}
} // namespace

RecorderController::RecorderController() = default;

RecorderController::~RecorderController() {
    stopRecording();
}

void RecorderController::setOutputDir(const QString& dir) {
    std::lock_guard<std::mutex> lock(m_pathMutex);
    m_outputDir = dir;
}

QString RecorderController::lastSavedPath() const {
    std::lock_guard<std::mutex> lock(m_pathMutex);
    return m_savedPath;
}

QString RecorderController::error() const {
    std::lock_guard<std::mutex> lock(m_errorMutex);
    return m_error;
}

void RecorderController::pushNotice(const QString& text) {
    std::lock_guard<std::mutex> lock(m_noticeMutex);
    m_notices.push_back(text);
}

QVector<QString> RecorderController::consumeNotices() {
    std::lock_guard<std::mutex> lock(m_noticeMutex);
    QVector<QString> out = m_notices;
    m_notices.clear();
    return out;
}

QImage RecorderController::previewImage() const {
    std::lock_guard<std::mutex> lock(m_previewMutex);
    return m_preview;
}

std::vector<float> RecorderController::consumePeaks() {
    std::lock_guard<std::mutex> lock(m_peakMutex);
    std::vector<float> out(m_peaks.begin(), m_peaks.end());
    m_peaks.clear();
    return out;
}

bool RecorderController::startRecording(const TargetItem& target, const QString& audioSourceName) {
    if (isBusy()) { m_error = QStringLiteral("Recording is already in progress."); return false; }
    {
        std::lock_guard<std::mutex> lock(m_errorMutex);
        m_error.clear();
    }
    {
        std::lock_guard<std::mutex> lock(m_pathMutex);
        m_savedPath.clear();
    }

    CaptureSpec spec;
    if (target.type == TargetItem::Type::Window) {
        spec.window = target.window;
        spec.region = target.screenRect.isEmpty() ? QRect(0, 0, 2, 2) : target.screenRect;
    } else {
        spec.window = 0;
        spec.region = target.screenRect;
    }
    if (!m_capturer.open(spec)) {
        m_error = QStringLiteral("Could not open the capture session.");
        return false;
    }
    m_capturer.setShowCursor(m_cursorEnabled.load(std::memory_order_relaxed));

    // Prime the first frame to learn the source size.
    if (!m_capturer.grab()) {
        m_error = QStringLiteral("Target window is not available.");
        m_capturer.close();
        return false;
    }
    m_sourceW = m_capturer.width();
    m_sourceH = m_capturer.height();

    // Decide where to save (user-picked folder, else default Movies location).
    QString dirPath;
    {
        std::lock_guard<std::mutex> lock(m_pathMutex);
        dirPath = m_outputDir;
    }
    QDir dir;
    if (!dirPath.isEmpty()) {
        dir = QDir(dirPath);
        if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
            m_error = QStringLiteral("Output folder is not writable: %1").arg(dirPath);
            m_capturer.close();
            return false;
        }
    } else {
        QString baseDir = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
        if (baseDir.isEmpty()) baseDir = QDir::homePath() + QStringLiteral("/Videos");
        dir = QDir(baseDir + QStringLiteral("/Azravibe Recorder"));
        if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
            m_error = QStringLiteral("Cannot create output folder.");
            m_capturer.close();
            return false;
        }
    }

    const CodecProbe probeResult = Encoder::probe();
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
    const QString path = dir.filePath(QStringLiteral("azravibe-%1%2")
                                          .arg(stamp, QString::fromUtf8(probeResult.suggestedExtension())));

    // 4K UHD output: fit inside 3840x2160 (upscaled when the source is smaller),
    // even dimensions for yuv420p.
    const double scale = std::min(3840.0 / m_sourceW, 2160.0 / m_sourceH);
    EncoderConfig cfg;
    cfg.fps        = m_fps;
    cfg.outWidth   = ((int)(m_sourceW * scale)) & ~1;
    cfg.outHeight  = ((int)(m_sourceH * scale)) & ~1;
    if (cfg.outWidth < 2 || cfg.outHeight < 2) {
        m_error = QStringLiteral("Output size computation failed.");
        m_capturer.close();
        return false;
    }
    cfg.sampleRate = 48000;
    cfg.channels   = 2;

    if (!m_encoder.open(path.toStdString(), cfg)) {
        m_error = QStringLiteral("Encoder failed: %1").arg(QString::fromStdString(m_encoder.error()));
        m_capturer.close();
        return false;
    }

    // Preview pipeline is created lazily on the first encoded frame.
    m_previewW = 0;
    m_previewH = 0;

    // Audio capture (best effort: video keeps recording even if audio fails).
    m_audioActive = m_audio.open(audioSourceName, 48000, 2);
    if (!m_audioActive) {
        pushNotice(QStringLiteral("Audio unavailable — recording video only."));
    }

    m_mediaUs.store(0, std::memory_order_release);
    m_lastTick = std::chrono::steady_clock::now();
    m_stopRequested.store(false, std::memory_order_release);
    m_autoStopRequested.store(false, std::memory_order_release);
    m_captureFailures.store(0, std::memory_order_release);
    {
        std::lock_guard<std::mutex> lock(m_peakMutex);
        m_peaks.clear();
    }
    m_state.store(State::Recording, std::memory_order_release);

    m_videoThread = std::thread(&RecorderController::videoLoop, this);
    if (m_audioActive)
        m_audioThread = std::thread(&RecorderController::audioLoop, this);

    {
        std::lock_guard<std::mutex> lock(m_pathMutex);
        m_savedPath = path;
    }
    return true;
}

void RecorderController::togglePause() {
    if (state() == State::Recording)
        m_state.store(State::Paused, std::memory_order_release);
    else if (state() == State::Paused)
        m_state.store(State::Recording, std::memory_order_release);
}

void RecorderController::tickMediaClock(std::chrono::steady_clock::time_point now) {
    const int64_t delta = std::chrono::duration_cast<std::chrono::microseconds>(now - m_lastTick).count();
    if (state() == State::Recording && delta > 0) {
        m_mediaUs.fetch_add(delta, std::memory_order_acq_rel);
    }
    m_lastTick = now;
}

void RecorderController::videoLoop() {
    const int64_t frameDurUs = kUsPerSecond / m_fps;
    auto nextFrame = std::chrono::steady_clock::now();

    while (!m_stopRequested.load(std::memory_order_acquire)) {
        nextFrame += std::chrono::microseconds(frameDurUs);
        std::this_thread::sleep_until(nextFrame);

        const auto now = std::chrono::steady_clock::now();
        tickMediaClock(now);

        const State s = state();
        if (s != State::Recording) continue;   // paused: clock frozen, ffmpeg idle

        if (!m_capturer.grab()) {
            if (m_captureFailures.fetch_add(1) >= 2) {
                pushNotice(QStringLiteral("Target window disappeared — recording saved."));
                m_autoStopRequested.store(true, std::memory_order_release);
                break;
            }
            continue;
        }

        const int64_t mediaUs = m_mediaUs.load(std::memory_order_acquire);
        const int w = m_capturer.width();
        const int h = m_capturer.height();
        const int stride = m_capturer.bytesPerLine();

        if (!m_encoder.writeVideoFrame(m_capturer.data(), stride, w, h, m_capturer.format(), mediaUs)) {
            {
                std::lock_guard<std::mutex> lock(m_errorMutex);
                m_error = QStringLiteral("Encoding failed: %1").arg(QString::fromStdString(m_encoder.error()));
            }
            pushNotice(QStringLiteral("Recording stopped: encoder error."));
            m_autoStopRequested.store(true, std::memory_order_release);
            break;
        }

        // ---- preview (small, cheap; skipped entirely while minimized) ----
        if (!m_previewWanted.load(std::memory_order_relaxed)) {
            // Stale frame kept on purpose; the GUI shows it when it comes back.
        } else {
        const int pw = qMin(512, w & ~1);
        const int ph = qMax(2, (int)std::llround((double)pw * h / (double)w) & ~1);
        if (!m_swsPreview || pw != m_previewW || ph != m_previewH) {
            if (m_swsPreview) { sws_freeContext(m_swsPreview); m_swsPreview = nullptr; }
            if (m_previewFrame) { av_frame_free(&m_previewFrame); }
            m_previewBuf.clear();
            m_previewW = pw;
            m_previewH = ph;

            m_swsPreview = sws_getCachedContext(nullptr, w, h,
                                                m_capturer.format() == RawPixelFormat::BGR0 ? AV_PIX_FMT_BGR0 : AV_PIX_FMT_RGB0,
                                                pw, ph, AV_PIX_FMT_RGB24,
                                                SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
            m_previewFrame = av_frame_alloc();
            m_previewFrame->format = AV_PIX_FMT_RGB24;
            m_previewFrame->width  = pw;
            m_previewFrame->height = ph;
            const int bufSize = av_image_get_buffer_size(AV_PIX_FMT_RGB24, pw, ph, 1);
            m_previewBuf.resize((size_t)bufSize);
            av_image_fill_arrays(m_previewFrame->data, m_previewFrame->linesize,
                                 m_previewBuf.data(), AV_PIX_FMT_RGB24, pw, ph, 1);
        }

        if (m_swsPreview && m_previewFrame) {
            const uint8_t* srcSlices[1] = { m_capturer.data() };
            int srcStrideArr[1]         = { stride };
            sws_scale(m_swsPreview, srcSlices, srcStrideArr, 0, h, m_previewFrame->data, m_previewFrame->linesize);
            QImage img(m_previewFrame->data[0], pw, ph, m_previewFrame->linesize[0], QImage::Format_RGB888);
            const QImage copy = img.copy();
            std::lock_guard<std::mutex> lock(m_previewMutex);
            m_preview = copy;
        }
        }
    }
}

void RecorderController::audioLoop() {
    constexpr int kFramesPerBlock = 480;   // 10 ms @ 48 kHz
    int16_t block[480 * 2];

    while (!m_stopRequested.load(std::memory_order_acquire)) {
        if (!m_audio.read(block, kFramesPerBlock)) {
            pushNotice(QStringLiteral("Audio capture stopped unexpectedly."));
            break;
        }

        const double peak = peakAbs(block, kFramesPerBlock * 2);
        {
            std::lock_guard<std::mutex> lock(m_peakMutex);
            m_peaks.push_back((float)peak);
            if (m_peaks.size() > 512) m_peaks.pop_front();
        }

        if (state() != State::Recording) {
            // Paused: drop audio so file length tracks the frozen media clock.
            continue;
        }
        m_encoder.writeAudioPcm(block, kFramesPerBlock);
    }
}

void RecorderController::stopRecording() {
    if (state() == State::Idle && !m_videoThread.joinable() && !m_audioThread.joinable()) {
        m_autoStopRequested.store(false, std::memory_order_release);
        return;
    }

    m_stopRequested.store(true, std::memory_order_release);

    if (m_videoThread.joinable()) m_videoThread.join();
    if (m_audioThread.joinable()) m_audioThread.join();

    m_state.store(State::Idle, std::memory_order_release);
    m_autoStopRequested.store(false, std::memory_order_release);
    m_audio.close();
    m_encoder.close();
    m_capturer.close();

    if (m_swsPreview) { sws_freeContext(m_swsPreview); m_swsPreview = nullptr; }
    if (m_previewFrame) { av_frame_free(&m_previewFrame); m_previewFrame = nullptr; }
    m_previewBuf.clear();
}
