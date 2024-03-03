#include "cli.h"

#include <pixie/log.h>

#include <inttypes.h>
#include <errno.h>
#include <stdlib.h>

// change the basename of `*path` to `filename`
static int scroll_next_filename(char** path, const char* folder_name, const char* filename) {
    size_t out_path_len = strlen(folder_name) + sizeof PX_PATH_SEP + strlen(filename);

    *path = realloc(*path, out_path_len + 1);
    if (!*path) {
        px_oom_msg(out_path_len + 1);
        return PXERROR(ENOMEM);
    }

    memset(*path, 0, out_path_len + 1);
    strcat(*path, folder_name);
    strcat(*path, PX_PATH_SEP);
    strcat(*path, filename);

    return 0;
}

static void print_progress(PXMediaContext* ctx) {
    px_log(PX_LOG_PROGRESS,
           "Decoded %" PRIi64 " frames, dropped %" PRIi64 " frames, encoded %" PRIi64 " frames\r",
           ctx->frames_decoded, ctx->decoded_frames_dropped, ctx->frames_output);
}

int px_main(PXSettings* settings) {
    int ret = 0;

    PXContext* pxc = px_ctx_alloc();
    if (!pxc)
        return PXERROR(ENOMEM);

    ret = px_filter_ctx_new(&pxc->fltr_ctx, settings);
    if (ret < 0)
        goto end;

    pxc->transc_thread = (PXThread) {
        .func = (PXThreadFunc)px_transcode,
        .args = pxc,
    };

    for (pxc->input_idx = 0; pxc->input_idx < settings->n_input_files; pxc->input_idx++) {
        if (settings->n_input_files > 1) {
            const char* basename = px_get_basename(settings->input_files[pxc->input_idx]);
            ret = scroll_next_filename(&settings->output_file, settings->output_folder, basename);
            if (ret < 0)
                goto end;
        }

        // TODO: check if input is same as output
        ret = px_media_ctx_new(&pxc->media_ctx, settings, pxc->input_idx);
        if (ret < 0)
            goto end;

        ret = px_thrd_launch(&pxc->transc_thread);
        if (ret < 0)
            goto end;

        while (!pxc->transc_thread.done) {
            px_sleep_ms(10);
            print_progress(pxc->media_ctx);
        }

        print_progress(pxc->media_ctx);
        putchar('\n');

        int transc_ret = 0;
        ret = px_thrd_join(&pxc->transc_thread, &transc_ret);
        if (ret < 0)
            goto end;

        if (transc_ret < 0) {
            px_log(PX_LOG_ERROR, "Error occurred while processing file \"%settings\" (stream index %d)\n",
                   settings->input_files[pxc->input_idx], pxc->media_ctx->stream_idx);
            ret = transc_ret;
            goto end;
        }
    }

end:
    px_ctx_free(&pxc);
    return ret;
}

int main(int argc, char** argv) {
    PXSettings* settings = px_settings_alloc();
    int ret = px_parse_args(argc, argv, settings);
    if (ret) {
        if (ret == PX_HELP_PRINTED)
            ret = 0;
        goto early_ret;
    }

    ret = px_settings_check(settings);
    if (ret)
        goto early_ret;

    ret = px_main(settings);

early_ret:
    // TODO: better ownership
    px_free(&settings->output_file);
    px_free(&settings->filter_opts);

    px_settings_free(&settings);
    return ret;
}
