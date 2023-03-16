#pragma once

#include "log.h"
#include "util/sync.h"
#include "util/utils.h"

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>

#define PX_VERSION "0.1.1"

#define PX_HELP_PRINTED 2

typedef struct PXSettings {

    char** input_files;
    int n_input_files;

    char* output_file;

    char* enc_name_v;
    char* enc_name_a;
    AVDictionary* enc_opts_v;
    AVDictionary* enc_opts_a;

    PXLogLevel loglevel;

} PXSettings;

typedef struct PXStreamContext {

    AVCodecContext* dec_ctx;
    AVCodecContext* enc_ctx;

} PXStreamContext;

typedef struct PXMediaContext {

    AVFormatContext* ifmt_ctx;
    AVFormatContext* ofmt_ctx;

    PXStreamContext* stream_ctx_vec;

} PXMediaContext;

typedef struct PXContext {

    PXSettings settings;
    PXMediaContext media_ctx;

    PXThread transc_thread;

    uint64_t frames_decoded;
    uint64_t decoded_frames_dropped;
    uint64_t frames_output;

    int input_idx;
    int stream_idx;

} PXContext;

int px_main(PXSettings s);

int px_transcode_init(PXContext* pxc, AVPacket** packet, AVFrame** frame);
int px_transcode(PXContext* pxc);

void px_ctx_free(PXContext* pxc);

int px_settings_init(int argc, char** argv, PXSettings* s);
void px_settings_free(PXSettings* s);