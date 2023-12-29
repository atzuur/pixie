#pragma once

#include <pixie/coding.h>
#include <pixie/filter.h>
#include <pixie/settings.h>
#include <pixie/util/thread.h>

#include <stdbool.h>

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
