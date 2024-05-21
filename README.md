# game-stream-client

The client part of a pair of software intended for playing games on a remote computer and works in a similar way to a remote desktop protocol (RDP). A client connects to the server and performs actions on it remotely. The client receives audio and video streams in return.

The client simply displays the video stream sent by the server and listen to peripherals' system events so it can notify user action to the server.

## Overview

![CG client](https://github.com/Nayald/game-stream-client/blob/main/image/CG_Client_BM.png?raw=true)

The client uses TCP and UDP ports 9999 for commands and listens to UDP ports 10000 to 10003 for RTP/RTCP flows when receiving the SDPs. The RTP/RTCP stream are handle internally by the FFmpeg API.

The behavior of the client is fairly simple: upon audio/video frame reception, it will forward it to the right decoder, selected based on the SDP. The decoded frame is then forwarded to the SDL Display so it can be played.

SDL Display also handle system events, it will forge a JSON string with related events from keyboard, mouse and controllers. it covers axes' position and up/down events on buttons. Note that In case of many controllers, it will aggregate all their inputs as if there was only one. The JSON string is then passed to the CommandSocket so it can be sent to the server.

The client is the weak point of the whole solution, it may happen that the display window froze or it may have some difficulties to understand the first frame(s), restart it one time is enough in most cases.

## Dependencies
FFmpeg 4.4.2 dev libs:
* libavdevice
* libavfilter
* libavformat
* libavcodec
* libswresample
* libswscale
* libavutil

SDL2 2.0.16+

## Installation
On Ubuntu 22.04
* sudo apt install cmake libavdevice-dev libsdl2-dev
* git clone --recurse-submodules https://github.com/Nayald/game-stream-client.git
* cd game-stream-client
* cmake CMakeLists.txt
* make all

run : ./remote_client IP_SERVER

