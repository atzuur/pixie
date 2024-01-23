#include <libavutil/intfloat.h>
#include <libswscale/swscale.h>
#include <libavutil/frame.h>
#include <libavutil/bswap.h>

#include <stdint.h>

#define WIDTH 100
#define HEIGHT 100

#define PIXEL_TYPE float
#define PIXEL_MAX 1.f
#define PIXEL_FMT AV_PIX_FMT_GRAYF32LE
#define N_PLANES 1

static void fill(AVFrame* frame) {
    for (int p = 0; p < N_PLANES; p++) {
        for (int y = 0; y < HEIGHT; y++) {
            PIXEL_TYPE* line = (PIXEL_TYPE*)(frame->data[p] + y * frame->linesize[p]);
            for (int x = 0; x < WIDTH; x++) {
                // line[x] = av_int2float(av_bswap32(av_float2int(PIXEL_MAX)));
                line[x] = PIXEL_MAX;
            }
        }
    }
}

static AVFrame* alloc(enum AVPixelFormat pix_fmt, int width, int height) {
    AVFrame* frame = av_frame_alloc();
    if (!frame)
        return NULL;

    frame->format = pix_fmt;
    frame->width = width;
    frame->height = height;

    int ret = av_frame_get_buffer(frame, 0);
    if (ret < 0) {
        fprintf(stderr, "av_frame_get_buffer: %d %s\n", ret, av_err2str(ret));
        return NULL;
    }

    return frame;
}

[[maybe_unused]] static void swap_line_iter_order(AVFrame* frame) {
    for (int p = 0; p < 3; p++) {
        frame->data[p] += frame->linesize[p] * (HEIGHT - 1);
        frame->linesize[p] = -frame->linesize[p];
    }
}

int main(void) {
    av_log_set_level(AV_LOG_TRACE);

    AVFrame* frame = alloc(PIXEL_FMT, WIDTH, HEIGHT);
    if (!frame)
        return 1;
    fill(frame);

    AVFrame* out_frame = alloc(PIXEL_FMT, WIDTH, HEIGHT);
    if (!out_frame)
        return 1;

    // swap_line_iter_order(frame);

    // for (int p = 0; p < N_PLANES; p++) {
    //     for (int y = 0; y < HEIGHT; y++) {
    //         memcpy(out_frame->data[p] + y * out_frame->linesize[p], frame->data[p] + y *
    //         frame->linesize[p],
    //                WIDTH * sizeof(PIXEL_TYPE));
    //     }
    // }

    struct SwsContext* sws =
        sws_getContext(WIDTH, HEIGHT, PIXEL_FMT, WIDTH, HEIGHT, PIXEL_FMT, SWS_BILINEAR, NULL, NULL, NULL);
    if (!sws)
        return 1;
    sws_scale(sws, (const uint8_t* const*)frame->data, frame->linesize, 0, HEIGHT, out_frame->data,
              out_frame->linesize);
    sws_freeContext(sws);

    FILE* out = fopen("out", "wb");
    if (!out)
        return 1;

    for (int p = 0; p < N_PLANES; p++) {
        for (int y = 0; y < HEIGHT; y++) {
            size_t written =
                fwrite(out_frame->data[p] + y * out_frame->linesize[p], sizeof(PIXEL_TYPE), WIDTH, out);
            if (written != WIDTH)
                printf("written: %zu, expected %zu\n", written, WIDTH * sizeof(PIXEL_TYPE));
        }
    }

    av_frame_free(&frame);
    av_frame_free(&out_frame);
    fclose(out);
}
