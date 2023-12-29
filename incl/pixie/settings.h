#pragma once

#include <pixie/log.h>

#include <libavutil/dict.h>

typedef struct PXSettings {

    char** input_files;
    int n_input_files;

    char* output_file;
    char* output_folder; // only set if n_input_files > 1

    char* enc_name_v;
    AVDictionary* enc_opts_v;

    char** filter_names;
    AVDictionary** filter_opts;
    int n_filters;

    PXLogLevel loglevel;

} PXSettings;

// validate settings and prepare `output_url` for use if it's a folder
int px_settings_check(PXSettings* s);

// deallocate members
void px_settings_free(PXSettings* s);
