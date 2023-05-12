#include "../src/frame.h"
#include <libavutil/pixfmt.h>
#include <stdio.h>
#include <stdlib.h>

void write_y4m_header(FILE* out_file, int width, int height) {
    fprintf(out_file, "YUV4MPEG2 W%d H%d F10:1 Ip A1:1 C420 XYSCSS=420\n", width, height);
}

void write_frame(FILE* out_file, const PXFrame* frame, int frame_num) {

    fprintf(out_file, "FRAME\n");

    for (int p = 0; p < 3; p++) {
        const PXVideoPlane* plane = &frame->planes[p];

        for (int y = 0; y < plane->height; y++) {
            for (int x = 0; x < plane->width; x++) {
                plane->data[y][x] = x * y + p * 100 + frame_num * 10;
            }

            fwrite(plane->data[y], frame->bytes_per_comp, plane->width, out_file);
        }
    }
}

int main(void) {

    PXFrame* frame = px_frame_new(800, 800, AV_PIX_FMT_YUV420P);
    if (!frame)
        return 1;

    FILE* out_file = fopen("test.y4m", "wb");
    if (!out_file)
        return 1;

    write_y4m_header(out_file, frame->width, frame->height);

    for (int i = 0; i < 100; i++) {
        write_frame(out_file, frame, i);
    }

    px_frame_free(&frame);
    fclose(out_file);

    return 0;
}
