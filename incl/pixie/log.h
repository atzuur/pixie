#pragma once

#include <stdio.h>
#include <string.h>

typedef enum PxLogLevel : int {
    PX_LOG_NONE = -1,
    PX_LOG_QUIET, // no output
    PX_LOG_ERROR, // errors only
    PX_LOG_PROGRESS, // + progress info
    PX_LOG_WARN, // + warnings (default)
    PX_LOG_INFO, // + misc. info
    PX_LOG_VERBOSE, // + everything else
    PX_LOG_COUNT, // number of log levels (not a valid log level)
} PXLogLevel;

extern PXLogLevel px_global_log_level;

extern const char* const px_log_names[];
extern const int px_log_colors[];

#define PX_LOG_C(c) (px_log_color(c, (char[PX_LOG_MAX_COLOR_LEN]) {0}))
#define PX_RESET "\033[0m"
#define PX_LOG_MAX_COLOR_LEN 16

char* px_log_color(int c, char buf[static PX_LOG_MAX_COLOR_LEN]);

PXLogLevel px_log_level_from_str(const char* str);

void px_log_set_level(PXLogLevel level);

// abi-safe way to get PX_LOG_COUNT
int px_log_num_levels(void);

void px_log(PXLogLevel level, const char* msg, ...);
