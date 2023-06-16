#include "cli.h"
#include "log.h"
#include "util/utils.h"

void px_print_info(const char* prog_name, bool full) {

    printf("pixie v%s, using FFmpeg version %s\n"
           "Usage: %s -i <input file(s)> [options] -o <output file/folder>\n",
           PX_VERSION, av_version_info(), prog_name);

    if (full)
        puts(
            "Options:\n"
            "  -i <file> [file ...]     Input file(s), separated by space\n"
            "  -o <file>                Output file, treated as a folder if inputs > 1\n"
            "  -v <encoder>[,opt1:...]  Video encoder name and optionally settings\n"
            "  -t <int>                 Number of threads to use for filtering\n"
            "  -l <int>                 Log level: quiet|error|progress|warn|info|verbose (default: progress)\n"
            "  -h                       Print this help message");
}

static inline bool arg_matches(const char* arg, const char* long_arg, const char* short_arg) {

    if (strcmp(arg, long_arg) == 0)
        return true;

    if (short_arg && strcmp(arg, short_arg) == 0)
        return true;

    return false;
}

static inline bool is_arg(const char* arg) {

    int len = strlen(arg);
    if (len < 2) // has to be at least "-x"
        return false;
    
    if (arg[0] != '-')
        return false;
    
    

    for (int i = 0; arg[i]; i++) {
        if (isspace(*arg))
            return false;
    }
}

int px_parse_args(int argc, char** argv, PXSettings* s) {

    int ret = 0;

    *s = (PXSettings) {.loglevel = PX_LOG_NONE};

    argv++, argc--; // skip program name

    if (argc < 1) {
        px_print_info(argv[-1], 0);
        return PX_HELP_PRINTED;
    }

    for (int i = 0; i < argc; i++) {
    }

    return ret;
}
