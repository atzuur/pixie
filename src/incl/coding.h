#pragma once

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

#include "pixie.h"

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

void uninit_px_mediactx(PXMediaContext* ctx);

int init_input(PXMediaContext* ctx, const char* filename);
int init_output(PXMediaContext* ctx, const char* filename, PXSettings* s);

int init_decoder(PXMediaContext* ctx, const unsigned int stream_idx);
int init_encoder(PXMediaContext* ctx, const char* enc_name, AVDictionary** enc_opts,
                 const unsigned int stream_idx, AVStream* out_stream);

int decode_frame(PXStreamContext* ctx, AVFrame* frame, AVPacket* packet);
int encode_frame(PXMediaContext* ctx, AVFrame* frame, AVPacket* packet,
                 const unsigned int stream_idx);

int flush_encoder(PXMediaContext* ctx, AVPacket* packet, const unsigned int stream_idx);