// Windows audio capture via WASAPI.
// sourceName is an IMMDevice ID from the enumerator; ids that belong to a render
// endpoint are captured with AUDCLNT_STREAMFLAGS_LOOPBACK (system audio), the
// rest are captured from microphones.
//
// Only 48 kHz float32 / S16 mix formats with 1–2 channels are supported
// (what every real render/capture endpoint exposes today). Anything else fails
// gracefully — video keeps recording without audio.
#include "AudioCapturer.h"

#include <objbase.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>

#include <cstring>
#include <vector>
#include <algorithm>

struct AudioCapturer::State {
    IMMDeviceEnumerator* enumerator = nullptr;
    IMMDevice*           device     = nullptr;
    IAudioClient*        client     = nullptr;
    IAudioCaptureClient* captureClient = nullptr;
    HANDLE               event      = nullptr;
    WAVEFORMATEX*        mixFormat  = nullptr;
    bool                 loopback   = false;
    int                  channels   = 2;
    int                  sampleRate = 48000;
};

static QString hresultError(const char* what, HRESULT hr) {
    char buf[128];
    std::snprintf(buf, sizeof(buf), "%s failed (0x%08lx)", what, (unsigned long)hr);
    return QString::fromLocal8Bit(buf);
}

AudioCapturer::AudioCapturer() : m_state(std::make_unique<State>()) {}
AudioCapturer::~AudioCapturer() { close(); }

static bool openDevice(AudioCapturer::State* s, const QString& sourceName) {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                  IID_PPV_ARGS(&s->enumerator));
    if (FAILED(hr) || !s->enumerator) return false;

    auto tryOpen = [&](EDataFlow flow, bool loopback, int attempts) -> bool {
        (void)attempts;
        if (!sourceName.isEmpty()) {
            if (FAILED(s->enumerator->GetDevice(reinterpret_cast<LPCWSTR>(sourceName.utf16()), &s->device)))
                return false;
        } else {
            if (FAILED(s->enumerator->GetDefaultAudioEndpoint(flow, eConsole, &s->device)))
                return false;
        }
        HRESULT a = s->device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                        reinterpret_cast<void**>(&s->client));
        if (FAILED(a) || !s->client) { s->device->Release(); s->device = nullptr; return false; }

        s->loopback = loopback;
        return true;
    };

    // An id belongs to exactly one flow; try render (loopback) first, then capture (mic).
    if (!tryOpen(eRender,  /*loopback=*/true,  1))
        if (!tryOpen(eCapture, /*loopback=*/false, 1))
            return false;

    if (FAILED(s->client->GetMixFormat(&s->mixFormat)) || !s->mixFormat) {
        s->client->Release(); s->client = nullptr;
        s->device->Release(); s->device = nullptr;
        return false;
    }
    return true;
}

bool AudioCapturer::open(const QString& sourceName, int sampleRate, int channels) {
    close();
    if (!openDevice(m_state.get(), sourceName)) {
        m_error = QStringLiteral("Could not open audio device");
        return false;
    }
    State* s = m_state.get();

    const int      tag   = s->mixFormat->wFormatTag;
    const uint16_t bits  = s->mixFormat->wBitsPerSample;
    const uint16_t nch   = s->mixFormat->nChannels;
    const uint32_t rate  = s->mixFormat->nSamplesPerSec;

    // We only understand float32 / PCM16, 1–2 channels, 48 kHz.
    const bool formatOk = ((tag == 1 /*PCM*/ && bits == 16) ||
                           (tag == 3 /*IEEE_FLOAT*/ && bits == 32) ||
                           (tag == 0xFFFE && bits == 32))  // WAVE_FORMAT_EXTENSIBLE
                          && (nch == 1 || nch == 2)
                          && rate == (uint32_t)sampleRate;
    if (!formatOk) {
        m_error = QStringLiteral("Unsupported audio mix format: tag %1, %2 bits, %3 ch, %4 Hz")
                      .arg(tag).arg(bits).arg(nch).arg(rate);
        close();
        return false;
    }

    const DWORD streamFlags = s->loopback
        ? (AUDCLNT_STREAMFLAGS_LOOPBACK | AUDCLNT_STREAMFLAGS_EVENTCALLBACK)
        : AUDCLNT_STREAMFLAGS_EVENTCALLBACK;

    HRESULT hr = s->client->Initialize(AUDCLNT_SHAREMODE_SHARED, streamFlags,
                                       0, 0, s->mixFormat, nullptr);
    if (FAILED(hr)) {
        m_error = hresultError("AudioClient Initialize", hr);
        close();
        return false;
    }

    hr = s->client->GetService(IID_PPV_ARGS(&s->captureClient));
    if (FAILED(hr)) {
        m_error = hresultError("AudioClient GetService(IAudioCaptureClient)", hr);
        close();
        return false;
    }

    s->event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!s->event) {
        m_error = QStringLiteral("Could not create audio event handle");
        close();
        return false;
    }
    s->client->SetEventHandle(s->event);

    hr = s->client->Start();
    if (FAILED(hr)) {
        m_error = hresultError("AudioClient Start", hr);
        close();
        return false;
    }

    s->channels    = channels;
    s->sampleRate  = sampleRate;
    return true;
}

bool AudioCapturer::read(int16_t* interleaved, int frames) {
    if (!m_state->client || !m_state->captureClient) return false;
    State* s = m_state.get();

    int collected = 0;
    int16_t* out = interleaved;

    while (collected < frames) {
        if (WaitForSingleObject(s->event, 500) == WAIT_TIMEOUT) return false;   // stalled stream

        UINT32 packetFrames = 0;
        BYTE*  data = nullptr;
        DWORD  flags = 0;
        HRESULT hr = s->captureClient->GetBuffer(&data, &packetFrames, &flags, nullptr, nullptr);
        if (FAILED(hr)) {
            m_error = hresultError("GetBuffer", hr);
            return false;
        }

        const bool silent = (flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0;
        const int  nch    = s->mixFormat->nChannels;
        const bool isFloat = (s->mixFormat->wBitsPerSample == 32);

        for (UINT32 fi = 0; fi < packetFrames && collected < frames; ++fi, ++out, ++collected) {
            if (silent) {
                out[0] = 0;
                out[1] = 0;
                continue;
            }
            if (isFloat) {
                const float* src = reinterpret_cast<const float*>(data) + (size_t)fi * nch;
                const float l = src[0];
                const float r = (nch > 1) ? src[1] : src[0];
                out[0] = (int16_t)std::clamp(l, -1.0f, 1.0f) * 32767.0f;
                out[1] = (int16_t)std::clamp(r, -1.0f, 1.0f) * 32767.0f;
            } else {
                const int16_t* src = reinterpret_cast<const int16_t*>(data) + (size_t)fi * nch;
                out[0] = src[0];
                out[1] = (nch > 1) ? src[1] : src[0];
            }
        }

        s->captureClient->ReleaseBuffer(packetFrames);

        // If the packet had more frames than we consumed this call, leftover samples
        // are discarded — that's exactly what "drop to keep realtime" means for capture.
    }
    return true;
}

void AudioCapturer::close() {
    if (m_state->client) {
        m_state->client->Stop();
        m_state->client->Release();
        m_state->client = nullptr;
    }
    if (m_state->captureClient) {
        m_state->captureClient->Release();
        m_state->captureClient = nullptr;
    }
    if (m_state->mixFormat) { CoTaskMemFree(m_state->mixFormat); m_state->mixFormat = nullptr; }
    if (m_state->event) { CloseHandle(m_state->event); m_state->event = nullptr; }
    if (m_state->device) { m_state->device->Release(); m_state->device = nullptr; }
    if (m_state->enumerator) {
        m_state->enumerator->Release();
        m_state->enumerator = nullptr;
        CoUninitialize();
    }
}

bool AudioCapturer::isOpen() const {
    return m_state && m_state->client != nullptr;
}
