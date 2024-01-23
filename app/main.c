#include "cli.h"
#include "../tests/test_filter.c" // TODO: remove this

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

int px_main(PXSettings* s) {
    int ret = 0;

    PXFilter* test_fltr = px_filter_alloc();
    *test_fltr = (PXFilter) {
        .name = "test",
        .init = test_filter_init,
        .apply = test_filter_apply,
        .free = test_filter_free,
    };

    PXContext* pxc = px_ctx_alloc();
    pxc->fltr_ctx = px_filter_ctx_alloc(1);
    pxc->fltr_ctx->filters[0] = test_fltr;

    pxc->transc_thread = (PXThread) {
        .func = (PXThreadFunc)px_transcode,
        .args = pxc,
    };

    for (pxc->input_idx = 0; pxc->input_idx < s->n_input_files; pxc->input_idx++) {
        if (s->n_input_files > 1) {
            const char* basename = px_get_basename(s->input_files[pxc->input_idx]);
            ret = scroll_next_filename(&s->output_file, s->output_folder, basename);
            if (ret < 0)
                break;
        }

        // TODO: check if input is same as output
        ret = px_media_ctx_new(&pxc->media_ctx, s, pxc->input_idx);
        if (ret < 0)
            break;

        ret = px_thrd_launch(&pxc->transc_thread);
        if (ret < 0)
            break;

        while (!pxc->transc_thread.done) {
            px_sleep_ms(10);
            PX_LOG(PX_LOG_PROGRESS,
                   "Decoded %" PRIi64 " frames, dropped %" PRIi64 " frames, encoded %" PRIi64 " frames\r",
                   pxc->media_ctx->frames_decoded, pxc->media_ctx->decoded_frames_dropped,
                   pxc->media_ctx->frames_output);
        }

        putchar('\n');

        int transc_ret = 0;
        ret = px_thrd_join(&pxc->transc_thread, &transc_ret);
        if (ret < 0)
            break;

        if (transc_ret < 0) {
            PX_LOG(PX_LOG_ERROR, "Error occurred while processing file \"%s\" (stream index %d)\n",
                   s->input_files[pxc->input_idx], pxc->media_ctx->stream_idx);
            ret = transc_ret;
            break;
        }
    }

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
    free(settings->output_file); // TODO: better ownership
    px_settings_free(&settings);
    return ret;
}
