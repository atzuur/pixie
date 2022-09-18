#include <libavutil/dict.h>

#define PX_VERSION "0.1.0"

typedef struct PXSettings {
    char** input_files;
    int n_input_files;

    char* output_file;

    char* enc_name_v;
    char* enc_name_a;
    AVDictionary* enc_opts_v;
    AVDictionary* enc_opts_a;

    unsigned int n_threads;
} PXSettings;