#include <pixie/frame.h>

#include <libavutil/frame.h>

int px_frame_from_av(PXFrame* dest, const AVFrame* av_frame);
void px_frame_to_av(AVFrame* dest, const PXFrame* px_frame);
