#include "frame.h"
#include <libavutil/dict.h>

typedef struct PXFilter {

    PXFrame* frame; // modified in-place
    int frame_num;

    int (*init)(struct PXFilter* filter, AVDictionary* args);
    int (*apply)(struct PXFilter* filter);
    void (*free)(struct PXFilter* filter);

    char* name;

} PXFilter;

typedef struct PXFilterContext {

    PXFilter* filters;
    int n_filters;

} PXFilterContext;