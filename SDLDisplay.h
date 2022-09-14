#ifndef REMOTE_DESKTOP_SDLDISPLAY_H
#define REMOTE_DESKTOP_SDLDISPLAY_H

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/avfilter.h>
#include <libavdevice/avdevice.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <SDL2/SDL.h>
};

#include <thread>
#include <unordered_map>
#include <deque>

#include "concurrentqueue/blockingconcurrentqueue.h"

#include "sink.h"
#include "CommandSource.h"

class SDLDisplay : public Sink<AVFrame>, public CommandSource {
private:
    bool NV12;

    SDL_Window *screen;
    SDL_Renderer *renderer;
    SDL_Texture *texture_rgb;
    SDL_Texture *texture_yuv420;
    SDL_Texture *texture_nv12;
    SDL_Rect rectangle;
    SDL_Event event;
    std::unordered_set<SDL_GameController*> gamepads;

    bool display_stop_condition = true;
    std::thread display_thread;
    moodycamel::BlockingConcurrentQueue<AVFrame*> video_frame_queue;

    bool audio_stop_condition = true;
    std::thread audio_thread;
    moodycamel::BlockingConcurrentQueue<AVFrame*> audio_frame_queue;

    bool event_stop_condition = true;
    std::thread event_thread;

public:
    SDLDisplay();
    ~SDLDisplay() override;

    void startDisplay();
    void runDisplay();
    void stopDisplay();

    void startAudio();
    void runAudio();
    void stopAudio();

    void startListening();
    void listenEvent();
    void stopListening();

    void handle(AVFrame *frame) override;
};

#endif //REMOTE_DESKTOP_SDLDISPLAY_H
