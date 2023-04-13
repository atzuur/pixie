#include "frame.h"
#include "log.h"
#include "util/utils.h"
#include <libavutil/avutil.h>
#include <libavutil/pixdesc.h>

#include <stdlib.h>

// -1 if no luma present
static int get_luma_idx(AVPixFmtDescriptor desc) {
    return (desc.flags & AV_PIX_FMT_FLAG_RGB) ? -1 : 0;
}

PXFrame* px_frame_new(int width, int height, enum AVPixelFormat pix_fmt) {

    assert(width > 0), assert(height > 0);

    PXFrame* frame = calloc(1, sizeof(PXFrame));
    if (!frame) {
        oom(sizeof *frame);
        return NULL;
    }

    const AVPixFmtDescriptor* fmt_desc = av_pix_fmt_desc_get(pix_fmt);
    if (!fmt_desc) {
        px_log(PX_LOG_ERROR, "Invalid pixel format: %d\n", pix_fmt);
        goto fail;
    }

    // any pixel format that is not RGB or planar, endianness is irrelevant
    const int unsupported_fmts = ~(AV_PIX_FMT_FLAG_RGB | AV_PIX_FMT_FLAG_PLANAR | AV_PIX_FMT_FLAG_BE);
    if (fmt_desc->flags & unsupported_fmts) {
        px_log(PX_LOG_ERROR, "Unsupported pixel format: %d\n", pix_fmt);
        goto fail;
    }

    int n_planes = av_pix_fmt_count_planes(pix_fmt);
    frame->num_planes = n_planes;

    int bytes_per_comp = ceil_div(av_get_padded_bits_per_pixel(fmt_desc), CHAR_BIT);
    frame->bytes_per_comp = bytes_per_comp;

    int luma_idx = get_luma_idx(*fmt_desc);
    int chroma_width = AV_CEIL_RSHIFT(width, fmt_desc->log2_chroma_w);
    int chroma_height = AV_CEIL_RSHIFT(height, fmt_desc->log2_chroma_h);

    for (int i = 0; i < n_planes; i++) {

        frame->planes[i] = calloc(1, sizeof(PXVideoPlane));
        if (!frame->planes[i]) {
            oom(sizeof(PXVideoPlane));
            goto fail;
        }

        int plane_width = i == luma_idx ? width : chroma_width;
        int plane_height = i == luma_idx ? height : chroma_height;

        size_t plane_sz = plane_width * plane_height * bytes_per_comp;
        uint8_t* data = calloc(1, plane_sz);
        if (!data) {
            oom(plane_sz);
            goto fail;
        }

        frame->planes[i]->data_flat = data;

        frame->planes[i]->data = malloc(sizeof(uint8_t*) * plane_height);
        if (!frame->planes[i]->data) {
            oom(sizeof(uint8_t*) * plane_height);
            goto fail;
        }

        for (int line = 0; line < plane_height; line++) {
            size_t stride = plane_width * bytes_per_comp;
            frame->planes[i]->data[line] = data + stride * line;
        }

        frame->planes[i]->width = plane_width;
        frame->planes[i]->height = plane_height;
    }

    return frame;

fail:
    px_frame_free(&frame);
    return NULL;
}

void px_frame_free(PXFrame** frame) {

    PXFrame* fp = *frame;
    for (int i = 0; i < fp->num_planes; i++) {
        free_s(&fp->planes[i]->data);
        free_s(&fp->planes[i]->data_flat);
        free_s(&fp->planes[i]);
    }

    free_s(frame);
}

void px_frame_copy(PXFrame* dest, const PXFrame* src) {

    assert(src->pix_fmt == dest->pix_fmt);

    *dest = *src;

    for (int i = 0; i < src->num_planes; i++) {

        PXVideoPlane* dest_plane = dest->planes[i];
        const PXVideoPlane* src_plane = src->planes[i];

        size_t plane_sz = src_plane->width * src_plane->height * src->bytes_per_comp;
        memcpy(dest_plane->data_flat, src_plane->data_flat, plane_sz);

        *dest_plane = *src_plane;

        for (int line = 0; line < dest_plane->height; line++) {
            size_t stride = dest_plane->width * dest->bytes_per_comp;
            dest_plane->data[line] = src_plane->data_flat + stride * line;
        }
    }
}

size_t px_plane_size(const PXFrame* frame, int idx) {
    return frame->planes[idx]->width * frame->planes[idx]->height * frame->bytes_per_comp;
}

size_t px_frame_size(const PXFrame* frame) {

    size_t size = 0;
    for (int i = 0; i < frame->num_planes; i++) {
        size += px_plane_size(frame, i);
    }

    return size;
}

int px_fb_init(PXFrameBuffer* fb, int max_frames, int width, int height, enum AVPixelFormat pix_fmt) {

    assert(max_frames > 0);

    fb->frames = calloc(max_frames, sizeof(PXFrame*));
    if (!fb->frames) {
        oom(max_frames * sizeof(PXFrame*));
        return AVERROR(ENOMEM);
    }

    for (int i = 0; i < max_frames; i++) {
        fb->frames[i] = px_frame_new(width, height, pix_fmt);
        if (!fb->frames[i]) {
            px_fb_free(fb);
            return AVERROR(ENOMEM);
        }
    }

    fb->num_frames = 0;
    fb->max_frames = max_frames;

    return 0;
}

void px_fb_free(PXFrameBuffer* fb) {

    px_fb_clear(fb);
    free_s(&fb->frames);
}

int px_fb_add(PXFrameBuffer* fb, const PXFrame* frame) {

    if (fb->num_frames >= fb->max_frames) {
        px_log(PX_LOG_ERROR, "px_fb_add(): Frame buffer at %p is full (%d/%d)\n", fb, fb->num_frames,
               fb->max_frames);
        return AVERROR(ENOMEM);
    }

    PXFrame* new_frame = px_frame_new(frame->width, frame->height, frame->pix_fmt);
    if (!new_frame)
        return AVERROR(ENOMEM);

    px_frame_copy(new_frame, frame);

    fb->frames[fb->num_frames++] = new_frame;

    return 0;
}

void px_fb_clear(PXFrameBuffer* fb) {

    for (int i = 0; i < fb->num_frames; i++) {
        px_frame_free(&fb->frames[i]);
    }

    fb->num_frames = 0;
}
