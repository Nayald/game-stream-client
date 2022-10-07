extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/avfilter.h>
#include <libavdevice/avdevice.h>
#include <libswscale/swscale.h>
};

#include <iostream>
#include <thread>
#include <atomic>
#include <csignal>
#include "RTPAudioReceiver.h"
#include "SDLDisplay.h"
#include "CommandSocket.h"

std::atomic<bool> stop = false;
void signalHandler( int signum ) {
    std::cout << "Interrupt signal (" << signum << ") received.\n";
    stop.store(true, std::memory_order_relaxed);
}

int main(int argc, char **argv) {
    if (argc <= 1) {
        std::cout << argv[0] << ": <remote_ip> [remote_port] [local_port]" << std::endl;
        return -1;
    }

    const char* remote_ip = argv[1];
    const uint16_t remote_port = argc > 2 ? std::strtoul(argv[2], nullptr, 10) : 9999;
    const uint16_t local_port = argc > 3 ? std::strtoul(argv[3], nullptr, 10) : 9999;

    signal(SIGINT, signalHandler);
    //signal(SIGTERM, signalHandler);

    avdevice_register_all();
    avformat_network_init();

    try {
        SDLDisplay display;
        CommandSocket client(display);
        display.attachInputSink(&client);
        client.init(remote_ip, remote_port, local_port);

        display.startDisplay();
        client.start();
        client.writeCommand(R"({"t":"r","q":"rtp"})");
        display.startEvent();

        while (!stop.load(std::memory_order_relaxed)) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        display.stopEvent();
        client.stop();

    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}
