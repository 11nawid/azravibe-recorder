// Minimal blocking audio capture wrapper with a per-platform backend:
//   Linux   → PulseAudio simple API (pa_simple)
//   Windows → WASAPI (IMMDevice + IAudioClient, loopback or microphone)
//   macOS   → AVAudioEngine input tap (microphone)
// Each backend maps to the same blocking pull API the recorder thread uses.
#pragma once

#include <QString>
#include <cstdint>
#include <memory>

class AudioCapturer {
public:
    AudioCapturer();
    ~AudioCapturer();

    AudioCapturer(const AudioCapturer&) = delete;
    AudioCapturer& operator=(const AudioCapturer&) = delete;

    // sourceName: device id from the platform enumerator, or empty for the
    // server default. Returns false and sets error() on failure.
    bool open(const QString& sourceName, int sampleRate = 48000, int channels = 2);

    // Blocking read of exactly `frames` sample-frames of S16 interleaved PCM.
    // False on error (e.g. source disconnected or starved).
    bool read(int16_t* interleaved, int frames);

    void close();
    bool isOpen() const;
    QString error() const { return m_error; }

private:
    struct State;
    std::unique_ptr<State> m_state;
    int     m_rate     = 48000;
    int     m_channels = 2;
    QString m_error;
};
