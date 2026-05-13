cmake_minimum_required(VERSION 3.22.1)
project(voiceflux VERSION 1.0.0 LANGUAGES CXX)

# Oboe via Prefab (comes from the Gradle AAR)
find_package(oboe REQUIRED CONFIG)

add_library(voiceflux SHARED
    jni_bridge.cpp
    audio_engine.cpp
    voice_processor.cpp
    effects/pitch_shifter.cpp
    effects/formant_shifter.cpp
    effects/reverb.cpp
    effects/distortion.cpp
    effects/eq_filter.cpp
    effects/robot_effect.cpp
)

target_include_directories(voiceflux PRIVATE ${CMAKE_SOURCE_DIR})

target_compile_options(voiceflux PRIVATE
    -std=c++17
    -O3
    -ffast-math
    -Wall
)

target_link_libraries(voiceflux
    oboe::oboe
    android
    log
    OpenSLES
    atomic
)
