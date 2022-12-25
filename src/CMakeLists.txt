add_executable(RasPicoDacFg
        sys_rp2040.c fg_commands.c com_ctrl.c
        )


# pico_set_binary_type(sys_rp2040 copy_to_ram)

# pull in common dependencies
target_link_libraries(RasPicoDacFg pico_stdlib pico_multicore)

# enable usb output, disable uart output
pico_enable_stdio_usb(RasPicoDacFg 1)
pico_enable_stdio_uart(RasPicoDacFg 0)

# create map/bin/hex file etc.
pico_add_extra_outputs(RasPicoDacFg)

# add url via pico_set_program_url
example_auto_set_url(RasPicoDacFg)
