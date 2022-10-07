#ifdef Debug
#include <unistd.h>
#endif

#include <unistd.h>
#include <iostream>
#include <sstream>
#include <chrono>
#include <unordered_set>

#include "SDLDisplay.h"
#include "exception.h"

constexpr int32_t LOOP_MIN_TIME = 8; // max 125Hz, most common polling freq

void fill_audio(void *userdata, Uint8 *stream, int len) {
    //std::cout << sample_queue.size_approx() << std::endl;
    /*while (sample_queue.size_approx() > 2 * len) {
        sample_queue.try_dequeue_bulk(stream, len);
    }*/
    //if (ssize_t extra_size = ((moodycamel::ConcurrentQueue<uint8_t>*)userdata)->size_approx() - (40 * 48 * 2 * sizeof(float)); extra_size > 0) {
    //    ((moodycamel::ConcurrentQueue<uint8_t>*)userdata)->try_dequeue_bulk(stream, extra_size);
    //}

    size_t size = ((moodycamel::ConcurrentQueue<uint8_t>*)userdata)->try_dequeue_bulk(stream, len);
    memset(stream + size, 0, len - size);
}

SDLDisplay::SDLDisplay() : name("sdl display"), audio_frame_queue(4), sample_queue(8192), video_frame_queue(8) {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_TIMER);
}

SDLDisplay::~SDLDisplay() {
    stop();
    SDL_DestroyTexture(texture_yuv420);
    SDL_DestroyTexture(texture_nv12);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(screen);
    for (SDL_GameController* gamepad : gamepads) {
        SDL_GameControllerClose(gamepad);
    }
    SDL_CloseAudioDevice(dev);
}

void SDLDisplay::init(AVCodecContext *audio_ctx, AVCodecContext *video_ctx) {
    SDL_DestroyWindow(screen);
    screen = SDL_CreateWindow("Remote Desktop Client",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,
                              1280, 720,SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

    SDL_DestroyRenderer(renderer);
    renderer = SDL_CreateRenderer(screen, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE);
    SDL_RenderSetViewport(renderer, NULL);

    //SDL_RenderSetIntegerScale(renderer, SDL_TRUE);
    const char scale_mode = SDL_ScaleModeLinear;
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, &scale_mode);

    //texture_rgb = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, 1920, 1080);
    SDL_DestroyTexture(texture_yuv420);
    texture_yuv420 = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STREAMING, video_ctx->width, video_ctx->height);
    SDL_SetTextureBlendMode(texture_yuv420, SDL_BLENDMODE_NONE);
    SDL_DestroyTexture(texture_nv12);
    texture_nv12 = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_NV12, SDL_TEXTUREACCESS_STREAMING, video_ctx->width, video_ctx->height);
    SDL_SetTextureBlendMode(texture_nv12, SDL_BLENDMODE_NONE);

    SDL_AudioSpec wanted;
    SDL_zero(wanted);
    SDL_zero(given);
    wanted.format = AUDIO_F32SYS;
    wanted.freq = audio_ctx->sample_rate;
    wanted.channels = audio_ctx->channels;
    wanted.samples = 512;
    wanted.callback = nullptr;
    //wanted.callback = fill_audio;
    //wanted.userdata = &sample_queue;

    SDL_CloseAudioDevice(dev);
    dev = SDL_OpenAudioDevice(NULL, 0, &wanted, &given, 0);
    if (dev <= 0) {
        std::cout << "audio error" << std::endl;
    }

    std::cerr << "audio open with the given values: " << given.freq << "Hz, " << (int)given.channels << "ch, buffer total size is " << given.size << " bytes" << std::endl;
    SDL_PauseAudioDevice(dev, 0);
}

void SDLDisplay::initAudio(AVCodecContext *audio_ctx) {
    SDL_AudioSpec wanted;
    SDL_zero(wanted);
    SDL_zero(given);
    wanted.format = AUDIO_F32SYS;
    wanted.freq = audio_ctx->sample_rate;
    wanted.channels = audio_ctx->channels;
    wanted.samples = 512;
    wanted.callback = nullptr;
    //wanted.callback = fill_audio;
    //wanted.userdata = &sample_queue;

    SDL_CloseAudioDevice(dev);
    dev = SDL_OpenAudioDevice(NULL, 0, &wanted, &given, 0);
    if (dev <= 0) {
        std::cout << "audio error" << std::endl;
    }

    std::cerr << "audio open with the given values: " << given.freq << "Hz, " << (int)given.channels << "ch, buffer total size is " << given.size << " bytes" << std::endl;
    SDL_PauseAudioDevice(dev, 0);
}

void SDLDisplay::initVideo(AVCodecContext *video_ctx) {
    SDL_DestroyWindow(screen);
    screen = SDL_CreateWindow("Remote Desktop Client",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,
                              1280, 720,SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

    SDL_DestroyRenderer(renderer);
    renderer = SDL_CreateRenderer(screen, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE);
    SDL_RenderSetViewport(renderer, NULL);

    //SDL_RenderSetIntegerScale(renderer, SDL_TRUE);
    const char scale_mode = SDL_ScaleModeLinear;
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, &scale_mode);

    //texture_rgb = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, 1920, 1080);
    SDL_DestroyTexture(texture_yuv420);
    texture_yuv420 = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STREAMING, video_ctx->width, video_ctx->height);
    SDL_SetTextureBlendMode(texture_yuv420, SDL_BLENDMODE_NONE);
    SDL_DestroyTexture(texture_nv12);
    texture_nv12 = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_NV12, SDL_TEXTUREACCESS_STREAMING, video_ctx->width, video_ctx->height);
    SDL_SetTextureBlendMode(texture_nv12, SDL_BLENDMODE_NONE);
}

void SDLDisplay::start() {
    startDisplay();
    startAudio();
    startEvent();
}

void SDLDisplay::stop() {
    stopEvent();
    stopAudio();
    stopDisplay();
}

void SDLDisplay::startDisplay() {
    if (display_stop_condition) {
        display_stop_condition = false;
        display_thread = std::thread(&SDLDisplay::runDisplay, this);
    }
}

static inline uint32_t log_2(const uint32_t x) {
    return x ? (31 - __builtin_clz (x)) : 0;
}

void SDLDisplay::runDisplay() {
    Uint32 start = SDL_GetTicks();
    Uint32 last = 0;
    int delay = 0;
    int i = 0;
    int j = 0;
    AVFrame *frame;
    uint64_t calculated_next_pts = 0;
    while (!display_stop_condition) {
        if (!video_frame_queue.wait_dequeue_timed(frame, std::chrono::milliseconds(100))) {
            continue;
        }

        if (int64_t wait_ticks = calculated_next_pts - frame->pts; wait_ticks > 0) {
            std::cout << "need to wait " << wait_ticks / 90 << "ms before present next frame" << std::endl;
            SDL_Delay(wait_ticks / 90); // in milliseconds, rtp sample rate = 90000Hz
        }

        calculated_next_pts = frame->pts + frame->pkt_duration;
        int64_t presentation_time = frame->pkt_duration / 90 - log_2(video_frame_queue.size_approx());
        displayImpl(frame);
        av_frame_free(&frame);
        ++j;
        if (SDL_GetTicks() - start >= 1000) {
            start = SDL_GetTicks();
            std::stringstream ss;
            ss << "Remote Desktop Client (framerate = " << j - i << " fps, pipeline latency = " << ((j - i > 0) ? delay / (j - i) : -1) << " ms)";
            SDL_SetWindowTitle(screen, ss.str().c_str());
            i = j;
            delay = 0;
        }

        //std::cout << "frame will be displayed " << presentation_time << "ms, " << video_frame_queue.size_approx() << " waiting" << std::endl;
        //SDL_Delay(12);
        SDL_Delay(presentation_time > 0 ? presentation_time : 0);
    }
}

void SDLDisplay::stopDisplay() {
    if (!display_stop_condition) {
        display_stop_condition = true;
        if (display_thread.joinable()) {
            display_thread.join();
        }
    }
}

void SDLDisplay::startAudio() {
    if (audio_stop_condition) {
        audio_stop_condition = false;
        audio_thread = std::thread(&SDLDisplay::runAudio, this);
    }
}

void SDLDisplay::runAudio() {
    /*SwrContext *swr_ctx = swr_alloc();
    //swr_alloc_set_opts(NULL, av_get_default_channel_layout(2), AV_SAMPLE_FMT_S16, 48000,
    //                   av_get_default_channel_layout(2), AV_SAMPLE_FMT_FLTP, 48000, 0, NULL);
    av_opt_set_int(swr_ctx, "in_channel_layout", av_get_default_channel_layout(2), 0);
    av_opt_set_int(swr_ctx, "in_sample_rate", 48000, 0);
    av_opt_set_sample_fmt(swr_ctx, "in_sample_fmt", AV_SAMPLE_FMT_FLTP, 0);
    av_opt_set_int(swr_ctx, "out_channel_layout", av_get_default_channel_layout(2), 0);
    av_opt_set_int(swr_ctx, "out_sample_rate", 48000, 0);
    av_opt_set_sample_fmt(swr_ctx, "out_sample_fmt", AV_SAMPLE_FMT_FLT, 0);
    if (swr_init(swr_ctx) < 0) {
        std::cout << "converter init fail" << std::endl;
        exit(-1);
    }*/

    AVFrame *frame;
    /*AVFrame *frame_out = av_frame_alloc();
    frame_out->channels = 2;
    frame_out->channel_layout = av_get_default_channel_layout(frame_out->channels);
    frame_out->format = AV_SAMPLE_FMT_FLT;
    frame_out->sample_rate = 48000;*/
    //uint8_t data[frame_out->channels * 2048 * sizeof(float)];

    try {
        while (!audio_stop_condition) {
            if (!audio_frame_queue.wait_dequeue_timed(frame, std::chrono::milliseconds(100))) {
                continue;
            }

            //int size = swr_convert(swr_ctx, (uint8_t**)&data, 512, (const uint8_t**)frame_in->data, frame_in->linesize[0]);
            audioImpl(frame);
        }
    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
    }

    av_frame_free(&frame);
    SDL_CloseAudioDevice(dev);
}

void SDLDisplay::stopAudio() {
    if (!audio_stop_condition) {
        audio_stop_condition = true;
        if (audio_thread.joinable()) {
            audio_thread.join();
        }
    }
}

void SDLDisplay::startEvent() {
    if (event_stop_condition) {
        event_stop_condition = false;
        event_thread = std::thread(&SDLDisplay::runEvent, this);
    }
}

void SDLDisplay::runEvent() {
#ifdef DEBUG
    std::cerr << "input listening thread is " << gettid() << std::endl;
#endif

    std::unordered_set<int> keyup;
    std::unordered_set<int> keydown;
    float x = 0;
    float y = 0;
    int wx = 0;
    int wy = 0;
    unsigned char mouse_button_states = 0;
    unsigned char last_mouse_button_states = 0;
    std::array<int16_t, 6> gamepad_axis = {0};
    uint32_t gamepad_button_states = 0;
    uint32_t last_gamepad_button_states = 0;
    bool relative = false;
    bool lock = false;
    int last_key;
    int count = 50;

    std::stringstream ss;
    while (!event_stop_condition) {
        Uint32 start = SDL_GetTicks();
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_CONTROLLERDEVICEADDED: {
                    if (SDL_GameController *gamepad = SDL_GameControllerOpen(event.cdevice.which)) {
                        gamepads.insert(gamepad);
                    } else {
                        std::cerr << SDL_GetError() << std::endl;
                    }

                    break;
                }
                case SDL_CONTROLLERDEVICEREMOVED: {
                    if (SDL_GameController *gamepad = SDL_GameControllerFromInstanceID(event.cdevice.which)) {
                        gamepads.erase(gamepad);
                    } else {
                        std::cerr << SDL_GetError() << std::endl;
                    }

                    break;
                }
                case SDL_CONTROLLERDEVICEREMAPPED: {
                    std::cout << "controller remap event" << std::endl;
                    break;
                }
                case SDL_KEYDOWN: {
                    if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE && last_key == SDL_SCANCODE_ESCAPE) {
                        if (--count <= 0 && SDL_SetRelativeMouseMode(!lock ? SDL_TRUE : SDL_FALSE) == 0) {
                            lock ^= true;
                            relative ^= true;
                            std::cout << "relative mode" << std::endl;
                            count = 50;
                        }
                    } else {
                        count = 50;
                    }
                    last_key = event.key.keysym.scancode;
                    keydown.emplace(event.key.keysym.scancode);
                    keyup.erase(event.key.keysym.scancode);
                    break;
                }
                case SDL_KEYUP: {
                    keyup.emplace(event.key.keysym.scancode);
                    keydown.erase(event.key.keysym.scancode);
                    break;
                }
                case SDL_MOUSEMOTION: {
                    if (relative) {
                        x += event.motion.xrel;
                        y += event.motion.yrel;
                    } else {
                        int max_x, max_y;
                        SDL_GetWindowSize(screen, &max_x, &max_y);
                        x = event.motion.x / (max_x - 1.f);
                        y = event.motion.y / (max_y - 1.f);
                    }
                    break;
                }
                case SDL_MOUSEBUTTONDOWN: {
                    mouse_button_states |= 1U << (event.button.button - 1);
                    break;
                }
                case SDL_MOUSEBUTTONUP: {
                    mouse_button_states &= ~(1U << (event.button.button - 1));
                    break;
                }
                case SDL_MOUSEWHEEL: {
                    wx = event.wheel.x;
                    wy = event.wheel.y;
                    break;
                }
                case SDL_CONTROLLERAXISMOTION: {
                    if (event.jaxis.axis >= 0 && event.jaxis.axis <= 5) {
                        gamepad_axis[event.jaxis.axis] = event.jaxis.value;
                    }
                    break;
                }
                case SDL_CONTROLLERBUTTONDOWN: {
                    if (event.jbutton.button >= 0 && event.jbutton.button <= 14) {
                        gamepad_button_states |= 1U << (event.jbutton.button);
                    } else {
                        std::cout << "button not mapped" << std::endl;
                    }
                    break;
                }
                case SDL_CONTROLLERBUTTONUP: {
                    if (event.jbutton.button >= 0 && event.jbutton.button <= 14) {
                        gamepad_button_states  &= ~(1U << event.jbutton.button);
                    } else {
                        std::cout << "button not mapped" << std::endl;
                    }
                    break;
                }
            }
        }

        // build JSON
        ss << R"({"t":"i")";
        if (!keyup.empty() || !keydown.empty()) {
            ss << R"(,"k":[[)";
            bool comma = false;
            for (int key: keyup) {
                ss << (comma ? "," : "") << key;
                comma = true;
            }
            ss << "],[";
            comma = false;
            for (int key: keydown) {
                ss << (comma ? "," : "") << key;
                comma = true;
            }
            ss << "]]";
            keyup.clear();
        }

        if (x != 0 || y != 0) {
            ss << R"(,"m":[)" << x << ',' << y << ']';
            x = 0;
            y = 0;
        }

        //if (mouse_button_states != last_mouse_button_states) {
            ss << R"(,"b":)" << (int) mouse_button_states;
            last_mouse_button_states = mouse_button_states;
        //}

        if (wx || wy) {
            ss << R"(,"w":[)" << wx << ',' << wy << ']';
            wx = 0;
            wy = 0;
        }

        //if (std::find_if(gamepad_axis.begin(), gamepad_axis.end(), [](auto e) { return e != 0; }) != gamepad_axis.end()) {
            ss << R"(,"a":[)" << (int) gamepad_axis[0] << ',' << (int) gamepad_axis[1] << ',' << (int) gamepad_axis[2]
               << ',' << (int) gamepad_axis[3] << ',' << (int) gamepad_axis[4] << ',' << (int) gamepad_axis[5] << ']';
        //}

        //if (gamepad_button_states != last_gamepad_button_states) {
            ss << R"(,"c":)" << (int) gamepad_button_states;
            last_gamepad_button_states = gamepad_button_states;
        //}

        ss << '}';

        const std::string &json = ss.str();
        if (json.size() > 9) { // no command == R"({"t":"i"})";
            //std::cout << json << std::endl;
            for (const auto &command_sink: command_sinks) {
                command_sink->handle(json);
            }
        }

        ss.clear();
        ss.str(std::string());

        int32_t time = LOOP_MIN_TIME + start - SDL_GetTicks();
        if (time > 0) {
            //std::cout << "wait " << time << std::endl;
            SDL_Delay(time);
        }
    }
}

void SDLDisplay::stopEvent() {
    if (!event_stop_condition) {
        event_stop_condition = true;
        if (event_thread.joinable()) {
            event_thread.join();
        }
    }
}

void SDLDisplay::handle(AVFrame *frame) {
    if (frame->width == 0) {
        if (audio_thread.joinable()) {
            if (!audio_frame_queue.try_enqueue(frame)) {
                std::cout << name << ": audio queue full, drop" << std::endl;
                av_frame_free(&frame);
            }
        } else {
            audioImpl(frame);
            av_frame_free(&frame);
        }
    } else {
        if (display_thread.joinable()) {
            if (!video_frame_queue.try_enqueue(frame)) {
                std::cout << name << ": video queue full, drop" << std::endl;
                av_frame_free(&frame);
            }
        } else {
            displayImpl(frame);
            av_frame_free(&frame);
        }
    }
}

void SDLDisplay::displayImpl(AVFrame *frame) {
    SDL_Texture *texture;
    int ret;
    switch (frame->format) {
/*            case AV_PIX_FMT_GBRP:
                Uint8 *pixels;
                int pitch;
                SDL_LockTexture(texture = texture_rgb, NULL, (void**)&pixels, &pitch);
                for (int k = 0; k < 100; ++k) {
                    memcpy(pixels + 3 * k * pitch, frame->data[0] + k * pitch, pitch);
                    memcpy(pixels + (3 * k + 1) * pitch, frame->data[1] + k * pitch, pitch);
                    memcpy(pixels + (3 * k + 2) * pitch, frame->data[2] + k * pitch, pitch);
                }

                SDL_UnlockTexture(texture);
                break;*/
        case AV_PIX_FMT_YUV420P:
            ret = SDL_UpdateYUVTexture(texture = texture_yuv420, NULL,
                                       frame->data[0], frame->linesize[0],
                                       frame->data[1], frame->linesize[1],
                                       frame->data[2], frame->linesize[2]);
            break;
        case AV_PIX_FMT_NV12:
            ret = SDL_UpdateNVTexture(texture = texture_nv12, NULL,
                                      frame->data[0], frame->linesize[0],
                                      frame->data[1], frame->linesize[1]);
            break;
        default:
            char buffer[32];
            std::cout << "no rule for " << av_fourcc_make_string(buffer, avcodec_pix_fmt_to_codec_tag((AVPixelFormat)frame->format)) << std::endl;
            break;
    }

    if (ret < 0) {
        std::cout << SDL_GetError() << std::endl;
    } else {
        //SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
    }
}

void SDLDisplay::audioImpl(AVFrame *frame) {
    // for callback
    // planar to interleaved audio ([a1,a2], [b1,b2]) -> (a1,b1,a2,b2)
    /*for (int i = 0; i < frame->nb_samples; ++i) {
        for (int c = 0; c < given.channels; ++c) {
            sample_queue.enqueue_bulk(frame->data[c] + sizeof(float) * i, sizeof(float));
        }
    }*/

    if (SDL_GetQueuedAudioSize(dev) > 8 * given.size) {
        SDL_ClearQueuedAudio(dev);
    }

    for (int i = 0; i < frame->nb_samples; ++i) {
        for (int c = 0; c < given.channels; ++c) {
            SDL_QueueAudio(dev, frame->data[c] + sizeof(float) * i, sizeof(float));
        }
    }
}