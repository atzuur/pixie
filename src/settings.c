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
        PX_LOG(PX_LOG_ERROR, "No input file specified\n");
        return PXERROR(EINVAL);
    }

    if (!s->output_file) {
        PX_LOG(PX_LOG_ERROR, "No output file specified\n");
        return PXERROR(EINVAL);
    }

    for (int i = 0; i < s->n_input_files; i++) {
        if (!px_file_exists(s->input_files[i])) {
            PX_LOG(PX_LOG_ERROR, "File \"%s\" does not exist\n", s->input_files[i]);
            return PXERROR(ENOENT);
        }
    }

    if (s->n_input_files > 1) {
        if (!s->output_folder) {
            // /input_files[i] will be appended later
            s->output_folder = s->output_file;
            s->output_file = NULL;
        }

        if (!px_create_folder(s->output_folder)) {
            char err_msg[256];
            px_last_os_errstr(err_msg, 0);
            int err = PX_LAST_OS_ERR();
            PX_LOG(PX_LOG_ERROR, "Failed to create output directory: %s (%d)\n", err_msg, err);
            return err;
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
