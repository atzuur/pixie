#include <pixie/util/utils.h>
#include <pixie/settings.h>

#include <stdlib.h>
#include <errno.h>

PXSettings* px_settings_alloc(void) {
    PXSettings* settings = calloc(1, sizeof *settings);
    if (!settings)
        px_oom_msg(sizeof *settings);
    return settings;
}

int px_settings_check(PXSettings* s) {
    if (!s->n_input_files) {
        px_log(PX_LOG_ERROR, "No input file specified\n");
        return PXERROR(EINVAL);
    }

    if (!s->output_file) {
        px_log(PX_LOG_ERROR, "No output file specified\n");
        return PXERROR(EINVAL);
    }

    for (int i = 0; i < s->n_input_files; i++) {
        if (!px_file_exists(s->input_files[i])) {
            px_log(PX_LOG_ERROR, "File \"%s\" does not exist\n", s->input_files[i]);
            return PXERROR(ENOENT);
        }
    }

    if (s->n_input_files > 1) {
        if (!s->output_folder) {
            // /input_files[i] will be appended during transcoding
            s->output_folder = s->output_file;
            s->output_file = NULL;
        }

        int ret = px_create_folder(s->output_folder);
        if (ret < 0) {
            px_log(PX_LOG_ERROR, "Failed to create output directory \"%s\"\n", s->output_folder);
            return ret;
        }
    }

    px_log_set_level(s->loglevel);

    return 0;
}

void px_settings_free(PXSettings** s) {
    if (!s || !*s)
        return;
    px_free(s);
}
