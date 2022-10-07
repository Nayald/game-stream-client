cmake_minimum_required(VERSION 3.16)
project(remote_client)

set(CMAKE_CXX_STANDARD 17)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE debug)
endif()

set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "-O0")

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBAV REQUIRED IMPORTED_TARGET
        libavdevice
        libavfilter
        libavformat
        libavcodec
        libswresample
        libswscale
        libavutil
        )

find_package(SDL2 REQUIRED) # expected 2.0.16+ for SDL_UpdateNVTexture
include_directories(${SDL2_INCLUDE_DIRS})

find_package(Threads REQUIRED)

add_executable(remote_client
        main.cpp
        source.cpp source.h sink.h
        SDLDisplay.cpp SDLDisplay.h
        RTPAudioReceiver.cpp RTPAudioReceiver.h RTPVideoReceiver.cpp RTPVideoReceiver.h
        CommandSocket.cpp CommandSocket.h CommandSource.h CommandSink.h
        simdjson/singleheader/simdjson.cpp simdjson/singleheader/simdjson.h spinlock.h)

target_link_libraries(remote_client PkgConfig::LIBAV ${SDL2_LIBRARIES} Threads::Threads)
