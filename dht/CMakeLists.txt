add_library(dht INTERFACE)

pico_generate_pio_header(dht ${CMAKE_CURRENT_LIST_DIR}/dht.pio)

target_include_directories(dht
    INTERFACE
    ./include)

target_sources(dht
    INTERFACE
    dht.c
)

target_link_libraries(dht
    INTERFACE
    hardware_clocks
    hardware_dma
    hardware_pio
)
