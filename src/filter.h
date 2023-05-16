#include "frame.h"
#include <libavutil/dict.h>

typedef struct PXFilter {

    PXFrame* frame; // modified in-place

    int (*init)(struct PXFilter* filter, AVDictionary* args);
    int (*apply)(struct PXFilter* filter);
    void (*free)(struct PXFilter* filter);

    char* name;

} PXFilter;