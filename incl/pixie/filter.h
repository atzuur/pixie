#include <pixie/frame.h>
#include <pixie/util/map.h>
#include <pixie/util/dll.h>

#define PX_FILTER_EXPORT_FUNC "pixie_export_filter"

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
    const PXMap* filter_opts;
    int n_filters;
} PXFilterContext;

PXFilter* px_filter_alloc(void);
int px_filter_from_dll(PXFilter** filter, const char* dll_path);
void px_filter_free(PXFilter** filter);

PXFilterContext* px_filter_ctx_alloc(void);
int px_filter_ctx_new(PXFilterContext** ctx, const char* filter_dir, const char* const* filter_names,
                      const PXMap* filter_opts, int n_filters);
void px_filter_ctx_free(PXFilterContext** ctx);
