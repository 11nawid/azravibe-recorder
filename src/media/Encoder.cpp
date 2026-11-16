#include "Encoder.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace {
inline AVPixelFormat lavFmt(RawPixelFormat f) {
    return f == RawPixelFormat::BGR0 ? AV_PIX_FMT_BGR0 : AV_PIX_FMT_RGB0;
}
inline int64_t usToVideoPts(int64_t us, int fps) {
    return (int64_t)std::llround((double)us * (double)fps / 1000000.0);
}
} // namespace

Encoder::Encoder() {
    av_log_set_level(AV_LOG_ERROR);   // keep x264/aac chatter out of the terminal
}
Encoder::~Encoder() { release(); }

CodecProbe Encoder::probe() {
    CodecProbe p;
    if (const AVCodec* c = avcodec_find_encoder_by_name("h264_nvenc")) {
        p.videoName = c->name; p.h264 = true;   // GPU path: realtime 4K on NVENC
    } else if (const AVCodec* c = avcodec_find_encoder_by_name("libx264")) {
        p.videoName = c->name; p.h264 = true;
    } else if (avcodec_find_encoder(AV_CODEC_ID_H264)) {
        p.videoName = "h264"; p.h264 = true;
    } else {
        p.videoName = "mpeg4"; p.h264 = false;
    }
    if (const AVCodec* c = avcodec_find_encoder(AV_CODEC_ID_AAC)) {
        p.audioName = c->name; p.aac = true;
    } else if (avcodec_find_encoder_by_name("libmp3lame")) {
        p.audioName = "libmp3lame"; p.aac = false;
    } else {
        p.audioName = "mp2"; p.aac = false;
    }
    return p;
}

std::string Encoder::codecSummary() const {
    std::string s = m_videoCodecName.empty() ? "video:?" : "video:" + m_videoCodecName;
    s += " audio:";
    s += m_audioCodecName.empty() ? "?" : m_audioCodecName;
    return s;
}

void Encoder::fail(const std::string& msg) { m_error = msg; m_broken = true; }

void Encoder::failAv(const char* what, int code) {
    char buf[AV_ERROR_MAX_STRING_SIZE] = {0};
    av_strerror(code, buf, sizeof(buf));
    m_error = std::string(what) + ": " + buf;
    m_broken = true;
}

bool Encoder::open(const std::string& outputPath, const EncoderConfig& cfgIn) {
    std::lock_guard<std::mutex> lock(m_mutex);
    release();
    m_error.clear();
    m_broken = false;
    m_cfg = cfgIn;
    m_cfg.outWidth  = cfgIn.outWidth  & ~1;
    m_cfg.outHeight = cfgIn.outHeight & ~1;
    if (m_cfg.outWidth < 2 || m_cfg.outHeight < 2) { fail("target is too small"); return false; }
    if (m_cfg.fps < 1) m_cfg.fps = 30;

    const CodecProbe probeResult = probe();
    m_videoCodecName = probeResult.videoName;
    m_audioCodecName = probeResult.audioName;

    int ret = avformat_alloc_output_context2(&m_ofmt, nullptr, nullptr, outputPath.c_str());
    if (ret < 0 || !m_ofmt) { failAv("cannot allocate output context", ret); release(); return false; }

    if (!addVideoStream() || !addAudioStream() || !openVideoCodec() || !openAudioCodec()) {
        const std::string savedError = m_error.empty() ? std::string("encoder setup failed") : m_error;
        release();
        m_error = savedError;   // release() clears nothing; keep a useful message
        m_broken = true;
        return false;
    }

    if (!(m_ofmt->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&m_ofmt->pb, outputPath.c_str(), AVIO_FLAG_WRITE);
        if (ret < 0) { failAv("cannot open output file", ret); release(); return false; }
    }
    if (!writeHeader()) { release(); return false; }
    return true;
}

bool Encoder::addVideoStream() {
    const AVCodec* codec = avcodec_find_encoder_by_name(m_videoCodecName.c_str());
    if (!codec) codec = avcodec_find_encoder(AV_CODEC_ID_MPEG4);
    if (!codec) { fail("no suitable video encoder"); return false; }

    m_vstream = avformat_new_stream(m_ofmt, nullptr);
    if (!m_vstream) { fail("cannot create video stream"); return false; }
    m_vstream->id = m_ofmt->nb_streams - 1;

    m_vctx = avcodec_alloc_context3(codec);
    if (!m_vctx) { fail("cannot allocate video codec context"); return false; }

    m_vctx->width     = m_cfg.outWidth;
    m_vctx->height    = m_cfg.outHeight;
    m_vctx->pix_fmt   = AV_PIX_FMT_YUV420P;
    m_vctx->time_base = AVRational{1, m_cfg.fps};
    m_vctx->framerate = AVRational{m_cfg.fps, 1};
    m_vctx->gop_size  = m_cfg.fps * 2;
    m_vctx->max_b_frames = 0;              // keep dts == pts so plain mp4 muxing is always valid
    m_vctx->flags    &= ~AV_CODEC_FLAG_GLOBAL_HEADER;

    if (m_ofmt->oformat->flags & AVFMT_GLOBALHEADER)
        m_vctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    if (m_videoCodecName == "libx264") {
        av_opt_set(m_vctx->priv_data, "preset", m_cfg.x264Preset, 0);
        char crf[16];
        std::snprintf(crf, sizeof(crf), "%d", m_cfg.crf);
        av_opt_set(m_vctx->priv_data, "crf", crf, 0);
        av_opt_set(m_vctx->priv_data, "bframes", "0", 0);
        av_opt_set(m_vctx->priv_data, "tune", "zerolatency", 0);
        m_vctx->bit_rate = (int64_t)m_cfg.maxrateKbps * 1000;
        m_vctx->rc_buffer_size = m_cfg.bufsizeKbps * 1000;
    } else if (m_videoCodecName == "h264_nvenc") {
        char cq[16];
        std::snprintf(cq, sizeof(cq), "%d", m_cfg.crf);
        av_opt_set(m_vctx->priv_data, "preset", "p5", 0);
        av_opt_set(m_vctx->priv_data, "rc", "vbr", 0);
        av_opt_set(m_vctx->priv_data, "cq", cq, 0);
        av_opt_set(m_vctx->priv_data, "maxrate", "25M", 0);
        av_opt_set(m_vctx->priv_data, "bufsize", "50M", 0);
        av_opt_set(m_vctx->priv_data, "b_ref_mode", "disabled", 0);
        av_opt_set(m_vctx->priv_data, "zerolatency", "1", 0);
    } else {
        m_vctx->bit_rate = (int64_t)m_cfg.maxrateKbps * 1000;
        m_vctx->rc_buffer_size = m_cfg.bufsizeKbps * 1000;
    }
    return true;
}

bool Encoder::addAudioStream() {
    const AVCodec* codec = nullptr;
    AVCodecID id = AV_CODEC_ID_AAC;
    if (m_audioCodecName == "aac" || m_audioCodecName == "libfdk_aac")      id = AV_CODEC_ID_AAC;
    else if (m_audioCodecName == "libmp3lame")                              id = AV_CODEC_ID_MP3;
    else                                                                    id = AV_CODEC_ID_MP2;

    codec = avcodec_find_encoder(id);
    if (!codec) codec = avcodec_find_encoder_by_name(m_audioCodecName.c_str());
    if (!codec) { fail("no suitable audio encoder"); return false; }

    m_astream = avformat_new_stream(m_ofmt, nullptr);
    if (!m_astream) { fail("cannot create audio stream"); return false; }
    m_astream->id = m_ofmt->nb_streams - 1;

    m_actx = avcodec_alloc_context3(codec);
    if (!m_actx) { fail("cannot allocate audio codec context"); return false; }

    m_actx->sample_rate = m_cfg.sampleRate;
    m_actx->sample_fmt  = AV_SAMPLE_FMT_FLTP;   // native aac + libmp3lame both accept it
    av_channel_layout_default(&m_actx->ch_layout, m_cfg.channels);
    m_actx->time_base   = AVRational{1, m_cfg.sampleRate};
    m_actx->bit_rate    = (int64_t)m_cfg.audioKbps * 1000;

    if (m_ofmt->oformat->flags & AVFMT_GLOBALHEADER)
        m_actx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    return true;
}

bool Encoder::openVideoCodec() {
    int ret = avcodec_open2(m_vctx, nullptr, nullptr);
    if (ret < 0) { failAv("cannot open video encoder", ret); return false; }
    ret = avcodec_parameters_from_context(m_vstream->codecpar, m_vctx);
    if (ret < 0) { failAv("cannot copy video parameters", ret); return false; }
    if (m_vstream->time_base.num == 0) m_vstream->time_base = m_vctx->time_base;
    return true;
}

bool Encoder::openAudioCodec() {
    int ret = avcodec_open2(m_actx, nullptr, nullptr);
    if (ret < 0) { failAv("cannot open audio encoder", ret); return false; }
    // Use the encoder's frame granularity (1024 for AAC).
    if (m_actx->frame_size > 0) m_aframeSamples = m_actx->frame_size;

    ret = avcodec_parameters_from_context(m_astream->codecpar, m_actx);
    if (ret < 0) { failAv("cannot copy audio parameters", ret); return false; }
    if (m_astream->time_base.num == 0) m_astream->time_base = m_actx->time_base;

    // Swr: interleaved S16 -> planar float, same rate/layout.
    SwrContext* swr = swr_alloc();
    av_opt_set_chlayout(swr, "in_chlayout",    &m_actx->ch_layout, 0);
    av_opt_set_chlayout(swr, "out_chlayout",   &m_actx->ch_layout, 0);
    av_opt_set_int(swr, "in_sample_rate",      m_cfg.sampleRate, 0);
    av_opt_set_int(swr, "out_sample_rate",     m_cfg.sampleRate, 0);
    av_opt_set_sample_fmt(swr, "in_sample_fmt",  AV_SAMPLE_FMT_S16, 0);
    av_opt_set_sample_fmt(swr, "out_sample_fmt", AV_SAMPLE_FMT_FLTP, 0);
    ret = swr_init(swr);
    if (ret < 0) { failAv("cannot init resampler", ret); swr_free(&swr); return false; }
    m_swr = swr;
    return true;
}

bool Encoder::writeHeader() {
    AVDictionary* opts = nullptr;
    const char* fmtName = m_ofmt->oformat->name;
    if (fmtName && std::string(fmtName).find("mp4") != std::string::npos)
        av_dict_set(&opts, "movflags", "+faststart", 0);
    int ret = avformat_write_header(m_ofmt, &opts);
    av_dict_free(&opts);
    if (ret < 0) { failAv("cannot write header", ret); release(); return false; }
    m_headerWritten = true;
    return true;
}

bool Encoder::muxPending(AVCodecContext* codec, AVStream* stream) {
    AVPacket* pkt = av_packet_alloc();
    bool ok = true;
    for (;;) {
        int ret = avcodec_receive_packet(codec, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
        if (ret < 0) { failAv("receive_packet", ret); ok = false; break; }
        av_packet_rescale_ts(pkt, codec->time_base, stream->time_base);
        pkt->stream_index = stream->index;
        ret = av_interleaved_write_frame(m_ofmt, pkt);
        av_packet_unref(pkt);
        if (ret < 0) { failAv("write_frame", ret); ok = false; break; }
    }
    av_packet_free(&pkt);
    return ok;
}

bool Encoder::writeFrameInternal(AVCodecContext* codec, AVStream* stream, AVFrame* frame) {
    int ret = avcodec_send_frame(codec, frame);
    if (ret < 0) { failAv("send_frame", ret); return false; }
    return muxPending(codec, stream);
}

bool Encoder::writeVideoFrame(const uint8_t* src, int srcStride, int srcW, int srcH,
                              RawPixelFormat fmt, int64_t mediaUs) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!isOpen() || m_broken) return false;

    // Lanzcos for upscales (4K output from a lower-res source), fast bilinear otherwise.
    const int swsFlags = (srcW < m_cfg.outWidth || srcH < m_cfg.outHeight)
                             ? SWS_LANCZOS : SWS_FAST_BILINEAR;
    m_sws = sws_getCachedContext(m_sws, srcW, srcH, lavFmt(fmt),
                                 m_cfg.outWidth, m_cfg.outHeight, AV_PIX_FMT_YUV420P,
                                 swsFlags, nullptr, nullptr, nullptr);
    if (!m_sws) { fail("sws init failed"); return false; }

    if (!m_vframe) {
        m_vframe = av_frame_alloc();
        m_vframe->format = AV_PIX_FMT_YUV420P;
        m_vframe->width  = m_cfg.outWidth;
        m_vframe->height = m_cfg.outHeight;
        int bufSize = av_image_get_buffer_size(AV_PIX_FMT_YUV420P, m_cfg.outWidth, m_cfg.outHeight, 1);
        m_vframeBuf.resize((size_t)bufSize);
        av_image_fill_arrays(m_vframe->data, m_vframe->linesize, m_vframeBuf.data(),
                             AV_PIX_FMT_YUV420P, m_cfg.outWidth, m_cfg.outHeight, 1);
    }

    // Note: sws_scale reads directly from the caller's buffer; do NOT av_frame_make_writable here.
    const uint8_t* srcSlices[1] = { src };
    int srcStrideArr[1]         = { srcStride };
    int ret = sws_scale(m_sws, srcSlices, srcStrideArr, 0, srcH, m_vframe->data, m_vframe->linesize);
    if (ret <= 0) { fail("sws_scale failed"); return false; }

    int64_t pts = usToVideoPts(mediaUs, m_cfg.fps);
    if (pts <= m_lastVideoPts) pts = m_lastVideoPts + 1;
    m_lastVideoPts   = pts;
    m_vframe->pts    = pts;

    return writeFrameInternal(m_vctx, m_vstream, m_vframe);
}

bool Encoder::writeAudioPcm(const int16_t* pcm, int sampleFrames) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!isOpen() || m_broken) return false;
    if (sampleFrames <= 0) return true;

    const size_t samples = (size_t)sampleFrames * m_cfg.channels;
    m_pcmPending.insert(m_pcmPending.end(), pcm, pcm + samples);

    bool ok = true;
    while ((int)(m_pcmPending.size() / (size_t)m_cfg.channels) >= m_aframeSamples && ok) {
        const size_t takeSamples = (size_t)m_aframeSamples * m_cfg.channels;

        if (!m_aframe) {
            m_aframe = av_frame_alloc();
            m_aframe->format     = AV_SAMPLE_FMT_FLTP;
            m_aframe->sample_rate = m_cfg.sampleRate;
            av_channel_layout_default(&m_aframe->ch_layout, m_cfg.channels);
            m_aframe->nb_samples = m_aframeSamples;
            if (av_frame_get_buffer(m_aframe, 0) < 0) { fail("audio frame alloc failed"); ok = false; break; }
        }

        const uint8_t* inData[1] = { reinterpret_cast<const uint8_t*>(m_pcmPending.data()) };
        int inLine[1] = { (int)(m_cfg.channels * sizeof(int16_t)) };
        int converted = swr_convert(m_swr, m_aframe->data, m_aframeSamples, inData, m_aframeSamples);
        if (converted < 0) { failAv("swr_convert", converted); ok = false; break; }

        m_aframe->nb_samples = converted;
        m_aframe->pts        = m_audioPts;
        m_audioPts          += converted;
        m_audioTimeUs       += (int64_t)converted * 1000000 / m_cfg.sampleRate;

        ok = writeFrameInternal(m_actx, m_astream, m_aframe);

        // Consume exactly the frames we fed to the resampler.
        m_pcmPending.erase(m_pcmPending.begin(), m_pcmPending.begin() + takeSamples);
    }
    return ok;
}

void Encoder::close() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_ofmt) { release(); return; }

    // Drain encoders.
    if (m_vctx) { avcodec_send_frame(m_vctx, nullptr); muxPending(m_vctx, m_vstream); }
    if (m_actx) { avcodec_send_frame(m_actx, nullptr); muxPending(m_actx, m_astream); }

    if (m_headerWritten) av_write_trailer(m_ofmt);
    release();
}

void Encoder::release() {
    if (m_vframe) { av_frame_free(&m_vframe); }
    if (m_aframe) { av_frame_free(&m_aframe); }
    m_vframeBuf.clear();
    m_pcmPending.clear();
    if (m_sws)    { sws_freeContext(m_sws);        m_sws = nullptr; }
    if (m_swr)    { swr_free(&m_swr);              m_swr = nullptr; }
    if (m_vctx)   { avcodec_free_context(&m_vctx); }
    if (m_actx)   { avcodec_free_context(&m_actx); }
    m_vstream = nullptr;  // owned by m_ofmt
    m_astream = nullptr;
    if (m_ofmt) {
        if (m_ofmt->pb) { avio_closep(&m_ofmt->pb); }
        avformat_free_context(m_ofmt);
        m_ofmt = nullptr;
    }
    m_headerWritten = false;
    m_lastVideoPts  = -1;
    m_audioPts      = 0;
    m_audioTimeUs   = 0;
}
