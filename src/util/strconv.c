#include "strconv.h"
#include <errno.h>
#include <stdlib.h>

AtoiResult checked_atoi(const char* str) {

    AtoiResult ret = {0};

    char* end;
    errno = 0;
    int64_t val = strtol(str, &end, 10);

    if (*str == '\0' || *end != '\0' || errno != 0)
        ret.fail = true;
    else
        ret.value = val;

    return ret;
}

AtofResult checked_atof(const char* str) {

    AtofResult ret = {0};

    char* end;
    errno = 0;
    double val = strtod(str, &end);

    if (*str == '\0' || *end != '\0' || errno != 0)
        ret.fail = true;
    else
        ret.value = val;

    return ret;
}
