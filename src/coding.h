#pragma once

#include "pixie.h"

void px_mediactx_deinit(PXMediaContext* ctx);

int init_input(PXMediaContext* ctx, const char* filename);
int init_output(PXMediaContext* ctx, const char* filename, PXSettings* s);

int init_decoder(PXMediaContext* ctx, unsigned stream_idx);
int init_encoder(const PXMediaContext* ctx, const char* enc_name, AVDictionary** enc_opts,
                 unsigned stream_idx, AVStream* out_stream);

int decode_frame(const PXStreamContext* ctx, AVFrame* frame, AVPacket* packet);
int encode_frame(const PXMediaContext* ctx, AVFrame* frame, AVPacket* packet, unsigned stream_idx);

int flush_encoder(const PXMediaContext* ctx, AVPacket* packet, unsigned stream_idx);