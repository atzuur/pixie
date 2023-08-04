#include "log.h"

PXLogLevel px_global_loglevel = PX_LOG_WARN; // default

const char* const px_log_names[] = {
    "quiet", "error", "progress", "warn", "info", "verbose",
};
const int px_log_colors[PX_LOG_COUNT] = {
    0, 196, 229, 214, 255, 221,
};

void px_log_msg(PXLogLevel level, const char* msg, ...) {

    if (level > px_global_loglevel)
        return;

    va_list args;
    va_start(args, msg);

    FILE* stream = (level == PX_LOG_INFO) ? stdout : stderr;

    char prefix[64];

    char* pink = PX_LOG_C(213);
    char* msg_color = PX_LOG_C(px_log_colors[level]);

    snprintf(prefix, sizeof prefix, "[%spixie %s// %s%s%s] %s", pink, PX_RESET, msg_color,
             px_log_names[level], PX_RESET, msg_color);

    fprintf(stream, "%s", prefix);
    vfprintf(stream, msg, args);
    fprintf(stream, "%s", PX_RESET);

    va_end(args);
    fflush(stream);
}
