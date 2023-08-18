#include "cli.h"
#include "log.h"
#include "util/strconv.h"
#include <ctype.h>

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

static inline bool opt_matches(const char* opt, const char* long_opt, const char* short_opt) {

    if (strcmp(opt, long_opt) == 0)
        return true;

    if (short_opt && strcmp(opt, short_opt) == 0)
        return true;

    return false;
}

static inline bool is_opt(const char* str) {

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
    return str && *str && !is_opt(str);
}

static inline int missing_value(const char* opt) {
    $px_log(PX_LOG_ERROR, "Missing value for option \"%s\"\n", opt);
    return 1;
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

        const char* opt = argv[i];
        if (!is_opt(opt)) {
            $px_log(PX_LOG_WARN, "Ignoring argument \"%s\"\n", opt);
            continue;
        }

        if (opt_matches(opt, "--help", "-h")) {
            px_print_info(argv[-1], true);
            return PX_HELP_PRINTED;
        }

        if (opt_matches(opt, "--input", "-i")) {

            s->input_files = &argv[++i];
            if (!is_value(s->input_files[0]))
                return missing_value(opt);

            s->n_input_files = 1;

            // argv is NULL-terminated, so we iterate until we find a NULL or another arg
            for (char** next = s->input_files + 1; is_value(*next); next++) {
                s->n_input_files++;
                i++;
            }

            continue;
        }

        if (opt_matches(opt, "--output", "-o")) {

            // folder check done later
            s->output_file = strdup(argv[++i]);
            if (!is_value(s->output_file))
                return missing_value(opt);

            continue;
        }

        if (opt_matches(opt, "--video-enc", "-v")) {

            s->enc_name_v = argv[++i];
            if (!is_value(s->enc_name_v))
                return missing_value(opt);

            const char* settings = strchr(s->enc_name_v, ',');
            if (!settings)
                continue;

            settings++; // skip comma

            int ret = av_dict_parse_string(&s->enc_opts_v, settings, "=", ":", 0);
            if (ret < 0) {
                $px_log(PX_LOG_ERROR, "Failed to parse video encoder settings \"%s\": %s (%d)\n", settings,
                        av_err2str(ret), ret);
                return ret;
            }

            continue;
        }

        if (opt_matches(opt, "--log-level", "-l")) {

            const char* value = argv[++i];
            if (!is_value(value))
                return missing_value(opt);

            AtoiResult res = checked_atoi(value);
            if (!res.fail)
                s->loglevel = res.value;
            else
                s->loglevel = px_loglevel_from_str(value);

            if (s->loglevel < 0 || s->loglevel >= PX_LOG_COUNT) {
                $px_log(PX_LOG_ERROR, "Invalid log level: \"%s\"\n", value);
                return 1;
            }

            continue;
        }

        $px_log(PX_LOG_WARN, "Ignoring unknown option \"%s\"\n", opt);
    }

    return ret;
}
