#include <unistd.h>
#include <iostream>

#include "RTPAudioReceiver.h"
#include "exception.h"

RTPAudioReceiver::RTPAudioReceiver() : name("rtp audio receiver") {

}

RTPAudioReceiver::RTPAudioReceiver(std::string name) : name(std::move(name)) {

}

RTPAudioReceiver::~RTPAudioReceiver() {
    stop();
    if (format_ctx) {
        avformat_free_context(format_ctx);
        format_ctx = nullptr;
    }

    if (codec_ctx) {
        avcodec_free_context(&codec_ctx);
    }
}

void RTPAudioReceiver::init(const char *path) {
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
    av_dict_set(&options,"protocol_whitelist","file,udp,rtp,rtp_mpegts,rtcp",0);
    //av_dict_set(&options,"probesize","1M",0);
    //av_dict_set(&options,"fifo_size","1M",0);
    //av_dict_set(&options,"buffer_size","384K",0);
    if (avformat_open_input(&format_ctx, path, NULL, &options) != 0) {
        throw InitFail("Couldn't open input stream");
    }

    if(avformat_find_stream_info(format_ctx,NULL) < 0) {
        throw InitFail("Couldn't find stream information");
    }
    av_dump_format(format_ctx, 0, NULL, 0);

    AVCodec *codec;
    stream_index = av_find_best_stream(format_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0);
    if(stream_index < 0) {
        throw InitFail("Couldn't find a video stream");
    }

    codec_ctx = avcodec_alloc_context3(codec);
    if (!codec_ctx) {
        throw InitFail("Could not allocate video codec context");
    }

    if (avcodec_parameters_to_context(codec_ctx, format_ctx->streams[stream_index]->codecpar) < 0) {
        throw InitFail("Could not allocate video codec context");
    }

    if(avcodec_open2(codec_ctx, codec, NULL) < 0) {
        throw InitFail("Could not open codec");
    }

    initialized = true;
    std::cerr << name << ": initialized" << std::endl;
}

void RTPAudioReceiver::start() {
    startDrain();
    startReceive();
}

void RTPAudioReceiver::stop() {
    stopReceive();
    stopDrain();
    flush();
}

void RTPAudioReceiver::startReceive() {
    if (receive_stop_condition) {
        receive_stop_condition = false;
        receive_thread = std::thread(&RTPAudioReceiver::receive, this);
    }
}

void RTPAudioReceiver::receive() {
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
            decoder_lock.lock();
            ret = avcodec_send_packet(codec_ctx, packet);
            decoder_lock.unlock();
            decoder_cv.notify_all();
            if (ret == AVERROR(EAGAIN)) {
                std::cout << name << ": encoder buffer may be full, drop frame" << std::endl;
            } else if (ret < 0) {
                throw RunError("error when sending frame to encoder");
            }

            // for 1 thread
            /*ret = avcodec_send_packet(codec_ctx, packet);
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
            }*/

            av_packet_unref(packet);
        }
    } catch (const std::exception &e) {
        std::cerr << name << ": " << e.what() << std::endl;
    }

    av_packet_free(&packet);
}

void RTPAudioReceiver::stopReceive() {
    if (!receive_stop_condition) {
        receive_stop_condition = true;
        if (receive_thread.joinable()) {
            receive_thread.join();
        }
    }
}

void RTPAudioReceiver::startDrain() {
    if (drain_stop_condition) {
        drain_stop_condition = false;
        drain_thread = std::thread(&RTPAudioReceiver::drain, this);
    }
}

void RTPAudioReceiver::drain() {
    std::cerr << name << ": drain thread pid is " << gettid() << std::endl;
    std::mutex m;
    AVFrame *frame = av_frame_alloc();
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
                Source<AVFrame>::forward(frame);
                av_frame_unref(frame);
            }
        }
    } catch (const std::exception &e) {
        std::cout << e.what() << std::endl;
    }

    av_frame_free(&frame);
}

void RTPAudioReceiver::stopDrain() {
    if (!drain_stop_condition) {
        drain_stop_condition = true;
        if (drain_thread.joinable()) {
            drain_thread.join();
        }
    }
}

void RTPAudioReceiver::flush() {
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