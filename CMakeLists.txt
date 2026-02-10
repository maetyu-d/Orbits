cmake_minimum_required(VERSION 3.22)

project(SpiralSynthTimeline VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(SPIRAL_TIMELINE_FETCH_JUCE "Fetch JUCE from GitHub if JUCE package is not found" ON)

find_package(JUCE CONFIG QUIET)

if (NOT JUCE_FOUND)
    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
        add_subdirectory(JUCE)
    elseif (SPIRAL_TIMELINE_FETCH_JUCE)
        include(FetchContent)
        FetchContent_Declare(
            juce
            GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG 8.0.5
        )
        FetchContent_MakeAvailable(juce)
    else()
        message(FATAL_ERROR "JUCE not found. Set JUCE_DIR or enable SPIRAL_TIMELINE_FETCH_JUCE.")
    endif()
endif()

juce_add_gui_app(SpiralSynthTimeline
    PRODUCT_NAME "Spiral Synth Timeline"
)

target_sources(SpiralSynthTimeline PRIVATE
    Source/Main.cpp
    Source/MainComponent.h
    Source/MainComponent.cpp
    Source/SpiralSequencerEngine.h
    Source/SpiralSequencerEngine.cpp
)

target_compile_definitions(SpiralSynthTimeline PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)

target_link_libraries(SpiralSynthTimeline PRIVATE
    juce::juce_gui_extra
    juce::juce_audio_utils
)

juce_generate_juce_header(SpiralSynthTimeline)
