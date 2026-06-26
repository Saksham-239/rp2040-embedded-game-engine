# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico CACHE STRING "Board type")

cmake_minimum_required(VERSION 3.13)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(pico_sdk_import.cmake)

project(rp2040_embedded_game_engine C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

if (PICO_SDK_VERSION_STRING VERSION_LESS "1.3.0")
    message(FATAL_ERROR "Pico SDK 1.3.0 or later required.")
endif()

pico_sdk_init()

# Define common libraries
add_library(common_display STATIC
    common/display/display_ssd1306.c
)
target_include_directories(common_display PUBLIC
    common/display
)
target_link_libraries(common_display
    pico_stdlib
    hardware_i2c
)

add_library(common_input STATIC
    common/input/input.c
)
target_include_directories(common_input PUBLIC
    common/input
)
target_link_libraries(common_input
    pico_stdlib
    hardware_gpio
)

# Add subdirectories
add_subdirectory(snake)
add_subdirectory(paradox-drift)
