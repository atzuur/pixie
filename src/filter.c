#include <pixie/filter.h>
#include <pixie/log.h>
#include <pixie/util/utils.h>

#include <stdlib.h>
#include <errno.h>

PXFilter* px_filter_alloc(void) {
    PXFilter* filter = calloc(1, sizeof *filter);
    if (!filter)
        px_oom_msg(sizeof *filter);
    return filter;
}

int px_filter_from_dll(PXFilter* filter, const char* filter_name) {
    (void)filter, (void)filter_name;
    PX_LOG(PX_LOG_ERROR, "todo");
    return PXERROR(ENOSYS);
}

void px_filter_free(PXFilter** filter) {
    if (!filter || !*filter)
        return;
    px_free(filter);
}

PXFilterContext* px_filter_ctx_alloc(int n_filters) {
    PXFilterContext* ctx = calloc(1, sizeof *ctx);
    if (!ctx) {
        px_oom_msg(sizeof *ctx);
        return NULL;
    }

    ctx->filters = malloc((size_t)n_filters * sizeof(PXFilter*));
    if (!ctx->filters) {
        px_oom_msg((size_t)n_filters * sizeof(PXFilter*));
        goto fail;
    }
    for (int i = 0; i < n_filters; i++) {
        ctx->filters[i] = px_filter_alloc();
        if (!ctx->filters[i]) {
            px_oom_msg(sizeof *ctx->filters[i]);
            goto fail;
        }
    }

    ctx->filter_opts = calloc((size_t)n_filters, sizeof *ctx->filter_opts);
    if (!ctx->filter_opts) {
        px_oom_msg((size_t)n_filters * sizeof *ctx->filter_opts);
        goto fail;
    }

    ctx->n_filters = n_filters;
    return ctx;

fail:
    px_filter_ctx_free(&ctx);
    return NULL;
}

int px_filter_ctx_new(PXFilterContext** ctx, const char* const* filter_names, const char* const* filter_opts,
                      int n_filters) {
    int ret = 0;

    *ctx = px_filter_ctx_alloc(n_filters);
    if (!*ctx)
        return PXERROR(ENOMEM);

    PXFilterContext* pctx = *ctx;
    for (int i = 0; i < pctx->n_filters; i++) {
        PXFilter* fltr = pctx->filters[i];
        ret = px_filter_from_dll(fltr, filter_names[i]);
        if (ret < 0)
            goto fail;

        ret = px_map_parse(&pctx->filter_opts[i], filter_opts[i]);
        if (ret < 0)
            goto fail;

        ret = fltr->init(fltr, &pctx->filter_opts[i]);
        if (ret < 0) {
            PX_LOG(PX_LOG_ERROR, "Failed to initialize filter \"%s\"\n", fltr->name);
            goto fail;
        }
    }

    return 0;

fail:
    px_filter_ctx_free(ctx);
    return ret;
}

void px_filter_ctx_free(PXFilterContext** ctx) {
    if (!ctx || !*ctx)
        return;
    PXFilterContext* pctx = *ctx;

    for (int i = 0; i < pctx->n_filters; i++) {
        if (pctx->filters[i]->free)
            pctx->filters[i]->free(pctx->filters[i]);

        px_filter_free(&pctx->filters[i]);
        px_map_free(&pctx->filter_opts[i]);
    }

    px_free(pctx->filters);
    px_free(&pctx->filter_opts);
    px_free(ctx);
}
