#pragma once

#include "coding.h"
#include "filter.h"
#include "log.h"
#include "settings.h"
#include "util/thread.h"

#include <stdbool.h>
#include <stdint.h>

#define PX_VERSION "0.1.1"

typedef struct PXContext {

    PXSettings settings;

    PXMediaContext media_ctx;
    PXFilterContext fltr_ctx;

    PXThread transc_thread;

    int input_idx;
} PXContext;

int px_main(PXSettings s);

int px_transcode(PXContext* pxc);

void px_ctx_free(PXContext* pxc);
