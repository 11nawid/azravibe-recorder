#include "AudioCapturer.h"

#include <pulse/simple.h>
#include <pulse/error.h>

AudioCapturer::AudioCapturer() = default;
AudioCapturer::~AudioCapturer() { close(); }

bool AudioCapturer::open(const QString& sourceName, int sampleRate, int channels) {
    close();
    m_rate     = sampleRate;
    m_channels = channels;
    m_error.clear();

    pa_sample_spec spec;
    spec.format   = PA_SAMPLE_S16LE;
    spec.rate     = (uint32_t)sampleRate;
    spec.channels = (uint8_t)channels;

    int error = 0;
    // Generous server-side buffer (~500 ms of audio) so CPU bursts in the
    // encoder don't make the source drop packets before we can read them.
    pa_buffer_attr bufferAttr;
    bufferAttr.maxlength = (uint32_t)-1;
    bufferAttr.tlength   = (uint32_t)-1;
    bufferAttr.prebuf    = (uint32_t)-1;
    bufferAttr.minreq    = (uint32_t)-1;
    bufferAttr.fragsize  = (uint32_t)(sampleRate * channels * sizeof(int16_t) / 2);

    const QByteArray dev = sourceName.isEmpty() ? QByteArray() : sourceName.toUtf8();
    m_simple = pa_simple_new(nullptr, "azravibe-recorder", PA_STREAM_RECORD,
                             dev.isEmpty() ? nullptr : dev.constData(),
                             "Azravibe Recorder capture", &spec, nullptr, &bufferAttr, &error);
    if (!m_simple) {
        m_error = QStringLiteral("PulseAudio/PipeWire capture failed: %1").arg(pa_strerror(error));
        return false;
    }
    return true;
}

bool AudioCapturer::read(int16_t* interleaved, int frames) {
    if (!m_simple) return false;
    int error = 0;
    const size_t bytes = (size_t)frames * (size_t)m_channels * sizeof(int16_t);
    if (pa_simple_read(m_simple, interleaved, bytes, &error) < 0) {
        m_error = QStringLiteral("capture read failed: %1").arg(pa_strerror(error));
        return false;
    }
    return true;
}

void AudioCapturer::close() {
    if (m_simple) {
        pa_simple_free(m_simple);
        m_simple = nullptr;
    }
}
