#include "coding.h"
#include "util/utils.h"
#include <libavutil/avutil.h>

static int init_input(PXMediaContext* ctx, const char* in_file);
static int init_output(PXMediaContext* ctx, PXSettings* s);

int px_media_ctx_init(PXMediaContext* ctx, PXSettings* s, int input_idx) {

    int ret = 0;
    ctx->stream_idx = -1;

    ret = init_input(ctx, s->input_files[input_idx]);
    if (ret < 0) {
        $px_log(PX_LOG_ERROR, "Error occurred while processing input file \"%s\"\n",
                s->input_files[input_idx]);
        return ret;
    }

    ret = init_output(ctx, s);
    if (ret < 0) {
        $px_log(PX_LOG_ERROR, "Error occurred while processing output file \"%s\"\n", s->output_file);
        return ret;
    }

    return 0;
}

void px_media_ctx_free(PXMediaContext* ctx) {

    if (ctx->ifmt_ctx) {
        for (unsigned i = 0; i < ctx->ifmt_ctx->nb_streams; i++) {
            if (ctx->coding_ctx_arr[i].dec_ctx) {
                avcodec_free_context(&ctx->coding_ctx_arr[i].dec_ctx);
            }
            if (ctx->coding_ctx_arr[i].enc_ctx) {
                avcodec_free_context(&ctx->coding_ctx_arr[i].enc_ctx);
            }
        }

        avformat_close_input(&ctx->ifmt_ctx);
        ctx->ifmt_ctx = NULL;
    }

    free_s(&ctx->coding_ctx_arr);

    if (ctx->ofmt_ctx) {
        if (!(ctx->ofmt_ctx->oformat->flags & AVFMT_NOFILE))
            avio_closep(&ctx->ofmt_ctx->pb);

        avformat_free_context(ctx->ofmt_ctx);
        ctx->ofmt_ctx = NULL;
    }

    ctx->stream_idx = -1;
}

static int init_input(PXMediaContext* ctx, const char* in_file) {

    int ret = 0;

    ret = avformat_open_input(&ctx->ifmt_ctx, in_file, NULL, NULL);
    if (ret < 0) {
        $lav_throw_msg("avformat_open_input", ret);
        return ret;
    }

    ret = avformat_find_stream_info(ctx->ifmt_ctx, NULL);
    if (ret < 0) {
        $lav_throw_msg("avformat_find_stream_info", ret);
        return ret;
    }

    ctx->coding_ctx_arr = calloc(ctx->ifmt_ctx->nb_streams, sizeof *ctx->coding_ctx_arr);
    if (!ctx->coding_ctx_arr) {
        oom(sizeof *ctx->coding_ctx_arr);
        return AVERROR(ENOMEM);
    }

    bool streams_found = false;

    for (unsigned i = 0; i < ctx->ifmt_ctx->nb_streams; i++) {

        enum AVMediaType stream_type = ctx->ifmt_ctx->streams[i]->codecpar->codec_type;
        if (stream_type == AVMEDIA_TYPE_VIDEO) {
            streams_found = true;

            AVStream* istream = ctx->ifmt_ctx->streams[i];

            const AVCodec* decoder = avcodec_find_decoder(istream->codecpar->codec_id);
            if (!decoder) {
                $px_log(PX_LOG_ERROR, "Failed to find decoder for stream %d\n", i);
                return AVERROR_DECODER_NOT_FOUND;
            }

            AVCodecContext* dec_ctx = avcodec_alloc_context3(decoder);
            if (!dec_ctx) {
                oom(sizeof *dec_ctx);
                return AVERROR(ENOMEM);
            }

            ret = avcodec_parameters_to_context(dec_ctx, istream->codecpar);
            if (ret < 0) {
                $lav_throw_msg("avcodec_parameters_to_context", ret);
                return ret;
            }

            ret = avcodec_open2(dec_ctx, decoder, NULL);
            if (ret < 0) {
                $lav_throw_msg("avcodec_open2", ret);
                return ret;
            }

            dec_ctx->framerate = av_guess_frame_rate(ctx->ifmt_ctx, istream, NULL);

            ctx->coding_ctx_arr[i].dec_ctx = dec_ctx;

            if (av_log_get_level() >= AV_LOG_INFO)
                av_dump_format(ctx->ifmt_ctx, i, in_file, false);
        }
    }

    if (!streams_found) {
        $px_log(PX_LOG_ERROR, "No video streams found in file \"%s\"\n", in_file);
        return AVERROR_INVALIDDATA;
    }

    return 0;
}

static int init_output(PXMediaContext* ctx, PXSettings* s) {

    int ret = 0;

    ret = avformat_alloc_output_context2(&ctx->ofmt_ctx, NULL, NULL, s->output_file);
    if (ret < 0) {
        $lav_throw_msg("avformat_alloc_output_context2", ret);
        return ret;
    }

    for (unsigned i = 0; i < ctx->ifmt_ctx->nb_streams; i++) {

        AVStream* ostream = avformat_new_stream(ctx->ofmt_ctx, NULL);
        if (!ostream) {
            $lav_throw_msg("avformat_new_stream", ret);
            return ret;
        }

        enum AVMediaType stream_type = ctx->ifmt_ctx->streams[i]->codecpar->codec_type;

        // copy non-video streams
        if (stream_type != AVMEDIA_TYPE_VIDEO) {
            ret = avcodec_parameters_copy(ostream->codecpar, ctx->ifmt_ctx->streams[i]->codecpar);
            if (ret < 0) {
                $lav_throw_msg("avcodec_parameters_copy", ret);
                return ret;
            }
            ostream->time_base = ctx->ifmt_ctx->streams[i]->time_base;
            continue;
        }

        // init video encoder
        const AVCodec* encoder = avcodec_find_encoder_by_name(s->enc_name_v);
        if (!encoder) {
            const char* default_enc = "libx264";
            $px_log(PX_LOG_WARN, "Failed to find encoder \"%s\", using default encoder %s\n", s->enc_name_v,
                    default_enc);
            encoder = avcodec_find_encoder_by_name(default_enc);
        }

        AVCodecContext* enc_ctx = avcodec_alloc_context3(encoder);
        if (!enc_ctx) {
            oom(sizeof *enc_ctx);
            return AVERROR(ENOMEM);
        }

        const AVCodecContext* dec_ctx = ctx->coding_ctx_arr[i].dec_ctx;

        enc_ctx->time_base = av_inv_q(dec_ctx->framerate);
        ostream->time_base = enc_ctx->time_base;

        enc_ctx->width = dec_ctx->width;
        enc_ctx->height = dec_ctx->height;
        enc_ctx->sample_aspect_ratio = dec_ctx->sample_aspect_ratio;
        enc_ctx->pix_fmt = dec_ctx->pix_fmt;

        ret = avcodec_open2(enc_ctx, encoder, &s->enc_opts_v);
        if (ret < 0) {
            $lav_throw_msg("avcodec_open2", ret);
            return ret;
        }

        ret = avcodec_parameters_from_context(ostream->codecpar, enc_ctx);
        if (ret < 0) {
            $lav_throw_msg("avcodec_parameters_from_context", ret);
            return ret;
        }

        ctx->coding_ctx_arr[i].enc_ctx = enc_ctx;

        if (ctx->ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
            enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

        if (av_log_get_level() >= AV_LOG_INFO)
            av_dump_format(ctx->ofmt_ctx, i, s->output_file, true);
    }

    if (!(ctx->ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&ctx->ofmt_ctx->pb, s->output_file, AVIO_FLAG_WRITE);
        if (ret < 0) {
            $lav_throw_msg("avio_open", ret);
            return ret;
        }
    }

    ret = avformat_write_header(ctx->ofmt_ctx, NULL);
    if (ret < 0) {
        $lav_throw_msg("avformat_write_header", ret);
        return ret;
    }

    return 0;
}

int px_read_video_frame(PXMediaContext* ctx, AVPacket* pkt) {

    int ret = 0;

    ret = av_read_frame(ctx->ifmt_ctx, pkt);
    if (ret == AVERROR_EOF) {
        goto early_ret;
    } else if (ret < 0) {
        $lav_throw_msg("av_read_frame", ret);
        goto early_ret;
    }

    ctx->stream_idx = pkt->stream_index;

    enum AVMediaType stream_type = ctx->ifmt_ctx->streams[ctx->stream_idx]->codecpar->codec_type;
    if (stream_type != AVMEDIA_TYPE_VIDEO) {

        AVRational in_tb = ctx->ifmt_ctx->streams[ctx->stream_idx]->time_base;
        AVRational out_tb = ctx->ofmt_ctx->streams[ctx->stream_idx]->time_base;
        av_packet_rescale_ts(pkt, in_tb, out_tb);

        ret = av_interleaved_write_frame(ctx->ofmt_ctx, pkt);
        if (ret < 0) {
            $lav_throw_msg("av_interleaved_write_frame", ret);
            goto early_ret;
        }
        ret = AVERROR(EAGAIN);
        goto early_ret;
    }

    // AVRational in_tb = ctx->ifmt_ctx->streams[ctx->stream_idx]->time_base;
    // AVRational enc_tb = ctx->coding_ctx_arr[ctx->stream_idx].enc_ctx->time_base;
    // av_packet_rescale_ts(pkt, in_tb, enc_tb);

    return 0;

early_ret:
    av_packet_unref(pkt);
    return ret;
}

int px_encode_frame(PXMediaContext* ctx, const AVFrame* frame) {

    int ret = 0;
    AVCodecContext* enc_ctx = ctx->coding_ctx_arr[ctx->stream_idx].enc_ctx;

    ret = avcodec_send_frame(enc_ctx, frame);
    if (ret < 0) {
        $lav_throw_msg("avcodec_send_frame", ret);
        return ret;
    }

    while (ret >= 0) {
        AVPacket pkt = {0};
        ret = avcodec_receive_packet(enc_ctx, &pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            av_packet_unref(&pkt);
            return 0;
        } else if (ret < 0) {
            $lav_throw_msg("avcodec_receive_packet", ret);
            return ret;
        }

        pkt.stream_index = ctx->stream_idx;

        const int fps = ctx->ifmt_ctx->streams[ctx->stream_idx]->avg_frame_rate.num /
                        ctx->ifmt_ctx->streams[ctx->stream_idx]->avg_frame_rate.den;
        pkt.duration = enc_ctx->time_base.den / enc_ctx->time_base.num / fps;

        av_packet_rescale_ts(&pkt, ctx->ifmt_ctx->streams[ctx->stream_idx]->time_base,
                             ctx->ofmt_ctx->streams[ctx->stream_idx]->time_base);

        ret = av_interleaved_write_frame(ctx->ofmt_ctx, &pkt);
        if (ret < 0) {
            $lav_throw_msg("av_interleaved_write_frame", ret);
            return ret;
        }

        ctx->frames_output++;
        av_packet_unref(&pkt);
    }

    return 0;
}

int px_flush_encoder(PXMediaContext* ctx) {
    if (ctx->coding_ctx_arr[ctx->stream_idx].enc_ctx->codec->capabilities & AV_CODEC_CAP_DELAY)
        return px_encode_frame(ctx, NULL);
    return 0;
}

int px_flush_decoder(PXMediaContext* ctx) {

    int ret = 0;

    AVCodecContext* dec_ctx = ctx->coding_ctx_arr[ctx->stream_idx].dec_ctx;
    if (!(dec_ctx->codec->capabilities & AV_CODEC_CAP_DELAY))
        return 0;

    ret = avcodec_send_packet(dec_ctx, NULL);
    if (ret < 0) {
        $lav_throw_msg("avcodec_send_packet", ret);
        return ret;
    }

    while (1) {
        AVFrame frame = {0};
        ret = avcodec_receive_frame(dec_ctx, &frame);
        if (ret < 0) {
            switch (ret) {
                case AVERROR(EAGAIN):
                    return 0;
                case AVERROR_EOF:
                    return ret;
                default:
                    $lav_throw_msg("avcodec_receive_frame", ret);
                    return ret;
            }
        }
        ctx->frames_decoded++;
        frame.pts = frame.best_effort_timestamp;

        ret = px_encode_frame(ctx, &frame);
        if (ret < 0)
            return ret;

        av_frame_unref(&frame);
    }

    return 0;
}