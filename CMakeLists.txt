cmake_minimum_required(VERSION 3.15)
project(tconnect VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(tconnect STATIC
    src/transport_tcp.c
    src/async.c
)

target_include_directories(tconnect
    PUBLIC  include
    PRIVATE src
)

target_compile_options(tconnect PRIVATE -Wall -Wextra -Wpedantic)

enable_testing()
add_subdirectory(tests)
add_subdirectory(sandbox)
