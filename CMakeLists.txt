cmake_minimum_required(VERSION 3.15)
project(tconnect VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(tconnect STATIC
    src/url.c
    src/transport_tcp.c
    src/transport_udp.c
    src/async.c
    src/transport_http.c
    src/transport_tls.c
    src/transport_ws.c
)

# TLS backend - OpenSSL.
# to swap: replace find_package + target_link_libraries with another TLS library.
find_package(OpenSSL REQUIRED)
target_link_libraries(tconnect PRIVATE OpenSSL::SSL OpenSSL::Crypto)

target_include_directories(tconnect
    PUBLIC  include
    PRIVATE src
)

target_compile_options(tconnect PRIVATE -Wall -Wextra -Wpedantic)

enable_testing()
add_subdirectory(tests)
add_subdirectory(sandbox)
add_subdirectory(examples)
