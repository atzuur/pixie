#include "util/utils.h"

#include <pixie/settings.h>

#include <libavutil/error.h>

#include <stdlib.h>

int px_settings_check(PXSettings* s) {

    if (!s->n_input_files) {
        $px_log(PX_LOG_ERROR, "No input file specified\n");
        return AVERROR(EINVAL);
    }

    if (!s->output_file) {
        $px_log(PX_LOG_ERROR, "No output file specified\n");
        return AVERROR(EINVAL);
    }

    if (s->n_input_files > 1) {

        if (!s->output_folder) {
            s->output_folder = s->output_file; // /path/to/output_file/input_files[i]
            s->output_file = NULL;
        }

        if (!create_folder(s->output_folder)) {
            char err[256];
            last_errstr(err, 0);
            $px_log(PX_LOG_ERROR, "Failed to create output directory: %s (%d)\n", err, $last_errcode());
            return $last_errcode();
        }

        for (int i = 0; i < s->n_input_files; i++) {
            if (!file_exists(s->input_files[i])) {
                $px_log(PX_LOG_ERROR, "File \"%s\" does not exist\n", s->input_files[i]);
                return AVERROR(ENOENT);
            }
        }
    }

    px_log_set_level(s->loglevel);

    return 0;
}

void px_settings_free(PXSettings* s) {

    free_s(&s->output_file);

    if (s->enc_opts_v)
        av_dict_free(&s->enc_opts_v);

    free_s(&s->filter_names);
}
