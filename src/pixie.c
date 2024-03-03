#include "internals.h"

#include <pixie/pixie.h>
#include <pixie/util/utils.h>

#include <libswscale/swscale.h>
#include <libavutil/pixdesc.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>

PXContext* px_ctx_alloc(void) {
    PXContext* pxc = calloc(1, sizeof *pxc);
    if (!pxc)
        px_oom_msg(sizeof *pxc);
    return pxc;
}

static int conv_pix_fmt(AVFrame* dest, const AVFrame* src, enum AVPixelFormat dest_pix_fmt) {
    struct SwsContext* sws = sws_getContext(src->width, src->height, src->format, src->width, src->height,
                                            dest_pix_fmt, SWS_BILINEAR, NULL, NULL, NULL);
    if (!sws) {
        LAV_THROW_MSG("sws_getContext", AVERROR(EINVAL));
        return AVERROR(EINVAL);
    }

    int ret = av_frame_copy_props(dest, src);
    if (ret < 0) {
        LAV_THROW_MSG("av_frame_copy_props", ret);
        return ret;
    }
    dest->format = dest_pix_fmt;
    dest->width = src->width;
    dest->height = src->height;

    ret = sws_scale_frame(sws, dest, src);
    if (ret < 0) {
        LAV_THROW_MSG("sws_scale_frame", ret);
        return ret;
    }
    sws_freeContext(sws);

    return 0;
}

static int read_frame(PXMediaContext* ctx, AVPacket* pkt) {
    int ret = av_read_frame(ctx->ifmt_ctx, pkt);
    if (ret == AVERROR_EOF) {
        goto early_ret;
    } else if (ret < 0) {
        LAV_THROW_MSG("av_read_frame", ret);
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
            LAV_THROW_MSG("av_interleaved_write_frame", ret);
            goto early_ret;
        }
        ret = AVERROR(EAGAIN);
        goto early_ret;
    }

    return 0;

early_ret:
    av_packet_unref(pkt);
    return ret;
}

static int encode_frame(PXMediaContext* ctx, const AVFrame* frame) {
    AVCodecContext* enc_ctx = ctx->coding_ctx_arr[ctx->stream_idx].enc_ctx;
    int ret = avcodec_send_frame(enc_ctx, frame);
    if (ret < 0) {
        LAV_THROW_MSG("avcodec_send_frame", ret);
        return ret;
    }

    AVPacket* pkt = av_packet_alloc();
    while (ret >= 0) {
        ret = avcodec_receive_packet(enc_ctx, pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            ret = 0;
            goto end;
        } else if (ret < 0) {
            LAV_THROW_MSG("avcodec_receive_packet", ret);
            goto end;
        }

        pkt->stream_index = ctx->stream_idx;

        const AVStream* istream = ctx->ifmt_ctx->streams[ctx->stream_idx];
        const AVStream* ostream = ctx->ofmt_ctx->streams[ctx->stream_idx];
        pkt->duration = ostream->time_base.den / ostream->time_base.num / istream->avg_frame_rate.num *
                        istream->avg_frame_rate.den;

        av_packet_rescale_ts(pkt, istream->time_base, ostream->time_base);

        ret = av_interleaved_write_frame(ctx->ofmt_ctx, pkt);
        if (ret < 0) {
            LAV_THROW_MSG("av_interleaved_write_frame", ret);
            goto end;
        }

        ctx->frames_output++;
        av_packet_unref(pkt);
    }

end:
    av_packet_free(&pkt);
    return 0;
}

static int filter_encode_frame(PXContext* pxc, AVFrame* frame) {
    PXFrame px_frame = {0};
    int ret = px_frame_from_av(&px_frame, frame);
    if (ret < 0)
        return ret;
    const PXFrame* last_out_frame = &px_frame;

    for (int i = 0; i < pxc->fltr_ctx->n_filters; i++) {
        PXFilter* fltr = pxc->fltr_ctx->filters[i];

        fltr->in_frame = last_out_frame;
        px_frame_new(&fltr->out_frame, fltr->in_frame->width, fltr->in_frame->height, fltr->in_frame->pix_fmt,
                     NULL);
        fltr->out_frame->av_pix_fmt = fltr->in_frame->av_pix_fmt;
        fltr->frame_num = pxc->media_ctx->frames_decoded;

        ret = fltr->apply(fltr); // TODO: optional apply
        if (ret < 0) {
            px_log(PX_LOG_ERROR, "Failed to apply filter \"%s\"\n", fltr->name);
            goto end;
        }
        last_out_frame = fltr->out_frame;
    }
    px_frame_to_av(frame, last_out_frame);

    enum AVPixelFormat enc_pix_fmt =
        pxc->media_ctx->coding_ctx_arr[pxc->media_ctx->stream_idx].enc_ctx->pix_fmt;
    bool conv_needed = frame->format != enc_pix_fmt;

    AVFrame* conv_frame = frame;
    if (conv_needed) {
        conv_frame = av_frame_alloc();
        if (!conv_frame) {
            px_oom_msg(sizeof *conv_frame);
            ret = AVERROR(ENOMEM);
            goto end;
        }

        px_log(PX_LOG_INFO, "Converting frame from %s to %s\n", av_get_pix_fmt_name(frame->format),
               av_get_pix_fmt_name(enc_pix_fmt));
        ret = conv_pix_fmt(conv_frame, frame, enc_pix_fmt);
        if (ret < 0) {
            av_frame_free(&conv_frame);
            goto end;
        }
    }

    ret = encode_frame(pxc->media_ctx, conv_frame);
    if (ret == AVERROR_EOF)
        ret = 0;

    if (conv_needed)
        av_frame_free(&conv_frame);

end:
    px_frame_free_internal(&px_frame);
    return ret;
}

static int transcode_packet(PXContext* pxc, AVPacket* pkt) {
    AVCodecContext* dec_ctx = pxc->media_ctx->coding_ctx_arr[pxc->media_ctx->stream_idx].dec_ctx;
    int ret = avcodec_send_packet(dec_ctx, pkt);
    if (ret < 0) {
        LAV_THROW_MSG("avcodec_send_packet", ret);
        return ret;
    }

    if (pkt)
        av_packet_unref(pkt);

    AVFrame* frame = av_frame_alloc();
    while (ret >= 0) {
        ret = avcodec_receive_frame(dec_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            ret = 0;
            break;
        } else if (ret < 0) {
            LAV_THROW_MSG("avcodec_receive_frame", ret);
            break;
        }
        pxc->media_ctx->frames_decoded++;
        frame->pts = frame->best_effort_timestamp;

        ret = filter_encode_frame(pxc, frame);
        if (ret < 0)
            break;

        av_frame_unref(frame);
    }

    av_frame_free(&frame);
    return ret;
}

int px_transcode(PXContext* pxc) {
    int ret = 0;

    AVPacket* pkt = av_packet_alloc();
    while (true) {
        ret = read_frame(pxc->media_ctx, pkt);
        if (ret == AVERROR(EAGAIN)) {
            continue;
        } else if (ret == AVERROR_EOF) {
            ret = 0;
            break;
        } else if (ret < 0) {
            goto end;
        }

        ret = transcode_packet(pxc, pkt);
        if (ret < 0)
            goto end;
    }

    // flush
    for (unsigned i = 0; i < pxc->media_ctx->ifmt_ctx->nb_streams; i++) {
        enum AVMediaType stream_type = pxc->media_ctx->ifmt_ctx->streams[i]->codecpar->codec_type;
        if (stream_type != AVMEDIA_TYPE_VIDEO)
            continue;

        pxc->media_ctx->stream_idx = (int)i;

        if (pxc->media_ctx->coding_ctx_arr[i].dec_ctx->codec->capabilities & AV_CODEC_CAP_DELAY) {
            ret = transcode_packet(pxc, NULL);
            if (ret < 0)
                goto end;
        }

        if (pxc->media_ctx->coding_ctx_arr[i].enc_ctx->codec->capabilities & AV_CODEC_CAP_DELAY) {
            ret = encode_frame(pxc->media_ctx, NULL);
            if (ret != AVERROR_EOF && ret < 0)
                goto end;
        }
    }

    ret = av_write_trailer(pxc->media_ctx->ofmt_ctx);
    if (ret < 0)
        LAV_THROW_MSG("av_write_trailer", ret);

end:
    pxc->transc_thread.done = true;
    av_packet_free(&pkt);
    return ret;
}

void px_ctx_free(PXContext** pxc) {
    if (!pxc || !*pxc)
        return;
    PXContext* ppxc = *pxc;

    px_filter_ctx_free(&ppxc->fltr_ctx);
    px_media_ctx_free(&ppxc->media_ctx);

    px_free(pxc);
}

const char* px_ffmpeg_version(void) {
    return av_version_info();
}
