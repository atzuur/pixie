#include "incl/coding.h"
#include "incl/pixie.h"

#include <ctype.h> // isalpha
#include <sys/stat.h> // mkdir

void px_print_info(const char* prog_name, const char full) {

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
             "  -l <int>              Log level: 0 = error (default), 1 = verbose, 2 = debug\n"
             "  -h                    Print this help message");
    }
}

int px_parse_args(int argc, char** argv, PXSettings* s, int* should_exit) {

    int i;
    int ret = 0;

    argc--;

    if (argc < 1) {
        px_print_info(argv[0], 0);
        return 1;
    }

    // clang-format off

    #define if_arg_is(arg, code)                                  \
        if (strcmp(argv[i], arg) == 0) {                          \
            if ( /* if the next element is also an arg */         \
                argv[i + 1][0] == '-' && isalpha(argv[i + 1][1])) \
                goto missing_value;                               \
            code;                                                 \
            continue;                                             \
        }

    // clang-format on
    for (i = 1; i < argc + 1; i++) {

        if (argc >= i + 1) {

            if_arg_is("-i", {
                s->input_files = &argv[i + 1];
                while (argc > i && argv[i + 1][0] != '-') {
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
                        av_log(NULL, AV_LOG_ERROR, "Failed to parse encoding options: %s\n",
                               enc_opts);
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
                        av_log(NULL, AV_LOG_ERROR, "Failed to parse encoding options: %s\n",
                               enc_opts);
                        break;
                    }
                }
                i++;
            });

            if_arg_is("-t", {
                s->n_threads = atoi(argv[i + 1]);
                i++;
            });

            if_arg_is("-o", {
                s->output_file = argv[i + 1];
                i++;
            });

            if_arg_is("-l", {
                s->log_level = atoi(argv[i + 1]);
                i++;
            });

        } else {

            if (strcmp(argv[i], "-h") == 0) {
                px_print_info(argv[0], 1);
                *should_exit = 1;
                break;
            }

            goto missing_value;
        }

        // if we reach here, the arg is invalid
        av_log(NULL, AV_LOG_ERROR, "Unknown option: %s\n", argv[i]);
        break;

    missing_value:
        av_log(NULL, AV_LOG_ERROR, "Missing value for %s\n", argv[i]);
        ret = 1;
        break;
    }

    return ret;
}

int main(int argc, char** argv) {

    int ret;

    // clang-format off

    #define check_err(msg, ...)                             \
        if (ret == AVERROR_EOF)                             \
            goto success;                                   \
        else if (ret < 0) {                                 \
            av_log(NULL, AV_LOG_ERROR, msg, ##__VA_ARGS__); \
            goto fail;                                      \
        }

    // clang-format on

    PXSettings s = {.output_file = NULL,
                    .enc_opts_v = NULL,
                    .enc_opts_a = NULL,
                    .log_level = 0,
                    .n_threads = 0};

    int should_exit = 0;
    ret = px_parse_args(argc, argv, &s, &should_exit);
    if (ret || should_exit)
        return ret;

    switch (s.log_level) {
        case 0:
            av_log_set_level(AV_LOG_ERROR);
            break;
        case 1:
            av_log_set_level(AV_LOG_VERBOSE);
            break;
        case 2:
            av_log_set_level(AV_LOG_DEBUG);
            break;
        default:
            av_log(NULL, AV_LOG_ERROR, "Invalid log level: %d\n", s.log_level);
            return 1;
    }

    if (!s.n_input_files) {
        av_log(NULL, AV_LOG_ERROR, "No input file specified\n");
        return 1;
    }

    if (!s.output_file) {
        av_log(NULL, AV_LOG_ERROR, "No output file specified\n");
        return 1;
    }

    PXMediaContext ctx = {0};

    AVPacket* dec_pkt = av_packet_alloc();
    if (!dec_pkt) {
        av_log(NULL, AV_LOG_ERROR, "Failed to allocate packet\n");
        goto fail;
    }

    if (s.n_input_files > 1) { // /path/to/{output_file}/{input_files[i]}
        strcat(s.output_file, "/");
        if (mkdir(s.output_file, 0777) < 0) {
            av_log(NULL, AV_LOG_ERROR, "Failed to create output directory: %s\n", strerror(errno));
            goto fail;
        }
    }

    for (int i = 0; i < s.n_input_files; i++) {

        if ((ret = init_input(&ctx, s.input_files[i])) < 0)
            goto fail;

        char* outfile =
            s.n_input_files > 1 ? strcat(s.output_file, s.input_files[i]) : s.output_file;

        if ((ret = init_output(&ctx, outfile, &s) < 0))
            goto fail;

        AVStream* ist; // current input stream
        AVStream* ost; // current output stream
        PXStreamContext* stc; // current stream context

        long int frames_done = 0;

        long int curr_pts = 0;
        long int last_pts = INT_MIN; // avoid skipping the first frame

        long int skipped = 0;

        while (1) {

            ret = av_read_frame(ctx.ifmt_ctx, dec_pkt);
            check_err("Failed to read frame\n");

            ist = ctx.ifmt_ctx->streams[dec_pkt->stream_index];
            ost = ctx.ofmt_ctx->streams[dec_pkt->stream_index];
            stc = &ctx.stream_ctx_vec[dec_pkt->stream_index];

            if (ist->codecpar->codec_type == AVMEDIA_TYPE_VIDEO ||
                ist->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {

                ret = decode_frame(stc, stc->dec_frame, dec_pkt);
                check_err("Failed to decode frame\n");

                curr_pts = stc->dec_frame->pts;
                if (curr_pts != AV_NOPTS_VALUE && last_pts >= curr_pts) { // skip duplicate pts
                    skipped++;
                    goto skip;
                }
                last_pts = curr_pts;

                ret = encode_frame(&ctx, stc->dec_frame, stc->enc_pkt, dec_pkt->stream_index);
                check_err("Failed to encode frame\n");

            } else {
                // remux this frame without reencoding
                av_packet_rescale_ts(dec_pkt, ist->time_base, ost->time_base);
                ret = av_interleaved_write_frame(ctx.ofmt_ctx, dec_pkt);
                check_err("Failed to write remuxed packet\n");
            }

        skip:
            av_packet_unref(dec_pkt);

            frames_done++;
            long int total_frames = ist->nb_frames
                                        ? ist->nb_frames
                                        : ctx.ifmt_ctx->duration / 1000000.0 * // duration * fps
                                              av_q2d(stc->dec_ctx->framerate);

            printf("\rProgress: %ld/%ld %ld skipped (%.1f%%)", frames_done, total_frames, skipped,
                   (float)frames_done / total_frames * 100);
            fflush(stdout);
        }

    success:
        putchar('\n');

        for (int j = 0; j < ctx.ifmt_ctx->nb_streams; j++) {
            ret = flush_encoder(&ctx, stc->enc_pkt, j);
            if (ret < 0 && ret != AVERROR_EOF) {
                av_log(NULL, AV_LOG_ERROR, "Failed to flush encoder\n");
                goto fail;
            }
        }

        ret = av_write_trailer(ctx.ofmt_ctx);
        if (ret < 0)
            av_log(NULL, AV_LOG_ERROR, "Failed to write output file trailer to %s\n", outfile);

    fail:
        uninit_px_mediactx(&ctx);

        if (dec_pkt)
            av_packet_free(&dec_pkt);

        if (ret < 0 && ret != AVERROR_EOF) {
            av_log(NULL, AV_LOG_ERROR, "Error occurred: %s (%d)\n", av_err2str(ret), ret);
            break;
        }

        if (s.n_input_files > 1) { // reset output file path
            s.output_file[strlen(s.output_file) - strlen(s.input_files[i])] = '\0';
        }
    }

    return ret ? 1 : 0;
}
