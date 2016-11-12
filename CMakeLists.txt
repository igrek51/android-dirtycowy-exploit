cmake_minimum_required(VERSION 3.6)
project(dirtycowy)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES src/dirtycowy.c src/run-as.c)
set(CMAKE_BINARY_DIR /home/igrek/dev/dirtycowy/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})

find_package (Threads)

add_executable(dirtycowy src/dirtycowy.c)
add_executable(run-as src/run-as.c)

target_link_libraries(dirtycowy ${CMAKE_THREAD_LIBS_INIT})
