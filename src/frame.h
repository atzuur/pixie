#include <libavutil/pixfmt.h>
#include <libavutil/rational.h>
#include <stddef.h>

typedef struct PXVideoPlane {

    // `height` pointers to each line
    uint8_t** data;

    // contiguous array of data
    uint8_t* data_flat;

    int width;
    int height;

} PXVideoPlane;

typedef struct PXFrame {

    PXVideoPlane* planes[4]; // 4 planes max
    int num_planes;
    enum AVPixelFormat pix_fmt;

    int width;
    int height;

    // bytes per pixel component, e.g. 1 for 8-bit, 2 for 16-bit
    // padding is included (10-bit will be 2)
    size_t bytes_per_comp;

    int64_t pts;
    AVRational timebase;

} PXFrame;

typedef struct PXFrameBuffer {

    PXFrame** frames;
    int num_frames;
    int max_frames;

} PXFrameBuffer;

PXFrame* px_frame_new(int width, int height, enum AVPixelFormat pix_fmt);
void px_frame_free(PXFrame** frame);

void px_frame_copy(PXFrame* dest, const PXFrame* src);

size_t px_plane_size(const PXFrame* frame, int idx);
size_t px_frame_size(const PXFrame* frame);

int px_fb_init(PXFrameBuffer* fb, int max_frames, int width, int height, enum AVPixelFormat pix_fmt);
void px_fb_free(PXFrameBuffer* fb);

int px_fb_add(PXFrameBuffer* fb, const PXFrame* frame);
void px_fb_clear(PXFrameBuffer* fb);
