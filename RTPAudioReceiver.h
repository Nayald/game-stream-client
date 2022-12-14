#ifndef REMOTE_CLIENT_RTPAUDIORECEIVER_H
#define REMOTE_CLIENT_RTPAUDIORECEIVER_H

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/avfilter.h>
#include <libavdevice/avdevice.h>
#include <libswscale/swscale.h>
}

#include <thread>
#include <condition_variable>

#include "source.h"
#include "spinlock.h"

class RTPAudioReceiver : public Source<AVPacket>, public Source<AVFrame> {
private:
    std::string name;
    bool initialized = false;

    AVFormatContext *format_ctx = nullptr;
    int stream_index;
    AVCodecContext *codec_ctx = nullptr;

    bool receive_stop_condition = true;
    std::thread receive_thread;

    bool drain_stop_condition = true;
    std::thread drain_thread;

    spinlock decoder_lock;
    std::condition_variable decoder_cv;

public:
    explicit RTPAudioReceiver();
    explicit RTPAudioReceiver(std::string name);
    ~RTPAudioReceiver();

    void init(const char *path);
    AVCodecContext* getContext() const;

    void start();
    void stop();

    void startReceive();
    void receive();
    void stopReceive();

    void startDrain();
    void drain();
    void stopDrain();

    void flush();
};


#endif //REMOTE_CLIENT_RTPAUDIORECEIVER_H
