#include <pixie/filter.h>
#include <pixie/log.h>
#include <pixie/util/utils.h>

#include <stdlib.h>
#include <errno.h>

#ifdef PX_PLATFORM_WINDOWS
#define DLL_EXT ".dll"
#elif defined(PX_PLATFORM_UNIX)
#define DLL_EXT ".so"
#endif

PXFilter* px_filter_alloc(void) {
    PXFilter* filter = calloc(1, sizeof *filter);
    if (!filter)
        px_oom_msg(sizeof *filter);
    return filter;
}

int px_filter_from_dll(PXFilter** filter, const char* dll_path) {
    assert(filter);
    assert(dll_path);

    PXDllHandle dll_handle = px_dll_open(dll_path);
    if (!dll_handle) {
        px_log(PX_LOG_ERROR, "Failed to open filter dll \"%s\"\n", dll_path);
        return PXERROR(ENOENT);
    }

    void* export_addr = px_dll_get_sym(dll_handle, PX_FILTER_EXPORT_FUNC);
    if (!export_addr) {
        px_log(PX_LOG_ERROR, "Failed to find symbol \"" PX_FILTER_EXPORT_FUNC "\" from \"%s\"\n", dll_path);
        return PXERROR(ENOENT);
    }

    typedef PXFilter* (*ExportFilterFunc)(void);
    ExportFilterFunc export_filter = *(ExportFilterFunc*)&export_addr;

    *filter = export_filter();
    if (!*filter) {
        px_log(PX_LOG_ERROR, "Failed to create filter from \"%s\"\n", dll_path);
        return PXERROR(ENOMEM);
    }
    if (!(*filter)->name) {
        px_log(PX_LOG_ERROR, "Filter name not set in \"%s\"\n", dll_path);
        return PXERROR(EINVAL);
    }

    (*filter)->dll_handle = dll_handle;
    return 0;
}

void px_filter_free(PXFilter** filter) {
    if (!filter || !*filter)
        return;

    if ((*filter)->dll_handle)
        px_dll_close((*filter)->dll_handle);
    px_free(filter);
}

PXFilterContext* px_filter_ctx_alloc(void) {
    PXFilterContext* ctx = calloc(1, sizeof *ctx);
    if (!ctx)
        px_oom_msg(sizeof *ctx);
    return ctx;
}

static char* get_dll_path(const char* filter_dir, const char* filter_name) {
    assert(filter_name);
    if (!filter_dir)
        filter_dir = ".";

    size_t dll_path_len =
        strlen(filter_dir) + strlen(PX_PATH_SEP) + strlen(filter_name) + strlen(DLL_EXT) + 1;
    char* dll_path = malloc(dll_path_len);
    if (!dll_path) {
        px_oom_msg(dll_path_len);
        return NULL;
    }

    sprintf(dll_path, "%s" PX_PATH_SEP "%s%s", filter_dir, filter_name, DLL_EXT);
    return dll_path;
}

int px_filter_ctx_new(PXFilterContext** ctx, const PXSettings* settings) {
    int ret = 0;

    *ctx = px_filter_ctx_alloc();
    if (!*ctx)
        return PXERROR(ENOMEM);

    PXFilterContext* pctx = *ctx;
    pctx->n_filters = settings->n_filters;

    pctx->filters = calloc((size_t)pctx->n_filters, sizeof(PXFilter*));
    if (!pctx->filters) {
        px_oom_msg((size_t)pctx->n_filters * sizeof(PXFilter*));
        goto fail;
    }

    pctx->filter_opts = calloc((size_t)pctx->n_filters, sizeof *pctx->filter_opts);
    if (!pctx->filter_opts) {
        px_oom_msg((size_t)pctx->n_filters * sizeof *pctx->filter_opts);
        goto fail;
    }

    for (int i = 0; i < pctx->n_filters; i++) {
        char* dll_path = get_dll_path(settings->filter_dir, settings->filter_names[i]);
        if (!dll_path) {
            ret = PXERROR(ENOMEM);
            goto fail;
        }
        ret = px_filter_from_dll(&pctx->filters[i], dll_path);
        px_free(&dll_path);
        if (ret < 0)
            goto fail;

        if (settings->filter_opts[i]) {
            ret = px_map_parse(&pctx->filter_opts[i], settings->filter_opts[i]);
            if (ret < 0)
                goto fail;
        }

        if (pctx->filters[i]->init) {
            ret = pctx->filters[i]->init(pctx->filters[i], &pctx->filter_opts[i]);
            if (ret < 0) {
                px_log(PX_LOG_ERROR, "Failed to initialize filter \"%s\"\n", pctx->filters[i]->name);
                goto fail;
            }
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
        if (pctx->filters[i] && pctx->filters[i]->free)
            pctx->filters[i]->free(pctx->filters[i]);

        px_filter_free(&pctx->filters[i]);
        px_map_free(&pctx->filter_opts[i]);
    }

    px_free(pctx->filters);
    px_free(&pctx->filter_opts);
    px_free(ctx);
}
