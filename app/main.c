#include "cli.h"
#include "app.h"

#include <pixie/pixie.h>

#include <inttypes.h>
#include <errno.h>
#include <stdlib.h>

static void print_progress(PXMediaContext* ctx) {
    px_log(PX_LOG_PROGRESS,
           "Decoded %" PRIi64 " frames, dropped %" PRIi64 " frames, encoded %" PRIi64 " frames\r",
           ctx->frames_decoded, ctx->decoded_frames_dropped, ctx->frames_output);
}

int main(int argc, char** argv) {
    Settings settings = {.log_level = PX_LOG_NONE};
    int ret = parse_args(argc, argv, &settings);
    if (ret < 0) {
        if (ret == HELP_PRINTED)
            ret = 0;
        goto end;
    }

    if (!settings.n_input_files) {
        px_log(PX_LOG_ERROR, "No input files specified\n");
        ret = PXERROR(EINVAL);
        goto end;
    }

    if (!settings.output_file) {
        px_log(PX_LOG_ERROR, "No output file specified\n");
        ret = PXERROR(EINVAL);
        goto end;
    }

    for (int i = 0; i < settings.n_input_files; i++) {
        if (!px_file_exists(settings.input_files[i])) {
            px_log(PX_LOG_ERROR, "Input file \"%s\" does not exist\n", settings.input_files[i]);
            ret = PXERROR(ENOENT);
            goto end;
        }
    }

    if (settings.n_input_files > 1) {
        ret = px_create_folder(settings.output_file);
        if (ret < 0) {
            px_log(PX_LOG_ERROR, "Failed to create output directory \"%s\"\n", settings.output_file);
            goto end;
        }
    }

    px_log_set_level(settings.log_level);

    PXContext* pxc = px_ctx_alloc();
    if (!pxc) {
        ret = PXERROR(ENOMEM);
        goto end;
    }

    const char* const* filter_names_view = (const char* const*)settings.filter_names;
    ret = px_filter_ctx_new(&pxc->fltr_ctx, settings.filter_dir, filter_names_view, settings.filter_opts,
                            settings.n_filters);
    if (ret < 0)
        goto end;

    pxc->transc_thread = (PXThread) {
        .func = (PXThreadFunc)px_transcode,
        .args = pxc,
    };

    for (pxc->input_idx = 0; pxc->input_idx < settings.n_input_files; pxc->input_idx++) {
        if (settings.n_input_files > 1) {
            const char* basename = px_get_basename(settings.input_files[pxc->input_idx]);
            size_t out_file_len = strlen(settings.output_file) + strlen(basename) + sizeof PX_PATH_SEP;
            settings.output_file = realloc(settings.output_file, out_file_len);
            if (!settings.output_file) {
                ret = PXERROR(ENOMEM);
                goto end;
            }
            sprintf(settings.output_file, "%s" PX_PATH_SEP "%s", settings.output_file, basename);
        }

        // TODO: check if input is same as output
        ret = px_media_ctx_new(&pxc->media_ctx, settings.input_files[pxc->input_idx], settings.output_file,
                               settings.enc_name_v, settings.enc_opts_v);
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
            px_log(PX_LOG_ERROR, "Error occurred while processing file \"%s\" (stream index %d)\n",
                   settings.input_files[pxc->input_idx], pxc->media_ctx->stream_idx);
            ret = transc_ret;
            goto end;
        }

        px_media_ctx_free(&pxc->media_ctx);
    }

end:
    parsed_args_free(&settings);
    px_ctx_free(&pxc);
    return ret;
}
