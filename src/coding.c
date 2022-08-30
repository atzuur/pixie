#include "incl/coding.h"
#include <libavformat/avformat.h>

int open_input(CodingContext* ctx, const char* filename, const char* decoder_name) {

    int ret;
    int i;

    if ((ret = avformat_open_input(&ctx->fmt_ctx, filename, NULL, NULL)) < 0) {
        die(ctx, "Cannot open input file: \"%s\"\n", filename);
        return ret;
    }

    if ((ret = avformat_find_stream_info(ctx->fmt_ctx, NULL)) < 0) {
        die(ctx, "Cannot find stream information\n");
        return ret;
    }

    char vstream_found = 0, astream_found = 0;
    ctx->vstream_idx = 0;
    ctx->astream_idx = 0;

    // TODO https://github.com/FFmpeg/FFmpeg/blob/master/doc/examples/transcoding.c#L79-L107
    for (i = 0; i < ctx->fmt_ctx->nb_streams; i++) {

        const int stream_type = ctx->fmt_ctx->streams[i]->codecpar->codec_type;

        if (stream_type == AVMEDIA_TYPE_VIDEO) {
            ctx->codec_ctx->framerate =
                av_guess_frame_rate(ctx->fmt_ctx, ctx->fmt_ctx->streams[i], NULL);
            ctx->vstream_idx = i;
            vstream_found = 1;

        } else if (stream_type == AVMEDIA_TYPE_AUDIO) {
            ctx->astream_idx = i;
            astream_found = 1;
        }
    }

    if (!vstream_found) {
        die(ctx, "Cannot find video stream\n");
        return AVERROR_STREAM_NOT_FOUND;
    }

    // open decoder for video stream [and audio stream if it exists]
    for (i = ctx->vstream_idx; i <= ctx->vstream_idx + ctx->astream_idx; i += ctx->astream_idx) {

        const AVCodec* decoder = avcodec_find_decoder(ctx->fmt_ctx->streams[i]->codecpar->codec_id);
        if (!decoder) {
            die(ctx, "Failed to find decoder for stream %d\n", i);
            return AVERROR_DECODER_NOT_FOUND;
        }

        if ((ret = open_decoder(&ctx->codec_ctx, decoder, ctx->fmt_ctx->streams[i]->codecpar))) {
            die(ctx, "Failed to open decoder\n");
            return ret;
        }
    }

    av_dump_format(ctx->fmt_ctx, 0, filename, 0);

    return 0;
}

int open_decoder(AVCodecContext** ctx, const AVCodec* dec, AVCodecParameters* params) {

    int ret;

    *ctx = avcodec_alloc_context3(dec);
    if (!*ctx) {
        av_log(NULL, AV_LOG_ERROR, "Failed to allocate decoder context\n");
        return AVERROR(ENOMEM);
    }

    if (params) {
        if ((ret = avcodec_parameters_to_context(*ctx, params)) < 0) {
            av_log(NULL, AV_LOG_ERROR,
                   "Failed to copy decoder parameters to input decoder context\n");
            return ret;
        }
    }

    if ((ret = avcodec_open2(*ctx, dec, NULL)) < 0) {
        // no message here as it'd just be a duplicate of the one from open_input()
        avcodec_free_context(ctx);
        return ret;
    }

    return 0;
}

inline int decode_frame(CodingContext* ctx, AVFrame* frame, AVPacket* packet) {

    int ret;

    if ((ret = avcodec_send_packet(ctx->codec_ctx, packet)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Error sending a packet for decoding\n");
        return ret;
    }

    ret = avcodec_receive_frame(ctx->codec_ctx, frame);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        return 1;
    } else if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Failed to decode frame\n");
        return ret;
    }

    return 0;
}

int open_output(AVFormatContext* input, CodingContext* output, const char* filename) {

    int ret;

    if ((ret = avformat_alloc_output_context2(&output->fmt_ctx, NULL, NULL, filename)) < 0) {
        die(output, "Failed to allocate output context\n");
        return AVERROR_UNKNOWN;
    }

    AVStream* ostream;

    // copy input streams to output
    for (int i = 0; i < input->nb_streams; i++) {
        ostream = avformat_new_stream(output->fmt_ctx, 0);
        if (!ostream) {
            die(output, "Failed to allocate output stream\n");
        }

        if ((ret = avcodec_parameters_copy(ostream->codecpar, input->streams[i]->codecpar)) < 0) {
            die(output, "Failed to copy codec parameters to output stream\n");
            return 1;
        }
    }

    av_dump_format(output->fmt_ctx, 0, filename, 1);

    if (!(output->fmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&output->fmt_ctx->pb, filename, AVIO_FLAG_WRITE);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Failed to open output file '%s'\n", filename);
            return ret;
        }
    }

    ret = avformat_write_header(output->fmt_ctx, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Failed to write output stream header\n");
        return ret;
    }

    return 0;
}

int open_encoder(CodingContext* ctx, char* encoder_name, AVDictionary** enc_options,
                 AVRational timebase, AVCodecParameters* input_params) {

    int ret;

    const AVCodec* encoder = avcodec_find_encoder_by_name(encoder_name);
    ctx->codec_ctx = avcodec_alloc_context3(encoder);
    if (!ctx->codec_ctx) {
        die(ctx, "Failed to allocate encoder context\n");
        return AVERROR(ENOMEM);
    }

    ctx->codec_ctx->profile = FF_PROFILE_UNKNOWN;
    ctx->codec_ctx->time_base = timebase;
    ctx->codec_ctx->width = input_params->width;
    ctx->codec_ctx->height = input_params->height;
    ctx->codec_ctx->sample_aspect_ratio = input_params->sample_aspect_ratio;
    ctx->codec_ctx->pix_fmt = input_params->format;

    if (ctx->fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
        ctx->codec_ctx->flags |= AVFMT_GLOBALHEADER;

    if ((ret = avcodec_open2(ctx->codec_ctx, encoder, enc_options)) < 0) {
        die(ctx, "Failed to open encoder\n");
        return ret;
    }

    return 0;
}

int close_output_file(AVFormatContext* fmt_ctx) {

    int ret;

    if ((ret = av_write_trailer(fmt_ctx))) {
        av_log(NULL, AV_LOG_ERROR, "Failed to write output file trailer\n");
        return ret;
    }

    if (!(fmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        if ((ret = avio_closep(&fmt_ctx->pb) < 0)) {
            av_log(NULL, AV_LOG_ERROR, "Failed to close output file\n");
            return ret;
        }
    }

    return 0;
}

void close_coding_context(CodingContext* ctx) {

    if (ctx->codec_ctx) {
        avcodec_close(ctx->codec_ctx);
        avcodec_free_context(&ctx->codec_ctx);
        ctx->codec_ctx = 0;
    }

    if (ctx->fmt_ctx) {
        avformat_free_context(ctx->fmt_ctx);
        ctx->fmt_ctx = 0;
    }

    ctx->vstream_idx = 0;
}

int main(int argc, char** argv) {}