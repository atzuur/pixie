#include <pixie/pixie.h>
#include <pixie/log.h>
#include <pixie/util/map.h>

#include <errno.h>

typedef struct TestFilterOptions {
    int foo;
    bool bar;
} TestFilterOptions;

void test_filter_free(PXFilter* filter) {
    free(filter->user_data);
}

int test_filter_init(PXFilter* filter, const PXMap* args) {
    TestFilterOptions* opts = calloc(1, sizeof *opts);
    if (!opts)
        return PXERROR(ENOMEM);

    int ret = px_map_get_int(args, &opts->foo, "foo");
    if (ret == PXERROR(ENOENT)) {
        px_log(PX_LOG_ERROR, "Missing required argument \"foo\"\n");
        return PXERROR(EINVAL);
    } else if (ret < 0) {
        return ret;
    }

    ret = px_map_get_bool(args, &opts->bar, "bar");
    if (ret == PXERROR(ENOENT)) {
        opts->bar = true; // default value
    } else if (ret < 0) {
        return ret;
    }

    filter->user_data = opts;
    return 0;
}

int test_filter_apply(PXFilter* filter) {
    TestFilterOptions* opts = filter->user_data;

    for (int p = 0; p < filter->in_frame->n_planes; p++) {
        const PXVideoPlane* in_plane = &filter->in_frame->planes[p];
        PXVideoPlane* out_plane = &filter->out_frame->planes[p];

        for (int y = 0; y < in_plane->height; y++) {
            for (int x = 0; x < in_plane->width; x++) {
                uint8_t pixel = in_plane->data[y * in_plane->stride + x];
                if (opts->bar) {
                    pixel = ~pixel;
                }
                pixel += opts->foo;

                out_plane->data[y * out_plane->stride + x] = pixel;
            }
        }
    }

    return 0;
}

PXFilter* pixie_export_filter(void) {
    PXFilter* filter = px_filter_alloc();
    if (!filter) {
        return NULL;
    }

    *filter = (PXFilter) {
        .name = "test",
        .init = test_filter_init,
        .apply = test_filter_apply,
        .free = test_filter_free,
    };

    return filter;
}
