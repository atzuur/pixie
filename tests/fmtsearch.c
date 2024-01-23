#include <libavcodec/codec.h>
#include <libavutil/pixdesc.h>

#include <stddef.h>
#include <stdio.h>
#include <assert.h>

int main(void) {
    const AVCodec* codec = NULL;
    void* opaque = NULL;
    while ((codec = av_codec_iterate(&opaque))) {
        const enum AVPixelFormat* pix_fmt = codec->pix_fmts;
        while (pix_fmt && *pix_fmt++ != AV_PIX_FMT_NONE) {
            const AVPixFmtDescriptor* desc = av_pix_fmt_desc_get(*pix_fmt);
            if (!desc) {
                continue;
            }
            if (desc->flags & AV_PIX_FMT_FLAG_FLOAT) {
                printf("%s: %s\n", codec->name, desc->name);
            }
        }
    }
}
