#include "cli.h"
#include "app.h"

#include <pixie/pixie.h>
#include <pixie/log.h>
#include <pixie/util/strconv.h>

#include <limits.h>
#include <ctype.h>
#include <errno.h>

static const char* const full_help_msg =
    "Options:\n"
    "  -i <file> [...]                  Input file(s), separated by space\n"
    "  -o <file>                        Output file, treated as a folder if more than one input\n"
    "  -e <encoder>[:opt=val:...]       Video encoder name and optionally settings\n"
    "  -f <filter>[:opt=val:...] [...]  Video filter names and optionally settings, filters separated by space\n"
    "  -d <dir>                         Directory to load filters from\n"
    "  -l <level>                       Log level: quiet|error|progress|warn|info|verbose (default: progress)\n"
    "  -h                               Print this help message";

void px_print_info(const char* prog_name, bool full) {
    printf("pixie v%s, using FFmpeg version %s\n"
           "Usage: %s -i <input file(s)> [options] -o <output file/folder>\n",
           PX_VERSION, px_ffmpeg_version(), prog_name);
    if (full)
        puts(full_help_msg);
}

static inline bool opt_matches(const char* opt, const char* long_opt, const char* short_opt) {
    if (strcmp(opt, long_opt) == 0)
        return true;

    if (short_opt && strcmp(opt, short_opt) == 0)
        return true;

    return false;
}

static inline bool is_opt(const char* str) {
    assert(str);
    size_t len = strlen(str);
    if (len < 2) // has to be at least "-x"
        return false;

    if (str[0] != '-')
        return false;

    if (px_is_digit(str[1])) // negative number
        return false;

    for (size_t i = 0; i < len; i++) {
        if (isspace(str[i]))
            return false;
    }

    return true;
}

static inline bool is_value(const char* str) {
    return str && *str && !is_opt(str);
}

static inline int missing_value(const char* opt) {
    px_log(PX_LOG_ERROR, "Missing value for option \"%s\"\n", opt);
    return PXERROR(EINVAL);
}

int parse_args(int argc, char** argv, Settings* s) {
    if (argc <= 1) {
        px_print_info(argv[0], false);
        return HELP_PRINTED;
    }

    for (char** arg_it = argv + 1; *arg_it != NULL; arg_it++) {
        const char* opt = *arg_it;
        if (!is_opt(opt)) {
            px_log(PX_LOG_WARN, "Ignoring argument \"%s\"\n", opt);
            continue;
        }

        if (opt_matches(opt, "--help", "-h")) {
            px_print_info(argv[0], true);
            return HELP_PRINTED;
        }

        if (opt_matches(opt, "--input", "-i")) {
            s->input_files = ++arg_it;
            if (!is_value(s->input_files[0]))
                return missing_value(opt);
            s->n_input_files = 1;

            while (is_value(arg_it[1])) {
                s->n_input_files++;
                arg_it++;
            }
            continue;
        }

        if (opt_matches(opt, "--output", "-o")) {
            s->output_file = strdup(*++arg_it);
            if (!is_value(s->output_file))
                return missing_value(opt);

            continue;
        }

        if (opt_matches(opt, "--video-enc", "-e")) {
            s->enc_name_v = *++arg_it;
            if (!is_value(s->enc_name_v))
                return missing_value(opt);

            char* settings = strchr(s->enc_name_v, ':');
            if (!settings)
                continue;

            *settings = '\0';
            s->enc_opts_v = ++settings;
            continue;
        }

        if (opt_matches(opt, "--video-filters", "-f")) {
            s->filter_names = ++arg_it;
            if (!is_value(s->filter_names[0]))
                return missing_value(opt);
            s->n_filters = 1;

            while (is_value(arg_it[1])) {
                s->n_filters++;
                arg_it++;
            }

            s->filter_opts = calloc((size_t)s->n_filters, sizeof *s->filter_opts);
            if (!s->filter_opts) {
                px_oom_msg((size_t)s->n_filters * sizeof *s->filter_opts);
                return PXERROR(ENOMEM);
            }

            for (int i = 0; i < s->n_filters; i++) {
                char* settings = strchr(s->filter_names[i], ':');
                if (!settings)
                    continue;

                *settings = '\0';
                if (!*++settings) {
                    px_log(PX_LOG_ERROR, "Expected settings for filter \"%s\" after \":\"\n",
                           s->filter_names[i]);
                    return PXERROR(EINVAL);
                }

                int ret = px_map_parse(&s->filter_opts[i], settings);
                if (ret < 0) {
                    px_log(PX_LOG_ERROR, "Failed to parse settings \"%s\" for filter \"%s\"\n", settings,
                           s->filter_names[i]);
                    return ret;
                }
            }

            continue;
        }

        if (opt_matches(opt, "--filter-dir", "-d")) {
            s->filter_dir = *++arg_it;
            if (!is_value(s->filter_dir))
                return missing_value(opt);

            continue;
        }

        if (opt_matches(opt, "--log-level", "-l")) {
            const char* value = *++arg_it;
            if (!is_value(value))
                return missing_value(opt);

            int ret = px_strtoi(&s->log_level, value);
            if (ret < 0)
                s->log_level = px_log_level_from_str(value);

            if (s->log_level <= PX_LOG_NONE || s->log_level >= px_log_num_levels()) {
                px_log(PX_LOG_ERROR, "Invalid log level: \"%s\"\n", value);
                return PXERROR(EINVAL);
            }
            continue;
        }

        px_log(PX_LOG_WARN, "Ignoring unknown option \"%s\"\n", opt);
    }

    return 0;
}

void parsed_args_free(Settings* settings) {
    px_free(&settings->filter_opts);
    px_free(&settings->output_file);
}
