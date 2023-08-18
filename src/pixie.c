#include "pixie.h"
#include "../tests/test_filter.c"
#include "util/utils.h"

// make `*path` point to a string containing `folder_name` + PATH_SEP + `filename`
static int scroll_next_filename(char** path, const char* folder_name, const char* filename) {
    size_t out_path_len = strlen(folder_name) + sizeof PATH_SEP + strlen(filename);

    *path = realloc(*path, out_path_len + 1);
    if (!*path) {
        oom(out_path_len + 1);
        return AVERROR(ENOMEM);
    }

    memset(*path, 0, out_path_len + 1);
    strcat(*path, folder_name);
    strcat(*path, PATH_SEP);
    strcat(*path, filename);

    return 0;
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

    for (int i = 0; i < pxc.fltr_ctx.n_filters; i++) {
        PXFilter* fltr = &pxc.fltr_ctx.filters[i];
        ret = fltr->init(fltr, NULL); // TODO: pass settings
        if (ret < 0) {
            $px_log(PX_LOG_ERROR, "Failed to initialize filter \"%s\"\n", fltr->name);
            return ret;
        }
    }

    pxc.transc_thread = (PXThread) {
        .func = (PXThreadFunc)px_transcode,
        .args = &pxc,
    };

    for (pxc.input_idx = 0; pxc.input_idx < pxc.settings.n_input_files; pxc.input_idx++) {

        if (pxc.settings.n_input_files > 1) {
            ret = scroll_next_filename(&pxc.settings.output_file, pxc.settings.output_folder,
                                       get_basename(pxc.settings.input_files[pxc.input_idx]));
            if (ret < 0)
                break;
        }

        // TODO: check if input is same as output

        ret = px_media_ctx_init(&pxc.media_ctx, &pxc.settings, pxc.input_idx);
        if (ret < 0)
            break;

        px_thrd_launch(&pxc.transc_thread);

        while (!pxc.transc_thread.done) {
            sleep_ms(10);
            $px_log(PX_LOG_PROGRESS, "Decoded %lu frames, dropped %lu frames, encoded %lu frames\r",
                    pxc.media_ctx.frames_decoded, pxc.media_ctx.decoded_frames_dropped,
                    pxc.media_ctx.frames_output);
        }

        putchar('\n');

        int transc_ret = 0;
        px_thrd_join(&pxc.transc_thread, &transc_ret);

        if (transc_ret < 0) {
            $px_log(PX_LOG_ERROR, "Error occurred while processing file \"%s\" (stream index %d)\n",
                    pxc.settings.input_files[pxc.input_idx], pxc.media_ctx.stream_idx);
            ret = transc_ret;
            break;
        }
    }

    px_ctx_free(&pxc);

    return ret;
}

int px_transcode(PXContext* pxc) {

    int ret = 0;

    while (1) {
        AVPacket pkt = {0};
        ret = px_read_video_frame(&pxc->media_ctx, &pkt);
        if (ret == AVERROR(EAGAIN)) {
            continue;
        } else if (ret == AVERROR_EOF) {
            ret = 0;
            break;
        } else if (ret < 0) {
            goto end;
        }

        AVCodecContext* dec_ctx = pxc->media_ctx.coding_ctx_arr[pxc->media_ctx.stream_idx].dec_ctx;
        ret = avcodec_send_packet(dec_ctx, &pkt);
        if (ret < 0)
            goto end;

        av_packet_unref(&pkt);

        while (ret >= 0) {
            AVFrame frame = {0};
            ret = avcodec_receive_frame(dec_ctx, &frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                ret = 0;
                break;
            } else if (ret < 0) {
                $lav_throw_msg("avcodec_receive_frame", ret);
                goto end;
            }
            pxc->media_ctx.frames_decoded++;
            frame.pts = frame.best_effort_timestamp;

            /* filtering
            {
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
            }
            */

            ret = px_encode_frame(&pxc->media_ctx, &frame);
            if (ret == AVERROR_EOF) {
                ret = 0;
                break;
            } else if (ret < 0) {
                goto end;
            }
            av_frame_unref(&frame);
        }
    }

    for (unsigned i = 0; i < pxc->media_ctx.ifmt_ctx->nb_streams; i++) {
        enum AVMediaType stream_type = pxc->media_ctx.ifmt_ctx->streams[i]->codecpar->codec_type;
        if (stream_type != AVMEDIA_TYPE_VIDEO)
            continue;

        pxc->media_ctx.stream_idx = i;

        ret = px_flush_decoder(&pxc->media_ctx);
        if (ret < 0 && ret != AVERROR_EOF) {
            $px_log(PX_LOG_ERROR, "Failed to flush decoder\n");
            goto end;
        }

        ret = px_flush_encoder(&pxc->media_ctx);
        if (ret < 0 && ret != AVERROR_EOF) {
            $px_log(PX_LOG_ERROR, "Failed to flush encoder\n");
            goto end;
        }
    }

    ret = av_write_trailer(pxc->media_ctx.ofmt_ctx);
    if (ret < 0)
        $lav_throw_msg("av_write_trailer", ret);

end:
    pxc->transc_thread.done = true;
    return ret;
}

void px_ctx_free(PXContext* pxc) {

    for (int i = 0; i < pxc->fltr_ctx.n_filters; i++) {
        PXFilter* fltr = &pxc->fltr_ctx.filters[i];
        fltr->free(fltr);
    }

    px_media_ctx_free(&pxc->media_ctx);
    px_settings_free(&pxc->settings);
}
