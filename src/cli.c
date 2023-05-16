#include "cli.h"
#include "log.h"

void px_print_info(const char* prog_name, bool full) {

    printf("pixie v%s, using FFmpeg version %s\n"
           "Usage: %s -i <input file(s)> [options] -o <output file/folder>\n",
           PX_VERSION, av_version_info(), prog_name);

    if (full)
        puts(
            "Options:\n"
            "  -i <file> [file ...]     Input file(s), separated by space\n"
            "  -o <file>                Output file, treated as a folder if inputs > 1\n"
            "  -v <encoder>,[opt1:...]  Video encoder name\n"
            "  -t <int>                 Number of threads to use for filtering\n"
            "  -l <int>                 Log level: quiet|error|progress|warn|info|verbose (default: progress)\n"
            "  -h                       Print this help message");
}

int px_parse_args(int argc, char** argv, PXSettings* s) {

    int i;
    int ret = 0;

    *s = (PXSettings) {.loglevel = PX_LOG_NONE};

    argc--;

    if (argc < 1) {
        px_print_info(argv[0], 0);
        return PX_HELP_PRINTED;
    }

#define if_arg_is(arg, code)                                  \
    if (strcmp(argv[i], arg) == 0) {                          \
        if (/* if the next element is also an arg */          \
            argv[i + 1][0] == '-' && isalpha(argv[i + 1][1])) \
            goto missing_value;                               \
        code;                                                 \
        continue;                                             \
    }

    for (i = 1; i < argc + 1; i++) {

        if (argc >= i + 1) {

            if_arg_is("-i", {
                s->input_files = &argv[i + 1];
                while (i < argc && argv[i + 1][0] != '-') {
                    s->n_input_files++;
                    i++;
                }
            });

            if_arg_is("-v", {
                s->enc_name_v = strtok(argv[i + 1], ",");
                char* enc_opts = strtok(NULL, ",");
                if (enc_opts) {
                    ret = av_dict_parse_string(&s->enc_opts_v, enc_opts, "=", ":", 0);
                    if (ret < 0) {
                        px_log(PX_LOG_ERROR, "Failed to parse encoding options (\"%s\")\n", enc_opts);
                        break;
                    }
                }
                i++;
            });

            if_arg_is("-o", {
                s->output_url = strdup(argv[i + 1]); // strdup because may be modified
                i++;
            });

            if_arg_is("-l", {
                if (is_int(argv[i + 1])) {
                    s->loglevel = atoi(argv[i + 1]);
                } else {
                    s->loglevel = px_loglevel_from_str(argv[i + 1]);
                }

                if (s->loglevel < 0 || s->loglevel >= PX_LOG_COUNT) {
                    px_log(PX_LOG_ERROR, "Invalid log level: \"%s\"\n", argv[i + 1]);
                    ret = 1;
                    break;
                }
                i++;
            });

        } else {

            if (strcmp(argv[i], "-h") == 0) {
                px_print_info(argv[0], 1);
                return PX_HELP_PRINTED;
            }

            goto missing_value;
        }

        // if we reach here, the arg is invalid
        px_log(PX_LOG_ERROR, "Unknown option: %s\n", argv[i]);
        return 1;

    missing_value:
        px_log(PX_LOG_ERROR, "Missing value for option %s\n", argv[i]);
        return 1;
    }

    return ret;

#undef if_arg_is
}
