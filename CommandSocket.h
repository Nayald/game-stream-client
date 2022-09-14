#ifndef REMOTE_CLIENT_COMMANDSOCKET_H
#define REMOTE_CLIENT_COMMANDSOCKET_H

#include <vector>
#include <unordered_set>
#include <chrono>
#include <thread>
#include "spinlock.h"
#include "CommandSink.h"

#include "simdjson/singleheader/simdjson.h"

#include "SDLDisplay.h"
#include "RTPVideoReceiver.h"
#include "RTPAudioReceiver.h"

class CommandSocket: public CommandSink {
private:
    RTPAudioReceiver &rtp_audio;
    RTPVideoReceiver &rtp_video;
    SDLDisplay &display;

    int tcp_socket = -1;
    int udp_socket = -1;

    bool listen_stop_condition = true;
    std::thread listen_thread;
    simdjson::ondemand::parser parser;
    std::chrono::steady_clock::time_point send_timepoint;
    spinlock send_lock;

    bool keepalive_stop_condition = true;
    std::thread keepalive_thread;

public:
    explicit CommandSocket(RTPAudioReceiver &rtp_audio, RTPVideoReceiver &rtp_video, SDLDisplay &display);
    ~CommandSocket() override;

    void init(const char *remote_ip, uint16_t remote_port, uint16_t local_port);

    void start();
    void stop();

    void startListen();
    void listen();
    void stopListen();

    void startKeepAlive();
    void keepAlive();
    void stopKeepAlive();

    size_t handleCommand(const char *buffer, size_t size, size_t capacity);
    void writeCommand(const std::string &msg);
    void writeCommand(const char *msg, size_t size);
    void writeCommandImpl(const char *msg, size_t size);

    virtual void handle(const std::string &msg) override;
    virtual void handle(const char *msg, size_t size) override;
};


#endif //REMOTE_CLIENT_COMMANDSOCKET_H
