#include "pixie.h"
#include "cli.h"
#include "coding.h"

static bool should_skip_frame(AVFrame* frame) {

    if (frame->flags & AV_FRAME_FLAG_DISCARD)
        return true;

    static int64_t last_pts = INT_MIN; // avoid skipping the first frame unintentionally
    int64_t pts = frame->pts;

    if (pts != AV_NOPTS_VALUE && last_pts >= pts) {
        return true;
    } else {
        last_pts = pts;
        return false;
    }
}

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

    px_ctx_free(&pxc);

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

    ret = px_transcode_init(pxc, &packet, &frame);
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

            if (should_skip_frame(frame)) {
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

    px_mediactx_free(&pxc->media_ctx);
    px_settings_free(&pxc->settings);
}

void px_settings_free(PXSettings* s) {

    free_s(&s->output_file);

    if (s->enc_opts_v)
        av_dict_free(&s->enc_opts_v);
    if (s->enc_opts_a)
        av_dict_free(&s->enc_opts_a);
}
