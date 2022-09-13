#pragma once

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

typedef struct PXStreamContext {
    AVCodecContext* dec_ctx;
    AVCodecContext* enc_ctx;

    AVFrame* dec_frame;
    AVPacket* enc_pkt;
} PXStreamContext;

typedef struct PXMediaContext {
    AVFormatContext* ifmt_ctx;
    AVFormatContext* ofmt_ctx;

    PXStreamContext* stream_ctx_vec;
} PXMediaContext;

// int open_input(CodingContext* ctx, const char* file);
// int open_decoder(AVCodecContext* ctx, const AVCodec* codec, AVCodecParameters* params);
// int decode_frame(CodingContext* ctx, AVFrame* frame, AVPacket* packet);

// int open_output(AVFormatContext* input, CodingContext* output, const char* file);
// int open_encoder(CodingContext* ctx, char* encoder_name, AVDictionary** codec_options,
//                  AVRational timebase, AVCodecParameters* input_params);

// int close_output_file(AVFormatContext* format);
// void close_coding_context(CodingContext* ctx);