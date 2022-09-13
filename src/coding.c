#include "incl/coding.h"
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/frame.h>
#include <libavutil/mem.h>

int init_px_mediactx(PXMediaContext* ctx) {

    ctx->ifmt_ctx = NULL;
    ctx->ofmt_ctx = NULL;

    ctx->stream_ctx_vec->dec_frame = av_frame_alloc();
    if (!ctx->stream_ctx_vec->dec_frame)
        return AVERROR(ENOMEM);

    ctx->stream_ctx_vec->enc_pkt = av_packet_alloc();
    if (!ctx->stream_ctx_vec->enc_pkt)
        return AVERROR(ENOMEM);

    return 0;
}

void uninit_px_mediactx(PXMediaContext* ctx) {

    avformat_free_context(ctx->ifmt_ctx);
    avformat_free_context(ctx->ofmt_ctx);

    for (int i = 0; i < ctx->ifmt_ctx->nb_streams; i++) {
        avcodec_free_context(&ctx->stream_ctx_vec[i].dec_ctx);
        avcodec_free_context(&ctx->stream_ctx_vec[i].enc_ctx);
        av_frame_free(&ctx->stream_ctx_vec[i].dec_frame);
        av_packet_free(&ctx->stream_ctx_vec[i].enc_pkt);
    }

    av_freep(ctx->stream_ctx_vec);

    avformat_close_input(&ctx->ifmt_ctx);

    if (!(ctx->ofmt_ctx->oformat->flags & AVFMT_NOFILE))
        avio_closep(&ctx->ofmt_ctx->pb);
    avformat_free_context(ctx->ofmt_ctx);
}

int open_input(CodingContext* ctx, const char* filename) {

    int ret;
    int i;

    ctx->fmt_ctx = avformat_alloc_context();

    if ((ret = avformat_open_input(&ctx->fmt_ctx, filename, NULL, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot open input file: '%s'\n", filename);
        return ret;
    }

    if ((ret = avformat_find_stream_info(ctx->fmt_ctx, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
        return ret;
    }

    ctx->vstream_found = 0;
    ctx->astream_found = 0;

    for (i = 0; i < ctx->fmt_ctx->nb_streams; i++) {

        const int stream_type = ctx->fmt_ctx->streams[i]->codecpar->codec_type;

        switch (stream_type) {
            case AVMEDIA_TYPE_VIDEO:
                ctx->vstream_found = 1;
            case AVMEDIA_TYPE_AUDIO:
                ctx->astream_found = 1;
        }

        if (stream_type == AVMEDIA_TYPE_VIDEO || stream_type == AVMEDIA_TYPE_AUDIO) {

            const AVCodec* decoder =
                avcodec_find_decoder(ctx->fmt_ctx->streams[i]->codecpar->codec_id);
            if (!decoder) {
                av_log(NULL, AV_LOG_ERROR, "Failed to find decoder for video stream %d\n", i);
                return AVERROR_DECODER_NOT_FOUND;
            }

            if ((ret = open_decoder(ctx->codec_ctx, decoder, ctx->fmt_ctx->streams[i]->codecpar))) {
                av_log(NULL, AV_LOG_ERROR, "Failed to open decoder\n");
                return ret;
            }
        }
    }

    if (!ctx->vstream_found) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find video stream\n");
        return AVERROR_STREAM_NOT_FOUND;
    }

    av_dump_format(ctx->fmt_ctx, 0, filename, 0);

    return 0;
}

int open_decoder(AVCodecContext* ctx, const AVCodec* dec, AVCodecParameters* params) {

    int ret;

    ctx = avcodec_alloc_context3(dec);
    if (!ctx) {
        av_log(NULL, AV_LOG_ERROR, "Failed to allocate decoder context\n");
        return AVERROR(ENOMEM);
    }

    if (params) {
        if ((ret = avcodec_parameters_to_context(ctx, params)) < 0) {
            av_log(NULL, AV_LOG_ERROR,
                   "Failed to copy decoder parameters to input decoder context\n");
            return ret;
        }
    }

    if ((ret = avcodec_open2(ctx, dec, NULL)) < 0) {
        // no message here as it'd just be a duplicate
        return ret;
    }

    return 0;
}

int decode_frame(CodingContext* ctx, AVFrame* frame, AVPacket* packet) {

    int ret;

    if ((ret = avcodec_send_packet(ctx->codec_ctx, packet)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Error sending a packet for decoding\n");
        return ret;
    }

    ret = avcodec_receive_frame(ctx->codec_ctx, frame);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        return 0;
    } else if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Failed to decode frame\n");
        return ret;
    }

    return 0;
}

int open_output(AVFormatContext* input, CodingContext* output, const char* filename) {

    int ret;

    if ((ret = avformat_alloc_output_context2(&output->fmt_ctx, NULL, NULL, filename)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Failed to allocate output context\n");
        return AVERROR_UNKNOWN;
    }

    AVStream* ostream;

    // copy input streams to output
    for (int i = 0; i < input->nb_streams; i++) {
        ostream = avformat_new_stream(output->fmt_ctx, 0);
        if (!ostream) {
            av_log(NULL, AV_LOG_ERROR, "Failed to add output stream\n");
            return AVERROR_UNKNOWN;
        }

        if ((ret = avcodec_parameters_copy(ostream->codecpar, input->streams[i]->codecpar)) < 0) {
            av_log(NULL, AV_LOG_ERROR, "Failed to copy codec parameters to output stream\n");
            return ret;
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
    if (!encoder)
        encoder = avcodec_find_encoder(input_params->codec_id);

    ctx->codec_ctx = avcodec_alloc_context3(encoder);
    if (!ctx->codec_ctx) {
        av_log(NULL, AV_LOG_ERROR, "Failed to allocate encoder context\n");
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
        av_log(NULL, AV_LOG_ERROR, "Failed to open encoder\n");
        return ret;
    }

    return 0;
}

int encode_frame(CodingContext* ctx, AVFrame* frame, AVPacket* packet, unsigned int stream_idx) {

    int ret;
    av_packet_unref(packet);

    ret = avcodec_send_frame(ctx->codec_ctx, frame);
    if (ret < 0)
        return ret;

    while (ret >= 0) {
        ret = avcodec_receive_packet(ctx->codec_ctx, packet);

        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return 0;

        packet->stream_index = stream_idx;
        av_packet_rescale_ts(packet, ctx->codec_ctx->time_base,
                             ctx->fmt_ctx->streams[stream_idx]->time_base);

        ret = av_interleaved_write_frame(ctx->fmt_ctx, packet);
    }

    return ret;
}

int flush_encoder(CodingContext* ctx, AVPacket* packet, unsigned int stream_idx) {
    return (ctx->codec_ctx->codec->capabilities & AV_CODEC_CAP_DELAY)
               ? encode_frame(ctx, 0, packet, stream_idx)
               : 0;
}

int close_output_file(AVFormatContext* fmt_ctx) {

    int ret;

    if ((ret = av_write_trailer(fmt_ctx))) {
        av_log(NULL, AV_LOG_ERROR, "Failed to write output file trailer\n");
        return ret;
    }

    if (!(fmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        if ((ret = avio_close(fmt_ctx->pb) < 0)) {
            av_log(NULL, AV_LOG_ERROR, "Failed to close output file\n");
            return ret;
        }
    }

    return 0;
}

void close_coding_context(CodingContext* ctx) {
    if (ctx->codec_ctx) {
        avcodec_free_context(&ctx->codec_ctx);
        ctx->codec_ctx = 0;
    }

    if (ctx->fmt_ctx) {
        avformat_free_context(ctx->fmt_ctx);
        ctx->fmt_ctx = 0;
    }
}

int main(int argc, char** argv) {

    int ret;
    int i;
    char output_open_success = 1;

    char* enc_name = NULL;
    AVDictionary* enc_opts_dict = NULL;

    CodingContext input = {0};
    CodingContext output = {0};

    AVPacket* packet = av_packet_alloc();
    if (!packet) {
        av_log(NULL, AV_LOG_ERROR, "Failed to allocate packet\n");
        goto end;
    }

    AVFrame* frame = av_frame_alloc();
    if (!frame) {
        av_log(NULL, AV_LOG_ERROR, "Failed to allocate frame\n");
        goto end;
    }

    if (argc < 3) {
        av_log(NULL, AV_LOG_ERROR,
               "Usage: %s <input file>"
               " <output file> [-e <encoding settings>]\n",
               argv[0]);
        ret = 1;
        goto end;
    }

    if ((ret = open_input(&input, argv[1])) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Failed to open input file\n");
        goto end;
    }

    if ((ret = open_output(input.fmt_ctx, &output, argv[2])) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Failed to open output file\n");
        output_open_success = 0;
        goto end;
    }

    if (argc > 3) {
        if (argv[3][0] == '-' && argv[3][1] == 'e') {

            av_log(NULL, AV_LOG_INFO, "Parsing encoding settings\n");

            if (argc < 5) {
                av_log(NULL, AV_LOG_ERROR, "No encoder provided\n");
                goto end;
            }

            enc_name = strtok(argv[4], ":");
            char* enc_opts = strtok(NULL, ":");
            if (enc_opts) {
                if ((ret = av_dict_parse_string(&enc_opts_dict, enc_opts, "=", ",", 0)) < 0) {
                    av_log(NULL, AV_LOG_ERROR, "Failed to parse encoding options\n");
                    goto end;
                }
                av_log(NULL, AV_LOG_INFO, "Encoding settings %s parsed and set to dict at %p\n",
                       enc_opts, enc_opts_dict);
            }

        } else {
            av_log(NULL, AV_LOG_ERROR, "Unknown option: %s\n", argv[3]);
            goto end;
        }
    }

    // open codecs
    for (i = 0; i < input.fmt_ctx->nb_streams; i++) {

        int stream_type = input.fmt_ctx->streams[i]->codecpar->codec_type;
        if (stream_type == AVMEDIA_TYPE_VIDEO || stream_type == AVMEDIA_TYPE_AUDIO) {

            const AVCodec* decoder =
                avcodec_find_decoder(input.fmt_ctx->streams[i]->codecpar->codec_id);

            if (!decoder) {
                av_log(NULL, AV_LOG_ERROR, "Failed to find decoder for video stream %d\n", i);
                return AVERROR_DECODER_NOT_FOUND;
            }

            ret = open_decoder(input.codec_ctx, decoder, input.fmt_ctx->streams[i]->codecpar);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Failed to open decoder\n");
                goto end;
            }

            ret = open_encoder(&output, enc_name, &enc_opts_dict,
                               input.fmt_ctx->streams[i]->time_base,
                               input.fmt_ctx->streams[i]->codecpar);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Failed to open encoder\n");
                goto end;
            }
        }
    }

    AVStream* cur_in_stream;
    AVStream* cur_out_stream;

    while (1) {

        if ((ret = av_read_frame(input.fmt_ctx, packet)) < 0)
            goto end;

        cur_in_stream = input.fmt_ctx->streams[packet->stream_index];
        cur_out_stream = output.fmt_ctx->streams[packet->stream_index];

        if (cur_in_stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO ||
            cur_in_stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {

            ret = decode_frame(&input, frame, packet);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Failed to decode frame\n");
                goto end;
            }

            frame->pts = frame->best_effort_timestamp;

            ret = encode_frame(&output, frame, packet, packet->stream_index);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Failed to encode frame\n");
                goto end;
            }

        } else {
            // remux this frame without reencoding
            av_packet_rescale_ts(packet, cur_in_stream->time_base, cur_out_stream->time_base);
            ret = av_interleaved_write_frame(output.fmt_ctx, packet);
            if (ret < 0)
                goto end;
        }

        av_packet_unref(packet);
    }

    for (int i = 0; i < input.fmt_ctx->nb_streams; i++) {
        ret = flush_encoder(&output, packet, i);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Failed to flush encoder\n");
            goto end;
        }
    }

end:
    close_coding_context(&input);
    avformat_close_input(&input.fmt_ctx);

    close_coding_context(&output);

    if (output_open_success)
        close_output_file(output.fmt_ctx);

    av_packet_free(&packet);
    av_frame_free(&frame);

    av_dict_free(&enc_opts_dict);

    if (ret < 0)
        av_log(NULL, AV_LOG_ERROR, "Error occurred: %s\n", av_err2str(ret));

    return ret ? 1 : 0;
}