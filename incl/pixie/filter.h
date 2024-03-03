#include <pixie/frame.h>
#include <pixie/settings.h>
#include <pixie/util/map.h>
#include <pixie/util/dll.h>

#define PX_FILTER_EXPORT_FUNC "pixie_export_filter"

#define PX_FILTER_OPT_FLAG_REQUIRED (1 << 0)

typedef struct PXFilter {
    const PXFrame* in_frame;
    PXFrame* out_frame;
    uint64_t frame_num;

    void* user_data;

    int (*init)(struct PXFilter* filter, const PXMap* args);
    int (*apply)(struct PXFilter* filter);
    void (*free)(struct PXFilter* filter);

    const char* name;

    void* dll_handle;
} PXFilter;

typedef struct PXFilterContext {
    PXFilter** filters;
    PXMap* filter_opts;
    int n_filters;
} PXFilterContext;

PXFilter* px_filter_alloc(void);
int px_filter_from_dll(PXFilter** filter, const char* dll_path);
void px_filter_free(PXFilter** filter);

PXFilterContext* px_filter_ctx_alloc(void);
int px_filter_ctx_new(PXFilterContext** ctx, const PXSettings* settings);
void px_filter_ctx_free(PXFilterContext** ctx);
