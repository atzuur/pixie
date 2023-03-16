#include "log.h"

PXLogLevel px_global_loglevel = PX_LOG_ERROR;

const char* const px_log_names[] = {
    "quiet", "warn", "error", "info", "verbose",
};
const int px_log_colors[PX_LOG_COUNT] = {
    0, 214, 196, 255, 221,
};

void px_log(PXLogLevel level, const char* msg, ...) {

    if (level > px_global_loglevel)
        return;

    va_list args;
    va_start(args, msg);

    FILE* stream = (level == PX_LOG_INFO) ? stdout : stderr;

    char prefix[64];

    char* pink = PX_LOG_C(213);
    char* msg_color = PX_LOG_C(px_log_colors[level]);

    snprintf(prefix, sizeof prefix, "[%spixie%s] %s", pink, PX_RESET, msg_color);

    fprintf(stream, "%s", prefix);
    vfprintf(stream, msg, args);
    fprintf(stream, "%s", PX_RESET);

    va_end(args);
    fflush(stream);
}