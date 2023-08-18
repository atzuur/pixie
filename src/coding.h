#pragma once

#include "settings.h"

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

typedef struct PXCodingContext {

    AVCodecContext* dec_ctx;
    AVCodecContext* enc_ctx;

} PXCodingContext;

// context for processing a media file
typedef struct PXMediaContext {

    AVFormatContext* ifmt_ctx;
    AVFormatContext* ofmt_ctx;

    // index of the stream currently being processed, -1 if none
    int stream_idx;

    // context for transcoding each stream
    PXCodingContext* coding_ctx_arr;

    uint64_t frames_decoded;
    uint64_t decoded_frames_dropped;
    uint64_t frames_output;

} PXMediaContext;

#define $lav_throw_msg(func, err)                                                            \
    $px_log(PX_LOG_ERROR, "%s() failed at %s:%d : %s (code %d)\n", func, __FILE__, __LINE__, \
            av_err2str(err), err)

int px_media_ctx_init(PXMediaContext* ctx, PXSettings* s, int input_idx);
void px_media_ctx_free(PXMediaContext* ctx);

int px_read_video_frame(PXMediaContext* ctx, AVPacket* pkt);
int px_encode_frame(PXMediaContext* ctx, const AVFrame* frame);

int px_flush_encoder(PXMediaContext* ctx);
int px_flush_decoder(PXMediaContext* ctx);
