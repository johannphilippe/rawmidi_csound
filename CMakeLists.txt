cmake_minimum_required(VERSION 3.5)

project(rawmidi LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}
"${CMAKE_SOURCE_DIR}/cmake/Modules/")
find_package(Csound REQUIRED)

add_subdirectory("rtmidi")

add_library(rawmidi SHARED rawmidi.cpp)
target_include_directories(rawmidi PRIVATE ${CSOUND_INCLUDE_DIRS})
target_include_directories(rawmidi PRIVATE "rtmidi/RtMidi.h")
target_link_libraries(rawmidi PUBLIC rtmidi)
