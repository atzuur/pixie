#include "frame.h"
#include "log.h"
#include "util/utils.h"
#include <libavutil/avutil.h>
#include <libavutil/pixdesc.h>

#include <stdlib.h>

int px_frame_alloc_bufs(PXFrame* frame) {

    for (int i = 0; i < frame->n_planes; i++) {

        int plane_width = frame->planes[i].width;
        int plane_height = frame->planes[i].height;

        size_t plane_sz = plane_width * plane_height * frame->bytes_per_comp;
        uint8_t* data = calloc(1, plane_sz);
        if (!data) {
            oom(plane_sz);
            goto fail;
        }

        frame->planes[i].data = malloc(sizeof(uint8_t*) * plane_height);
        if (!frame->planes[i].data) {
            oom(sizeof(uint8_t*) * plane_height);
            goto fail;
        }

        for (int line = 0; line < plane_height; line++) {
            size_t stride = plane_width * frame->bytes_per_comp;
            frame->planes[i].data[line] = data + stride * line;
        }
    }

    return 0;

fail:
    px_frame_free(&frame);
    return AVERROR(ENOMEM);
}

int px_frame_init(PXFrame* frame) {

    if (frame->width <= 0 || frame->height <= 0) {
        $px_log(PX_LOG_ERROR, "Invalid frame dimensions: %dx%d\n", frame->width, frame->height);
        return AVERROR(EINVAL);
    }

    const AVPixFmtDescriptor* fmt_desc = av_pix_fmt_desc_get(frame->pix_fmt);
    if (!fmt_desc) {
        $px_log(PX_LOG_ERROR, "Unknown pixel format: %d\n", frame->pix_fmt);
        return AVERROR(EINVAL);
    }

    // any pixel format that is not planar is unsupported
    if (!(fmt_desc->flags & AV_PIX_FMT_FLAG_PLANAR)) {
        $px_log(PX_LOG_ERROR, "Unsupported pixel format: %s\n", av_get_pix_fmt_name(frame->pix_fmt));
        return AVERROR(EINVAL);
    }

    frame->n_planes = av_pix_fmt_count_planes(frame->pix_fmt);

    // all components have the same number of bits in planar formats
    frame->bits_per_comp = fmt_desc->comp[0].depth;
    frame->bytes_per_comp = ceil_div(frame->bits_per_comp, CHAR_BIT);

    int chroma_width = AV_CEIL_RSHIFT(frame->width, fmt_desc->log2_chroma_w);
    int chroma_height = AV_CEIL_RSHIFT(frame->height, fmt_desc->log2_chroma_h);

    for (int i = 0; i < frame->n_planes; i++) {
        frame->planes[i].width = i == 0 ? frame->width : chroma_width;
        frame->planes[i].height = i == 0 ? frame->height : chroma_height;
    }

    return 0;
}

PXFrame* px_frame_new(int width, int height, enum AVPixelFormat pix_fmt) {

    PXFrame* frame = calloc(1, sizeof(PXFrame));
    if (!frame) {
        oom(sizeof(PXFrame));
        return NULL;
    }

    frame->width = width;
    frame->height = height;
    frame->pix_fmt = pix_fmt;

    if (px_frame_init(frame) < 0) {
        goto fail;
    }

    if (px_frame_alloc_bufs(frame) < 0) {
        goto fail;
    }

    return frame;

fail:
    px_frame_free(&frame);
    return NULL;
}

void px_frame_free(PXFrame** frame) {

    PXFrame* fp = *frame;
    for (int i = 0; i < fp->n_planes; i++) {
        free_s(&fp->planes[i].data);
        free_s(&fp->planes[i].data[0]);
        free_s(&fp->planes[i]);
    }

    free_s(frame);
}

void px_frame_copy(PXFrame* dest, const PXFrame* src) {

    assert(src->pix_fmt == dest->pix_fmt);
    assert(src->width == dest->width);
    assert(src->height == dest->height);

    for (int i = 0; i < src->n_planes; i++) {

        PXVideoPlane* dest_plane = &dest->planes[i];
        const PXVideoPlane* src_plane = &src->planes[i];

        for (int line = 0; line < dest_plane->height; line++) {
            size_t line_sz = dest_plane->width * dest->bytes_per_comp;
            memcpy(dest_plane->data[line], src_plane->data[line], line_sz);
        }
    }

    dest->pts = src->pts;
    dest->timebase = src->timebase;
}

size_t px_plane_size(const PXFrame* frame, int idx) {
    return frame->planes[idx].width * frame->planes[idx].height * frame->bytes_per_comp;
}

size_t px_frame_size(const PXFrame* frame) {

    size_t size = 0;
    for (int i = 0; i < frame->n_planes; i++) {
        size += px_plane_size(frame, i);
    }

    return size;
}

int px_frame_from_av(PXFrame* dest, const AVFrame* avframe) {

    *dest = (PXFrame) {
        .width = avframe->width,
        .height = avframe->height,
        .pix_fmt = avframe->format,
        .pts = avframe->pts,
        .timebase = avframe->time_base,
    };

    px_frame_init(dest);

    for (int i = 0; i < dest->n_planes; i++) {

        dest->planes[i].data = malloc(sizeof(uint8_t*) * dest->planes[i].height);
        if (!dest->planes[i].data) {
            oom(sizeof(uint8_t*) * dest->planes[i].height);
            return AVERROR(ENOMEM);
        }

        for (int line = 0; line < dest->planes[i].height; line++) {
            dest->planes[i].data[line] = avframe->data[i] + avframe->linesize[i] * line;
        }
    }

    return 0;
}

void px_frame_assert_correctly_converted(const AVFrame* src, const PXFrame* dest) {

    assert(src->width == dest->width);
    assert(src->height == dest->height);
    assert(src->format == dest->pix_fmt);

    for (int i = 0; i < dest->n_planes; i++) {
        for (int line = 0; line < dest->planes[i].height; line++) {

            size_t av_stride = src->linesize[i];
            size_t actual_stride = dest->planes[i].width * dest->bytes_per_comp;

            uint8_t* dest_line = dest->planes[i].data[line];
            uint8_t* src_line = src->data[i] + av_stride * line;

            assert(memcmp(dest_line, src_line, actual_stride) == 0);
        }
    }
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
        $px_log(PX_LOG_ERROR, "Frame buffer at %p is full (%d/%d)\n", fb, fb->num_frames, fb->max_frames);
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
