#include "incl/coding.h"

void uninit_px_mediactx(PXMediaContext* ctx) {

    for (int i = 0; i < ctx->ifmt_ctx->nb_streams; i++) {

        if (ctx->stream_ctx_vec[i].dec_ctx)
            avcodec_free_context(&ctx->stream_ctx_vec[i].dec_ctx);
        if (ctx->stream_ctx_vec[i].enc_ctx)
            avcodec_free_context(&ctx->stream_ctx_vec[i].enc_ctx);

        if (ctx->stream_ctx_vec[i].dec_frame)
            av_frame_free(&ctx->stream_ctx_vec[i].dec_frame);
        if (ctx->stream_ctx_vec[i].enc_pkt)
            av_packet_free(&ctx->stream_ctx_vec[i].enc_pkt);
    }

    av_freep(ctx->stream_ctx_vec);

    avformat_close_input(&ctx->ifmt_ctx);

    if (!(ctx->ofmt_ctx->oformat->flags & AVFMT_NOFILE))
        avio_closep(&ctx->ofmt_ctx->pb);
    avformat_free_context(ctx->ofmt_ctx);
}

int init_input(PXMediaContext* ctx, const char* filename) {

    int ret;
    int i;

    ctx->ifmt_ctx = NULL;

    if ((ret = avformat_open_input(&ctx->ifmt_ctx, filename, NULL, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot open input file '%s'\n", filename);
        return ret;
    }

    if ((ret = avformat_find_stream_info(ctx->ifmt_ctx, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
        return ret;
    }

    ctx->stream_ctx_vec = av_calloc(ctx->ifmt_ctx->nb_streams, sizeof(*ctx->stream_ctx_vec));
    if (!ctx->stream_ctx_vec)
        return AVERROR(ENOMEM);

    ctx->stream_ctx_vec->dec_frame = av_frame_alloc();
    if (!ctx->stream_ctx_vec->dec_frame)
        return AVERROR(ENOMEM);

    ctx->stream_ctx_vec->enc_pkt = av_packet_alloc();
    if (!ctx->stream_ctx_vec->enc_pkt)
        return AVERROR(ENOMEM);

    const AVCodec* decoder = NULL;

    for (i = 0; i < ctx->ifmt_ctx->nb_streams; i++) {

        int stream_type = ctx->ifmt_ctx->streams[i]->codecpar->codec_type;

        if (stream_type == AVMEDIA_TYPE_UNKNOWN) {
            av_log(NULL, AV_LOG_WARNING, "Stream %d is of unknown type, cannot proceed\n", i);
            return AVERROR_INVALIDDATA;
        }

        if (stream_type == AVMEDIA_TYPE_VIDEO || stream_type == AVMEDIA_TYPE_AUDIO) {

            if ((ret = init_decoder(ctx, i)) < 0) {
                av_log(NULL, AV_LOG_ERROR, "Failed to initialize decoder for stream %d\n", i);
                return ret;
            }
            ctx->stream_ctx_vec[i].dec_ctx->framerate =
                av_guess_frame_rate(ctx->ifmt_ctx, ctx->ifmt_ctx->streams[i], NULL);
        }
    }

    av_dump_format(ctx->ifmt_ctx, 0, filename, 0);

    return 0;
}

int init_decoder(PXMediaContext* ctx, const unsigned int stream_idx) {

    int ret;
    const AVCodecParameters* dec_params = ctx->ifmt_ctx->streams[stream_idx]->codecpar;

    const AVCodec* decoder =
        avcodec_find_decoder(ctx->ifmt_ctx->streams[stream_idx]->codecpar->codec_id);
    if (!decoder) {
        av_log(NULL, AV_LOG_ERROR, "Failed to find decoder for stream %d\n", stream_idx);
        return AVERROR_DECODER_NOT_FOUND;
    }

    AVCodecContext* dec_ctx = avcodec_alloc_context3(decoder);
    if (!dec_ctx) {
        av_log(NULL, AV_LOG_ERROR, "Failed to allocate decoder context\n");
        return AVERROR(ENOMEM);
    }

    if ((ret = avcodec_parameters_to_context(dec_ctx, dec_params)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Failed to copy decoder parameters to input decoder context\n");
        return ret;
    }

    if ((ret = avcodec_open2(dec_ctx, decoder, NULL)) < 0) {
        // no message here as it'd just be a duplicate
        return ret;
    }

    ctx->stream_ctx_vec[stream_idx].dec_ctx = dec_ctx;

    return 0;
}

int decode_frame(PXStreamContext* ctx, AVFrame* frame, AVPacket* packet) {

    int ret;

    if ((ret = avcodec_send_packet(ctx->dec_ctx, packet)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Error sending a packet for decoding\n");
        return ret;
    }

    ret = avcodec_receive_frame(ctx->dec_ctx, frame);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        return 0;
    } else if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Failed to decode frame\n");
        return ret;
    }

    return 0;
}

int init_output(PXMediaContext* ctx, const char* filename, const char* enc_name,
                AVDictionary** enc_opts_v, AVDictionary** enc_opts_a) {

    int ret;
    ctx->ofmt_ctx = NULL;

    if ((ret = avformat_alloc_output_context2(&ctx->ofmt_ctx, NULL, NULL, filename)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Failed to allocate output context\n");
        return AVERROR_UNKNOWN;
    }

    // copy input streams to output and open encoders
    for (int i = 0; i < ctx->ifmt_ctx->nb_streams; i++) {

        // clang-format off
        AVStream* ostream = avformat_new_stream(ctx->ofmt_ctx, NULL);
        if (!ostream) {
            av_log(NULL, AV_LOG_ERROR, "Failed to add output stream\n");
            return AVERROR_UNKNOWN;
        }

        #define stream_is(type) ctx->stream_ctx_vec[i].dec_ctx->codec_type == AVMEDIA_TYPE_##type

        if (stream_is(VIDEO) || stream_is(AUDIO)) {

            ret = init_encoder(ctx, enc_name, stream_is(VIDEO) ? enc_opts_v : enc_opts_a, i, ostream);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Failed to initialize encoder for stream %d\n", i);
                return ret;
            }

        } else {
            ret = avcodec_parameters_copy(ostream->codecpar, ctx->ifmt_ctx->streams[i]->codecpar);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Failed to copy codec parameters to output stream\n");
                return ret;
            }
            ostream->time_base = ctx->ifmt_ctx->streams[i]->time_base;
        }
        // clang-format on
    }

    av_dump_format(ctx->ofmt_ctx, 0, filename, 1);

    if (!(ctx->ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&ctx->ofmt_ctx->pb, filename, AVIO_FLAG_WRITE);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Failed to open output file '%s'\n", filename);
            return ret;
        }
    }

    ret = avformat_write_header(ctx->ofmt_ctx, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Failed to write output stream header\n");
        return ret;
    }

    return 0;
}

int init_encoder(PXMediaContext* ctx, const char* enc_name, AVDictionary** enc_opts,
                 const unsigned int stream_idx, AVStream* out_stream) {
    int ret;

    const AVCodecContext* dec_ctx = ctx->stream_ctx_vec[stream_idx].dec_ctx;

    const AVCodec* encoder = avcodec_find_encoder_by_name(enc_name);
    if (!encoder) // use default
        encoder = avcodec_find_encoder_by_name(dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO ? "libx264"
                                                                                         : "aac");
    AVCodecContext* enc_ctx = avcodec_alloc_context3(encoder);
    if (!enc_ctx) {
        av_log(NULL, AV_LOG_ERROR, "Failed to allocate encoder context\n");
        return AVERROR(ENOMEM);
    }

    if (dec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {

        enc_ctx->time_base = av_inv_q(dec_ctx->framerate);
        enc_ctx->width = dec_ctx->width;
        enc_ctx->height = dec_ctx->height;
        enc_ctx->sample_aspect_ratio = dec_ctx->sample_aspect_ratio;
        enc_ctx->pix_fmt = dec_ctx->pix_fmt;

    } else {
        enc_ctx->sample_rate = dec_ctx->sample_rate;
        ret = av_channel_layout_copy(&enc_ctx->ch_layout, &dec_ctx->ch_layout);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Failed to copy channel layout for stream %d\n", stream_idx);
            return ret;
        }
        enc_ctx->sample_fmt = dec_ctx->sample_fmt;
        enc_ctx->time_base = (AVRational) {1, enc_ctx->sample_rate};
    }

    if (ctx->ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        enc_ctx->flags |= AVFMT_GLOBALHEADER;

    ret = avcodec_parameters_from_context(out_stream->codecpar, enc_ctx);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Failed to copy encoder parameters to output stream #%u\n",
               stream_idx);
        return ret;
    }

    av_log(NULL, AV_LOG_INFO, "Opening encoder for stream %d, timebase: %d/%d\n", stream_idx,
           enc_ctx->time_base.num, enc_ctx->time_base.den);

    if ((ret = avcodec_open2(enc_ctx, encoder, enc_opts)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Failed to open encoder\n");
        return ret;
    }

    ctx->stream_ctx_vec[stream_idx].enc_ctx = enc_ctx;

    return 0;
}

int encode_frame(PXMediaContext* ctx, AVFrame* frame, AVPacket* packet, unsigned int stream_idx) {

    int ret;
    av_packet_unref(packet);

    ret = avcodec_send_frame(ctx->stream_ctx_vec[stream_idx].enc_ctx, frame);
    if (ret < 0)
        return ret;

    while (ret >= 0) {
        ret = avcodec_receive_packet(ctx->stream_ctx_vec[stream_idx].enc_ctx, packet);

        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return 0;

        packet->stream_index = stream_idx;
        av_packet_rescale_ts(packet, ctx->stream_ctx_vec[stream_idx].enc_ctx->time_base,
                             ctx->ofmt_ctx->streams[stream_idx]->time_base);

        ret = av_interleaved_write_frame(ctx->ofmt_ctx, packet);
    }

    av_frame_unref(frame);

    return ret;
}

int flush_encoder(PXMediaContext* ctx, AVPacket* packet, unsigned int stream_idx) {
    return (ctx->stream_ctx_vec[stream_idx].enc_ctx->codec->capabilities & AV_CODEC_CAP_DELAY)
               ? encode_frame(ctx, 0, packet, stream_idx)
               : 0;
}
