#pragma once

#include <pixie/log.h>
#include <pixie/util/map.h>

typedef struct Settings {
    char** input_files;
    int n_input_files;

    char* output_file;

    char* enc_name_v;
    char* enc_opts_v;

    char* filter_dir;
    char** filter_names;
    PXMap* filter_opts;
    int n_filters;

    PXLogLevel log_level;
} Settings;
