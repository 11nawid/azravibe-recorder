// macOS audio capture via AVAudioEngine's input tap (microphone).
// The engine is asked for 48 kHz stereo float; native hardware rates/channels
// are resampled implicitly by the engine. PCM is converted to S16 interleaved
// and queued for the blocking read() the recorder thread calls.
#import <AVFoundation/AVFoundation.h>

#include "AudioCapturer.h"
#include <deque>
#include <mutex>
#include <condition_variable>

struct AudioCapturer::State {
    AVAudioEngine*      engine  = nil;
    AVAudioInputNode*   input   = nil;
    std::deque<int16_t> pcm;
    std::mutex          mutex;
    std::condition_variable cv;
    bool                running = false;
};

AudioCapturer::AudioCapturer() = default;
AudioCapturer::~AudioCapturer() { close(); }

bool AudioCapturer::isOpen() const { return m_state && m_state->running; }

bool AudioCapturer::open(const QString& sourceName, int sampleRate, int channels) {
    close();
    m_error.clear();
    m_rate     = sampleRate;
    m_channels = channels;

    AVAuthorizationStatus status = [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio];
    if (status == AVAuthorizationStatusNotDetermined) {
        dispatch_semaphore_t done = dispatch_semaphore_create(0);
        [AVCaptureDevice requestAccessForMediaType:AVMediaTypeAudio completionHandler:^(BOOL granted) {
            (void)granted;
            dispatch_semaphore_signal(done);
        }];
        dispatch_semaphore_wait(done, dispatch_time(DISPATCH_TIME_NOW, 10 * NSEC_PER_SEC));
        status = [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio];
    }
    if (status != AVAuthorizationStatusAuthorized) {
        m_error = QStringLiteral("Microphone permission denied — enable it in System Settings → Privacy & Security → Microphone.");
        return false;
    }

    m_state = std::make_unique<State>();
    m_state->engine = [[AVAudioEngine alloc] init];

    // Pick the input device when a UID was provided; otherwise the system default.
    if (!sourceName.isEmpty()) {
        NSString* uid = sourceName.toNSString();
        AVCaptureDevice* device = [AVCaptureDevice defaultDeviceWithMediaType:AVMediaTypeAudio];
        if (device && [device.uniqueID isEqualToString:uid]) {
            m_state->input = m_state->engine.inputNode;   // default engine input matches the UID
        } else {
            // Engine taps the default input; the UID is informational on macOS,
            // we can't re-route devices without a process-wide aggregate tap.
            m_state->input = m_state->engine.inputNode;
        }
    } else {
        m_state->input = m_state->engine.inputNode;
    }

    AVAudioFormat* format = [[AVAudioFormat alloc] initWithStandardFormatWithSampleRate:double(sampleRate)
                                                                                channels:2];

    AVAudioInputNode* input = m_state->input;
    [input installTapOnNode:input
                 bufferSize:1024
                     format:format
                      block:^(AVAudioPCMBuffer* buffer, AVAudioTime* when) {
        (void)when;
        if (!buffer || buffer.frameLength == 0) return;
        const float* const* ch = (const float* const*)buffer.floatChannelData;
        const float* left  = ch[0];
        const float* right = (buffer.format.channelCount > 1) ? ch[1] : ch[0];
        const AVAudioFrameCount frames = buffer.frameLength;

        std::lock_guard<std::mutex> lock(m_state->mutex);
        for (AVAudioFrameCount i = 0; i < frames; ++i) {
            m_state->pcm.push_back((int16_t)(std::clamp(left[i],  -1.0f, 1.0f) * 32767.0f));
            m_state->pcm.push_back((int16_t)(std::clamp(right[i], -1.0f, 1.0f) * 32767.0f));
        }
        m_state->cv.notify_all();
    }];

    NSError* error = nil;
    if (![m_state->engine startAndReturnError:&error]) {
        m_error = QStringLiteral("Could not start audio engine: %1").arg(QString::fromNSString(error.localizedDescription));
        [m_state->engine release];
        m_state.reset();
        return false;
    }
    m_state->running = true;
    return true;
}

bool AudioCapturer::read(int16_t* interleaved, int frames) {
    if (!m_state || !m_state->running) return false;

    std::unique_lock<std::mutex> lock(m_state->mutex);
    const size_t want = size_t(frames) * 2;
    m_state->cv.wait(lock, [&] { return m_state->pcm.size() >= want || !m_state->running; });
    if (m_state->pcm.size() < want) return false;

    std::copy_n(m_state->pcm.begin(), want, interleaved);
    m_state->pcm.erase(m_state->pcm.begin(), m_state->pcm.begin() + want);
    return true;
}

void AudioCapturer::close() {
    if (!m_state) return;
    m_state->mutex.lock();
    m_state->running = false;
    m_state->cv.notify_all();
    m_state->mutex.unlock();

    if (m_state->engine) {
        [m_state->engine stop];
        [m_state->input removeTapOnNode:m_state->input];
        [m_state->engine release];
        m_state->engine = nil;
    }
    m_state.reset();
}
