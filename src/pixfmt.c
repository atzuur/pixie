#include <pixie/pixfmt.h>
#include <pixie/log.h>

#include <limits.h>
#include <string.h>

void px_pix_fmt_get_name(char dest[static PX_PIX_FMT_MAX_NAME_LEN], PXPixelFormat pix_fmt) {
    PXPixFmtDescriptor fmt_desc = px_pix_fmt_get_desc(pix_fmt);

    const char* cmodel = (const char*[]) {"yuv", "gbr", "y"}[fmt_desc.color_model];
    const char* alpha = (fmt_desc.n_planes == 4 || fmt_desc.n_planes == 2) ? "a" : "";

    // https://en.wikipedia.org/wiki/Chroma_subsampling#Sampling_systems_and_ratios
    int subsamp_w = (4 >> fmt_desc.log2_chroma[0]);
    int subsamp_h = (fmt_desc.log2_chroma[1] == 0 ? subsamp_w : 0);

    snprintf(dest, PX_PIX_FMT_MAX_NAME_LEN, "%s%s", cmodel, alpha);

    if (fmt_desc.color_model == PX_COLOR_MODEL_YUV)
        snprintf(dest + strlen(dest), PX_PIX_FMT_MAX_NAME_LEN - strlen(dest), "4%d%d", subsamp_w, subsamp_h);

    snprintf(dest + strlen(dest), PX_PIX_FMT_MAX_NAME_LEN - strlen(dest), "%s%s%d",
             fmt_desc.color_model == PX_COLOR_MODEL_GRAY ? "" : "p",
             fmt_desc.comp_type == PX_COMP_TYPE_FLOAT ? "f" : "", fmt_desc.bits_per_comp);
}
