#include <iostream>
#include <fstream>
#include <chrono>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sstream>
#include <vector>

#include "CommandSocket.h"
#include "exception.h"

constexpr size_t BUFFER_SIZE = 4096;
constexpr auto KEEPALIVE_DELAY = std::chrono::seconds(1);

CommandSocket::CommandSocket(RTPAudioReceiver &rtp_audio, RTPVideoReceiver &rtp_video, SDLDisplay &display) : rtp_audio(rtp_audio), rtp_video(rtp_video), display(display) {

}

CommandSocket::~CommandSocket() {
    close(tcp_socket);
    close(udp_socket);
}

void CommandSocket::init(const char *remote_ip, uint16_t remote_port, uint16_t local_port) {
    if (tcp_socket >= 0) {
        close(tcp_socket);
    }

    if (udp_socket >= 0) {
        close(udp_socket);
    }

    tcp_socket =  socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_socket < 0) {
        throw InitFail(strerror(errno));
    }

    udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_socket < 0) {
        throw InitFail(strerror(errno));
    }

    sockaddr_in local_address = {};
    // Filling server information
    local_address.sin_family = AF_INET;
    local_address.sin_port = htons(local_port);
    if(inet_pton(AF_INET, "0.0.0.0", &local_address.sin_addr) <= 0){
        throw InitFail(strerror(errno));
    }

    sockaddr_in remote_address = {};
    // Filling server information
    remote_address.sin_family = AF_INET;
    remote_address.sin_port = htons(remote_port);
    if(inet_pton(AF_INET, remote_ip, &remote_address.sin_addr) <= 0){
        throw InitFail(strerror(errno));
    }

    const int enable = 1;
    if (setsockopt(tcp_socket, SOL_SOCKET, SO_REUSEPORT | SO_REUSEADDR | SO_DEBUG, &enable, sizeof(enable)) < 0) {
        throw InitFail(strerror(errno));;
    }

    if (bind(tcp_socket, (sockaddr*)&local_address, sizeof(local_address)) < 0) {
        throw InitFail(strerror(errno));
    }

    if (connect(tcp_socket, (sockaddr*)&remote_address, sizeof(remote_address)) < 0) {
        throw InitFail(strerror(errno));
    }

    if (setsockopt(udp_socket, SOL_SOCKET, SO_REUSEPORT | SO_REUSEADDR | SO_DEBUG, &enable, sizeof(enable)) < 0) {
        throw InitFail(strerror(errno));
    }

    if (bind(udp_socket, (sockaddr*)&local_address, sizeof(local_address)) < 0) {
        throw InitFail(strerror(errno));
    }

    if (connect(udp_socket, (sockaddr*)&remote_address, sizeof(remote_address)) < 0) {
        throw InitFail(strerror(errno));
    }
}

void CommandSocket::startListen() {
    listen_stop_condition = false;
    listen_thread = std::thread(&CommandSocket::listen, this);
}

void CommandSocket::listen() {
    fd_set fds;
    timeval tv;
    char buffer[BUFFER_SIZE];
    char stream_buffer[2 * BUFFER_SIZE];
    size_t stream_size = 0;
    while (!listen_stop_condition) {
        FD_ZERO(&fds);
        FD_SET(tcp_socket, &fds);
        FD_SET(udp_socket, &fds);
        tv.tv_sec = 0;
        tv.tv_usec = 100'000;
        int res = select(std::max(tcp_socket, udp_socket) + 1, &fds, NULL, NULL, &tv);
        if (res < 0) {
            throw std::exception();
        } else if (res == 0) {
            continue;
        } else {
            if (FD_ISSET(tcp_socket, &fds)) {
                if (stream_size >= sizeof(stream_buffer)) {
                    std::memmove(stream_buffer, stream_buffer + 1, stream_size -= 1);
                }

                ssize_t size = recv(tcp_socket, stream_buffer + stream_size, sizeof(stream_buffer) - stream_size, 0);
                if (size > 0) {
                    stream_size += size;
                    size_t parsed_size = handleCommand(stream_buffer, stream_size, sizeof(stream_buffer));
                    if (parsed_size > 0) {
                        std::memmove(stream_buffer, stream_buffer + parsed_size, stream_size -= parsed_size);
                    }
                }
            }

            if (FD_ISSET(udp_socket, &fds)) {
                ssize_t size = recv(udp_socket, buffer, sizeof(buffer), 0);
                if (size > 0) {
                    std::cout << '(' << size << "): " << buffer << std::endl;
                }
            }
        }
    }
}

void CommandSocket::stopListen() {
    listen_stop_condition = true;
    listen_thread.join();
}

void CommandSocket::startKeepAlive() {
    keepalive_stop_condition = false;
    keepalive_thread = std::thread(&CommandSocket::keepAlive, this);
}

void CommandSocket::keepAlive() {
    while (!keepalive_stop_condition) {
        const auto delta = KEEPALIVE_DELAY + send_timepoint - std::chrono::steady_clock::now();
        if (delta.count() <= 0) {
            writeCommand(R"({"t":"k"})");
        } else {
            std::this_thread::sleep_for(delta);
        }
    }
}

size_t CommandSocket::handleCommand(const char *buffer, size_t size, size_t capacity) {
    size_t parsed_size = 0;
    try {
        simdjson::ondemand::document document = parser.iterate(buffer, size, capacity);
        parsed_size = document.raw_json().value().size();
        const std::string_view type = document["t"];
        if (type == "R") {
            const int64_t idx = document["g"];
            if (idx == 0) {
                std::string_view reply = document["r"];
                std::ofstream file;
                file.open("./sdp_video");
                file << reply << std::endl;
                file.close();

                rtp_video.stop();
                rtp_video.init("./sdp_video");
                rtp_video.start();
            } else if (idx == 1) {
                std::string_view reply = document["r"];
                std::ofstream file;
                file.open("./sdp_audio");
                file << reply << std::endl;
                file.close();

                rtp_audio.stop();
                rtp_audio.init("./sdp_audio");
                rtp_audio.start();
            }
        }
    } catch (const simdjson::simdjson_error &err) {
        std::cout << err.what() << std::endl;
    }

    return parsed_size;
}

void CommandSocket::writeCommand(const std::string &msg) {
    writeCommandImpl(msg.c_str(), msg.size());
}

void CommandSocket::writeCommand(const char *msg, size_t size) {
    writeCommandImpl(msg, size);
}

void CommandSocket::writeCommandImpl(const char *msg, size_t size) {
    send_lock.lock();
    send(tcp_socket, msg, size, 0);
    send_timepoint = std::chrono::steady_clock::now();
    send_lock.unlock();
}

void CommandSocket::handle(const std::string &msg) {
    handle(msg.c_str(), msg.size());
}

void CommandSocket::handle(const char *msg, size_t size) {
    send_lock.lock();
    send(udp_socket, msg, size, 0);
    send_timepoint = std::chrono::steady_clock::now();
    send_lock.unlock();
}
