add_executable(snake
    src/main.c
    src/snake_game.c
)

target_include_directories(snake PRIVATE
    include
)

target_link_libraries(snake
    pico_stdlib
    hardware_timer
    common_display
    common_input
)

pico_enable_stdio_usb(snake 1)
pico_enable_stdio_uart(snake 0)

# create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(snake)
