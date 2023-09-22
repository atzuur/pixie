#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct AtoiResult {
    int64_t value;
    bool fail;
} AtoiResult;

typedef struct AtofResult {
    double value;
    bool fail;
} AtofResult;

static inline bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

AtoiResult checked_atoi(const char* str);
AtofResult checked_atof(const char* str);
