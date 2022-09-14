extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/avfilter.h>
#include <libavdevice/avdevice.h>
#include <libswscale/swscale.h>
};

#include <iostream>
#include <thread>
#include <csignal>
#include "RTPAudioReceiver.h"
#include "SDLDisplay.h"
#include "CommandSocket.h"

bool stop = false;
void signalHandler( int signum ) {
    std::cout << "Interrupt signal (" << signum << ") received.\n";
    stop = true;
}

int main(int argc, char **argv) {
    if (argc <= 1) {
        std::cout << argv[0] << ": remote_ip remote_port local_port" << std::endl;
    }

    const char* remote_ip = argv[1];
    //const uint16_t remote_port = std::strtoul(argv[2], nullptr, 10);
    const uint16_t remote_port = 9999;
    //const uint16_t local_port = std::strtoul(argv[3], nullptr, 10);
    const uint16_t local_port = 9999;

    signal(SIGINT, signalHandler);
    //signal(SIGTERM, signalHandler);

    avdevice_register_all();
    avformat_network_init();

    try {
        SDLDisplay display;
        RTPAudioReceiver rtp_audio;
        RTPVideoReceiver rtp_video;
        rtp_audio.Source<AVFrame>::attachSink(&display);
        rtp_video.Source<AVFrame>::attachSink(&display);

        CommandSocket client(rtp_audio, rtp_video, display);
        client.init(remote_ip, remote_port, local_port);
        display.attachInputSink(&client);

        client.startListen();
        display.startDisplay();
        display.startListening();
        display.startAudio();
        client.writeCommand(R"({"t":"r","q":"rtp"})");
        client.startKeepAlive();

        while (!stop) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        rtp_audio.stop();
        rtp_video.stop();
        display.stopListening();
        display.stopAudio();
        display.stopDisplay();

    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
    }
}
