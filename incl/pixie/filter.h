#include <pixie/frame.h>
#include <pixie/util/map.h>

typedef struct PXFilter {
    PXFrame* frame; // modified in-place
    int64_t frame_num;

    int (*init)(struct PXFilter* filter, const PXMap* args);
    int (*apply)(struct PXFilter* filter);
    void (*free)(struct PXFilter* filter);

    const char* name;
} PXFilter;

typedef struct PXFilterContext {
    PXFilter** filters;
    PXMap* filter_opts;
    int n_filters;
} PXFilterContext;

PXFilter* px_filter_alloc(void);
int px_filter_from_dll(PXFilter* filter, const char* filter_name);
void px_filter_free(PXFilter** filter);

PXFilterContext* px_filter_ctx_alloc(int n_filters);
int px_filter_ctx_new(PXFilterContext** ctx, const char* const* filter_names, const char* const* filter_opts,
                      int n_filters);
void px_filter_ctx_free(PXFilterContext** ctx);
