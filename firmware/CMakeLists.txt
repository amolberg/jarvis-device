cmake_minimum_required(VERSION 3.16)

set(CMAKE_SYSTEM_NAME ESP-IDF)
set(CMAKE_C_COMPILER xtensa-esp32-elf-gcc)
set(CMAKE_CXX_COMPILER xtensa-esp32-elf-g++)

# Project name
project(jarvis-device-firmware)

# Set component directories
set(EXTRA_COMPONENT_DIRS components)

# Get IDF_PATH from environment or default location
if(NOT DEFINED ENV{IDF_PATH})
    set(IDF_PATH "$ENV{HOME}/esp/esp-idf")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Build configuration
idf_component_register(
    SRCS
        src/main.cpp
        src/wifi.cpp
        src/mqtt.cpp
        src/audio.cpp
        src/led.cpp
        src/http_stream.cpp
    INCLUDE_DIRS
        include
    REQUIRES
        esp_wifi
        esp_http_client
        esp_websocket_client
        esp_dsp
        nvs_flash
        driver
        freertos
)
