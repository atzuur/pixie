#pragma once

#include <libavutil/dict.h>

#define PX_VERSION "0.1.1"

typedef struct PXSettings {
    char** input_files;
    int n_input_files;

    char* output_file;

    char* enc_name_v;
    char* enc_name_a;
    AVDictionary* enc_opts_v;
    AVDictionary* enc_opts_a;

    int n_threads;
    int log_level;
} PXSettings;
