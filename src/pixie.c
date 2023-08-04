#include "pixie.h"
#include "../tests/test_filter.c"
#include "cli.h"
#include "coding.h"
#include "frame.h"

static bool should_skip_frame(AVFrame* frame) {

    if (frame->flags & AV_FRAME_FLAG_DISCARD)
        return true;

    static int64_t last_pts = INT64_MIN; // avoid unintentionally skipping the first frame
    int64_t pts = frame->pts;

    if (pts != AV_NOPTS_VALUE && last_pts >= pts)
        return true;

    last_pts = pts;
    return false;
}

int px_main(PXSettings s) {

    int ret = 0;

    PXFilter test_fltr = {
        .name = "test",
        .init = test_filter_init,
        .apply = test_filter_apply,
        .free = test_filter_free,
    };

    PXContext pxc = {
        .settings = s,
        .fltr_ctx = {.filters = &test_fltr, .n_filters = 1},
    };

    pxc.transc_thread = (PXThread) {
        .func = (PXThreadFunc)px_transcode,
        .args = &pxc,
    };

    for (pxc.input_idx = 0; pxc.input_idx < pxc.settings.n_input_files; pxc.input_idx++) {

        char* current_file = pxc.settings.input_files[pxc.input_idx];
        if (!file_exists(current_file)) {
            $px_log(PX_LOG_ERROR, "File \"%s\" does not exist\n", current_file);
            ret = AVERROR(ENOENT);
            break;
        }

        if (strcmp(current_file, pxc.settings.output_url) == 0) {
            $px_log(PX_LOG_ERROR, "Input and output files cannot be the same\n");
            ret = AVERROR(EINVAL);
            break;
        }

        px_thrd_launch(&pxc.transc_thread);

        while (!pxc.transc_thread.done) {
            sleep_ms(10);
            $px_log(PX_LOG_PROGRESS, "Decoded %lu frames, dropped %lu frames, encoded %lu frames\r",
                    pxc.frames_decoded, pxc.decoded_frames_dropped, pxc.frames_output);
        }

        putchar('\n');

        int transc_ret = 0;
        px_thrd_join(&pxc.transc_thread, &transc_ret);

        if (transc_ret) {
            $px_log(PX_LOG_ERROR, "Error occurred while processing file \"%s\" (stream index %d)\n",
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

    ret = px_transcode_init(pxc, &packet, &frame);
    if (ret)
        goto end;

    while (1) {

        ret = av_read_frame(pxc->media_ctx.ifmt_ctx, packet);
        if (ret == AVERROR_EOF) {
            ret = 0;
            break;
        } else if (ret < 0) {
            $lav_throw_msg("av_read_frame", ret);
            goto end;
        }

        pxc->stream_idx = packet->stream_index;

        PXStreamContext* stc = &pxc->media_ctx.stream_ctx_vec[pxc->stream_idx];

        enum AVMediaType stream_type =
            pxc->media_ctx.ifmt_ctx->streams[pxc->stream_idx]->codecpar->codec_type;

        if (stream_type != AVMEDIA_TYPE_VIDEO) {
            ret = av_interleaved_write_frame(pxc->media_ctx.ofmt_ctx, packet);
            if (ret < 0) {
                $lav_throw_msg("av_interleaved_write_frame", ret);
                goto end;
            }
            continue;
        }

        ret = decode_frame(stc, frame, packet);
        if (ret == AVERROR_EOF) {
            ret = 0;
            break;
        } else if (ret < 0) {
            goto end;
        }

        av_packet_unref(packet);

        if (should_skip_frame(frame)) {
            pxc->decoded_frames_dropped++;
            continue;
        }

        pxc->frames_decoded++;

        ret = av_frame_make_writable(frame);
        if (ret < 0) {
            $lav_throw_msg("av_frame_make_writable", ret);
            goto end;
        }

        PXFrame px_frame = px_frame_from_av(frame);
        px_frame_assert_correctly_converted(frame, &px_frame);

        for (int i = 0; i < pxc->fltr_ctx.n_filters; i++) {
            PXFilter* fltr = &pxc->fltr_ctx.filters[i];

            fltr->frame = &px_frame;

            ret = fltr->apply(fltr);
            if (ret < 0) {
                $px_log(PX_LOG_ERROR, "Failed to apply filter \"%s\"\n", fltr->name);
                goto end;
            }
        }

        ret = encode_frame(&pxc->media_ctx, frame, packet, pxc->stream_idx);
        if (ret == AVERROR_EOF) {
            ret = 0;
            break;
        } else if (ret < 0) {
            goto end;
        }

        pxc->frames_output++;

        av_frame_unref(frame);
        av_packet_unref(packet);
    }

    for (unsigned i = 0; i < pxc->media_ctx.ifmt_ctx->nb_streams; i++) {
        enum AVMediaType stream_type = pxc->media_ctx.ifmt_ctx->streams[i]->codecpar->codec_type;
        if (stream_type != AVMEDIA_TYPE_VIDEO)
            continue;

        ret = flush_encoder(&pxc->media_ctx, packet, i);
        if (ret < 0 && ret != AVERROR_EOF) {
            $px_log(PX_LOG_ERROR, "Failed to flush encoder\n");
            goto end;
        }
    }

    ret = av_write_trailer(pxc->media_ctx.ofmt_ctx);
    if (ret < 0)
        $lav_throw_msg("av_write_trailer", ret);

end:
    px_transcode_free(&packet, &frame);
    pxc->transc_thread.done = true;
    return ret;
}

int px_transcode_init(PXContext* pxc, AVPacket** packet, AVFrame** frame) {

    int ret = 0;

    *packet = NULL;
    *frame = NULL;

    char* in_file = pxc->settings.input_files[pxc->input_idx];
    char* out_url = pxc->settings.output_url;

    if (pxc->settings.n_input_files > 1) {
        char* in_file_basename = get_basename(in_file);
        pxc->settings.output_url = out_url = realloc(out_url, strlen(out_url) + strlen(in_file_basename) + 1);
        strcat(out_url, in_file_basename);
    }

    ret = init_input(&pxc->media_ctx, in_file);
    if (ret) {
        $px_log(PX_LOG_ERROR, "Failed to open input file \"%s\": %s (%d)\n", in_file, av_err2str(ret), ret);
        return ret;
    }

    ret = init_output(&pxc->media_ctx, out_url, &pxc->settings);
    if (ret) {
        $px_log(PX_LOG_ERROR, "Failed to open output file \"%s\": %s (%d)\n", out_url, av_err2str(ret), ret);
        return ret;
    }

    for (int i = 0; i < pxc->fltr_ctx.n_filters; i++) {
        PXFilter* fltr = &pxc->fltr_ctx.filters[i];
        ret = fltr->init(fltr, NULL); // TODO: pass settings
        if (ret) {
            $px_log(PX_LOG_ERROR, "Failed to initialize filter \"%s\"\n", fltr->name);
            return ret;
        }
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

    return ret;
}

int px_settings_init(int argc, char** argv, PXSettings* s) {

    *s = (PXSettings) {0};

    int ret = px_parse_args(argc, argv, s);
    if (ret)
        return ret == PX_HELP_PRINTED ? 0 : ret;

    if (!s->n_input_files) {
        $px_log(PX_LOG_ERROR, "No input file specified\n");
        return 1;
    }

    if (!s->output_url) {
        $px_log(PX_LOG_ERROR, "No output file specified\n");
        return 1;
    }

    // /path/to/output_url/input_files[i]
    if (s->n_input_files > 1) {

        char last = s->output_url[strlen(s->output_url) - 1];
        if (last != *PATH_SEP) {
            // +1 for the path sep, +1 for the null terminator
            s->output_url = realloc(s->output_url, strlen(s->output_url) + 1 + 1);
            strcat(s->output_url, PATH_SEP);
        }

        if (!create_folder(s->output_url)) {
            char err[256];
            last_errstr(err, 0);
            $px_log(PX_LOG_ERROR, "Failed to create output directory: %s (%d)\n", err, $last_errcode());
            return 1;
        }
    }

    px_log_set_level(s->loglevel);

    return 0;
}

void px_ctx_free(PXContext* pxc) {

    for (int i = 0; i < pxc->fltr_ctx.n_filters; i++) {
        PXFilter* fltr = &pxc->fltr_ctx.filters[i];
        fltr->free(fltr);
    }

    px_media_ctx_free(&pxc->media_ctx);
    px_settings_free(&pxc->settings);
}

void px_settings_free(PXSettings* s) {

    if (s->n_input_files > 1)
        free_s(&s->output_url);

    if (s->enc_opts_v)
        av_dict_free(&s->enc_opts_v);
}

void px_transcode_free(AVPacket** packet, AVFrame** frame) {

    av_packet_free(packet);
    av_frame_free(frame);
}
