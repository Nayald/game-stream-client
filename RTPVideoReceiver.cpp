#include <unistd.h>
#include <iostream>

#include "RTPVideoReceiver.h"
#include "exception.h"

static AVHWAccel* ff_find_hwaccel(AVCodecID codec_id, AVPixelFormat pixel_format) {
    AVHWAccel *hwaccel = NULL;
    while ((hwaccel = av_hwaccel_next(hwaccel))) {
        if (hwaccel->id == codec_id && hwaccel->pix_fmt == pixel_format) {
            return hwaccel;
        }
    }
    return NULL;
}

static AVPixelFormat hw_pix_fmt;
static AVPixelFormat get_format_vaapi(AVCodecContext *ctx, const AVPixelFormat *pix_fmts){
    while (*pix_fmts != AV_PIX_FMT_NONE) {
        if (*pix_fmts++ == hw_pix_fmt) {
            return hw_pix_fmt;
        }
    }
    return AV_PIX_FMT_NONE;
}

RTPVideoReceiver::RTPVideoReceiver() : name("rtp video receiver") {

}

RTPVideoReceiver::RTPVideoReceiver(std::string name) : name(std::move(name)){

}

RTPVideoReceiver::~RTPVideoReceiver() {
    stop();
    if (format_ctx) {
        avformat_free_context(format_ctx);
        format_ctx = nullptr;
    }

    if (codec_ctx) {
        avcodec_free_context(&codec_ctx);
    }
}

void RTPVideoReceiver::init(const char *path) {
    // re-init check, free old context
    if (format_ctx) {
        avformat_free_context(format_ctx);
        format_ctx = nullptr;
    }

    if (codec_ctx) {
        avcodec_free_context(&codec_ctx);
    }

    format_ctx = avformat_alloc_context();
    AVDictionary *options = NULL;
    av_dict_set(&options, "protocol_whitelist", "file,udp,rtp,rtcp,rtp_mpegts", 0);
    //av_dict_set(&options,"framerate","-1",0);
    //av_dict_set(&options, "probesize", "4M", 0);
    av_dict_set(&options, "fifo_size", "16M", 0);
    av_dict_set(&options, "buffer_size", "384K", 0);
    if (avformat_open_input(&format_ctx, path, NULL, &options) != 0) {
    //if (avformat_open_input(&format_ctx, "rtp://127.0.0.1:10002", NULL, &options) != 0) {
        throw InitFail("Couldn't open input stream");
    }

    if (avformat_find_stream_info(format_ctx, NULL) < 0) {
        throw InitFail("Couldn't find stream information");
    }
    av_dump_format(format_ctx, 0, NULL, 0);

    AVCodec *codec;
    stream_index = av_find_best_stream(format_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0);
    if (stream_index < 0) {
        throw InitFail("Couldn't find a video stream");
    }

    codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        throw InitFail("Could not allocate video codec context");
    }

    if (avcodec_parameters_to_context(codec_ctx, format_ctx->streams[stream_index]->codecpar) < 0) {
        throw InitFail("Could not allocate video codec context");
    }

    if (codec->capabilities | AV_CODEC_CAP_SLICE_THREADS) {
        codec_ctx->thread_count = 4;
        codec_ctx->thread_type = FF_THREAD_SLICE;
    }

    /*AVHWDeviceType type = av_hwdevice_find_type_by_name("vaapi");
    if (type == AV_HWDEVICE_TYPE_NONE) {
        fprintf(stderr, "Available device types:");
        while((type = av_hwdevice_iterate_types(type)) != AV_HWDEVICE_TYPE_NONE)
            fprintf(stderr, " %s", av_hwdevice_get_type_name(type));
        fprintf(stderr, "\n");
        throw InitFail("type not found");
    }

    if (av_hwdevice_ctx_create(&hw_device_ctx, type, NULL, NULL, 0) >= 0) {
        codec_ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);
        if (codec_ctx->hw_device_ctx) {
            for (int i = 0;; i++) {
                const AVCodecHWConfig *config = avcodec_get_hw_config(codec, i);
                if (!config) {
                    fprintf(stderr, "Decoder %s does not support device type %s.\n", codec->name, av_hwdevice_get_type_name(type));
                    throw InitFail("not supported");
                }
                if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX && config->device_type == type) {
                    hw_pix_fmt = config->pix_fmt;
                    break;
                }
            }

            codec_ctx->get_format = get_format_vaapi;
            codec_ctx->hwaccel = ff_find_hwaccel(codec->id, hw_pix_fmt);
        } else {
            std::cout << "Failed to create VAAPI device, use software decoding" << std::endl;
        }
    } else {
        std::cout << "Failed to create VAAPI device, use software decoding" << std::endl;
    }*/

    if (avcodec_open2(codec_ctx, codec, NULL) < 0) {
        throw InitFail("Could not open codec");
    }

    initialized = true;
    std::cerr << name << ": initialized" << std::endl;
}

void RTPVideoReceiver::start() {
    //startDrain();
    startReceive();
}

void RTPVideoReceiver::stop() {
    stopReceive();
    stopDrain();
    flush();
}

void RTPVideoReceiver::startReceive() {
    if (receive_stop_condition) {
        receive_stop_condition = false;
        receive_thread = std::thread(&RTPVideoReceiver::receive, this);
    }
}

void RTPVideoReceiver::receive() {
    std::cerr << name << ": receive thread pid is " << gettid() << std::endl;
    int ret;
    AVPacket *packet = av_packet_alloc();
    try {
        while (initialized && !receive_stop_condition) {
            if (av_read_frame(format_ctx, packet) < 0) {
                throw RunError("can't grab frame");
            }

            if (packet->stream_index != stream_index) {
                throw RunError("wrong index");
            }

            Source<AVPacket>::forward(packet);

            // for 2 threads
            /*decoder_lock.lock();
            ret = avcodec_send_packet(codec_ctx, packet);
            decoder_lock.unlock();
            decoder_cv.notify_all();
            if (ret == AVERROR(EAGAIN)) {
                std::cout << name << ": encoder buffer may be full, drop frame" << std::endl;
            } else if (ret < 0) {
                throw RunError("error when sending frame to encoder");
            }*/

            // for 1 thread
            ret = avcodec_send_packet(codec_ctx, packet);
            if (ret < 0) {
                throw RunError("decode packet error");
            }
            while (ret >= 0) {
                AVFrame *frame = av_frame_alloc();
                ret = avcodec_receive_frame(codec_ctx, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                    break;
                } else if (ret < 0) {
                    throw RunError("error during decoding");
                }

                Source<AVFrame>::forward(frame);
                av_frame_unref(frame);
            }

            av_packet_unref(packet);
        }
    } catch (const std::exception &e) {
        std::cerr << name << ": " << e.what() << std::endl;
    }

    av_packet_free(&packet);
}

void RTPVideoReceiver::stopReceive() {
    if (!receive_stop_condition) {
        receive_stop_condition = true;
        if (receive_thread.joinable()) {
            receive_thread.join();
        }
    }
}

void RTPVideoReceiver::startDrain() {
    if (drain_stop_condition) {
        drain_stop_condition = false;
        drain_thread = std::thread(&RTPVideoReceiver::drain, this);
    }
}

void RTPVideoReceiver::drain() {
    std::cerr << name << ": drain thread pid is " << gettid() << std::endl;
    std::mutex m;
    AVFrame *frame = av_frame_alloc();
    //AVFrame *sw_frame = av_frame_alloc();
    int ret = 0;
    try {
        while (initialized && !drain_stop_condition) {
            decoder_lock.lock();
            ret = avcodec_receive_frame(codec_ctx, frame);
            decoder_lock.unlock();
            if (ret == AVERROR(EAGAIN)) {
                std::unique_lock<std::mutex> mlock(m);
                decoder_cv.wait(mlock, [this, &ret, &frame] {
                    decoder_lock.lock();
                    ret = avcodec_receive_frame(codec_ctx, frame);
                    decoder_lock.unlock();
                    return ret != AVERROR(EAGAIN);
                });
            }

            if (ret < 0) {
                throw RunError("error when receiving packet from encoder");
            } else {
                //ret = av_hwframe_transfer_data(sw_frame, frame, 0);
                Source<AVFrame>::forward(frame);
                av_frame_unref(frame);
            }
        }
    } catch (const std::exception &e) {
        std::cout << e.what() << std::endl;
    }

    av_frame_free(&frame);
    //av_frame_free(&sw_frame);
}

void RTPVideoReceiver::stopDrain() {
    if (!drain_stop_condition) {
        drain_stop_condition = true;
        if (drain_thread.joinable()) {
            drain_thread.join();
        }
    }
}

void RTPVideoReceiver::flush() {
    if (!receive_stop_condition || !drain_stop_condition) {
        std::cout << name << ": flush order ignored, stop threads first" << std::endl;
        return;
    }

    if (!initialized) {
        std::cout << name << ": is not initialized, nothing to do" << std::endl;
        return;
    }

    initialized = false;
    int ret = avcodec_send_frame(codec_ctx, nullptr);
    AVFrame *frame = av_frame_alloc();
    try {
        while (ret >= 0 || ret == AVERROR(EAGAIN)) {
            ret = avcodec_receive_frame(codec_ctx, frame);
            if (ret < 0 && ret != AVERROR_EOF) {
                throw RunError("error while flushing decoder");
            }

            Source<AVFrame>::forward(frame);
            av_frame_unref(frame);
        }
    } catch (const std::exception &e) {
        std::cout << e.what() << std::endl;
    }

    av_frame_free(&frame);
}