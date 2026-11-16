// Minimal blocking PulseAudio capture wrapper (pa_simple).
// Captures 48 kHz S16LE stereo from a named source. Runs on its own thread.
#pragma once

#include <QString>
#include <cstdint>
#include <memory>

struct pa_simple;

class AudioCapturer {
public:
    AudioCapturer();
    ~AudioCapturer();

    AudioCapturer(const AudioCapturer&) = delete;
    AudioCapturer& operator=(const AudioCapturer&) = delete;

    // sourceName: pulse source ("dev") name, or empty for the server default.
    bool open(const QString& sourceName, int sampleRate = 48000, int channels = 2);
    // Blocking read of exactly `frames` sample-frames. Returns false on error.
    bool read(int16_t* interleaved, int frames);
    void close();
    bool isOpen() const { return m_simple != nullptr; }
    QString error() const { return m_error; }

private:
    pa_simple* m_simple = nullptr;
    int        m_rate     = 48000;
    int        m_channels = 2;
    QString    m_error;
};
