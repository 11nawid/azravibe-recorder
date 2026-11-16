// Azravibe Recorder - FFmpeg muxing/encoding layer.
// Thread-safe: writeVideoFrame / writeAudioPcm / close may be called from
// different threads (they serialize internally on a mutex).
#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libavutil/error.h>
}

#include "RawPixel.h"

struct EncoderConfig {
    int fps          = 30;
    int outWidth     = 3840;   // 4K UHD (controller sets to fit within this box)
    int outHeight    = 2160;   // even, rounded down automatically
    int sampleRate   = 48000;
    int channels     = 2;
    int crf          = 21;     // libx264 quality (lower = better); NVENC maps to cq≈crf
    int audioKbps    = 160;
    int fallbackVideoKbps = 8000;
    const char* x264Preset = "veryfast";
};

// Which codecs are usable on this machine (probed once).
struct CodecProbe {
    std::string videoName;   // e.g. "libx264" (preferred) or "mpeg4"
    bool        h264 = false;
    std::string audioName;   // e.g. "aac" (preferred), "libmp3lame", "mp2"
    bool        aac  = false;
    const char* suggestedContainer() const { return (h264 && aac) ? "mp4" : "matroska"; }
    const char* suggestedExtension() const { return (h264 && aac) ? ".mp4" : ".mkv"; }
};

class Encoder {
public:
    Encoder();
    ~Encoder();

    // Probes available encoders. Safe to call without opening anything.
    static CodecProbe probe();

    // Opens the output file and all streams. On failure returns false and
    // error() is set.
    bool open(const std::string& outputPath, const EncoderConfig& cfg);

    // Converts src (32bpp BGR0/RGB0) -> yuv420p and encodes one frame.
    // mediaUs is the shared wall-clock media position in microseconds.
    bool writeVideoFrame(const uint8_t* src, int srcStride, int srcW, int srcH,
                         RawPixelFormat fmt, int64_t mediaUs);

    // Consumes native int16 interleaved PCM (sampleRate / channels from cfg),
    // resamples to float-planar for the audio encoder and emits 1024-sample
    // frames with a self-advancing clock (frozen while the caller stops
    // feeding us, which is what keeps audio in sync over pause/resume).
    bool writeAudioPcm(const int16_t* pcm, int sampleFrames);

    // Flushes encoders, writes the trailer and closes everything.
    void close();

    bool isOpen() const { return m_ofmt != nullptr; }
    const std::string& error() const { return m_error; }
    std::string codecSummary() const;

private:
    bool addVideoStream();
    bool addAudioStream();
    bool openVideoCodec();
    bool openAudioCodec();
    bool writeHeader();
    bool writeFrameInternal(AVCodecContext* codec, AVStream* stream, AVFrame* frame);
    bool muxPending(AVCodecContext* codec, AVStream* stream);
    void fail(const std::string& msg);
    void failAv(const char* what, int avErrCode);
    void release();

    mutable std::mutex m_mutex;

    EncoderConfig m_cfg{};
    std::string   m_error;
    std::string   m_videoCodecName;
    std::string   m_audioCodecName;

    AVFormatContext* m_ofmt   = nullptr;
    AVStream*        m_vstream = nullptr;
    AVStream*        m_astream = nullptr;
    AVCodecContext*  m_vctx   = nullptr;
    AVCodecContext*  m_actx   = nullptr;

    SwsContext*      m_sws    = nullptr;
    SwrContext*      m_swr    = nullptr;

    AVFrame*         m_vframe = nullptr;      // reused, yuv420p
    AVFrame*         m_aframe = nullptr;      // reused, fltp
    std::vector<uint8_t> m_vframeBuf;         // av_image_alloc backing for m_vframe
    std::vector<int16_t> m_pcmPending;        // int16 samples pending encode
    int              m_aframeSamples = 1024;  // emitted frame granularity

    int64_t          m_lastVideoPts = -1;
    int64_t          m_audioPts     = 0;      // in samples; converted to timebase
    int64_t          m_audioTimeUs  = 0;      // self advancing audio clock (us)
    bool             m_headerWritten = false;
    bool             m_broken = false;
};
