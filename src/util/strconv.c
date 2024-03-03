#include <pixie/util/utils.h>
#include <pixie/util/strconv.h>

#include <errno.h>
#include <stdlib.h>
#include <float.h>

int px_strtoi(int* dest, const char* str) {
    char* end;
    errno = 0;
    long val = strtol(str, &end, 10);

    if (*str == '\0' || *end != '\0' || errno != 0 || val < INT_MIN || val > INT_MAX)
        return PXERROR(EINVAL);

    *dest = (int)val;
    return 0;
}

int px_strtol(long* dest, const char* str) {
    char* end;
    errno = 0;
    long val = strtol(str, &end, 10);

    if (*str == '\0' || *end != '\0' || errno != 0)
        return PXERROR(EINVAL);

    *dest = val;
    return 0;
}

int px_strtof(float* dest, const char* str) {
    char* end;
    errno = 0;
    double val = strtod(str, &end);

    if (*str == '\0' || *end != '\0' || errno != 0)
        return PXERROR(EINVAL);

    *dest = (float)val; // TODO: determine if this is safe
    return 0;
}

int px_strtod(double* dest, const char* str) {
    char* end;
    errno = 0;
    double val = strtod(str, &end);

    if (*str == '\0' || *end != '\0' || errno != 0)
        return PXERROR(EINVAL);

    *dest = val;
    return 0;
}

int px_strtob(bool* dest, const char* str) {
    while (isspace(*str)) {
        str++;
    }
    // no constexpr in clang yet :(
    enum : size_t { MAX_BOOL_SIZE = sizeof "false" };
    char stripped[MAX_BOOL_SIZE];
    strncpy(stripped, str, MAX_BOOL_SIZE - 1);

    if (strcmp(stripped, "true") == 0)
        *dest = true;
    else if (strcmp(stripped, "false") == 0)
        *dest = false;
    else
        return PXERROR(EINVAL);

    return 0;
}
