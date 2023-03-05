#pragma once

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <libavutil/log.h>

typedef enum PxLogLevel {

    PX_LOG_NONE = -1,
    PX_LOG_QUIET,
    PX_LOG_WARN,
    PX_LOG_ERROR,
    PX_LOG_INFO,
    PX_LOG_VERBOSE,
    PX_LOG_COUNT, // number of log levels (not a valid log level)

} PXLogLevel;

// should
extern PXLogLevel px_global_loglevel;

extern const char* const px_log_names[];
extern const int px_log_colors[];

#define PX_LOG_C(c) (px_log_color(c, (char[16]) {0}))

#define PX_RESET "\033[0m"

// writes 16 chars at most
static inline char* px_log_color(int c, char* buf) {
    snprintf(buf, 16, "\033[38;5;%dm", c);
    return buf;
}

static inline int px_loglevel_from_str(const char* str) {
    for (int i = 0; i < PX_LOG_COUNT; i++) {
        if (strcmp(px_log_names[i], str) == 0)
            return i;
    }
    return -1;
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

    av_log_set_level(px_loglevel_to_av(level));
    px_global_loglevel = level;
}

void px_log(PXLogLevel level, const char* msg, ...);
