#pragma once

#include <pixie/pixfmt.h>

#include <stddef.h>
#include <stdint.h>

#define PX_FRAME_MAX_PLANES 4

typedef struct PXVideoPlane {
    int width;
    int height;

    uint8_t* data;
    int stride;
} PXVideoPlane;

typedef struct PXFrame {
    // y+u+v / r+g+b / gray [+a]
    PXVideoPlane planes[PX_FRAME_MAX_PLANES];
    int n_planes;

    int width;
    int height;

    int bytes_per_comp;
    PXPixelFormat pix_fmt;

    // AVPixelFormat enum value, only used internally for conversions
    int av_pix_fmt;
} PXFrame;

typedef struct PXFrameBuffer {
    PXFrame** frames;
    int num_frames;
    int max_frames;
} PXFrameBuffer;

PXFrame* px_frame_alloc(void);

int px_frame_init(PXFrame* frame, int width, int height, PXPixelFormat pix_fmt, const int* strides);
int px_frame_alloc_bufs(PXFrame* frame);

int px_frame_new(PXFrame** frame, int width, int height, PXPixelFormat pix_fmt, const int* strides);
void px_frame_free(PXFrame** frame);

void px_frame_copy(PXFrame* dest, const PXFrame* src);

size_t px_plane_size(const PXFrame* frame, int idx);
size_t px_frame_size(const PXFrame* frame);

int px_fb_init(PXFrameBuffer* fb, int width, int height, PXPixelFormat pix_fmt);
void px_fb_free(PXFrameBuffer* fb);

int px_fb_add(PXFrameBuffer* fb, const PXFrame* frame);
void px_fb_clear(PXFrameBuffer* fb);
