#include <pixie/pixie.h>

void test_filter_free(PXFilter* filter) {
    (void)filter;
}

int test_filter_init(PXFilter* filter, const PXMap* args) {
    (void)filter, (void)args;
    return 0;
}

int test_filter_apply(PXFilter* filter) {
    (void)filter;
    // for (int p = 0; p < 3; p++) {
    //     const PXVideoPlane* plane = &filter->frame->planes[p];
    //     for (int y = 0; y < plane->height; y++) {
    //         for (int x = 0; x < plane->width * filter->frame->bytes_per_comp; x++) {
    //             uint8_t* pixel = &plane->data[y * plane->stride + x];
    //             *pixel = ~*pixel;
    //         }
    //     }
    // }

    return 0;
}
