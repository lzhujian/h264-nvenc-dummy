#include <stdio.h>
#include <math.h>

#include "libavutil/avstring.h"
#include "libavutil/mathematics.h"
#include "libavutil/pixdesc.h"
#include "libavutil/imgutils.h"
#include "libavutil/dict.h"
#include "libavutil/parseutils.h"
#include "libavutil/samplefmt.h"
#include "libavutil/avassert.h"
#include "libavutil/time.h"

#include "libavformat/avformat.h"
#include "libavdevice/avdevice.h"
#include "libswscale/swscale.h"
#include "libavutil/opt.h"
#include "libavcodec/avfft.h"
#include "libswresample/swresample.h"

#include "libavcodec/avcodec.h"
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"
#include "libavutil/channel_layout.h"
#include "libavutil/common.h"
#include "libavutil/hwcontext.h"

/*
 * * Video encoding example
 * */
static void video_encode_example(const char *filename)
{
    AVCodec *codec;
    AVCodecContext *c = NULL;
    int i, ret, x, y, got_output;
    AVFrame *frame;
    AVPacket pkt;
    uint8_t endcode[] = { 0, 0, 1, 0xb7 };

    av_log_set_level(64);

    //AVBufferRef *device_ref = NULL;
    //AVBufferRef *hw_frames_ctx = NULL;
    //hw_frames_ctx = (AVBufferRef *)av_mallocz(sizeof(AVBufferRef));
    //if (!hw_frames_ctx) {
    //    ret = AVERROR(ENOMEM);
    //    return ;
    //}

    //ret = av_hwdevice_ctx_create(&device_ref, AV_HWDEVICE_TYPE_CUDA,"CUDA", NULL, 0);
    //if (ret < 0)
    //    return;

    //hw_frames_ctx = av_hwframe_ctx_alloc(device_ref);
    //if (!hw_frames_ctx) {
    //    av_log(NULL, AV_LOG_ERROR, "av_hwframe_ctx_alloc failed\n");
    //    ret = AVERROR(ENOMEM);
    //    return;
    //}
    //av_buffer_unref(&device_ref);

    //c->hw_frames_ctx = av_buffer_ref(hw_frames_ctx);
    //if (!hw_frames_ctx) {
    //    av_log(NULL, AV_LOG_ERROR, "av_buffer_ref failed\n");
    //    ret = AVERROR(ENOMEM);
    //    return;
    //}

    printf("Encode video file %s\n", filename);

    /* find the video encoder */
    //codec = avcodec_find_encoder_by_name("hevc_nvenc");
    //codec = avcodec_find_encoder(AV_CODEC_ID_H265);
    codec = avcodec_find_encoder_by_name("h264_nvenc");
    //codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!codec) {
        fprintf(stderr, "Codec not found\n");
        exit(1);
    }

    c = avcodec_alloc_context3(codec);
    if (!c) {
        fprintf(stderr, "Could not allocate video codec context\n");
        exit(1);
    }

    /* put sample parameters */
    c->bit_rate = 400000;
    /* resolution must be a multiple of two */
    c->width = 352;
    c->height = 288;
    /* frames per second */
    c->time_base.num = 1;
    c->time_base.den = 25;
    /* emit one intra frame every ten frames
     * check frame pict_type before passing frame
     * to encoder, if frame->pict_type is AV_PICTURE_TYPE_I
     * then gop_size is ignored and the output of encoder
     * will always be I frame irrespective to gop_size
     */
    c->gop_size = 10;
    c->max_b_frames = 1;
    c->pix_fmt = AV_PIX_FMT_YUV420P;//AV_PIX_FMT_CUDA;
    c->max_b_frames = 0;

    AVDictionary *param = 0;
    //H.264
    if (codec->id == AV_CODEC_ID_H264) {
        av_dict_set(&param, "preset", "medium", 0);
        av_dict_set(&param, "tune", "zerolatency", 0);
    }
    //H.265
    if (codec->id == AV_CODEC_ID_H265 || codec->id == AV_CODEC_ID_HEVC){
        //av_dict_set(&param, "x265-params", "qp=20", 0);
        av_dict_set(&param, "x265-params", "crf=25", 0);
        av_dict_set(&param, "preset", "fast", 0);
        av_dict_set(&param, "tune", "zero-latency", 0);
    }

    /* open it */
    if (avcodec_open2(c, codec, &param) < 0) {
        fprintf(stderr, "Could not open codec\n");
        system("pause");
        exit(1);
    }

    FILE *f;
    f = fopen(filename, "wb");
    if (!f) {
        fprintf(stderr, "Could not open %s\n", filename);
        exit(1);
    }

    frame = av_frame_alloc();
    if (!frame) {
        fprintf(stderr, "Could not allocate video frame\n");
        exit(1);
    }
    frame->format = c->pix_fmt;
    frame->width = c->width;
    frame->height = c->height;

    /* the image can be allocated by any means and av_image_alloc() is
     * just the most convenient way if av_malloc() is to be used */
    ret = av_image_alloc(frame->data, frame->linesize, c->width, c->height,
                         c->pix_fmt, 32);
    if (ret < 0) {
        fprintf(stderr, "Could not allocate raw picture buffer\n");
        exit(1);
    }

    const int kFrameSize = 5000;    // 5000 / 25 = 200s
    for (i = 0; i < kFrameSize; i++) {
        av_init_packet(&pkt);
        pkt.data = NULL;    // packet data will be allocated by the encoder
        pkt.size = 0;

        fflush(stdout);
        /* prepare a dummy image */
        /* Y */
        for (y = 0; y < c->height; y++) {
            for (x = 0; x < c->width; x++) {
                frame->data[0][y * frame->linesize[0] + x] = x + y + i * 3;
            }
        }

        /* Cb and Cr */
        for (y = 0; y < c->height / 2; y++) {
            for (x = 0; x < c->width / 2; x++) {
                frame->data[1][y * frame->linesize[1] + x] = 128 + y + i * 2;
                frame->data[2][y * frame->linesize[2] + x] = 64 + x + i * 5;
            }
        }

        frame->pts = i;

        /* encode the image */
        ret = avcodec_encode_video2(c, &pkt, frame, &got_output);
        if (ret < 0) {
            fprintf(stderr, "Error encoding frame\n");
            exit(1);
        }

        if (got_output) {
            printf("Write frame %3d (size=%5d)\n", i, pkt.size);
            fwrite(pkt.data, 1, pkt.size, f);
            av_packet_unref(&pkt);
        }
    }

    /* get the delayed frames */
    for (got_output = 1; got_output; i++) {
        fflush(stdout);

        ret = avcodec_encode_video2(c, &pkt, NULL, &got_output);
        if (ret < 0) {
            fprintf(stderr, "Error encoding frame\n");
            exit(1);
        }

        if (got_output) {
            printf("Write frame %3d (size=%5d)\n", i, pkt.size);
            fwrite(pkt.data, 1, pkt.size, f);
            av_packet_unref(&pkt);
        }
    }

    /* add sequence end code to have a real MPEG file */
    fwrite(endcode, 1, sizeof(endcode), f);
    fclose(f);

    avcodec_close(c);
    av_free(c);
    av_freep(&frame->data[0]);
    av_frame_free(&frame);
    printf("\n");
}

int main(int argc, char **argv)
{
    /* register all the codecs */
    avcodec_register_all();
    avdevice_register_all();
    avfilter_register_all();
    av_register_all();
    avformat_network_init();

    video_encode_example("test.h264");

    getchar();

    return 0;
}
