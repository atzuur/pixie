#include "internals.h"

#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
#include <pixie/frame.h>
#include <pixie/coding.h>
#include <pixie/util/utils.h>

#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>

#include <stdlib.h>

PXFrame* px_frame_alloc(void) {
    PXFrame* frame = calloc(1, sizeof *frame);
    if (!frame)
        px_oom_msg(sizeof *frame);
    return frame;
}

int px_frame_alloc_bufs(PXFrame* frame) {
    size_t frame_sz = px_frame_size(frame);
    uint8_t* data = px_aligned_alloc(32, frame_sz); // todo: platform dependent alignment
    if (!data) {
        px_oom_msg(frame_sz);
        return PXERROR(ENOMEM);
    }

    frame->planes[0].data = data;
    for (int i = 1; i < frame->n_planes; i++) {
        frame->planes[i].data = frame->planes[i - 1].data + px_plane_size(frame, i - 1);
    }

    return 0;
}

int px_frame_init(PXFrame* frame, int width, int height, PXPixelFormat pix_fmt, const int* strides) {
    assert(width > 0 && height > 0);
    assert(pix_fmt > PX_PIX_FMT_NONE);

    frame->width = width;
    frame->height = height;
    frame->pix_fmt = pix_fmt;

    PXPixFmtDescriptor fmt_desc = px_pix_fmt_get_desc(frame->pix_fmt);
    frame->n_planes = fmt_desc.n_planes;
    frame->bytes_per_comp = fmt_desc.bytes_per_comp;

    int chroma_width = frame->width >> fmt_desc.log2_chroma[0];
    int chroma_height = frame->height >> fmt_desc.log2_chroma[1];

    for (int i = 0; i < frame->n_planes; i++) {
        frame->planes[i].width = i == 0 ? frame->width : chroma_width;
        frame->planes[i].height = i == 0 ? frame->height : chroma_height;

        frame->planes[i].stride =
            strides ? strides[i] : FFALIGN(frame->planes[i].width * frame->bytes_per_comp, 32);
        assert(frame->planes[i].stride >= frame->planes[i].width * frame->bytes_per_comp);
    }

    frame->av_pix_fmt = AV_PIX_FMT_NONE;

    return 0;
}

int px_frame_new(PXFrame** frame, int width, int height, PXPixelFormat pix_fmt, const int* strides) {
    *frame = px_frame_alloc();
    if (!*frame)
        return PXERROR(ENOMEM);

    PXFrame* pframe = *frame;
    int ret = px_frame_init(pframe, width, height, pix_fmt, strides);
    if (ret < 0)
        return ret;

    ret = px_frame_alloc_bufs(pframe);
    if (ret < 0)
        return ret;

    return 0;
}

void px_frame_free(PXFrame** frame) {
    if (!frame || !*frame)
        return;

    px_frame_free_internal(*frame);
    px_free(frame);
}

void px_frame_free_internal(PXFrame* frame) {
    px_aligned_free(frame->planes[0].data);
    frame->planes[0].data = NULL;
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
    assert(idx >= 0 && idx < frame->n_planes);
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

    if (fmt_desc->flags & AV_PIX_FMT_FLAG_BE) {
        pix_fmt = av_pix_fmt_swap_endianness(pix_fmt);
        fmt_desc = av_pix_fmt_desc_get(pix_fmt);
        if (!fmt_desc)
            return AV_PIX_FMT_NONE;
    }

    // already true planar: yuvXXXp*, gbr[a]p*, gray[a]*
    if (strncmp(fmt_desc->name, "yuv", 3) == 0 || strncmp(fmt_desc->name, "gbr", 3) == 0 ||
        strncmp(fmt_desc->name, "gray", 4) == 0 || strncmp(fmt_desc->name, "ya", 2) == 0)
        return pix_fmt;

    switch (pix_fmt) {
        // nvXX -> yuvXXXp*
        case AV_PIX_FMT_NV12:
        case AV_PIX_FMT_NV21:
            return AV_PIX_FMT_YUV420P;
        case AV_PIX_FMT_NV16:
            return AV_PIX_FMT_YUV422P;
        case AV_PIX_FMT_NV24:
        case AV_PIX_FMT_NV42:
            return AV_PIX_FMT_YUV444P;
        case AV_PIX_FMT_NV20LE:
        case AV_PIX_FMT_NV20BE:
            return AV_PIX_FMT_YUV422P10LE;

        // rgb[a] -> gbr[a]p*
        case AV_PIX_FMT_RGB24:
        case AV_PIX_FMT_BGR24:
        case AV_PIX_FMT_RGB8:
        case AV_PIX_FMT_BGR8:
        case AV_PIX_FMT_RGB4:
        case AV_PIX_FMT_BGR4:
        case AV_PIX_FMT_RGB4_BYTE:
        case AV_PIX_FMT_BGR4_BYTE:
        case AV_PIX_FMT_RGB565LE:
        case AV_PIX_FMT_RGB565BE:
        case AV_PIX_FMT_BGR565LE:
            return AV_PIX_FMT_GBRP;
        case AV_PIX_FMT_RGB48LE:
        case AV_PIX_FMT_RGB48BE:
        case AV_PIX_FMT_BGR48LE:
        case AV_PIX_FMT_BGR48BE:
            return AV_PIX_FMT_GBRP16LE;
        case AV_PIX_FMT_RGBF32LE:
        case AV_PIX_FMT_RGBF32BE:
            return AV_PIX_FMT_GBRPF32LE;
        case AV_PIX_FMT_RGBA:
        case AV_PIX_FMT_BGRA:
        case AV_PIX_FMT_ARGB:
        case AV_PIX_FMT_ABGR:
            return AV_PIX_FMT_GBRAP;
        case AV_PIX_FMT_RGBA64LE:
        case AV_PIX_FMT_RGBA64BE:
        case AV_PIX_FMT_BGRA64LE:
        case AV_PIX_FMT_BGRA64BE:
            return AV_PIX_FMT_GBRAP16LE;
        case AV_PIX_FMT_RGBAF16LE:
        case AV_PIX_FMT_RGBAF16BE:
        case AV_PIX_FMT_RGBAF32LE:
        case AV_PIX_FMT_RGBAF32BE:
            return AV_PIX_FMT_GBRAPF32LE;

        // misc YCbCr -> yuvXXXp*
        case AV_PIX_FMT_P010LE:
        case AV_PIX_FMT_P010BE:
            return AV_PIX_FMT_YUV420P10LE;
        case AV_PIX_FMT_P012LE:
        case AV_PIX_FMT_P012BE:
            return AV_PIX_FMT_YUV420P12LE;
        case AV_PIX_FMT_P016LE:
        case AV_PIX_FMT_P016BE:
            return AV_PIX_FMT_YUV420P16LE;
        case AV_PIX_FMT_YVYU422:
        case AV_PIX_FMT_YUYV422:
        case AV_PIX_FMT_UYVY422:
            return AV_PIX_FMT_YUV422P;
        case AV_PIX_FMT_Y210LE:
        case AV_PIX_FMT_Y210BE:
        case AV_PIX_FMT_P210LE:
        case AV_PIX_FMT_P210BE:
            return AV_PIX_FMT_YUV422P10LE;
        case AV_PIX_FMT_P212LE:
        case AV_PIX_FMT_P212BE:
            return AV_PIX_FMT_YUV422P12LE;
        case AV_PIX_FMT_P216LE:
        case AV_PIX_FMT_P216BE:
            return AV_PIX_FMT_YUV422P16LE;
        case AV_PIX_FMT_P410LE:
        case AV_PIX_FMT_P410BE:
            return AV_PIX_FMT_YUV444P10LE;
        case AV_PIX_FMT_P412LE:
        case AV_PIX_FMT_P412BE:
            return AV_PIX_FMT_YUV444P12LE;
        case AV_PIX_FMT_P416LE:
        case AV_PIX_FMT_P416BE:
            return AV_PIX_FMT_YUV444P16LE;
        case AV_PIX_FMT_AYUV64LE:
        case AV_PIX_FMT_AYUV64BE:
            return AV_PIX_FMT_YUVA444P16LE;
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

    return PX_PIX_FMT_MAKE_TAG(color_model, av_fmt_desc->nb_components, comp_type, av_fmt_desc->comp[0].depth,
                               av_fmt_desc->log2_chroma_w, av_fmt_desc->log2_chroma_h);
}

static void array_abs(int* dest, const int* arr, size_t len) {
    for (size_t i = 0; i < len; i++) {
        dest[i] = abs(arr[i]);
    }
}

int px_frame_from_av(PXFrame* dest, const AVFrame* src) {
    enum AVPixelFormat planar_equiv = get_planar_equivalent(src->format);
    if (planar_equiv == AV_PIX_FMT_NONE) {
        const char* fmt_name = av_get_pix_fmt_name(src->format);
        px_log(PX_LOG_ERROR, "Unsupported pixel format \"%s\"\n", fmt_name ? fmt_name : "(unknown)");
        return PXERROR(EINVAL);
    }

    int src_n_planes = av_pix_fmt_count_planes(src->format);
    int abs_src_linesize[AV_NUM_DATA_POINTERS] = {0};
    array_abs(abs_src_linesize, src->linesize, (size_t)src_n_planes);

    int ret = px_frame_init(dest, src->width, src->height, px_pix_fmt_from_planar_av(planar_equiv),
                            planar_equiv == src->format ? abs_src_linesize : NULL);
    if (ret < 0)
        return ret;
    dest->av_pix_fmt = planar_equiv;

    ret = px_frame_alloc_bufs(dest);
    if (ret < 0)
        return ret;

    if (planar_equiv == src->format) {
        assert(src_n_planes == dest->n_planes);
        for (int i = 0; i < dest->n_planes; i++) {
            for (int y = 0; y < dest->planes[i].height; y++) {
                memcpy(dest->planes[i].data + y * dest->planes[i].stride, src->data[i] + y * src->linesize[i],
                       (size_t)(dest->planes[i].width * dest->bytes_per_comp));
            }
        }
        return 0;
    }

    px_log(PX_LOG_INFO, "Converting from %s to %s\n", av_get_pix_fmt_name(src->format),
           av_get_pix_fmt_name(planar_equiv));

    struct SwsContext* sws = sws_getContext(src->width, src->height, src->format, dest->width, dest->height,
                                            planar_equiv, SWS_BILINEAR, NULL, NULL, NULL);
    if (!sws) {
        LAV_THROW_MSG("sws_getContext", AVERROR(EINVAL));
        return AVERROR(EINVAL);
    }

    const uint8_t* const* src_data_decayed = (const uint8_t* const*)src->data;

    uint8_t* dest_plane_ptrs[PX_FRAME_MAX_PLANES] = {0};
    int strides[PX_FRAME_MAX_PLANES] = {0};
    for (int i = 0; i < dest->n_planes; i++) {
        dest_plane_ptrs[i] = dest->planes[i].data;
        strides[i] = dest->planes[i].stride;
    }

    sws_scale(sws, src_data_decayed, src->linesize, 0, src->height, dest_plane_ptrs, strides);
    sws_freeContext(sws);

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
