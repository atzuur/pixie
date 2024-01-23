#include <pixie/frame.h>
#include <pixie/util/utils.h>
#include <pixie/log.h>

#include <libavutil/frame.h>

#define LAV_THROW_MSG(func, err)                                                                            \
    PX_LOG(PX_LOG_ERROR, "%s() failed at %s:%d: %s (code %d)\n", func, __FILE__, __LINE__, av_err2str(err), \
           err)

#define OS_THROW_MSG(func, err)                                                       \
    fprintf(stderr, "%s() failed at %s:%d: %s (code %d)\n", func, __FILE__, __LINE__, \
            px_last_os_errstr((char[256]) {0}, err), err)

int px_frame_from_av(PXFrame* dest, const AVFrame* av_frame);
void px_frame_to_av(AVFrame* dest, const PXFrame* px_frame);

void px_frame_free_internal(PXFrame* frame);
