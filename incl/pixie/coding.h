#pragma once

#include <pixie/settings.h>

#include <stdint.h>
#include <stdatomic.h>

typedef struct AVCodecContext AVCodecContext;
typedef struct AVFormatContext AVFormatContext;

typedef struct PXCodingContext {
    AVCodecContext* dec_ctx;
    AVCodecContext* enc_ctx;
} PXCodingContext;

// context for processing a media file
typedef struct PXMediaContext {
    AVFormatContext* ifmt_ctx;
    AVFormatContext* ofmt_ctx;

    // index of the stream currently being processed, -1 if none
    atomic_int stream_idx;

    // context for transcoding each stream
    PXCodingContext* coding_ctx_arr;

    atomic_uint_fast64_t frames_decoded;
    atomic_uint_fast64_t decoded_frames_dropped;
    atomic_uint_fast64_t frames_output;
} PXMediaContext;

PXMediaContext* px_media_ctx_alloc(void);
int px_media_ctx_new(PXMediaContext** ctx, const PXSettings* s, int input_idx);
void px_media_ctx_free(PXMediaContext** ctx);
