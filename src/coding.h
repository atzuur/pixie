#pragma once

#include "pixie.h"

#define lav_throw_msg(func, err) \
    px_log(PX_LOG_ERROR, "%s() failed at %s:%d : code %d\n", func, __FILE__, __LINE__, err)

void px_media_ctx_free(PXMediaContext* ctx);

int init_input(PXMediaContext* ctx, const char* filename);
int init_output(PXMediaContext* ctx, const char* filename, PXSettings* s);

int init_decoder(PXMediaContext* ctx, unsigned stream_idx);
int init_encoder(const PXMediaContext* ctx, const char* enc_name, AVDictionary** enc_opts,
                 unsigned stream_idx, AVStream* out_stream);

int decode_frame(const PXStreamContext* ctx, AVFrame* frame, AVPacket* packet);
int encode_frame(const PXMediaContext* ctx, AVFrame* frame, AVPacket* packet, unsigned stream_idx);

int flush_encoder(const PXMediaContext* ctx, AVPacket* packet, unsigned stream_idx);
