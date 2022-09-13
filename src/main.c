#include "incl/coding.h"
#include <libavcodec/packet.h>
#include <libavutil/mem.h>

int main(int argc, char** argv) {

    int ret;
    int i;

    AVPacket* dec_packet = av_packet_alloc();
    if (!dec_packet) {
        av_log(NULL, AV_LOG_ERROR, "Failed to allocate packet\n");
        goto end;
    }

    PXMediaContext* ctx = av_mallocz(sizeof(*ctx));
    init_px_mediactx(ctx);

    if (argc < 3) {
        av_log(NULL, AV_LOG_ERROR,
               "Usage: %s <input file>"
               " <output file> [-e <encoding settings>]\n",
               argv[0]);
        ret = 1;
        goto end;
    }

    char* enc_name = NULL;
    AVDictionary* enc_opts_dict = NULL;

    if (argc > 3) {
        if (argv[3][0] == '-' && argv[3][1] == 'e') {

            av_log(NULL, AV_LOG_INFO, "Parsing encoding settings\n");

            if (argc < 5) {
                av_log(NULL, AV_LOG_ERROR, "No encoder provided\n");
                goto end;
            }

            enc_name = strtok(argv[4], ":");
            char* enc_opts = strtok(NULL, ":");
            if (enc_opts) {
                if ((ret = av_dict_parse_string(&enc_opts_dict, enc_opts, "=", ",", 0)) < 0) {
                    av_log(NULL, AV_LOG_ERROR, "Failed to parse encoding options\n");
                    goto end;
                }
                av_log(NULL, AV_LOG_INFO, "Encoding settings %s parsed and set to dict at %p\n",
                       enc_opts, enc_opts_dict);
            }

        } else {
            av_log(NULL, AV_LOG_ERROR, "Unknown option: %s\n", argv[3]);
            goto end;
        }
    }

    if ((ret = init_input(ctx, argv[1])) < 0)
        goto end;

    if ((ret = init_output(ctx, argv[2], enc_name, &enc_opts_dict)) < 0)
        goto end;

    AVStream* cur_in_stream;
    AVStream* cur_out_stream;

    PXStreamContext cur_stream_ctx;

    while (1) {

        if ((ret = av_read_frame(ctx->ifmt_ctx, dec_packet)) < 0)
            goto end;

        cur_in_stream = ctx->ifmt_ctx->streams[dec_packet->stream_index];
        cur_out_stream = ctx->ofmt_ctx->streams[dec_packet->stream_index];

        cur_stream_ctx = ctx->stream_ctx_vec[dec_packet->stream_index];

        if (cur_in_stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO ||
            cur_in_stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {

            ret = decode_frame(&cur_stream_ctx, cur_stream_ctx.dec_frame, dec_packet);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Failed to decode frame\n");
                goto end;
            }

            cur_stream_ctx.dec_frame->pts = cur_stream_ctx.dec_frame->best_effort_timestamp;

            ret = encode_frame(ctx, cur_stream_ctx.dec_frame, cur_stream_ctx.enc_pkt,
                               dec_packet->stream_index);
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Failed to encode frame\n");
                goto end;
            }

        } else {
            // remux this frame without reencoding
            av_packet_rescale_ts(cur_stream_ctx.enc_pkt, cur_in_stream->time_base,
                                 cur_out_stream->time_base);
            ret = av_interleaved_write_frame(ctx->ofmt_ctx, cur_stream_ctx.enc_pkt);
            if (ret < 0)
                goto end;
        }

        av_packet_unref(dec_packet);
        av_packet_unref(cur_stream_ctx.enc_pkt);
    }

    for (int i = 0; i < ctx->ifmt_ctx->nb_streams; i++) {
        ret = flush_encoder(ctx, cur_stream_ctx.enc_pkt, i);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Failed to flush encoder\n");
            goto end;
        }
    }

    av_write_trailer(ctx->ofmt_ctx);

end:
    uninit_px_mediactx(ctx);
    av_freep(ctx);
    av_dict_free(&enc_opts_dict);
    av_packet_free(&dec_packet);

    if (ret < 0)
        av_log(NULL, AV_LOG_ERROR, "Error occurred: %s\n", av_err2str(ret));

    return ret ? 1 : 0;
}
