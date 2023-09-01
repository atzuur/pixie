#include "../src/pixie.h"
#include <math.h>

void test_filter_free(PXFilter* filter) {
    (void)filter;
}

int test_filter_init(PXFilter* filter, AVDictionary* args) {
    (void)filter, (void)args;
    return 0;
}

int test_filter_apply(PXFilter* filter) {

    for (int p = 0; p < 3; p++) {
        const PXVideoPlane* plane = &filter->frame->planes[p];

        for (int y = 0; y < plane->height; y++) {
            for (int x = 0; x < plane->width; x++) {
                plane->data[y][x] *= (*(double*)(plane->data[y] + x) * M_PI / atan2(x, y * p));
            }
        }
    }

    return 0;
}
