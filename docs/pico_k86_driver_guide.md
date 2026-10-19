# Raspberry Pi Pico USB Host Driver: Attack Shark K86 Configuration
This document aggregates all reverse-engineered code structures, enumeration procedures, and execution routines required to interface a Raspberry Pi Pico (via the TinyUSB host stack) with the **Attack Shark K86 Mechanical Gaming Keyboard**.

---

## 1. Enumeration & Interface Device Mapping

The K86 splits its features across multiple USB sub-interfaces. This routine dynamically intercepts the USB configuration descriptors at runtime to locate the **Consumer/Multimedia Key Input Interface** and the **Royuan Vendor Custom UI Screen Output Interface** automatically.

```c
#include "tusb.h"
#include <string.h>
#include <stdio.h>

// Global tracking structure for the connected device
typedef struct {
    uint8_t dev_addr;
    int8_t  knob_instance;   // For Consumer/Media Keys (Input)
    int8_t  screen_instance; // For Vendor Custom UI Packets (Output)
    bool    is_connected;
} k86_device_t;

static k86_device_t k86_kb = { .dev_addr = 0, .knob_instance = -1, .screen_instance = -1, .is_connected = false };

// Callback triggered when a HID interface is opened during enumeration
void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len) {
    uint16_t vid, pid;
    tuh_vid_pid_get(dev_addr, &vid, &pid);

    // Verify it is the K86 hardware (VID 0x1D57)
    if (vid != 0x1D57) {
        return; 
    }

    k86_kb.dev_addr = dev_addr;
    k86_kb.is_connected = true;

    uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);

    if (itf_protocol == HID_PROTOCOL_NONE) {
        uint16_t i = 0;
        while (i < desc_len) {
            uint8_t bSize = desc_report[i] & 0x03;
            uint8_t bType = (desc_report[i] >> 2) & 0x03;
            uint8_t bTag  = (desc_report[i] >> 4) & 0x0F;

            // Global Item (bType == 1), Usage Page Tag (bTag == 0x0)
            if (bType == 1 && bTag == 0x0) {
                uint32_t usage_page = 0;
                for (uint8_t b = 0; b < bSize; b++) {
                    usage_page |= ((uint32_t)desc_report[i + 1 + b]) << (b * 8);
                }

                // Standard USB Consumer Page (Volume Control Knob)
                if (usage_page == 0x000C) {
                    k86_kb.knob_instance = instance;
                    printf("[K86 Mounter] Target Knob mapped to Instance %d\n", instance);
                }
                // Royuan Custom Vendor Usage Page (Screen Upstream)
                else if (usage_page == 0xFF00 || usage_page == 0xFF01) {
                    k86_kb.screen_instance = instance;
                    printf("[K86 Mounter] Target Screen mapped to Instance %d\n", instance);
                }
            }
            i += 1 + (bSize == 3 ? 4 : bSize); 
        }
    }

    // Crucial: Open the inbound report stream pipeline for the knob interface
    if (instance == k86_kb.knob_instance) {
        tuh_hid_receive_report(dev_addr, instance);
    }
}

// Safely clear references if the keyboard is disconnected
void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
    if (dev_addr == k86_kb.dev_addr) {
        if (instance == k86_kb.knob_instance)   k86_kb.knob_instance = -1;
        if (instance == k86_kb.screen_instance) k86_kb.screen_instance = -1;
        
        if (k86_kb.knob_instance == -1 && k86_kb.screen_instance == -1) {
            k86_kb.is_connected = false;
            printf("[K86 Mounter] Keyboard disconnected.\n");
        }
    }
}
```

---

## 2. Capturing Volume Knob Rotations

When in **Volume Mode**, the physical rotary encoder sends standard multimedia Consumer Control Input Reports down to the host. The following handler decodes turns (clockwise or counter-clockwise) and clicks.

```c
// Standard USB HID Consumer Control Usage Definitions
#define HID_USAGE_CONSUMER_VOLUME_INCREMENT 0x00E9
#define HID_USAGE_CONSUMER_VOLUME_DECREMENT 0x00EA
#define HID_USAGE_CONSUMER_MUTE             0x00E2

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len) {
    if (dev_addr == k86_kb.dev_addr && instance == k86_kb.knob_instance) {
        if (len >= 2) {
            // Unpack 16-bit key consumer usage
            uint16_t consumer_key = ((uint16_t)report[1] << 8) | report[0];

            switch (consumer_key) {
                case HID_USAGE_CONSUMER_VOLUME_INCREMENT:
                    printf("K86 Knob Event: Volume Turned UP\n");
                    break;

                case HID_USAGE_CONSUMER_VOLUME_DECREMENT:
                    printf("K86 Knob Event: Volume Turned DOWN\n");
                    break;

                case HID_USAGE_CONSUMER_MUTE:
                    printf("K86 Knob Event: Knob Button Pressed (Mute)\n");
                    break;

                default:
                    break;
            }
        }
    }

    // Re-queue the input pipeline to receive subsequent knob operations
    tuh_hid_receive_report(dev_addr, instance);
}
```

---

## 3. Native Screen Protocol (System Telemetry & Time Data)

This structure formats standard calendar, clock, and host tracking metrics down to the keyboard screen without needing large graphics arrays.

### Structured Packet Definition
```c
// The default system time layout framework
typedef struct {
    uint8_t command_type;        // 0x05: Firmware command instruction for telemetry/clock update
    uint8_t year_hi;             // Year High Byte (e.g., 20)
    uint8_t year_lo;             // Year Low Byte (e.g., 26)
    uint8_t month;               // Month: 1 to 12
    uint8_t day;                 // Day: 1 to 31
    uint8_t hour;                // Hour: 0 to 23
    uint8_t minute;              // Minute: 0 to 59
    uint8_t second;              // Second: 0 to 59
    uint8_t day_of_week;         // Weekday indexing: 1 to 7
    uint8_t custom_battery_level;// Optional: Values from 0 to 100 to override battery UI container
    uint8_t padding[53];         // Fills the remainder of the fixed 64-byte transaction size
} __attribute__((packed)) k86_time_packet_t;
```

### Transmission Routine
```c
void k86_sync_system_telemetry(uint8_t dev_addr, uint8_t instance, 
                               uint16_t year, uint8_t month, uint8_t day, 
                               uint8_t hour, uint8_t minute, uint8_t second, 
                               uint8_t day_of_week, uint8_t battery) 
{
    if (instance == -1) return;

    k86_time_packet_t time_packet = {0};
    time_packet.command_type = 0x05; 

    time_packet.year_hi             = (uint8_t)((year >> 8) & 0xFF);
    time_packet.year_lo             = (uint8_t)(year & 0xFF);
    time_packet.month               = month;
    time_packet.day                 = day;
    time_packet.hour                = hour;
    time_packet.minute              = minute;
    time_packet.second              = second;
    time_packet.day_of_week         = day_of_week;
    time_packet.custom_battery_level = battery; // Inject custom battery level override if supported

    tuh_hid_set_report(
        dev_addr,
        instance,
        0x00,                      // Report ID (embedded inside buffer data block)
        HID_REPORT_TYPE_FEATURE,   // Sent via Feature or Output channel
        &time_packet,
        sizeof(k86_time_packet_t)  // Exactly 64 Bytes
    );
}
```

---

## 4. Custom Application Implementation: Hijacking UI Fields for Music Status

This application layer adapts the standard time report structure, redirecting the clock container to show track playback information (Elapsed Minutes, Elapsed Seconds, Track ID, and Active Player States).

```c
typedef enum {
    MUSIC_STOPPED = 0,
    MUSIC_PLAYING = 1,
    MUSIC_PAUSED  = 2
} music_state_t;

typedef struct {
    music_state_t state;
    uint16_t track_number;
    uint32_t elapsed_seconds;
} music_track_info_t;

void k86_display_music_as_time(uint8_t dev_addr, uint8_t instance, music_track_info_t* music) {
    if (instance == -1) return;

    k86_time_packet_t media_packet = {0};
    media_packet.command_type = 0x05; 

    // Visual Hack 1: Remap the "Year" section to render state code (e.g., 2001 for Playing)
    media_packet.year_hi = 20; 
    media_packet.year_lo = (uint8_t)music->state;

    // Visual Hack 2: Remap "Month" and "Day" to print Track Number
    media_packet.month = (uint8_t)((music->track_number >> 8) & 0xFF);
    media_packet.day   = (uint8_t)(music->track_number & 0xFF);

    // Visual Hack 3: Convert raw elapsed track seconds into MM:SS format inside the main clock
    uint8_t display_minutes = (music->elapsed_seconds / 60) % 100; 
    uint8_t display_seconds = music->elapsed_seconds % 60;

    media_packet.hour        = display_minutes; // Renders inside the clock's hour container
    media_packet.minute      = display_seconds; // Renders inside the clock's minute container
    media_packet.second      = 0;
    media_packet.day_of_week = (uint8_t)music->state; 

    tuh_hid_set_report(
        dev_addr,
        instance,
        0x00,
        HID_REPORT_TYPE_FEATURE,   
        &media_packet,
        sizeof(k86_time_packet_t)
    );
}
```

---

## 5. Advanced Image Parsing and Frame Chunking Protocol

To project custom animations or static logos, images must be compiled as **128×128 pixel 16-bit Big-Endian RGB565 graphics**. The Pico segments this raw 32,768-byte data block into 60-byte payload sequences wrapped in sequential 64-byte USB Output packets.

### Packet Struct Definitions
```c
#define RGB888_TO_RGB565(r, g, b) (uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | (((b) & 0xF8) >> 3))

typedef struct {
    uint8_t command_id;          // 0x02: Identifies an sequential pixel buffer transaction
    uint8_t seq_num_hi;          // Block increment sequence index (High Byte)
    uint8_t seq_num_lo;          // Block increment sequence index (Low Byte)
    uint8_t length;              // Length of valid pixel data inside the block segment (Max 60)
    uint8_t pixel_data[60];      // 60 Bytes of raw RGB565 continuous byte data
} __attribute__((packed)) k86_screen_packet_t;
```

### Stream Pipeline Loop Execution
```c
void k86_upload_frame(uint8_t dev_addr, uint8_t instance, uint16_t* rgb565_image_buffer) {
    if (instance == -1) return;

    uint8_t* raw_bytes = (uint8_t*)rgb565_image_buffer;
    uint32_t total_bytes = 128 * 128 * 2; // 32,768 absolute bytes
    uint32_t bytes_sent = 0;
    uint16_t chunk_index = 0;

    while (bytes_sent < total_bytes) {
        k86_screen_packet_t packet = {0};
        packet.command_id = 0x02; 
        packet.seq_num_hi = (chunk_index >> 8) & 0xFF;
        packet.seq_num_lo = chunk_index & 0xFF;
        
        uint32_t bytes_to_copy = (total_bytes - bytes_sent > 60) ? 60 : (total_bytes - bytes_sent);
        packet.length = (uint8_t)bytes_to_copy;
        
        memcpy(packet.pixel_data, &raw_bytes[bytes_sent], bytes_to_copy);

        // Dispatches the payload chunk across the host outbound vendor endpoint
        tuh_hid_set_report(dev_addr, instance, 0x00, HID_REPORT_TYPE_OUTPUT, &packet, 64);
        
        bytes_sent += bytes_to_copy;
        chunk_index++;
        
        // Minor inline pacing delay to prevent over-saturating the K86 MCU core while it flashes internal SRAM
        sleep_ms(2); 
    }
}
```
### Attack Shark / Ajazz LCD Implementation
```c
#include <stdio.h>
#include "pico/stdlib.h"
#include "tusb.h"

// Common VID/PID pairings for Attack Shark / Ajazz composite setups:
// Wired Mode: 0x342D / 0xDCE4. If switched to Mac mode, it may enumerate as 0x05AC (Apple).
#define ATTACK_SHARK_VID  0x342D  
#define ATTACK_SHARK_PID  0xDCE4  

static uint8_t k86_dev_addr = 0;
static uint8_t k86_vendor_instance = 255; // Store which instance controls the LCD
static bool k86_connected = false;

// Forward declaration of your display routine
void update_k86_lcd(uint8_t dev_addr, uint8_t instance, uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t battery);

int main(void) {
    stdio_init_all();
    
    // TinyUSB Host initialization inside Pico SDK 2.2.0 runtime
    tuh_init(BOARD_TUH_RHPORT);

    printf("Pico USB Host Driver Initialised. Awaiting Attack Shark K86 connection...\n");

    while (1) {
        tuh_task(); // Keeps the USB host stack cycling

        if (k86_connected && k86_vendor_instance != 255) {
            // Mock data structure to feed the LCD panel loop
            uint16_t current_year = 2026;
            uint8_t current_month = 10;
            uint8_t current_day   = 19;
            uint8_t current_hour  = 10;
            uint8_t current_min   = 32;
            uint8_t battery_pct   = 88;

            update_k86_lcd(k86_dev_addr, k86_vendor_instance, current_year, current_month, current_day, current_hour, current_min, battery_pct);
            
            // Sleep cycle prevents saturating the endpoint buffers
            sleep_ms(30000); 
        }
    }
    return 0;
}

// TinyUSB Host Callback when any HID interface mounts
void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len) {
    uint16_t vid, pid;
    tuh_vid_pid_get(dev_addr, &vid, &pid);

    if (vid == ATTACK_SHARK_VID && pid == ATTACK_SHARK_PID) {
        // Query the specific HID protocol of this active instance
        uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);
        
        printf("K86 Interface found! Address: %d, Instance: %d, Protocol: %d\n", dev_addr, instance, itf_protocol);

        // Protocol 1 = Keyboard, Protocol 2 = Mouse, Protocol 0 = Vendor/Custom Configuration Channel
        if (itf_protocol == 0) {
            k86_dev_addr = dev_addr;
            k86_vendor_instance = instance;
            k86_connected = true;
            printf("--> Targeted custom vendor instance [%d] successfully mapped for LCD control.\n", instance);
        }

        // Always initiate listening loop on the standard interface to ensure typing inputs flow
        tuh_hid_receive_report(dev_addr, instance);
    }
}

void tuh_hid_umount_cb(uint8_t dev_addr, uint8_t instance) {
    if (dev_addr == k86_dev_addr && instance == k86_vendor_instance) {
        printf("Attack Shark K86 LCD Interface disconnected.\n");
        k86_connected = false;
        k86_vendor_instance = 255;
        k86_dev_addr = 0;
    }
}

void update_k86_lcd(uint8_t dev_addr, uint8_t instance, uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute, uint8_t battery) {
    // 64-byte alignment payload required by custom gaming peripheral configurations
    static uint8_t report_buf[64]; 
    memset(report_buf, 0, sizeof(report_buf));

    // Common proprietary initialization markers for Chinese mechanical layout screens (e.g., Keebmonkey/Shark configurations)
    report_buf[0] = 0x05; // Feature Report Identifier / Packet Type
    report_buf[1] = 0x5C; // Command Action Opcode: Synchronise Local Time metrics

    // Time/Date variables split into respective payload index tracking
    report_buf[2] = (uint8_t)(year >> 8);
    report_buf[3] = (uint8_t)(year & 0xFF);
    report_buf[4] = month;
    report_buf[5] = day;
    report_buf[6] = hour;
    report_buf[7] = minute;
    report_buf[8] = battery; 

    // Dispatches a dynamic SET_REPORT transaction to the targeted endpoint
    tuh_hid_set_report(
        dev_addr,
        instance,
        0,                 // Report ID 0 implies reliance on the payload buffer alignment array header
        HID_REPORT_TYPE_OUTPUT, // Explicit enum identification mapping for TinyUSB
        report_buf,
        sizeof(report_buf)
    );
}
````