#include <libavutil/pixfmt.h>
#include <libavutil/rational.h>

typedef struct PXVideoPlane {
    uint8_t** data;
    int width;
    int height;
} PXVideoPlane;

typedef struct PXFrame {

    PXVideoPlane* planes[4]; // 4 planes max, unused planes are NULL
    int num_planes;
    enum AVPixelFormat pix_fmt;

    int width;
    int height;

    int64_t pts;
    AVRational timebase;

} PXFrame;

typedef struct PXFrameBuffer {

    PXFrame* frames;
    int num_frames;
    int max_frames;

} PXFrameBuffer;
