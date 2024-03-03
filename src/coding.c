#include "internals.h"

#include <pixie/coding.h>
#include <pixie/util/utils.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

static int init_input(PXMediaContext* ctx, const char* in_file);
static int init_output(PXMediaContext* ctx, const PXSettings* s);

PXMediaContext* px_media_ctx_alloc(void) {
    PXMediaContext* ctx = calloc(1, sizeof *ctx);
    if (!ctx)
        px_oom_msg(sizeof *ctx);
    return ctx;
}

int px_media_ctx_new(PXMediaContext** ctx, const PXSettings* s, int input_idx) {
    *ctx = px_media_ctx_alloc();
    if (!*ctx)
        return PXERROR(ENOMEM);

    PXMediaContext* pctx = *ctx;

    pctx->stream_idx = -1;

    int ret = init_input(pctx, s->input_files[input_idx]);
    if (ret < 0) {
        px_log(PX_LOG_ERROR, "Error occurred while processing input file \"%s\"\n",
               s->input_files[input_idx]);
        return ret;
    }

    ret = init_output(pctx, s);
    if (ret < 0) {
        px_log(PX_LOG_ERROR, "Error occurred while processing output file \"%s\"\n", s->output_file);
        return ret;
    }

    return 0;
}

void px_media_ctx_free(PXMediaContext** ctx) {
    if (!ctx || !*ctx)
        return;
    PXMediaContext* pctx = *ctx;

    if (pctx->ifmt_ctx) {
        for (unsigned i = 0; i < pctx->ifmt_ctx->nb_streams; i++) {
            if (pctx->coding_ctx_arr[i].dec_ctx) {
                avcodec_free_context(&pctx->coding_ctx_arr[i].dec_ctx);
            }
            if (pctx->coding_ctx_arr[i].enc_ctx) {
                avcodec_free_context(&pctx->coding_ctx_arr[i].enc_ctx);
            }
        }

        avformat_close_input(&pctx->ifmt_ctx);
        pctx->ifmt_ctx = NULL;
    }

    px_free(&pctx->coding_ctx_arr);

    if (pctx->ofmt_ctx) {
        if (!(pctx->ofmt_ctx->oformat->flags & AVFMT_NOFILE))
            avio_closep(&pctx->ofmt_ctx->pb);

        avformat_free_context(pctx->ofmt_ctx);
        pctx->ofmt_ctx = NULL;
    }

    pctx->stream_idx = -1;

    px_free(ctx);
}

static int init_input(PXMediaContext* ctx, const char* in_file) {
    int ret = avformat_open_input(&ctx->ifmt_ctx, in_file, NULL, NULL);
    if (ret < 0) {
        LAV_THROW_MSG("avformat_open_input", ret);
        return ret;
    }

    ret = avformat_find_stream_info(ctx->ifmt_ctx, NULL);
    if (ret < 0) {
        LAV_THROW_MSG("avformat_find_stream_info", ret);
        return ret;
    }

    ctx->coding_ctx_arr = calloc(ctx->ifmt_ctx->nb_streams, sizeof *ctx->coding_ctx_arr);
    if (!ctx->coding_ctx_arr) {
        px_oom_msg(sizeof *ctx->coding_ctx_arr);
        return PXERROR(ENOMEM);
    }

    bool streams_found = false;

    for (unsigned i = 0; i < ctx->ifmt_ctx->nb_streams; i++) {
        enum AVMediaType stream_type = ctx->ifmt_ctx->streams[i]->codecpar->codec_type;
        if (stream_type != AVMEDIA_TYPE_VIDEO)
            continue;

        streams_found = true;

        AVStream* istream = ctx->ifmt_ctx->streams[i];

        const AVCodec* decoder = avcodec_find_decoder(istream->codecpar->codec_id);
        if (!decoder) {
            px_log(PX_LOG_ERROR, "Failed to find decoder for stream %d\n", i);
            return AVERROR_DECODER_NOT_FOUND;
        }

        AVCodecContext* dec_ctx = avcodec_alloc_context3(decoder);
        if (!dec_ctx) {
            px_oom_msg(sizeof *dec_ctx);
            return AVERROR(ENOMEM);
        }

        ret = avcodec_parameters_to_context(dec_ctx, istream->codecpar);
        if (ret < 0) {
            LAV_THROW_MSG("avcodec_parameters_to_context", ret);
            return ret;
        }

        ret = avcodec_open2(dec_ctx, decoder, NULL);
        if (ret < 0) {
            LAV_THROW_MSG("avcodec_open2", ret);
            return ret;
        }

        dec_ctx->framerate = av_guess_frame_rate(ctx->ifmt_ctx, istream, NULL);

        ctx->coding_ctx_arr[i].dec_ctx = dec_ctx;

        if (av_log_get_level() >= AV_LOG_INFO)
            av_dump_format(ctx->ifmt_ctx, (int)i, in_file, false);
    }

    if (!streams_found) {
        px_log(PX_LOG_ERROR, "No video streams found in file \"%s\"\n", in_file);
        return AVERROR_INVALIDDATA;
    }

    return 0;
}

static int init_output(PXMediaContext* ctx, const PXSettings* s) {
    int ret = avformat_alloc_output_context2(&ctx->ofmt_ctx, NULL, NULL, s->output_file);
    if (ret < 0) {
        LAV_THROW_MSG("avformat_alloc_output_context2", ret);
        return ret;
    }

    for (unsigned i = 0; i < ctx->ifmt_ctx->nb_streams; i++) {

        AVStream* ostream = avformat_new_stream(ctx->ofmt_ctx, NULL);
        if (!ostream) {
            LAV_THROW_MSG("avformat_new_stream", ret);
            return ret;
        }

        enum AVMediaType stream_type = ctx->ifmt_ctx->streams[i]->codecpar->codec_type;

        if (stream_type != AVMEDIA_TYPE_VIDEO) {
            ret = avcodec_parameters_copy(ostream->codecpar, ctx->ifmt_ctx->streams[i]->codecpar);
            if (ret < 0) {
                LAV_THROW_MSG("avcodec_parameters_copy", ret);
                return ret;
            }
            ostream->time_base = ctx->ifmt_ctx->streams[i]->time_base;
            continue;
        }

        const AVCodec* encoder = avcodec_find_encoder_by_name(s->enc_name_v);
        if (!encoder) {
            const char* default_enc = "libx264";
            if (s->enc_name_v != NULL)
                px_log(PX_LOG_WARN, "Failed to find encoder \"%s\", using default encoder %s\n",
                       s->enc_name_v, default_enc);
            encoder = avcodec_find_encoder_by_name(default_enc);
        }

        AVCodecContext* enc_ctx = avcodec_alloc_context3(encoder);
        if (!enc_ctx) {
            px_oom_msg(sizeof *enc_ctx);
            return AVERROR(ENOMEM);
        }

        const AVCodecContext* dec_ctx = ctx->coding_ctx_arr[i].dec_ctx;

        enc_ctx->time_base = av_inv_q(dec_ctx->framerate);
        ostream->time_base = enc_ctx->time_base;

        enc_ctx->width = dec_ctx->width;
        enc_ctx->height = dec_ctx->height;
        enc_ctx->sample_aspect_ratio = dec_ctx->sample_aspect_ratio;
        enc_ctx->pix_fmt = dec_ctx->pix_fmt;

        AVDictionary* opts = NULL;
        ret = av_dict_parse_string(&opts, s->enc_opts_v, "=", ":", 0);
        if (ret < 0) {
            LAV_THROW_MSG("av_dict_parse_string", ret);
            return ret;
        }

        ret = avcodec_open2(enc_ctx, encoder, &opts);
        if (ret < 0) {
            LAV_THROW_MSG("avcodec_open2", ret);
            return ret;
        }

        ret = avcodec_parameters_from_context(ostream->codecpar, enc_ctx);
        if (ret < 0) {
            LAV_THROW_MSG("avcodec_parameters_from_context", ret);
            return ret;
        }

        ctx->coding_ctx_arr[i].enc_ctx = enc_ctx;

        if (ctx->ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
            enc_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

        if (av_log_get_level() >= AV_LOG_INFO)
            av_dump_format(ctx->ofmt_ctx, (int)i, s->output_file, true);
    }

    if (!(ctx->ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&ctx->ofmt_ctx->pb, s->output_file, AVIO_FLAG_WRITE);
        if (ret < 0) {
            LAV_THROW_MSG("avio_open", ret);
            return ret;
        }
    }

    ret = avformat_write_header(ctx->ofmt_ctx, NULL);
    if (ret < 0) {
        LAV_THROW_MSG("avformat_write_header", ret);
        return ret;
    }

    return 0;
}
