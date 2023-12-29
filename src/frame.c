#include "util/utils.h"

#include <pixie/frame.h>
#include <pixie/coding.h>

#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>

#include <stdlib.h>

int px_frame_alloc_bufs(PXFrame* frame) {
    size_t frame_sz = px_frame_size(frame);
    uint8_t* data = aligned_alloc(32, frame_sz); // todo: platform dependent alignment
    if (!data) {
        oom(frame_sz);
        px_frame_free(&frame);
        return AVERROR(ENOMEM);
    }

    frame->planes[0].data = data;
    for (int i = 1; i < frame->n_planes; i++) {
        frame->planes[i].data = frame->planes[i - 1].data + px_plane_size(frame, i - 1);
    }

    return 0;
}

int px_frame_init(PXFrame* frame) {
    if (frame->width <= 0 || frame->height <= 0) {
        $px_log(PX_LOG_ERROR, "Invalid frame dimensions: %dx%d\n", frame->width, frame->height);
        return AVERROR(EINVAL);
    }

    PXPixFmtDescriptor fmt_desc;
    px_pix_fmt_get_desc(&fmt_desc, frame->pix_fmt);

    frame->n_planes = fmt_desc.n_planes;
    frame->bytes_per_comp = fmt_desc.bytes_per_comp;

    int chroma_width = frame->width >> fmt_desc.log2_chroma[0];
    int chroma_height = frame->height >> fmt_desc.log2_chroma[1];

    for (int i = 0; i < frame->n_planes; i++) {
        frame->planes[i].width = i == 0 ? frame->width : chroma_width;
        frame->planes[i].height = i == 0 ? frame->height : chroma_height;
    }

    return 0;
}

int px_frame_new(PXFrame* frame, int width, int height, PXPixelFormat pix_fmt,
                 int stride[static PX_FRAME_MAX_PLANES]) {
    *frame = (PXFrame) {
        .width = width,
        .height = height,
        .pix_fmt = pix_fmt,
    };

    int ret = px_frame_init(frame);
    if (ret < 0)
        return ret;

    for (int i = 0; i < frame->n_planes; i++) {
        frame->planes[i].stride = stride[i] ? stride[i] : frame->planes[i].width * frame->bytes_per_comp;
    }

    ret = px_frame_alloc_bufs(frame);
    if (ret < 0)
        return ret;

    return 0;
}

void px_frame_free(PXFrame** frame) {
    free_s((*frame)->planes[0].data);
    free_s(frame);
}

void px_frame_copy(PXFrame* dest, const PXFrame* src) {

    assert(src->pix_fmt == dest->pix_fmt);
    assert(src->width == dest->width);
    assert(src->height == dest->height);

    for (int i = 0; i < src->n_planes; i++) {
        assert(src->planes[i].stride == dest->planes[i].stride);
    }

    memcpy(dest->planes[0].data, src->planes[0].data, px_frame_size(src));
}

size_t px_plane_size(const PXFrame* frame, int idx) {
    return (size_t)(frame->planes[idx].stride * frame->planes[idx].height);
}

size_t px_frame_size(const PXFrame* frame) {
    size_t size = 0;
    for (int i = 0; i < frame->n_planes; i++) {
        size += px_plane_size(frame, i);
    }

    return size;
}

static enum AVPixelFormat get_planar_equivalent(enum AVPixelFormat pix_fmt) {
    const AVPixFmtDescriptor* fmt_desc = av_pix_fmt_desc_get(pix_fmt);
    if (!fmt_desc)
        return AV_PIX_FMT_NONE;

    // yuvXXXp*, gbr[a]p*, gray[a]*
    if (strncmp(fmt_desc->name, "yuv", 3) == 0 || strncmp(fmt_desc->name, "gbr", 3) == 0 ||
        strncmp(fmt_desc->name, "gray", 4) == 0 || strncmp(fmt_desc->name, "ya", 2) == 0)
        return pix_fmt;

    // nvXX
    switch (pix_fmt) {
        case AV_PIX_FMT_NV12:
        case AV_PIX_FMT_NV21:
            return AV_PIX_FMT_YUV420P;
        case AV_PIX_FMT_NV16:
            return AV_PIX_FMT_YUV422P;
        case AV_PIX_FMT_NV24:
        case AV_PIX_FMT_NV42:
            return AV_PIX_FMT_YUV444P;
        case AV_PIX_FMT_NV20:
            return AV_PIX_FMT_YUV422P10;
        default:
            break;
    }

    // convert rgb formats to gbr[a]p*
    if (fmt_desc->flags & AV_PIX_FMT_FLAG_RGB) {
        switch (pix_fmt) {
            case AV_PIX_FMT_RGB24:
            case AV_PIX_FMT_BGR24:
            case AV_PIX_FMT_RGB8:
            case AV_PIX_FMT_BGR8:
            case AV_PIX_FMT_RGB4:
            case AV_PIX_FMT_BGR4:
            case AV_PIX_FMT_RGB4_BYTE:
            case AV_PIX_FMT_BGR4_BYTE:
            case AV_PIX_FMT_RGB565:
            case AV_PIX_FMT_BGR565:
                return AV_PIX_FMT_GBRP;
            case AV_PIX_FMT_RGB48:
            case AV_PIX_FMT_BGR48:
                return AV_PIX_FMT_GBRP16;
            case AV_PIX_FMT_RGBF32:
                return AV_PIX_FMT_GBRPF32;
            case AV_PIX_FMT_RGBA:
            case AV_PIX_FMT_BGRA:
            case AV_PIX_FMT_ARGB:
            case AV_PIX_FMT_ABGR:
                return AV_PIX_FMT_GBRAP;
            case AV_PIX_FMT_RGBA64:
            case AV_PIX_FMT_BGRA64:
                return AV_PIX_FMT_GBRAP16;
            case AV_PIX_FMT_RGBAF16:
            case AV_PIX_FMT_RGBAF32:
                return AV_PIX_FMT_GBRAPF32;
            default:
                break;
        }
    }

    // misc
    switch (pix_fmt) {
        case AV_PIX_FMT_P010:
            return AV_PIX_FMT_YUV420P10;
        case AV_PIX_FMT_P012:
            return AV_PIX_FMT_YUV420P12;
        case AV_PIX_FMT_P016:
            return AV_PIX_FMT_YUV420P16;
        case AV_PIX_FMT_YVYU422:
        case AV_PIX_FMT_YUYV422:
        case AV_PIX_FMT_UYVY422:
        case AV_PIX_FMT_Y210:
        case AV_PIX_FMT_P210:
        // case AV_PIX_FMT_P212: // will be added once it makes it into a release
        case AV_PIX_FMT_P216:
            return AV_PIX_FMT_YUV422P;
        case AV_PIX_FMT_P410:
        // case AV_PIX_FMT_P412: // will be added once it makes it into a release
        case AV_PIX_FMT_P416:
            return AV_PIX_FMT_YUV444P;
        case AV_PIX_FMT_AYUV64:
            return AV_PIX_FMT_YUVA444P;
        default:
            return AV_PIX_FMT_NONE;
    }
}

// only valid for formats returned by get_planar_equivalent()
static PXPixelFormat px_pix_fmt_from_planar_av(enum AVPixelFormat av_fmt) {
    const AVPixFmtDescriptor* av_fmt_desc = av_pix_fmt_desc_get(av_fmt);
    if (!av_fmt_desc)
        return PX_PIX_FMT_NONE;

    PXComponentType comp_type =
        av_fmt_desc->flags & AV_PIX_FMT_FLAG_FLOAT ? PX_COMP_TYPE_FLOAT : PX_COMP_TYPE_INT;

    PXColorModel color_model =
        av_fmt_desc->flags & AV_PIX_FMT_FLAG_RGB
            ? PX_COLOR_MODEL_RGB
            : (av_fmt_desc->nb_components >= 3 ? PX_COLOR_MODEL_YUV : PX_COLOR_MODEL_GRAY);

    return $px_make_pix_fmt_tag(color_model, av_fmt_desc->nb_components, comp_type,
                                av_fmt_desc->comp[0].depth, av_fmt_desc->log2_chroma_w,
                                av_fmt_desc->log2_chroma_h);
}

static void px_frame_assert_correctly_converted(const AVFrame* src, const PXFrame* dest) {
    assert(src->width == dest->width);
    assert(src->height == dest->height);
    assert(src->format == dest->av_pix_fmt);

    const AVPixFmtDescriptor* av_fmt_desc = av_pix_fmt_desc_get(src->format);
    assert(av_fmt_desc);

    PXPixFmtDescriptor px_fmt_desc;
    px_pix_fmt_get_desc(&px_fmt_desc, dest->pix_fmt);

    assert(px_fmt_desc.n_planes == av_fmt_desc->nb_components);
    assert(px_fmt_desc.bits_per_comp == av_fmt_desc->comp[0].depth);
    assert(px_fmt_desc.log2_chroma[0] == av_fmt_desc->log2_chroma_w);
    assert(px_fmt_desc.log2_chroma[1] == av_fmt_desc->log2_chroma_h);

    for (int i = 0; i < dest->n_planes; i++) {
        assert(src->linesize[i] == dest->planes[i].stride);
        assert(memcmp(dest->planes[i].data, src->data[i], px_plane_size(dest, i)) == 0);
    }
}

int px_frame_from_av(PXFrame* dest, const AVFrame* av_frame) {
    enum AVPixelFormat planar_equiv = get_planar_equivalent(av_frame->format);
    if (planar_equiv == AV_PIX_FMT_NONE) {
        const char* fmt_name = av_get_pix_fmt_name(av_frame->format);
        $px_log(PX_LOG_ERROR, "Unsupported pixel format %s\n", fmt_name ? fmt_name : "(unknown)");
        return AVERROR(EINVAL);
    }

    *dest = (PXFrame) {
        .width = av_frame->width,
        .height = av_frame->height,
        .pix_fmt = px_pix_fmt_from_planar_av(planar_equiv),
    };

    int ret = px_frame_init(dest);
    if (ret < 0)
        return ret;

    int strides[PX_FRAME_MAX_PLANES] = {0};
    for (int i = 0; i < PX_FRAME_MAX_PLANES; i++) {
        dest->planes[i].stride = strides[i] = planar_equiv == av_frame->format
                                                  ? av_frame->linesize[i]
                                                  : dest->planes[i].width * dest->bytes_per_comp;
    }

    ret = px_frame_alloc_bufs(dest);
    if (ret < 0)
        return ret;

    dest->av_pix_fmt = planar_equiv;

    if (planar_equiv != av_frame->format) {
        $px_log(PX_LOG_INFO, "Converting from %s to %s\n", av_get_pix_fmt_name(av_frame->format),
                av_get_pix_fmt_name(planar_equiv));

        struct SwsContext* sws =
            sws_getContext(av_frame->width, av_frame->height, av_frame->format, dest->width, dest->height,
                           planar_equiv, SWS_BILINEAR, NULL, NULL, NULL);
        if (!sws) {
            $px_lav_throw_msg("sws_getContext", AVERROR(EINVAL));
            return AVERROR(EINVAL);
        }

        const uint8_t* const* src_data_decayed = (const uint8_t* const*)av_frame->data;
        uint8_t* dest_plane_ptrs[PX_FRAME_MAX_PLANES] = {0};
        for (int i = 0; i < dest->n_planes; i++) {
            dest_plane_ptrs[i] = dest->planes[i].data;
        }
        sws_scale(sws, src_data_decayed, av_frame->linesize, 0, av_frame->height, dest_plane_ptrs, strides);

    } else {
        for (int i = 0; i < dest->n_planes; i++) {
            memcpy(dest->planes[i].data, av_frame->data[i], px_plane_size(dest, i));
        }
        px_frame_assert_correctly_converted(av_frame, dest);
    }

    return 0;
}

void px_frame_to_av(AVFrame* dest, const PXFrame* px_frame) {
    for (size_t i = 0; i < FF_ARRAY_ELEMS(dest->buf); i++) {
        av_buffer_unref(&dest->buf[i]);
    }

    dest->width = px_frame->width;
    dest->height = px_frame->height;
    dest->format = px_frame->av_pix_fmt;

    for (int i = 0; i < px_frame->n_planes; i++) {
        dest->linesize[i] = px_frame->planes[i].stride;
        dest->data[i] = px_frame->planes[i].data;
    }
}

// int px_fb_init(PXFrameBuffer* fb, int width, int height, PXPixelFormat pix_fmt) {
//     assert(fb->max_frames > 0);

//     fb->frames = calloc(fb->max_frames, sizeof(PXFrame*));
//     if (!fb->frames) {
//         oom(fb->max_frames * sizeof(PXFrame*));
//         return AVERROR(ENOMEM);
//     }

//     for (int i = 0; i < fb->max_frames; i++) {
//         fb->frames[i] = px_frame_new(width, height, pix_fmt, NULL);
//         if (!fb->frames[i]) {
//             px_fb_free(fb);
//             return AVERROR(ENOMEM);
//         }
//     }

//     fb->num_frames = 0;
//     fb->max_frames = fb->max_frames;

//     return 0;
// }

// void px_fb_free(PXFrameBuffer* fb) {
//     px_fb_clear(fb);
//     free_s(&fb->frames);
// }

// int px_fb_add(PXFrameBuffer* fb, const PXFrame* frame) {
//     if (fb->num_frames >= fb->max_frames) {
//         $px_log(PX_LOG_ERROR, "Frame buffer at %p is full (%d/%d)\n", fb, fb->num_frames,
//         fb->max_frames); return AVERROR(ENOMEM);
//     }

//     PXFrame* new_frame = px_frame_new(frame->width, frame->height, frame->pix_fmt);
//     if (!new_frame)
//         return AVERROR(ENOMEM);

//     px_frame_copy(new_frame, frame);

//     fb->frames[fb->num_frames++] = new_frame;

//     return 0;
// }

// void px_fb_clear(PXFrameBuffer* fb) {
//     for (int i = 0; i < fb->num_frames; i++) {
//         px_frame_free(&fb->frames[i]);
//     }

//     fb->num_frames = 0;
// }
