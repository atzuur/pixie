#include "../tests/test_filter.c" // TODO: remove this
#include "util/utils.h"

#include <pixie/pixie.h>
#include "frame_internal.h"

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

        ret = px_thrd_launch(&pxc.transc_thread);
        if (ret < 0)
            break;

        while (!pxc.transc_thread.done) {
            sleep_ms(10);
            $px_log(PX_LOG_PROGRESS, "Decoded %lu frames, dropped %lu frames, encoded %lu frames\r",
                    pxc.media_ctx.frames_decoded, pxc.media_ctx.decoded_frames_dropped,
                    pxc.media_ctx.frames_output);
        }

        putchar('\n');

        int transc_ret = 0;
        ret = px_thrd_join(&pxc.transc_thread, &transc_ret);
        if (ret < 0)
            break;

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

static int filter_encode_frame(PXContext* pxc, AVFrame* frame) {

    int ret = 0;

    if (!frame) // flush
        goto skip_filters;

    PXFrame px_frame = {0};
    ret = px_frame_from_av(&px_frame, frame);
    if (ret < 0)
        return ret;

    for (int i = 0; i < pxc->fltr_ctx.n_filters; i++) {
        PXFilter* fltr = &pxc->fltr_ctx.filters[i];

        fltr->frame = &px_frame;
        fltr->frame_num = pxc->media_ctx.frames_decoded;

        ret = fltr->apply(fltr);
        if (ret < 0) {
            $px_log(PX_LOG_ERROR, "Failed to apply filter \"%s\"\n", fltr->name);
            return ret;
        }
    }

    px_frame_to_av(frame, &px_frame);

skip_filters:
    ret = px_encode_frame(&pxc->media_ctx, frame);
    if (ret == AVERROR_EOF) {
        return 0;
    } else if (ret < 0) {
        return ret;
    }

    return 0;
}

static int transcode_packet(PXContext* pxc, AVPacket* pkt) {

    int ret = 0;

    AVCodecContext* dec_ctx = pxc->media_ctx.coding_ctx_arr[pxc->media_ctx.stream_idx].dec_ctx;
    ret = avcodec_send_packet(dec_ctx, pkt);
    if (ret < 0) {
        $px_lav_throw_msg("avcodec_send_packet", ret);
        return ret;
    }

    if (pkt)
        av_packet_unref(pkt);

    while (ret >= 0) {
        AVFrame frame = {0};
        ret = avcodec_receive_frame(dec_ctx, &frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            ret = 0;
            break;
        } else if (ret < 0) {
            $px_lav_throw_msg("avcodec_receive_frame", ret);
            return ret;
        }
        pxc->media_ctx.frames_decoded++;
        frame.pts = frame.best_effort_timestamp;

        ret = filter_encode_frame(pxc, &frame);
        if (ret < 0)
            return ret;

        av_frame_unref(&frame);
    }

    return 0;
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

        ret = transcode_packet(pxc, &pkt);
        if (ret < 0)
            goto end;
    }

    // flush
    for (unsigned i = 0; i < pxc->media_ctx.ifmt_ctx->nb_streams; i++) {
        enum AVMediaType stream_type = pxc->media_ctx.ifmt_ctx->streams[i]->codecpar->codec_type;
        if (stream_type != AVMEDIA_TYPE_VIDEO)
            continue;

        pxc->media_ctx.stream_idx = (int)i;

        if (pxc->media_ctx.coding_ctx_arr[i].dec_ctx->codec->capabilities & AV_CODEC_CAP_DELAY) {
            ret = transcode_packet(pxc, NULL);
            if (ret < 0)
                goto end;
        }

        if (pxc->media_ctx.coding_ctx_arr[i].enc_ctx->codec->capabilities & AV_CODEC_CAP_DELAY) {
            ret = filter_encode_frame(pxc, NULL);
            if (ret < 0)
                goto end;
        }
    }

    ret = av_write_trailer(pxc->media_ctx.ofmt_ctx);
    if (ret < 0)
        $px_lav_throw_msg("av_write_trailer", ret);

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
