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
            "  -i <file> [file ...]        Input file(s), separated by space\n"
            "  -o <file>                   Output file, treated as a folder if inputs > 1\n"
            "  -v <encoder>[,opt=val:...]  Video encoder name and optionally settings\n"
            "  -l <int>                    Log level: quiet|error|progress|warn|info|verbose (default: progress)\n"
            "  -h                          Print this help message");
}

static inline int missing_value(const char* opt) {
    px_log(PX_LOG_ERROR, "Missing value for option %s", opt);
    return 1;
}

static inline bool arg_matches(const char* arg, const char* long_arg, const char* short_arg) {

    if (strcmp(arg, long_arg) == 0)
        return true;

    if (short_arg && strcmp(arg, short_arg) == 0)
        return true;

    return false;
}

static inline bool is_arg(const char* str) {

    int len = strlen(str);
    if (len < 2) // has to be at least "-x"
        return false;

    if (str[0] != '-')
        return false;

    if (is_digit(str[1])) // negative number
        return false;

    for (int i = 0; i < len; i++) {
        if (isspace(*str))
            return false;
    }

    return true;
}

static inline bool is_value(const char* str) {
    return *str && !is_arg(str);
}

int px_parse_args(int argc, char** argv, PXSettings* s) {

    int ret = 0;

    *s = (PXSettings) {.loglevel = PX_LOG_NONE};

    argv++, argc--; // skip program name

    if (argc < 1) {
        px_print_info(argv[-1], false);
        return PX_HELP_PRINTED;
    }

    for (int i = 0; i < argc; i++) {

        const char* arg = argv[i];
        if (!is_arg(arg))
            continue;

        // args with no value handled first
        if (arg_matches(arg, "--help", "-h")) {
            px_print_info(argv[-1], true);
            return PX_HELP_PRINTED;
        }

        char* value = argv[++i];
        if (!is_value(value))
            return missing_value(arg);

        if (arg_matches(arg, "--input", "-i")) {
            s->input_files = &value;
            s->n_input_files = 1;

            // argv is NULL-terminated, so we iterate until we find a NULL or another arg
            for (char** next = &value + 1; is_value(*next); next++) {
                s->n_input_files++;
                i++;
            }
        }

        if (arg_matches(arg, "--output", "-o"))
            s->output_url = value;

        if (arg_matches(arg, "--video-enc", "-v")) {
            s->enc_name_v = value;

            const char* settings = strchr(value, ',') + 1;
            if (!settings)
                continue;

            int ret = av_dict_parse_string(&s->enc_opts_v, settings, "=", ":", 0);
            if (ret < 0) {
                px_log(PX_LOG_ERROR, "Failed to parse video encoder settings \"%s\": %s (%d)", settings,
                       av_err2str(ret), ret);
                return ret;
            }
        }

        // TODO: implement a nicer str-to-int converter than ato* and add -l
    }

    return ret;
}
