#include <pixie/log.h>

#include <libavutil/log.h>

PXLogLevel px_global_log_level = PX_LOG_WARN; // default

const char* const px_log_names[PX_LOG_COUNT] = {
    "quiet", "error", "progress", "warn", "info", "verbose",
};
const int px_log_colors[PX_LOG_COUNT] = {
    0, 1, 229, 214, 255, 221,
};

char* px_log_color(int c, char buf[static PX_LOG_MAX_COLOR_LEN]) {
    snprintf(buf, PX_LOG_MAX_COLOR_LEN, "\033[38;5;%dm", c);
    return buf;
}

PXLogLevel px_log_level_from_str(const char* str) {
    for (int i = 0; i < PX_LOG_COUNT; i++) {
        if (strcmp(px_log_names[i], str) == 0)
            return (PXLogLevel)i;
    }
    return PX_LOG_NONE;
}

static inline int px_log_level_to_av(PXLogLevel level) {
    switch (level) {
        case PX_LOG_QUIET:
            return AV_LOG_QUIET;
        case PX_LOG_INFO:
            return AV_LOG_INFO;
        case PX_LOG_WARN:
            return AV_LOG_WARNING;
        case PX_LOG_ERROR:
            return AV_LOG_ERROR;
        case PX_LOG_VERBOSE:
            return AV_LOG_VERBOSE;
        default:
            return AV_LOG_ERROR;
    }
}

void px_log_set_level(PXLogLevel level) {
    if (level == PX_LOG_NONE)
        level = PX_LOG_WARN;

    av_log_set_level(px_log_level_to_av(level));
    px_global_log_level = level;
}

int px_log_num_levels(void) {
    return PX_LOG_COUNT;
}

void px_log(PXLogLevel level, const char* msg, ...) {
    if (level > px_global_log_level)
        return;

    va_list args;
    va_start(args, msg);

    FILE* stream = level == PX_LOG_INFO ? stdout : stderr;

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
