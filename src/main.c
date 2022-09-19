#include "incl/coding.h"
#include "incl/pixie.h"
#include <ctype.h> // isalpha
#include <string.h>

void px_print_info(char* prog_name, char full) {

    printf("pixie v%s, using FFmpeg version %s\n"
           "Usage: %s -i <input file(s)> [options] -o <output file/folder>\n",
           PX_VERSION, av_version_info(), prog_name);

    if (full) {
        puts("Options:\n"
             "  -i <input(s)>  Input file(s), separated by space\n"
             "  -o <output>    Output file, treated as a folder if inputs > 1\n"
             "  -v <encoder>   Video encoder name\n"
             "  -a <encoder>   Audio encoder name\n"
             "  -t <threads>   Number of threads to use for filtering\n"
             "  -h             Print this help message\n");
    }
}

int px_parse_args(int argc, char** argv, PXSettings* s) {

    int i;

    if (argc < 2) {
        px_print_info(argv[0], 0);
        return 1;
    }

    // clang-format off

    #define if_arg_is(arg, code)                                                                    \
        if (strcmp(argv[i], arg) == 0) {                                                            \
            if (argc < i + 1 || /* if argv[i] is the last arg or the next element is also an arg */ \
                (argc >= i + 1 && argv[i + 1][0] == '-' && !isalpha(argv[i + 1][1]))) {             \
                av_log(NULL, AV_LOG_ERROR, "Missing value for %s", argv[i]);                        \
                return 1;                                                                           \
            }                                                                                       \
            code;                                                                                   \
            continue;                                                                               \
        }

    // clang-format on
    for (i = 1; i < argc; i++) {

        if_arg_is("-i", {
            s->input_files = &argv[i + 1];
            while (argv[i + 1][0] != '-' && argc >= i + 1) {
                s->n_input_files++;
                i++;
            }
        });

        if_arg_is("-v", {
            s->enc_name_v = argv[i + 1];
            i++;
        });

        if_arg_is("-a", {
            s->enc_name_a = argv[i + 1];
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

        if (strcmp(argv[i], "-h") == 0) {
            px_print_info(argv[0], 1);
            break;
        }

        if (strcmp(argv[i], "-version") == 0) {
            px_print_info(argv[0], 0);
            break;
        }

        av_log(NULL, AV_LOG_ERROR, "Unknown option: %s", argv[i]);
    }

    return 0;
}

int main(int argc, char** argv) {
    int ret;
    int i;

    if (argc < 3) {
        av_log(NULL, AV_LOG_ERROR,
               "Usage: %s <input file>"
               " <output file> [-e <encoding settings>]\n",
               argv[0]);
        return 1;
    }

    char* enc_name = NULL;
    AVDictionary* enc_opts_dict = NULL;

    if (argc > 3) {
        if (argv[3][0] == '-' && argv[3][1] == 'v') {

            av_log(NULL, AV_LOG_INFO, "Parsing video encoding settings\n");

            if (argc < 5) {
                av_log(NULL, AV_LOG_ERROR, "No encoder provided\n");
                goto end;
            }

            enc_name = strtok(argv[4], ":");
            char* enc_opts = strtok(NULL, ":");
            if (enc_opts) {
                if ((ret = av_dict_parse_string(&enc_opts_dict, enc_opts, "=", ",", 0)) < 0) {
                    av_log(NULL, AV_LOG_ERROR, "Failed to parse encoding options\n");
                    goto end;
                }
            }

        } else {
            av_log(NULL, AV_LOG_ERROR, "Unknown option: %s\n", argv[3]);
            goto end;
        }
    }

    AVPacket* dec_pkt = av_packet_alloc();
    if (!dec_pkt) {
        av_log(NULL, AV_LOG_ERROR, "Failed to allocate packet\n");
        goto end;
    }

    PXMediaContext* ctx = {0};
    if (!ctx) {
        av_log(NULL, AV_LOG_ERROR, "Failed to allocate media context\n");
        goto end;
    }

    if ((ret = init_input(ctx, argv[1])) < 0)
        goto end;

    if ((ret = init_output(ctx, argv[2], enc_name, &enc_opts_dict, NULL)) < 0)
        goto end;

    AVStream* ist; // current input stream
    AVStream* ost; // current output stream
    PXStreamContext* stc; // current stream context

    while (1) {

        if ((ret = av_read_frame(ctx->ifmt_ctx, dec_pkt)) < 0)
            goto end;

        ist = ctx->ifmt_ctx->streams[dec_pkt->stream_index];
        ost = ctx->ofmt_ctx->streams[dec_pkt->stream_index];
        stc = &ctx->stream_ctx_vec[dec_pkt->stream_index];

        if (ist->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {

            ret = decode_frame(stc, stc->dec_frame, dec_pkt);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Failed to decode frame\n");
                goto end;
            }

            stc->dec_frame->pts = stc->dec_frame->best_effort_timestamp;

            ret = encode_frame(ctx, stc->dec_frame, stc->enc_pkt, dec_pkt->stream_index);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Failed to encode frame\n");
                goto end;
            }

            av_packet_unref(stc->enc_pkt);

        } else {
            // remux this frame without reencoding
            av_packet_rescale_ts(dec_pkt, ist->time_base, ost->time_base);
            ret = av_interleaved_write_frame(ctx->ofmt_ctx, dec_pkt);
            if (ret < 0)
                goto end;
        }

        av_packet_unref(dec_pkt);
    }

    for (int i = 0; i < ctx->ifmt_ctx->nb_streams; i++) {
        ret = flush_encoder(ctx, stc->enc_pkt, i);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Failed to flush encoder\n");
            goto end;
        }
    }

    av_write_trailer(ctx->ofmt_ctx);

end:
    if (ctx)
        uninit_px_mediactx(ctx);

    if (enc_opts_dict)
        av_dict_free(&enc_opts_dict);

    if (dec_pkt)
        av_packet_free(&dec_pkt);

    if (ret < 0)
        av_log(NULL, AV_LOG_ERROR, "Error occurred: %s\n", av_err2str(ret));

    return ret ? 1 : 0;
}
