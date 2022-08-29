cmake_minimum_required(VERSION 3.13)

# initialize the SDK directly
include($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)
project(10baset_raspico)
set(target_name 10baset_raspico)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

file(GLOB SRC_FILES src/*.c src/**/*.c)
add_executable(${target_name} ${SRC_FILES})

target_include_directories(${target_name} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>)

pico_generate_pio_header(${target_name} ${CMAKE_CURRENT_LIST_DIR}/src/eth_phy/eth_phy_tx.pio)
pico_generate_pio_header(${target_name} ${CMAKE_CURRENT_LIST_DIR}/src/eth_phy/eth_phy_rx.pio)

# Add pico_stdlib library which aggregates commonly used features
target_link_libraries(${target_name} PRIVATE
  pico_stdlib
  pico_stdio
  pico_lwip
  hardware_pio
  hardware_irq
  hardware_dma
  )

pico_enable_stdio_usb(${target_name} 1)
pico_enable_stdio_uart(${target_name} 0)

# create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(${target_name})
