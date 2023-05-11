#include "frame.h"
#include "log.h"
#include "util/utils.h"
#include <libavutil/avutil.h>
#include <libavutil/pixdesc.h>

#include <stdlib.h>

PXFrame* px_frame_new(int width, int height, enum AVPixelFormat pix_fmt) {

    assert(width > 0), assert(height > 0);

    PXFrame* frame = calloc(1, sizeof(PXFrame));
    if (!frame) {
        oom(sizeof *frame);
        return NULL;
    }

    frame->width = width;
    frame->height = height;
    frame->pix_fmt = pix_fmt;

    frame->pts = 0;
    frame->timebase = (AVRational) {0, 1};

    const AVPixFmtDescriptor* fmt_desc = av_pix_fmt_desc_get(pix_fmt);
    if (!fmt_desc) {
        px_log(PX_LOG_ERROR, "Unknown pixel format: %d\n", pix_fmt);
        goto fail;
    }

    // any pixel format that is not planar is unsupported
    if (!(fmt_desc->flags & AV_PIX_FMT_FLAG_PLANAR)) {
        px_log(PX_LOG_ERROR, "Unsupported pixel format: %s\n", av_get_pix_fmt_name(pix_fmt));
        goto fail;
    }

    int n_planes = av_pix_fmt_count_planes(pix_fmt);
    frame->num_planes = n_planes;

    // all components have the same number of bits in planar formats
    int bits_per_comp = fmt_desc->comp[0].depth;
    int bytes_per_comp = ceil_div(bits_per_comp, CHAR_BIT);
    frame->bytes_per_comp = bytes_per_comp;
    frame->bits_per_comp = bits_per_comp;

    int chroma_width = AV_CEIL_RSHIFT(width, fmt_desc->log2_chroma_w);
    int chroma_height = AV_CEIL_RSHIFT(height, fmt_desc->log2_chroma_h);

    for (int i = 0; i < n_planes; i++) {

        frame->planes[i] = calloc(1, sizeof(PXVideoPlane));
        if (!frame->planes[i]) {
            oom(sizeof(PXVideoPlane));
            goto fail;
        }

        // luma is 0
        int plane_width = i == 0 ? width : chroma_width;
        int plane_height = i == 0 ? height : chroma_height;

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

PXFrame* px_frame_from_av(const AVFrame* avframe) {

    PXFrame* frame = px_frame_new(avframe->width, avframe->height, avframe->format);
    if (!frame) {
        return NULL;
    }

    for (int i = 0; i < frame->num_planes; i++) {
        for (int line = 0; line < frame->planes[i]->height; line++) {

            size_t av_stride = avframe->linesize[i];
            size_t stride = frame->planes[i]->width * frame->bytes_per_comp;

            uint8_t* dest = frame->planes[i]->data_flat + stride * line;
            uint8_t* src = avframe->data[i] + av_stride * line;

            memcpy(dest, src, stride);
        }
    }

    frame->pts = avframe->pts;
    frame->timebase = avframe->time_base;

    return frame;
}

bool px_frame_assert_correctly_converted(const AVFrame* src, const PXFrame* dest) {

    assert(src->width == dest->width);
    assert(src->height == dest->height);
    assert(src->format == dest->pix_fmt);

    for (int i = 0; i < dest->num_planes; i++) {
        for (int line = 0; line < dest->planes[i]->height; line++) {

            size_t av_stride = src->linesize[i];
            size_t stride = dest->planes[i]->width * dest->bytes_per_comp;

            uint8_t* dest_line = dest->planes[i]->data_flat + stride * line;
            uint8_t* src_line = src->data[i] + av_stride * line;

            assert(memcmp(dest_line, src_line, stride) == 0);
        }
    }

    return true;
}

int px_fb_init(PXFrameBuffer* fb, int width, int height, enum AVPixelFormat pix_fmt) {

    assert(fb->max_frames > 0);

    fb->frames = calloc(fb->max_frames, sizeof(PXFrame*));
    if (!fb->frames) {
        oom(fb->max_frames * sizeof(PXFrame*));
        return AVERROR(ENOMEM);
    }

    for (int i = 0; i < fb->max_frames; i++) {
        fb->frames[i] = px_frame_new(width, height, pix_fmt);
        if (!fb->frames[i]) {
            px_fb_free(fb);
            return AVERROR(ENOMEM);
        }
    }

    fb->num_frames = 0;
    fb->max_frames = fb->max_frames;

    return 0;
}

void px_fb_free(PXFrameBuffer* fb) {

    px_fb_clear(fb);
    free_s(&fb->frames);
}

int px_fb_add(PXFrameBuffer* fb, const PXFrame* frame) {

    if (fb->num_frames >= fb->max_frames) {
        px_log(PX_LOG_ERROR, "Frame buffer at %p is full (%d/%d)\n", fb, fb->num_frames, fb->max_frames);
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
