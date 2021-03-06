cmake_minimum_required(VERSION 3.8)
project(Orion)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/../cmake")
find_package(SDL2 REQUIRED)

include_directories(${SDL2_INCLUDE_DIR})
include_directories(/usr/local/include)

find_library(VORBIS_LIBRARY NAMES vorbis)
find_library(VORBISFILE_LIBRARY NAMES vorbisfile)
find_library(SOUNDTOUCH_LIBRARY NAMES soundtouch)
find_library(IIR_LIBRARY NAMES iir)
find_library(PORTAUDIO_LIBRARY NAMES portaudio)

add_library(orion STATIC libs_wrapper.cpp orion.c)
target_link_libraries(orion ${SDL2_LIBRARY} ${VORBIS_LIBRARY} ${VORBISFILE_LIBRARY} ${SOUNDTOUCH_LIBRARY} ${IIR_LIBRARY} ${PORTAUDIO_LIBRARY})

if (BUILD_ORION_TESTS)
    add_executable(orion_playplay tests/playplay.c)
    target_link_libraries(orion_playplay orion)
    add_executable(orion_rhythm tests/rhythm.c)
    target_link_libraries(orion_rhythm orion)
    add_executable(orion_stress tests/stress.c)
    target_link_libraries(orion_stress orion)
endif (BUILD_ORION_TESTS)
