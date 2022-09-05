#pragma once

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

#define die(ctx, fmt, ...)                              \
    do {                                                \
        av_log(NULL, AV_LOG_ERROR, fmt, ##__VA_ARGS__); \
        close_coding_context(ctx);                      \
    } while (0)

typedef struct StreanContext {
    AVFormatContext* fmt_ctx;
    AVCodecContext* codec_ctx;

    unsigned int vstream_idx;
    unsigned int astream_idx;
} CodingContext;

int open_input(CodingContext* ctx, const char* file);
int open_decoder(AVCodecContext** ctx, const AVCodec* codec, AVCodecParameters* params);
int decode_frame(CodingContext* ctx, AVFrame* frame, AVPacket* packet);

int open_output(AVFormatContext* input, CodingContext* output, const char* file);
int open_encoder(CodingContext* ctx, char* encoder_name, AVDictionary** codec_options,
                 AVRational timebase, AVCodecParameters* input_params);

int close_output_file(AVFormatContext* format);
void close_coding_context(CodingContext* ctx);

void print_av_error(int error);