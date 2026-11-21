# instrdump: a development tool, built on its own, apart from the apps:
#
#     cmake -S tools/instrdump -B build/instrdump -G Ninja -DCMAKE_BUILD_TYPE=Release
#     cmake --build build/instrdump --parallel
#
# It reads modules through the parsers of the zxtune_aym library, whose headers are private to it.
cmake_minimum_required(VERSION 3.22)
project(instrdump CXX)

set(ZXTUNE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/zxtune)
add_subdirectory(${ZXTUNE_DIR} zxtune)

add_executable(instrdump
    Json.cpp
    Main.cpp
    Tracks.cpp
    formats/ASCSoundMaster.cpp
    formats/FastTracker.cpp
    formats/GlobalTracker.cpp
    formats/ProSoundCreator.cpp
    formats/ProSoundMaker.cpp
    formats/ProTracker1.cpp
    formats/ProTracker2.cpp
    formats/ProTracker3.cpp
    formats/SQTracker.cpp
    formats/SoundTracker.cpp
    formats/SoundTrackerPro.cpp
)

target_compile_features(instrdump PRIVATE cxx_std_20)
target_include_directories(instrdump PRIVATE
    ${ZXTUNE_DIR}/include
    ${ZXTUNE_DIR}/src
    ${ZXTUNE_DIR}/3rdparty/fmt/include
)
# As zxtune_aym is built: its headers assume both.
target_compile_definitions(instrdump PRIVATE SOURCES_ROOT="${ZXTUNE_DIR}")
target_compile_options(instrdump PRIVATE -funsigned-char -fno-strict-aliasing -Wall -Wextra)
target_link_libraries(instrdump PRIVATE zxtune_aym)
