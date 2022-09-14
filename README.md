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
