cmake_minimum_required(VERSION 3.15)
project(screenoff-anim-ksu CXX)
set(CMAKE_CXX_STANDARD 20)

set(DOBBY_ENABLE_CLOSURE_TRAMPOLINE OFF CACHE BOOL "" FORCE)
add_subdirectory(external/Dobby dobby)

add_library(screenoff-anim SHARED src/main.cpp)
target_include_directories(screenoff-anim PRIVATE external/Dobby/include)
target_link_libraries(screenoff-anim dobby log dl)
target_compile_options(screenoff-anim PRIVATE -Wall -O2 -fno-rtti -fno-exceptions)