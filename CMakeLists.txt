cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(dht_example C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_subdirectory(dht)

add_executable(dht_example dht_example.c)

pico_enable_stdio_uart(dht_example 1)
pico_enable_stdio_usb(dht_example 1)

pico_add_extra_outputs(dht_example)

target_compile_options(dht_example PRIVATE -Wall)

target_link_libraries(dht_example dht pico_stdlib)
