cmake_minimum_required(VERSION 3.15)
project(screenoff-anim-ksu CXX)
set(CMAKE_CXX_STANDARD 20)

add_library(screenoff-anim SHARED src/main.cpp)
target_link_libraries(screenoff-anim log dl)
target_compile_options(screenoff-anim PRIVATE -Wall -O2 -fno-rtti -fno-exceptions)