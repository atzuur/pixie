#include "../src/pixie.h"

void test_filter_free() {}

int test_filter_init() {
    return 0;
}

int test_filter_apply(PXFilter* filter) {

    for (int p = 0; p < 3; p++) {
        const PXVideoPlane* plane = &filter->frame->planes[p];

        for (int y = 0; y < plane->height; y++) {
            for (int x = 0; x < plane->width; x++) {
                plane->data[y][x] = ~plane->data[y][x];
            }
        }
    }

    return 0;
}
