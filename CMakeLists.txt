#  Pico Build Action - Orinayo CMake config.
#  Copyright 2024 Samyar Sadat Akhavi
#  Written by Samyar Sadat Akhavi, 2024.
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <https: www.gnu.org/licenses/>.


# Set minimum required version of CMake
cmake_minimum_required(VERSION 3.12)

# Set Pico Board and Pico Platform
set(PICO_PLATFORM rp2350-arm-s)
set(PICO_BOARD pico2_w)
set(RING_BUFFER_PATH ${CMAKE_CURRENT_LIST_DIR}/ring_buffer_lib)
set(PICO_BLE_MIDI_PATH ${CMAKE_CURRENT_LIST_DIR}/pico-w-ble-midi-lib) 
set(PICO_PIO_USB_PATH ${CMAKE_CURRENT_LIST_DIR}/pico_pio_usb) 
set(PICO_TINYUSB_PATH ${CMAKE_CURRENT_LIST_DIR}/tinyusb) 
set(FAMILY rp2040)

# Include build functions from the Pico SDK
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)
include(${PICO_TINYUSB_PATH}/hw/bsp/family_support.cmake)
include(${PICO_TINYUSB_PATH}/hw/bsp/rp2040/family.cmake)

# Set project name and C/C++ standards
string(TIMESTAMP DATEVER "%Y.%m.%d" UTC)
project(Orinayo VERSION ${DATEVER} LANGUAGES C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the Pico SDK
pico_sdk_init()

add_subdirectory(${PICO_TINYUSB_PATH}/src)
add_subdirectory(${RING_BUFFER_PATH})
add_subdirectory(${PICO_BLE_MIDI_PATH})
add_subdirectory(${PICO_PIO_USB_PATH})
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/bluepad32)

add_compile_definitions(
    PICO_DEFAULT_PIO_USB_DP_PIN=17
	CFG_TUH_RPI_PIO_USB=1
)

add_library(tinyusb_pico_pio_usb INTERFACE)
target_sources(tinyusb_device_base INTERFACE ${TOP}/src/portable/raspberrypi/pio_usb/dcd_pio_usb.c)
target_sources(tinyusb_host_base INTERFACE ${TOP}/src/portable/raspberrypi/pio_usb/hcd_pio_usb.c)
target_sources(tinyusb_pico_pio_usb INTERFACE ${PICO_PIO_USB_PATH}/src/pio_usb.c ${PICO_PIO_USB_PATH}/src/pio_usb_host.c ${PICO_PIO_USB_PATH}/src/pio_usb_device.c ${PICO_PIO_USB_PATH}/src/usb_crc.c)
target_include_directories(tinyusb_pico_pio_usb INTERFACE ${PICO_PIO_USB_PATH}/src)
target_link_libraries(tinyusb_pico_pio_usb INTERFACE hardware_dma hardware_pio pico_multicore)
target_compile_definitions(tinyusb_pico_pio_usb INTERFACE PIO_USB_USE_TINYUSB)

pico_generate_pio_header(tinyusb_pico_pio_usb ${PICO_PIO_USB_PATH}/src/usb_tx.pio)
pico_generate_pio_header(tinyusb_pico_pio_usb ${PICO_PIO_USB_PATH}/src/usb_rx.pio)

# Add source files 
add_library(orinayobt STATIC pico_bluetooth.c async_timer.c display.c storage.c)
target_include_directories(orinayobt PUBLIC ${CMAKE_CURRENT_LIST_DIR} ${PICO_TINYUSB_PATH}/src ${PICO_TINYUSB_PATH}/src/class/audio ${PICO_TINYUSB_PATH}/src/class/midi ${CMAKE_CURRENT_LIST_DIR}/bluepad32/include ${PICO_BLE_MIDI_PATH} ${RING_BUFFER_PATH} ${PICO_SDK_PATH}/lib/btstack/src ${CMAKE_CURRENT_LIST_DIR}/pico_pio_usb/src)
target_link_libraries(orinayobt pico_stdlib hardware_i2c hardware_clocks pico_cyw43_arch_none pico_cyw43_arch_threadsafe_background tinyusb_device tinyusb_host tinyusb_board pico_btstack_classic pico_pio_usb tinyusb_pico_pio_usb pico_btstack_ble pico_btstack_cyw43 bluepad32 ring_buffer_lib)
add_compile_definitions(orinayobt PICO_CYW43_ARCH_THREADSAFE_BACKGROUND)

add_executable(${PROJECT_NAME} main.c usb_descriptors.c tap_tempo.c looper.c note_scheduler.c ghost_note.c)

pico_enable_stdio_usb(${PROJECT_NAME} 0)
pico_enable_stdio_uart(${PROJECT_NAME} 0)

target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_LIST_DIR} ${PICO_TINYUSB_PATH}/src ${PICO_TINYUSB_PATH}/src/class/audio ${PICO_TINYUSB_PATH}/src/class/midi ${CMAKE_CURRENT_LIST_DIR}/bluepad32/include ${PICO_BLE_MIDI_PATH} ${RING_BUFFER_PATH} ${PICO_SDK_PATH}/lib/btstack/src ${CMAKE_CURRENT_LIST_DIR}/pico_pio_usb/src)

# Create map/bin/hex/uf2 files
pico_add_extra_outputs(${PROJECT_NAME})

# Link to libraries
target_link_libraries(${PROJECT_NAME} pico_stdlib hardware_i2c hardware_clocks pico_multicore pico_cyw43_arch_none pico_cyw43_arch_threadsafe_background tinyusb_device tinyusb_host tinyusb_board pico_btstack_classic pico_pio_usb  tinyusb_pico_pio_usb pico_btstack_ble pico_btstack_cyw43 orinayobt)

# PicoTool binary information
pico_set_program_name(${PROJECT_NAME} ${PROJECT_NAME})
pico_set_program_version(${PROJECT_NAME} ${PROJECT_VERSION})
pico_set_program_description(${PROJECT_NAME} "Pico Build Action - Orinayo")

# CMake flags
set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -ffunction-sections -fdata-sections")
set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -ffunction-sections -fdata-sections")

