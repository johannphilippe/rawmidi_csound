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

add_executable(rawmidi_test test.cpp)
target_include_directories(rawmidi_test PUBLIC ${CSOUND_INCLUDE_DIRS})
target_include_directories(rawmidi_test PUBLIC "rtmidi/RtMidi.h")
target_link_libraries(rawmidi_test PUBLIC rawmidi)
target_link_libraries(rawmidi_test PUBLIC ${CSOUND_LIBRARY})

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/test.csd DESTINATION ${CMAKE_BINARY_DIR})

enable_testing()
