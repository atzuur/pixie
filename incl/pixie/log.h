#pragma once

#include <libavutil/log.h>

#include <stdio.h>
#include <string.h>

typedef enum PxLogLevel {
    PX_LOG_NONE = -1,
    PX_LOG_QUIET, // no output
    PX_LOG_ERROR, // errors only
    PX_LOG_PROGRESS, // + progress info (default)
    PX_LOG_WARN, // + warnings
    PX_LOG_INFO, // + misc. info
    PX_LOG_VERBOSE, // + everything else
    PX_LOG_COUNT, // number of log levels (not a valid log level)
} PXLogLevel;

extern PXLogLevel px_global_loglevel;

extern const char* const px_log_names[];
extern const int px_log_colors[];

#define $px_log_c(c) (px_log_color(c, (char[16]) {0}))
#define PX_RESET "\033[0m"
#define PX_LOG_MAX_COLOR_LEN 16

static inline char* px_log_color(int c, char buf[static PX_LOG_MAX_COLOR_LEN]) {
    snprintf(buf, PX_LOG_MAX_COLOR_LEN, "\033[38;5;%dm", c);
    return buf;
}

static inline PXLogLevel px_loglevel_from_str(const char* str) {
    for (int i = 0; i < PX_LOG_COUNT; i++) {
        if (strcmp(px_log_names[i], str) == 0)
            return (PXLogLevel)i;
    }
    return PX_LOG_NONE;
}

static inline int px_loglevel_to_av(PXLogLevel level) {
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

static inline void px_log_set_level(PXLogLevel level) {
    if (level == PX_LOG_NONE)
        level = PX_LOG_WARN;

    av_log_set_level(px_loglevel_to_av(level));
    px_global_loglevel = level;
}

#define $px_log(level, msg, ...) px_log_msg(level, "%s(): " msg, __func__ __VA_OPT__(, ) __VA_ARGS__)
void px_log_msg(PXLogLevel level, const char* msg, ...);
