#include "pixie.h"
#include "coding.h"
#include "util/utils.h"

int px_main(PXSettings s) {

    int ret = 0;
    PXContext pxc = {.settings = s};

    pxc.transc_thread = (PXThread) {
        .func = (PXThreadFunc)px_transcode,
        .args = &pxc,
    };

    for (pxc.input_idx = 0; pxc.input_idx < pxc.settings.n_input_files; pxc.input_idx++) {

        char* current_file = pxc.settings.input_files[pxc.input_idx];
        if (!file_exists(current_file)) {
            px_log(PX_LOG_ERROR, "File \"%s\" does not exist\n", current_file);
            ret = AVERROR(ENOENT);
            break;
        }

        if (strcmp(current_file, pxc.settings.output_file) == 0) {
            px_log(PX_LOG_ERROR, "Input and output files cannot be the same\n");
            ret = AVERROR(EINVAL);
            break;
        }

        px_thrd_launch(&pxc.transc_thread);

        while (!pxc.transc_thread.done) {
            sleep_ms(10);
            px_log(PX_LOG_INFO, "Decoded %lu frames, dropped %lu frames, encoded %lu frames\r",
                   pxc.frames_decoded, pxc.decoded_frames_dropped, pxc.frames_output);
        }

        putchar('\n');

        int transc_ret = 0;
        px_thrd_join(&pxc.transc_thread, &transc_ret);

        if (transc_ret) {
            px_log(PX_LOG_ERROR, "Error occurred while processing file \"%s\" (stream index %d)\n",
                   current_file, pxc.stream_idx);
            ret = transc_ret;
            break;
        }
    }

    px_free_ctx(&pxc);

    return ret;
}

int px_transcode(PXContext* pxc) {

    int ret = 0;
    AVFrame* frame = NULL;
    AVPacket* packet = NULL;

#define FINISH(r)                       \
    do {                                \
        pxc->transc_thread.done = true; \
        return r;                       \
    } while (0)

    ret = px_init_transcode(pxc, &packet, &frame);
    if (ret)
        FINISH(ret);

    while (1) {

        ret = av_read_frame(pxc->media_ctx.ifmt_ctx, packet);
        if (ret == AVERROR_EOF) {
            ret = 0;
            break;
        } else if (ret < 0) {
            lav_throw_msg("av_read_frame", ret);
            FINISH(ret);
        }

        pxc->stream_idx = packet->stream_index;

        enum AVMediaType stream_type =
            pxc->media_ctx.ifmt_ctx->streams[pxc->stream_idx]->codecpar->codec_type;

        PXStreamContext* stc = &pxc->media_ctx.stream_ctx_vec[pxc->stream_idx];

        if (stream_type == AVMEDIA_TYPE_VIDEO) {

            ret = decode_frame(stc, frame, packet);
            if (ret == AVERROR_EOF) {
                ret = 0;
                break;
            } else if (ret < 0) {
                FINISH(ret);
            }

            av_packet_unref(packet);

            if (px_should_skip_frame(frame->pts)) {
                pxc->decoded_frames_dropped++;
                continue;
            }

            pxc->frames_decoded++;

            ret = encode_frame(&pxc->media_ctx, frame, packet, pxc->stream_idx);
            if (ret == AVERROR_EOF) {
                ret = 0;
                break;
            } else if (ret < 0) {
                FINISH(ret);
            }

            av_frame_unref(frame);

        } else {
            ret = av_interleaved_write_frame(pxc->media_ctx.ofmt_ctx, packet);
            if (ret < 0) {
                lav_throw_msg("av_interleaved_write_frame", ret);
                FINISH(ret);
            }
        }

        av_packet_unref(packet);
    }

    for (unsigned i = 0; i < pxc->media_ctx.ifmt_ctx->nb_streams; i++) {
        ret = flush_encoder(&pxc->media_ctx, packet, i);
        if (ret < 0 && ret != AVERROR_EOF) {
            px_log(PX_LOG_ERROR, "Failed to flush encoder\n");
            FINISH(ret);
        }
    }

    ret = av_write_trailer(pxc->media_ctx.ofmt_ctx);
    if (ret < 0)
        lav_throw_msg("av_write_trailer", ret);

    FINISH(ret);

#undef FINISH
}

int px_init_transcode(PXContext* pxc, AVPacket** packet, AVFrame** frame) {

    int ret = 0;

    *packet = NULL;
    *frame = NULL;

    char* infile = pxc->settings.input_files[pxc->input_idx];

    char* outfile = pxc->settings.n_input_files > 1
                        ? strcat(pxc->settings.output_file, pxc->settings.input_files[pxc->input_idx])
                        : pxc->settings.output_file;

    ret = init_input(&pxc->media_ctx, infile);
    if (ret) {
        px_log(PX_LOG_ERROR, "Failed to open input file \"%s\": %s (%d)\n", infile, av_err2str(ret), ret);
        goto end;
    }

    ret = init_output(&pxc->media_ctx, outfile, &pxc->settings);
    if (ret) {
        px_log(PX_LOG_ERROR, "Failed to open output file \"%s\": %s (%d)\n", outfile, av_err2str(ret), ret);
        goto end;
    }

    *packet = av_packet_alloc();
    if (!packet) {
        oom(sizeof **packet);
        return AVERROR(ENOMEM);
    }

    *frame = av_frame_alloc();
    if (!frame) {
        oom(sizeof **frame);
        av_packet_free(packet);
        return AVERROR(ENOMEM);
    }

end:
    return ret;
}

bool px_should_skip_frame(int64_t pts) {

    static int64_t last_pts = INT_MIN; // avoid skipping the first frame

    if (pts != AV_NOPTS_VALUE && last_pts >= pts) {
        return true;
    } else {
        last_pts = pts;
        return false;
    }
}

void px_print_info(const char* prog_name, bool full) {

    printf("pixie v%s, using FFmpeg version %s\n"
           "Usage: %s -i <input file(s)> [options] -o <output file/folder>\n",
           PX_VERSION, av_version_info(), prog_name);

    if (full) {
        puts("Options:\n"
             "  -i <file> [file ...]  Input file(s), separated by space\n"
             "  -o <file>             Output file, treated as a folder if inputs > 1\n"
             "  -v <encoder>          Video encoder name\n"
             "  -a <encoder>          Audio encoder name\n"
             "  -t <int>              Number of threads to use for filtering\n"
             "  -l <int>              Log level: quiet|warn|error|info|verbose (default: info)\n"
             "  -h                    Print this help message");
    }
}

int px_parse_args(int argc, char** argv, PXSettings* s) {

    int i;
    int ret = 0;

    *s = (PXSettings) {0};
    s->loglevel = PX_LOG_INFO;

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
                s->enc_name_v = strtok(argv[i + 1], ":");
                char* enc_opts = strtok(NULL, ":");
                if (enc_opts) {
                    ret = av_dict_parse_string(&s->enc_opts_v, enc_opts, "=", ",", 0);
                    if (ret < 0) {
                        px_log(PX_LOG_ERROR, "Failed to parse encoding options: %s\n", enc_opts);
                        break;
                    }
                }
                i++;
            });

            if_arg_is("-a", {
                s->enc_name_a = strtok(argv[i + 1], ":");
                char* enc_opts = strtok(NULL, ":");
                if (enc_opts) {
                    ret = av_dict_parse_string(&s->enc_opts_a, enc_opts, "=", ",", 0);
                    if (ret < 0) {
                        px_log(PX_LOG_ERROR, "Failed to parse encoding options: %s\n", enc_opts);
                        break;
                    }
                }
                i++;
            });

            if_arg_is("-o", {
                s->output_file = strdup(argv[i + 1]); // strdup because may be modified
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

#undef if_arg_is

    return ret;
}

int px_init_settings(int argc, char** argv, PXSettings* s) {

    *s = (PXSettings) {0};

    int ret = px_parse_args(argc, argv, s);
    if (ret)
        return ret == PX_HELP_PRINTED ? 0 : ret;

    if (!s->n_input_files) {
        px_log(PX_LOG_ERROR, "No input file specified\n");
        return 1;
    }

    if (!s->output_file) {
        px_log(PX_LOG_ERROR, "No output file specified\n");
        return 1;
    }

    // /path/to/output_file/input_files[i]
    if (s->n_input_files > 1) {

        char last = s->output_file[strlen(s->output_file) - 1];
        if (last != *PATH_SEP) {
            // +1 for the path sep, +1 for the null terminator
            s->output_file = realloc(s->output_file, strlen(s->output_file) + 1 + 1);
            strcat(s->output_file, PATH_SEP);
        }

        if (!create_folder(s->output_file)) {
            char err[256];
            last_errstr(err, 0);
            px_log(PX_LOG_ERROR, "Failed to create output directory: %s (%d)\n", err, last_errcode());
            return 1;
        }
    }

    px_log_set_level(s->loglevel);

    return 0;
}

void px_free_ctx(PXContext* pxc) {

    px_mediactx_deinit(&pxc->media_ctx);
    px_free_settings(&pxc->settings);
}

void px_free_settings(PXSettings* s) {

    free_s(&s->output_file);

    if (s->enc_opts_v)
        av_dict_free(&s->enc_opts_v);
    if (s->enc_opts_a)
        av_dict_free(&s->enc_opts_a);
}
