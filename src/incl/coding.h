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

int init_px_mediactx(PXMediaContext* ctx);
void uninit_px_mediactx(PXMediaContext* ctx);

int init_input(PXMediaContext* ctx, const char* filename);
int init_output(PXMediaContext* ctx, const char* filename, char* encoder_name,
                AVDictionary** enc_opts);

int init_decoder(AVCodecContext* dec_ctx, const AVCodec* dec, AVCodecParameters* params);
int init_encoder(PXMediaContext* ctx, char* encoder_name, AVDictionary** enc_opts,
                 AVRational timebase, AVCodecParameters* input_params, int stream_idx);

int decode_frame(PXStreamContext* ctx, AVFrame* frame, AVPacket* packet);
int encode_frame(PXMediaContext* ctx, AVFrame* frame, AVPacket* packet, unsigned int stream_idx);

int flush_encoder(PXMediaContext* ctx, AVPacket* packet, unsigned int stream_idx);