# game-stream-client
The client part of a pair of software intended for playing games on a remote computer

run : ./remote_client IP_SERVER

use port 9999 tcp and udp, 10000-10003 port range for rtp/rtcp

# dependencies
FFmpeg 4.4.2 dev libs:
* libavdevice
* libavfilter
* libavformat
* libavcodec
* libswresample
* libswscale
* libavutil

SDL2 2.0.16+

# installation
On Ubuntu 22.04
* sudo apt install cmake libavdevice-dev libsdl2-dev
* git clone --recurse-submodules https://github.com/Nayald/game-stream-client.git
* cd game-stream-client
* cmake CMakeLists.txt
* make all

