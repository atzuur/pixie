#include "coding.h"
#include "log.h"

void px_media_ctx_free(PXMediaContext* ctx) {

    if (ctx->ifmt_ctx) {

        for (unsigned i = 0; i < ctx->ifmt_ctx->nb_streams; i++) {
            if (ctx->stream_ctx_vec[i].dec_ctx)
                avcodec_free_context(&ctx->stream_ctx_vec[i].dec_ctx);
            if (ctx->stream_ctx_vec[i].enc_ctx)
                avcodec_free_context(&ctx->stream_ctx_vec[i].enc_ctx);
        }

        avformat_close_input(&ctx->ifmt_ctx);
    }

    free_s(&ctx->stream_ctx_vec);

    if (ctx->ofmt_ctx) {
        if (!(ctx->ofmt_ctx->oformat->flags & AVFMT_NOFILE))
            avio_closep(&ctx->ofmt_ctx->pb);

        avformat_free_context(ctx->ofmt_ctx);
        ctx->ofmt_ctx = NULL;
    }
}

int init_input(PXMediaContext* ctx, const char* filename) {

    int ret = 0;
    ctx->ifmt_ctx = NULL;

    ret = avformat_open_input(&ctx->ifmt_ctx, filename, NULL, NULL);
    if (ret < 0) {
        lav_throw_msg("avformat_open_input", ret);
        return ret;
    }

    ret = avformat_find_stream_info(ctx->ifmt_ctx, NULL);
    if (ret < 0) {
        lav_throw_msg("avformat_find_stream_info", ret);
        return ret;
    }

    ctx->stream_ctx_vec = calloc(ctx->ifmt_ctx->nb_streams, sizeof *ctx->stream_ctx_vec);
    if (!ctx->stream_ctx_vec) {
        oom(sizeof *ctx->stream_ctx_vec);
        return AVERROR(ENOMEM);
    }

    bool streams_found = false;

    for (unsigned i = 0; i < ctx->ifmt_ctx->nb_streams; i++) {

        enum AVMediaType stream_type = ctx->ifmt_ctx->streams[i]->codecpar->codec_type;

        if (stream_type == AVMEDIA_TYPE_UNKNOWN) {
            px_log(PX_LOG_ERROR, "Stream %d is of unknown type, cannot proceed\n", i);
            return AVERROR_INVALIDDATA;
        }

        if (stream_type == AVMEDIA_TYPE_VIDEO) {

            streams_found = true;

            ret = init_decoder(ctx, i);
            if (ret < 0)
                return ret;

            if (av_log_get_level() >= AV_LOG_INFO)
                av_dump_format(ctx->ifmt_ctx, i, filename, false);

            ctx->stream_ctx_vec[i].dec_ctx->framerate =
                av_guess_frame_rate(ctx->ifmt_ctx, ctx->ifmt_ctx->streams[i], NULL);
        }
    }

    if (!streams_found) {
        px_log(PX_LOG_ERROR, "No valid streams found in file \"%s\"\n", filename);
        return AVERROR_INVALIDDATA;
    }

    return 0;
}

int init_decoder(PXMediaContext* ctx, unsigned stream_idx) {

    int ret;
    const AVCodecParameters* dec_params = ctx->ifmt_ctx->streams[stream_idx]->codecpar;

    const AVCodec* decoder = avcodec_find_decoder(ctx->ifmt_ctx->streams[stream_idx]->codecpar->codec_id);
    if (!decoder) {
        px_log(PX_LOG_ERROR, "Failed to find decoder for stream %d\n", stream_idx);
        return AVERROR_DECODER_NOT_FOUND;
    }

    AVCodecContext* dec_ctx = avcodec_alloc_context3(decoder);
    if (!dec_ctx) {
        oom(sizeof *dec_ctx);
        return AVERROR(ENOMEM);
    }

    ctx->stream_ctx_vec[stream_idx].dec_ctx = dec_ctx;

    ret = avcodec_parameters_to_context(dec_ctx, dec_params);
    if (ret < 0) {
        lav_throw_msg("avcodec_parameters_to_context", ret);
        return ret;
    }

    ret = avcodec_open2(dec_ctx, decoder, NULL);
    if (ret < 0) {
        lav_throw_msg("avcodec_open2", ret);
        return ret;
    }

    return 0;
}

int decode_frame(const PXStreamContext* ctx, AVFrame* frame, AVPacket* packet) {

    int ret = 0;

    ret = avcodec_send_packet(ctx->dec_ctx, packet);
    if (ret < 0) {
        switch (ret) {
            case AVERROR(EAGAIN):
                return 0;
            case AVERROR_EOF:
                return ret;
            default:
                lav_throw_msg("avcodec_send_packet", ret);
                return ret;
        }
    }

    ret = avcodec_receive_frame(ctx->dec_ctx, frame);
    if (ret < 0) {
        switch (ret) {
            case AVERROR(EAGAIN):
                return 0;
            case AVERROR_EOF:
                return ret;
            default:
                lav_throw_msg("avcodec_receive_frame", ret);
                return ret;
        }
    }

    frame->pts = frame->best_effort_timestamp;

    av_packet_unref(packet);

    return 0;
}

int init_output(PXMediaContext* ctx, const char* filename, PXSettings* s) {

    int ret = 0;
    char* enc_name = NULL;

    ret = avformat_alloc_output_context2(&ctx->ofmt_ctx, NULL, NULL, filename);
    if (ret < 0) {
        lav_throw_msg("avformat_alloc_output_context2", ret);
        return ret;
    }

    // copy input streams to output and open encoders
    for (unsigned i = 0; i < ctx->ifmt_ctx->nb_streams; i++) {

        AVStream* ostream = avformat_new_stream(ctx->ofmt_ctx, NULL);
        if (!ostream) {
            lav_throw_msg("avformat_new_stream", ret);
            return ret;
        }

        enum AVMediaType stream_type = ctx->stream_ctx_vec[i].dec_ctx->codec_type;

        if (stream_type == AVMEDIA_TYPE_VIDEO) {

            ret = init_encoder(ctx, enc_name, &s->enc_opts_v, i, ostream);
            if (ret < 0)
                return ret;

        } else {
            ret = avcodec_parameters_copy(ostream->codecpar, ctx->ifmt_ctx->streams[i]->codecpar);
            if (ret < 0) {
                lav_throw_msg("avcodec_parameters_copy", ret);
                return ret;
            }
            ostream->time_base = ctx->ifmt_ctx->streams[i]->time_base;
        }
    }

    if (av_log_get_level() >= AV_LOG_INFO)
        av_dump_format(ctx->ofmt_ctx, 0, filename, 1);

    if (!(ctx->ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&ctx->ofmt_ctx->pb, filename, AVIO_FLAG_WRITE);
        if (ret < 0) {
            lav_throw_msg("avio_open", ret);
            return ret;
        }
    }

    ret = avformat_write_header(ctx->ofmt_ctx, NULL);
    if (ret < 0) {
        lav_throw_msg("avformat_write_header", ret);
        return ret;
    }

    return 0;
}

int init_encoder(const PXMediaContext* ctx, const char* enc_name, AVDictionary** enc_opts,
                 unsigned stream_idx, AVStream* out_stream) {
    int ret = 0;

    const AVCodecContext* dec_ctx = ctx->stream_ctx_vec[stream_idx].dec_ctx;

    const AVCodec* encoder = avcodec_find_encoder_by_name(enc_name);
    if (!encoder) {
        const char* default_enc = "libx264";
        encoder = avcodec_find_encoder_by_name(default_enc);
    }

    AVCodecContext* enc_ctx = avcodec_alloc_context3(encoder);
    if (!enc_ctx) {
        oom(sizeof *enc_ctx);
        return AVERROR(ENOMEM);
    }

    ctx->stream_ctx_vec[stream_idx].enc_ctx = enc_ctx;

    enc_ctx->time_base = av_inv_q(dec_ctx->framerate);
    enc_ctx->width = dec_ctx->width;
    enc_ctx->height = dec_ctx->height;
    enc_ctx->sample_aspect_ratio = dec_ctx->sample_aspect_ratio;
    enc_ctx->pix_fmt = dec_ctx->pix_fmt;

    if (ctx->ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        enc_ctx->flags |= AVFMT_GLOBALHEADER;

    ret = avcodec_parameters_from_context(out_stream->codecpar, enc_ctx);
    if (ret < 0) {
        lav_throw_msg("avcodec_parameters_from_context", ret);
        return ret;
    }

    ret = avcodec_open2(enc_ctx, encoder, enc_opts);
    if (ret < 0) {
        lav_throw_msg("avcodec_open2", ret);
        return ret;
    }

    return 0;
}

int encode_frame(const PXMediaContext* ctx, AVFrame* frame, AVPacket* packet, unsigned stream_idx) {

    int ret = 0;
    PXStreamContext* stc = &ctx->stream_ctx_vec[stream_idx];

    ret = avcodec_send_frame(stc->enc_ctx, frame);
    if (ret < 0)
        switch (ret) {
            case AVERROR(EAGAIN):
                return 0;
            case AVERROR_EOF:
                return ret;
            case AVERROR(EINVAL): // should not happen
                ret = 0;
                break;
            default:
                lav_throw_msg("avcodec_send_frame", ret);
                return ret;
        }

    while (ret >= 0) {

        ret = avcodec_receive_packet(stc->enc_ctx, packet);
        if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {
            return 0;
        } else if (ret < 0) {
            lav_throw_msg("avcodec_receive_packet", ret);
            return ret;
        }

        packet->stream_index = stream_idx;

        av_packet_rescale_ts(packet, ctx->ifmt_ctx->streams[stream_idx]->time_base,
                             ctx->ofmt_ctx->streams[stream_idx]->time_base);

        ret = av_interleaved_write_frame(ctx->ofmt_ctx, packet);
        if (ret < 0) {
            lav_throw_msg("av_interleaved_write_frame", ret);
            return ret;
        }
    }

    av_frame_unref(frame);

    return ret;
}

int flush_encoder(const PXMediaContext* ctx, AVPacket* packet, unsigned stream_idx) {
    return (ctx->stream_ctx_vec[stream_idx].enc_ctx->codec->capabilities & AV_CODEC_CAP_DELAY)
               ? encode_frame(ctx, NULL, packet, stream_idx)
               : 0;
}
