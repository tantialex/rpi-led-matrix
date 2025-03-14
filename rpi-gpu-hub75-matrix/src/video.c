#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>

#include "rpihub75.h"
#include "video.h"
#include "pixels.h"
#include "util.h"

/**
 * @brief pass this function to your pthread_create() call to render a video file
 * will render the video file pointed to by scene->shader_file until
 * scene->do_render is false;
 * 
 * @param arg 
 * @return void* 
 */

void* render_video_fn(void *arg) {
    scene_info *scene = (scene_info*)arg;
    while (scene->do_render) {
        if (!hub_render_video(scene, scene->shader_file)) {
            break;
        }
    }

    return NULL;
}

/**
 * @brief pass this function to your pthread_create() call to render a video file
 * will render the video file pointed to by scene->shader_file until
 * scene->do_render is false; returns once the video is done rendering
 * 
 * @param arg 
 * @return void* 
 */
bool hub_render_video(scene_info *scene, const char *filename) {
    AVFormatContext *format_ctx = NULL;
    AVCodecContext *codec_ctx = NULL;
    //AVCodec *codec = NULL;
    AVFrame *frame = NULL;
    AVFrame *frame_rgb = NULL;
    AVPacket packet;
    struct SwsContext *sws_ctx = NULL;

    int video_stream_index = -1;
    scene->stride = 3;

    // Register all formats and codecs
    // av_register_all();

    // Open video file
    if (avformat_open_input(&format_ctx, filename, NULL, NULL) != 0) {
        fprintf(stderr, "Could not open video file\n");
        return false;
    }

    // Retrieve stream information
    if (avformat_find_stream_info(format_ctx, NULL) < 0) {
        fprintf(stderr, "Could not find stream information\n");
        return false;
    }

    // Find the first video stream
    for (int i = 0; i < format_ctx->nb_streams; i++) {
        if (format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index = i;
            break;
        }
    }
    if (video_stream_index == -1) {
        fprintf(stderr, "No video stream found\n");
        return false;
    }

    AVStream *video_stream = format_ctx->streams[video_stream_index];
    AVRational frame_rate = video_stream->avg_frame_rate; // Use avg_frame_rate for variable frame rate videos
    float fps = (float)av_q2d(frame_rate);

    // Get codec parameters and find the decoder for the video stream
    AVCodecParameters *codec_params = format_ctx->streams[video_stream_index]->codecpar;
    const AVCodec *codec = avcodec_find_decoder(codec_params->codec_id);
    if (codec == NULL) {
        fprintf(stderr, "Unsupported codec\n");
        return false;
    }

    // Allocate codec context
    codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        fprintf(stderr, "Failed to allocate codec context\n");
        return false;
    }
    avcodec_parameters_to_context(codec_ctx, codec_params);

    // Open codec
    if (avcodec_open2(codec_ctx, codec, NULL) < 0) {
        fprintf(stderr, "Could not open codec\n");
        return false;
    }

    // Allocate frames
    frame = av_frame_alloc();
    frame_rgb = av_frame_alloc();
    if (!frame || !frame_rgb) {
        fprintf(stderr, "Could not allocate frame memory\n");
        return false;
    }

    //AVStream *video_stream = format_ctx->streams[video_stream_index];
    //AVRational frame_rate = video_stream->avg_frame_rate; // Use avg_frame_rate for variable frame rate videos
    //int fps = (int)av_q2d(frame_rate);

    // Set up RGB frame buffer
    int num_bytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, scene->width, scene->height, 1);
    uint8_t *buffer = (uint8_t *)av_malloc(num_bytes * sizeof(uint8_t)*2);
    av_image_fill_arrays(frame_rgb->data, frame_rgb->linesize, buffer, AV_PIX_FMT_RGB24, scene->width, scene->height, 1);

    // Set up scaling context
    sws_ctx = sws_getContext(codec_ctx->width, codec_ctx->height, codec_ctx->pix_fmt,
                             scene->width, scene->height, AV_PIX_FMT_RGB24,
                             SWS_BILINEAR, NULL, NULL, NULL);

    // Read frames
    while (av_read_frame(format_ctx, &packet) >= 0) {
        if (!scene->do_render) {
            break;
        }
        // Is this packet from the video stream?
        if (packet.stream_index == video_stream_index) {
            // Decode video frame
            int response = avcodec_send_packet(codec_ctx, &packet);
            if (response < 0) {
                fprintf(stderr, "Error sending packet for decoding\n");
                break;
            }
            while (response >= 0) {
                response = avcodec_receive_frame(codec_ctx, frame);
                if (response == AVERROR(EAGAIN) || response == AVERROR_EOF)
                    break;
                else if (response < 0) {
                    fprintf(stderr, "Error during decoding\n");
                    return false;
                }

                // Convert the image from its native format to RGB
                sws_scale(sws_ctx, (uint8_t const * const *)frame->data,
                          frame->linesize, 0, codec_ctx->height,
                          frame_rgb->data, frame_rgb->linesize);


                map_byte_image_to_bcm(scene, frame_rgb->data[0]);

		calculate_fps(fps, scene->show_fps);
            }
        }
        av_packet_unref(&packet);
    }

    // Clean up
    av_free(buffer);
    av_frame_free(&frame);
    av_frame_free(&frame_rgb);
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&format_ctx);
    sws_freeContext(sws_ctx);

    return true;
}

