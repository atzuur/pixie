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

    PXFilter* filters;
    int n_filters;

} PXFilterContext;
