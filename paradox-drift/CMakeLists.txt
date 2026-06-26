add_executable(paradox-drift
    src/main.c
    src/asteroids_game.c
)

target_include_directories(paradox-drift PRIVATE
    include
)

target_link_libraries(paradox-drift
    pico_stdlib
    hardware_timer
    common_display
    common_input
)

pico_enable_stdio_usb(paradox-drift 1)
pico_enable_stdio_uart(paradox-drift 0)

# create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(paradox-drift)
