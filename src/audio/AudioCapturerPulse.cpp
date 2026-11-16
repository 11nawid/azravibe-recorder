// Linux audio capture via PulseAudio simple API (pa_simple).
// Captures 48 kHz S16LE stereo from a named source; blocking read with a
// generous server-side buffer (~500 ms) so CPU bursts in the encoder don't
// drop samples.
#include "AudioCapturer.h"

#include <pulse/simple.h>
#include <pulse/error.h>
#include <pulse/timeval.h>
#include <cstring>

struct AudioCapturer::State {
    pa_simple* simple = nullptr;
};

AudioCapturer::AudioCapturer() = default;
AudioCapturer::~AudioCapturer() { close(); }

bool AudioCapturer::open(const QString& sourceName, int sampleRate, int channels) {
    close();
    m_rate     = sampleRate;
    m_channels = channels;
    m_error.clear();

    m_state = std::make_unique<State>();

    pa_sample_spec spec;
    spec.format   = PA_SAMPLE_S16LE;
    spec.rate     = (uint32_t)sampleRate;
    spec.channels = (uint8_t)channels;

    // Generous server-side buffer (~500 ms) so encoder CPU bursts don't drop packets.
    pa_buffer_attr bufferAttr;
    bufferAttr.maxlength = (uint32_t)-1;
    bufferAttr.tlength   = (uint32_t)-1;
    bufferAttr.prebuf    = (uint32_t)-1;
    bufferAttr.minreq    = (uint32_t)-1;
    bufferAttr.fragsize  = (uint32_t)(sampleRate * channels * sizeof(int16_t) / 2);

    int error = 0;
    const QByteArray dev = sourceName.isEmpty() ? QByteArray() : sourceName.toUtf8();
    m_state->simple = pa_simple_new(nullptr, "azravibe-recorder", PA_STREAM_RECORD,
                                    dev.isEmpty() ? nullptr : dev.constData(),
                                    "Azravibe Recorder capture", &spec, nullptr, &bufferAttr, &error);
    if (!m_state->simple) {
        m_error = QStringLiteral("PulseAudio/PipeWire capture failed: %1").arg(pa_strerror(error));
        m_state.reset();
        return false;
    }
    return true;
}

bool AudioCapturer::read(int16_t* interleaved, int frames) {
    if (!m_state || !m_state->simple) return false;
    int error = 0;
    const size_t bytes = (size_t)frames * (size_t)m_channels * sizeof(int16_t);
    if (pa_simple_read(m_state->simple, interleaved, bytes, &error) < 0) {
        m_error = QStringLiteral("capture read failed: %1").arg(pa_strerror(error));
        return false;
    }
    return true;
}

void AudioCapturer::close() {
    if (!m_state) return;
    if (m_state->simple) {
        pa_simple_free(m_state->simple);
        m_state->simple = nullptr;
    }
    m_state.reset();
}

bool AudioCapturer::isOpen() const {
    return m_state && m_state->simple != nullptr;
}
