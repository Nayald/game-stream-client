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
    std::string name;
    bool initialized = false;

    bool NV12;

    SDL_Window *screen = nullptr;
    SDL_Renderer *renderer = nullptr;
    SDL_Texture *texture_rgb = nullptr;
    SDL_Texture *texture_yuv420 = nullptr;
    SDL_Texture *texture_nv12 = nullptr;
    SDL_Event event;
    std::unordered_set<SDL_GameController*> gamepads;



    bool display_stop_condition = true;
    std::thread display_thread;
    moodycamel::BlockingConcurrentQueue<AVFrame*> video_frame_queue;

    bool audio_stop_condition = true;
    std::thread audio_thread;
    SDL_AudioDeviceID dev;
    SDL_AudioSpec given;
    moodycamel::ConcurrentQueue<uint8_t> sample_queue;
    moodycamel::BlockingConcurrentQueue<AVFrame*> audio_frame_queue;

    bool event_stop_condition = true;
    std::thread event_thread;

public:
    SDLDisplay();
    ~SDLDisplay() override;


    void init(AVCodecContext *audio_ctx, AVCodecContext *video_ctx);
    void initAudio(AVCodecContext *audio_ctx);
    void initVideo(AVCodecContext *video_ctx);

    void start();
    void stop();

    void startDisplay();
    void runDisplay();
    void stopDisplay();

    void startAudio();
    void runAudio();
    void stopAudio();

    void startEvent();
    void runEvent();
    void stopEvent();

    void handle(AVFrame *frame) override;

private:
    void displayImpl(AVFrame *frame);
    void audioImpl(AVFrame *frame);
};

#endif //REMOTE_DESKTOP_SDLDISPLAY_H
