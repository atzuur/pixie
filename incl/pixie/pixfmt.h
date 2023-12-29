#define PX_MAX_PIX_FMT_NAME_LEN 16

#define $px_make_pix_fmt_tag(color_model, n_planes, comp_type, bits_per_comp, log2_chroma_w, log2_chroma_h) \
    ((color_model) << 0) | ((comp_type) << 4) | ((n_planes) << 8) | ((bits_per_comp) << 16) |               \
        ((log2_chroma_w) << 24) | ((log2_chroma_h) << 28)

typedef enum PXColorModel : int {

    PX_COLOR_MODEL_YUV,
    PX_COLOR_MODEL_RGB,
    PX_COLOR_MODEL_GRAY,

} PXColorModel;

typedef enum PXComponentType : int {

    PX_COMP_TYPE_INT,
    PX_COMP_TYPE_FLOAT,

} PXComponentType;

typedef enum PXPixelFormat : int {

    PX_PIX_FMT_NONE = 0,

    // 3 planes, y+u+v, 4:2:0 subsampling
    PX_PIX_FMT_YUV420P8 = $px_make_pix_fmt_tag(PX_COLOR_MODEL_YUV, 3, PX_COMP_TYPE_INT, 8, 1, 1),
    PX_PIX_FMT_YUV420P9 = $px_make_pix_fmt_tag(PX_COLOR_MODEL_YUV, 3, PX_COMP_TYPE_INT, 9, 1, 1),
    PX_PIX_FMT_YUV420P10 = $px_make_pix_fmt_tag(PX_COLOR_MODEL_YUV, 3, PX_COMP_TYPE_INT, 10, 1, 1),
    PX_PIX_FMT_YUV420P12 = $px_make_pix_fmt_tag(PX_COLOR_MODEL_YUV, 3, PX_COMP_TYPE_INT, 12, 1, 1),
    PX_PIX_FMT_YUV420P14 = $px_make_pix_fmt_tag(PX_COLOR_MODEL_YUV, 3, PX_COMP_TYPE_INT, 14, 1, 1),
    PX_PIX_FMT_YUV420P16 = $px_make_pix_fmt_tag(PX_COLOR_MODEL_YUV, 3, PX_COMP_TYPE_INT, 16, 1, 1),

    // 3 planes, y+u+v, 4:2:2 subsampling
    PX_PIX_FMT_YUV422P8 = $px_make_pix_fmt_tag(PX_COLOR_MODEL_YUV, 3, PX_COMP_TYPE_INT, 8, 1, 0),
    PX_PIX_FMT_YUV422P9 = $px_make_pix_fmt_tag(PX_COLOR_MODEL_YUV, 3, PX_COMP_TYPE_INT, 9, 1, 0),
    PX_PIX_FMT_YUV422P10 = $px_make_pix_fmt_tag(PX_COLOR_MODEL_YUV, 3, PX_COMP_TYPE_INT, 10, 1, 0),
    PX_PIX_FMT_YUV422P12 = $px_make_pix_fmt_tag(PX_COLOR_MODEL_YUV, 3, PX_COMP_TYPE_INT, 12, 1, 0),
    PX_PIX_FMT_YUV422P14 = $px_make_pix_fmt_tag(PX_COLOR_MODEL_YUV, 3, PX_COMP_TYPE_INT, 14, 1, 0),
    PX_PIX_FMT_YUV422P16 = $px_make_pix_fmt_tag(PX_COLOR_MODEL_YUV, 3, PX_COMP_TYPE_INT, 16, 1, 0),

    // 3 planes, y+u+v, no subsampling
    PX_PIX_FMT_YUV444P8 = $px_make_pix_fmt_tag(PX_COLOR_MODEL_YUV, 3, PX_COMP_TYPE_INT, 8, 0, 0),
    PX_PIX_FMT_YUV444P9 = $px_make_pix_fmt_tag(PX_COLOR_MODEL_YUV, 3, PX_COMP_TYPE_INT, 9, 0, 0),
    PX_PIX_FMT_YUV444P10 = $px_make_pix_fmt_tag(PX_COLOR_MODEL_YUV, 3, PX_COMP_TYPE_INT, 10, 0, 0),
    PX_PIX_FMT_YUV444P12 = $px_make_pix_fmt_tag(PX_COLOR_MODEL_YUV, 3, PX_COMP_TYPE_INT, 12, 0, 0),
    PX_PIX_FMT_YUV444P14 = $px_make_pix_fmt_tag(PX_COLOR_MODEL_YUV, 3, PX_COMP_TYPE_INT, 14, 0, 0),
    PX_PIX_FMT_YUV444P16 = $px_make_pix_fmt_tag(PX_COLOR_MODEL_YUV, 3, PX_COMP_TYPE_INT, 16, 0, 0),

    // 3 planes, y+u+v, 4:1:0 subsampling
    PX_PIX_FMT_YUV410P8 = $px_make_pix_fmt_tag(PX_COLOR_MODEL_YUV, 3, PX_COMP_TYPE_INT, 8, 2, 2),

    // 3 planes, y+u+v, 4:1:1 subsampling
    PX_PIX_FMT_YUV411P8 = $px_make_pix_fmt_tag(PX_COLOR_MODEL_YUV, 3, PX_COMP_TYPE_INT, 8, 2, 0),

    // 3 planes, y+u+v, 4:4:0 subsampling
    PX_PIX_FMT_YUV440P8 = $px_make_pix_fmt_tag(PX_COLOR_MODEL_YUV, 3, PX_COMP_TYPE_INT, 8, 0, 1),
    PX_PIX_FMT_YUV440P10 = $px_make_pix_fmt_tag(PX_COLOR_MODEL_YUV, 3, PX_COMP_TYPE_INT, 10, 0, 1),
    PX_PIX_FMT_YUV440P12 = $px_make_pix_fmt_tag(PX_COLOR_MODEL_YUV, 3, PX_COMP_TYPE_INT, 12, 0, 1),

    // 4 planes, y+u+v+a, 4:2:0 subsampling
    PX_PIX_FMT_YUVA420P8 = $px_make_pix_fmt_tag(PX_COLOR_MODEL_YUV, 4, PX_COMP_TYPE_INT, 8, 1, 1),
    PX_PIX_FMT_YUVA420P9 = $px_make_pix_fmt_tag(PX_COLOR_MODEL_YUV, 4, PX_COMP_TYPE_INT, 9, 1, 1),
    PX_PIX_FMT_YUVA420P10 = $px_make_pix_fmt_tag(PX_COLOR_MODEL_YUV, 4, PX_COMP_TYPE_INT, 10, 1, 1),
    PX_PIX_FMT_YUVA420P16 = $px_make_pix_fmt_tag(PX_COLOR_MODEL_YUV, 4, PX_COMP_TYPE_INT, 16, 1, 1),

    // 4 planes, y+u+v+a, 4:2:2 subsampling
    PX_PIX_FMT_YUVA422P8 = $px_make_pix_fmt_tag(PX_COLOR_MODEL_YUV, 4, PX_COMP_TYPE_INT, 8, 1, 0),
    PX_PIX_FMT_YUVA422P9 = $px_make_pix_fmt_tag(PX_COLOR_MODEL_YUV, 4, PX_COMP_TYPE_INT, 9, 1, 0),
    PX_PIX_FMT_YUVA422P10 = $px_make_pix_fmt_tag(PX_COLOR_MODEL_YUV, 4, PX_COMP_TYPE_INT, 10, 1, 0),
    PX_PIX_FMT_YUVA422P12 = $px_make_pix_fmt_tag(PX_COLOR_MODEL_YUV, 4, PX_COMP_TYPE_INT, 12, 1, 0),
    PX_PIX_FMT_YUVA422P16 = $px_make_pix_fmt_tag(PX_COLOR_MODEL_YUV, 4, PX_COMP_TYPE_INT, 16, 1, 0),

    // 4 planes, y+u+v+a, no subsampling
    PX_PIX_FMT_YUVA444P8 = $px_make_pix_fmt_tag(PX_COLOR_MODEL_YUV, 4, PX_COMP_TYPE_INT, 8, 0, 0),
    PX_PIX_FMT_YUVA444P9 = $px_make_pix_fmt_tag(PX_COLOR_MODEL_YUV, 4, PX_COMP_TYPE_INT, 9, 0, 0),
    PX_PIX_FMT_YUVA444P10 = $px_make_pix_fmt_tag(PX_COLOR_MODEL_YUV, 4, PX_COMP_TYPE_INT, 10, 0, 0),
    PX_PIX_FMT_YUVA444P12 = $px_make_pix_fmt_tag(PX_COLOR_MODEL_YUV, 4, PX_COMP_TYPE_INT, 12, 0, 0),
    PX_PIX_FMT_YUVA444P16 = $px_make_pix_fmt_tag(PX_COLOR_MODEL_YUV, 4, PX_COMP_TYPE_INT, 16, 0, 0),

    // 1 plane, gray (luma only)
    PX_PIX_FMT_Y8 = $px_make_pix_fmt_tag(PX_COLOR_MODEL_GRAY, 1, PX_COMP_TYPE_INT, 8, 0, 0),
    PX_PIX_FMT_Y9 = $px_make_pix_fmt_tag(PX_COLOR_MODEL_GRAY, 1, PX_COMP_TYPE_INT, 9, 0, 0),
    PX_PIX_FMT_Y10 = $px_make_pix_fmt_tag(PX_COLOR_MODEL_GRAY, 1, PX_COMP_TYPE_INT, 10, 0, 0),
    PX_PIX_FMT_Y12 = $px_make_pix_fmt_tag(PX_COLOR_MODEL_GRAY, 1, PX_COMP_TYPE_INT, 12, 0, 0),
    PX_PIX_FMT_Y14 = $px_make_pix_fmt_tag(PX_COLOR_MODEL_GRAY, 1, PX_COMP_TYPE_INT, 14, 0, 0),
    PX_PIX_FMT_Y16 = $px_make_pix_fmt_tag(PX_COLOR_MODEL_GRAY, 1, PX_COMP_TYPE_INT, 16, 0, 0),
    PX_PIX_FMT_YF32 = $px_make_pix_fmt_tag(PX_COLOR_MODEL_GRAY, 1, PX_COMP_TYPE_FLOAT, 32, 0, 0),

    // 2 planes, gray+alpha (luma+alpha)
    PX_PIX_FMT_YA8 = $px_make_pix_fmt_tag(PX_COLOR_MODEL_GRAY, 2, PX_COMP_TYPE_INT, 8, 0, 0),
    PX_PIX_FMT_YA16 = $px_make_pix_fmt_tag(PX_COLOR_MODEL_GRAY, 2, PX_COMP_TYPE_INT, 16, 0, 0),

    // 3 planes, r+g+b
    PX_PIX_FMT_RGBP8 = $px_make_pix_fmt_tag(PX_COLOR_MODEL_RGB, 3, PX_COMP_TYPE_INT, 8, 0, 0),
    PX_PIX_FMT_RGBP9 = $px_make_pix_fmt_tag(PX_COLOR_MODEL_RGB, 3, PX_COMP_TYPE_INT, 9, 0, 0),
    PX_PIX_FMT_RGBP10 = $px_make_pix_fmt_tag(PX_COLOR_MODEL_RGB, 3, PX_COMP_TYPE_INT, 10, 0, 0),
    PX_PIX_FMT_RGBP12 = $px_make_pix_fmt_tag(PX_COLOR_MODEL_RGB, 3, PX_COMP_TYPE_INT, 12, 0, 0),
    PX_PIX_FMT_RGBP14 = $px_make_pix_fmt_tag(PX_COLOR_MODEL_RGB, 3, PX_COMP_TYPE_INT, 14, 0, 0),
    PX_PIX_FMT_RGBP16 = $px_make_pix_fmt_tag(PX_COLOR_MODEL_RGB, 3, PX_COMP_TYPE_INT, 16, 0, 0),
    PX_PIX_FMT_RGBPF32 = $px_make_pix_fmt_tag(PX_COLOR_MODEL_RGB, 3, PX_COMP_TYPE_FLOAT, 32, 0, 0),

    // 4 planes, r+g+b+a
    PX_PIX_FMT_RGBAP8 = $px_make_pix_fmt_tag(PX_COLOR_MODEL_RGB, 4, PX_COMP_TYPE_INT, 8, 0, 0),
    PX_PIX_FMT_RGBAP10 = $px_make_pix_fmt_tag(PX_COLOR_MODEL_RGB, 4, PX_COMP_TYPE_INT, 10, 0, 0),
    PX_PIX_FMT_RGBAP12 = $px_make_pix_fmt_tag(PX_COLOR_MODEL_RGB, 4, PX_COMP_TYPE_INT, 12, 0, 0),
    PX_PIX_FMT_RGBAP14 = $px_make_pix_fmt_tag(PX_COLOR_MODEL_RGB, 4, PX_COMP_TYPE_INT, 14, 0, 0),
    PX_PIX_FMT_RGBAP16 = $px_make_pix_fmt_tag(PX_COLOR_MODEL_RGB, 4, PX_COMP_TYPE_INT, 16, 0, 0),
    PX_PIX_FMT_RGBAPF32 = $px_make_pix_fmt_tag(PX_COLOR_MODEL_RGB, 4, PX_COMP_TYPE_FLOAT, 32, 0, 0),

} PXPixelFormat;

typedef struct PXPixFmtDescriptor {

    PXColorModel color_model;
    PXComponentType comp_type;

    int n_planes;
    int bits_per_comp;
    int bytes_per_comp;

    // log2 of the chroma subsampling factor w.r.t luma: 0 => no subsampling, 1 => 1/2, 2 => 1/4, etc.
    // `chroma_width = width >> log2_chroma[0]`
    // `chroma_height = height >> log2_chroma[1]`
    int log2_chroma[2];

} PXPixFmtDescriptor;

void px_pix_fmt_get_desc(PXPixFmtDescriptor* dest, PXPixelFormat pix_fmt);
void px_pix_fmt_get_name(char dest[static PX_MAX_PIX_FMT_NAME_LEN], PXPixelFormat pix_fmt);
