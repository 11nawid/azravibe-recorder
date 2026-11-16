// Small dev probe: prints streams/duration of a media file. Not part of the app.
extern "C" {
#include <libavformat/avformat.h>
}
#include <cstdio>
int main(int argc, char** argv) {
    if (argc < 2) { printf("usage: probe <file>\n"); return 2; }
    AVFormatContext* ctx = nullptr;
    if (avformat_open_input(&ctx, argv[1], nullptr, nullptr) < 0) { printf("open failed\n"); return 1; }
    avformat_find_stream_info(ctx, nullptr);
    printf("duration=%.2fs  streams=%d  container=%s\n", ctx->duration / 1e6, ctx->nb_streams, ctx->iformat->name);
    for (unsigned i = 0; i < ctx->nb_streams; ++i) {
        AVStream* s = ctx->streams[i];
        AVCodecParameters* p = s->codecpar;
        if (p->codec_type == AVMEDIA_TYPE_VIDEO)
            printf("video: %s %dx%d frames=%lld fps~%.2f\n", avcodec_get_name(p->codec_id), p->width, p->height,
                   (long long)s->nb_frames, av_q2d(s->avg_frame_rate));
        else if (p->codec_type == AVMEDIA_TYPE_AUDIO)
            printf("audio: %s %dHz ch=%d frames=%lld\n", avcodec_get_name(p->codec_id), p->sample_rate, p->ch_layout.nb_channels,
                   (long long)s->nb_frames);
    }
    return 0;
}
