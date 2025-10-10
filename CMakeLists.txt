cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

include(pico_sdk_import.cmake)

project(pico-gb-cartridge C CXX ASM)

pico_sdk_init()


add_executable(pico-gb-cartridge
    main.c
    bus.c
    launcher.c
)

target_compile_definitions(pico-gb-cartridge PRIVATE
  PICO_DEFAULT_UART=0
  PICO_DEFAULT_UART_TX_PIN=44
  PICO_DEFAULT_UART_RX_PIN=45

  #ENABLE_UART=1

  ENABLE_BUS=1
  #NO_LOAD=1
  #LOAD_NO_BANKS=1
  #LOAD_BANKS_16K=1
  LOAD_BANKS_4K=1
)

pico_set_program_name(pico-gb-cartridge "pico-gb-cartridge")
pico_set_program_version(pico-gb-cartridge "0.1")

pico_enable_stdio_uart(pico-gb-cartridge 1)
pico_enable_stdio_usb(pico-gb-cartridge 0)

target_link_libraries(pico-gb-cartridge pico_stdlib hardware_xip_cache)

pico_add_extra_outputs(pico-gb-cartridge)
