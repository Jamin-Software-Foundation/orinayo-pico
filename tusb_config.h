/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */
 
#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

//--------------------------------------------------------------------
// COMMON CONFIGURATION
//--------------------------------------------------------------------

// Board-specific root hub port selection for dual-role mode:
// - Port 0: native USB device (Pico USB connector)
// - Port 1: PIO USB host (external D+/D- wiring)

#define BOARD_TUD_RHPORT 			0
#define BOARD_TUH_RHPORT 			1
#define CFG_TUSB_RHPORT0_MODE     	OPT_MODE_DEVICE
#define CFG_TUSB_RHPORT1_MODE     	OPT_MODE_HOST
#define CFG_TUSB_OS                 OPT_OS_PICO
#define CFG_TUH_RPI_PIO_USB_PORT    1
#define CFG_TUD_RHPORT              0

// Enable both device + host stacks (dual-role).
#define CFG_TUD_ENABLED             1
#define CFG_TUH_ENABLED             1

// CFG_TUSB_DEBUG is defined by compiler in DEBUG build
// #define CFG_TUSB_DEBUG           0

/* USB DMA on some MCUs can only access a specific SRAM region with restriction on alignment.
 * Tinyusb use follows macros to declare transferring memory so that they can be put
 * into those specific section.
 * e.g
 * - CFG_TUSB_MEM SECTION : __attribute__ (( section(".usb_ram") ))
 * - CFG_TUSB_MEM_ALIGN   : __attribute__ ((aligned(4)))
 */
#ifndef CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_SECTION
#endif

#ifndef CFG_TUSB_MEM_ALIGN
#define CFG_TUSB_MEM_ALIGN          __attribute__ ((aligned(4)))
#endif

#ifndef CFG_TUH_MEM_SECTION
#define CFG_TUH_MEM_SECTION
#endif

#ifndef CFG_TUH_MEM_ALIGN
#define CFG_TUH_MEM_ALIGN           __attribute__ ((aligned(4)))
#endif

//--------------------------------------------------------------------
// DEVICE CONFIGURATION
//--------------------------------------------------------------------

#ifndef CFG_TUD_ENDPOINT0_SIZE
#define CFG_TUD_ENDPOINT0_SIZE    64
#endif

//------------- CLASS -------------//
#define CFG_TUD_CDC             0
#define CFG_TUD_MSC             0
#define CFG_TUD_HID             0 
#define CFG_TUD_MIDI            1 
#define CFG_TUD_VENDOR          0

// MIDI FIFO size of TX and RX
#define CFG_TUD_MIDI_RX_BUFSIZE   (TUD_OPT_HIGH_SPEED ? 512 : 64)
#define CFG_TUD_MIDI_TX_BUFSIZE   (TUD_OPT_HIGH_SPEED ? 512 : 64)

//--------------------------------------------------------------------
// HOST CONFIGURATION (USB MIDI host via PIO USB)
//--------------------------------------------------------------------

// Use PIO-USB as host controller.
#define CFG_TUH_RPI_PIO_USB         1

// Size of buffer to hold descriptors and other data used for enumeration.
#define CFG_TUH_ENUMERATION_BUFSIZE 1024

// Host class enablement.
#define CFG_TUH_HUB                 1
#define CFG_TUH_DEVICE_MAX          (3 * CFG_TUH_HUB + 1)

#define CFG_TUH_MIDI                1
#define CFG_TUH_MIDI_STREAM_API		1

#define CFG_TUH_HID                 1
#define CFG_TUH_HID_KEYBOARD        1
#define TUH_CFG_RHP_PIO             1  // Ensures Host routes properly to PIO layer


// Host MIDI buffers.
// Use host bulk endpoint max size for portability across full/high speed devices.
#define CFG_TUH_MIDI_RX_BUFSIZE     TUH_EPSIZE_BULK_MAX
#define CFG_TUH_MIDI_TX_BUFSIZE     TUH_EPSIZE_BULK_MAX

#ifdef __cplusplus
}
#endif

#endif /* _TUSB_CONFIG_H_ */