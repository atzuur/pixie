#pragma once

#include <pixie/log.h>
#include <pixie/util/map.h>

typedef struct PXSettings {
    char** input_files;
    int n_input_files;

    char* output_file;
    char* output_folder; // only set if n_input_files > 1

    char* enc_name_v;
    char* enc_opts_v;

    char** filter_names;
    char** filter_opts;
    int n_filters;

    PXLogLevel loglevel;
} PXSettings;

PXSettings* px_settings_alloc(void);

// validate settings and prepare `output_url` for use if it's a folder
int px_settings_check(PXSettings* s);

// free members of `s` and `s` itself
void px_settings_free(PXSettings** s);
