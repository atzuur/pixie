#pragma once

#include "filter.h"
#include "log.h"
#include "util/sync.h"
#include "util/utils.h"

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

#include <ctype.h>
#include <libavutil/dict.h>
#include <stdbool.h>
#include <stdint.h>

#define PX_VERSION "0.1.1"

typedef struct PXSettings {

    char** input_files;
    int n_input_files;

    // url because it can also be a folder
    char* output_url;

    char* enc_name_v;
    AVDictionary* enc_opts_v;

    char** filter_names;
    AVDictionary** filter_opts;
    int n_filters;

    PXLogLevel loglevel;

} PXSettings;

typedef struct PXCodingContext {

    AVCodecContext* dec_ctx;
    AVCodecContext* enc_ctx;

} PXCodingContext;

typedef struct PXMediaContext {

    AVFormatContext* ifmt_ctx;
    AVFormatContext* ofmt_ctx;

    PXCodingContext* coding_ctx_vec;

} PXMediaContext;

typedef struct PXFilterContext {

    PXFilter* filters;
    int n_filters;

} PXFilterContext;

typedef struct PXContext {

    PXSettings settings;
    PXMediaContext media_ctx;
    PXFilterContext fltr_ctx;

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
void px_transcode_free(AVPacket** packet, AVFrame** frame);

void px_ctx_free(PXContext* pxc);

int px_settings_init(int argc, char** argv, PXSettings* s);
void px_settings_free(PXSettings* s);
