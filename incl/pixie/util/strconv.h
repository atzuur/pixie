#pragma once

#include <stdbool.h>
#include <stdint.h>

static inline bool px_is_digit(char c) {
    return c >= '0' && c <= '9';
}

int px_strtoi(int* dest, const char* str);
int px_strtol(long* dest, const char* str);

int px_strtof(float* dest, const char* str);
int px_strtod(double* dest, const char* str);

int px_strtob(bool* dest, const char* str);
