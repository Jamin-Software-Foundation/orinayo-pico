/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <string.h>
#include "pico/cyw43_arch.h"
#include "pico/multicore.h"
#include "hardware/clocks.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "pico/binary_info.h"
#include "pico/flash.h"
#include "bsp/board.h"
#include "tusb.h"
#include "pio_usb.h"
#include "pico_bluetooth.h"
#include "ble_midi_controller.h"
#include "ble_midi_client.h"
#include "async_timer.h"
#include "storage.h"
#include "looper.h"
#include "note_scheduler.h"
#include "pico/stdlib.h"
#include "hardware/watchdog.h"
#include "hardware/i2c.h"

// Pico W devices use a GPIO on the WIFI chip for the LED,
// so when building for Pico W, CYW43_WL_GPIO_LED_PIN will be defined
#ifdef CYW43_WL_GPIO_LED_PIN
#include "pico/cyw43_arch.h"
#endif

#ifndef LED_DELAY_MS
#define LED_DELAY_MS 250
#endif

// Perform initialisation
int pico_led_init(void) {
#if defined(PICO_DEFAULT_LED_PIN)
    // A device like Pico that uses a GPIO for the LED will define PICO_DEFAULT_LED_PIN
    // so we can use normal GPIO functionality to turn the led on and off
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    return PICO_OK;
#elif defined(CYW43_WL_GPIO_LED_PIN)
    // For Pico W devices we need to initialise the driver etc
    return cyw43_arch_init();
#endif
}

// Turn the led on or off
void pico_set_led(bool led_on) {
#if defined(PICO_DEFAULT_LED_PIN)
    // Just set the GPIO on or off
    gpio_put(PICO_DEFAULT_LED_PIN, led_on);
#elif defined(CYW43_WL_GPIO_LED_PIN)
    // Ask the wifi "driver" to set the GPIO on or off
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_on);
#endif
}


// M5Stack Midi UART settings
#define UART_ID uart0
#define BAUD_RATE 31250
#define UART_TX_PIN 0
#define UART_RX_PIN 1
#define GPIO_FUNC_UART 2

// WAV Trigger Pro UART settings
#define UART_ID_1      uart1
#define BAUD_RATE_1    57600
#define UART_TX_PIN_1  4
#define UART_RX_PIN_1  5

// WAV Trigger I2C Peripheral Block and Pin Mapping
#define I2C_ID          i2c1
#define I2C_SDA_PIN     6
#define I2C_SCL_PIN     7
#define I2C_SPEED_HZ    100000     // Standard 100kHz clock speed

// I2C Command Protocol Constants 
#define WAV_TRIGGER_PRO_ADDR 0x13
#define CMD_GET_VERSION				1
#define CMD_GET_NUM_TRACKS			2
#define CMD_TRACK_PLAY_POLY			3
#define CMD_GET_TRACK_STATUS        4
#define CMD_GET_NUM_ACTIVE_VOICES	5
#define CMD_TRACK_SET_LOOP			6
#define CMD_TRACK_SET_LOCK			7
#define CMD_STOP_ALL                8
#define CMD_TRACK_STOP				9
#define CMD_TRACK_FADE				10
#define CMD_MIDI_MSG				11
#define CMD_LOAD_PRESET				12
#define CMD_SET_OUTPUT_GAIN			13
#define WAV_TRIGGER_PRO_MAX_MESSAGE_LEN   32
#define WAV_TRIGGER_PRO_MAX_PAYLOAD_LEN   (WAV_TRIGGER_PRO_MAX_MESSAGE_LEN - 1)
#define WAV_TRIGGER_PRO_VERSION_STRING_LEN 12
#define WAV_TRIGGER_PRO_RESPONSE_DELAY_MS  2
#define WAV_TRIGGER_PRO_LOOP_FLAG         0x01
#define WAV_TRIGGER_PRO_LOCK_FLAG         0x02
#define WAV_TRIGGER_PRO_PITCH_BEND_FLAG   0x04
#define WAV_TRIGGER_PRO_BALANCE_MID       64
#define MIDI_STATUS_BYTE_MASK             0x80
#define MIDI_DATA_BYTE_MASK               0x7f
#define MIDI_COMMAND_MASK                 0xF0
#define MIDI_PROGRAM_CHANGE               0xC0
#define MIDI_CHANNEL_PRESSURE             0xD0

#define END1 46

// launchkey_display_text SysEx buffer layout:
//   6 bytes Novation header + 1 sub-ID + 1 display-mode + text + 1 EOX = 9 + len
// Total msg[] is 48 bytes, so max text length = 48 - 9 = 39 characters.
#define LAUNCHKEY_DISPLAY_SYSEX_OVERHEAD  9
#define LAUNCHKEY_DISPLAY_MSG_SIZE       48
#define LAUNCHKEY_MAX_TEXT_LEN           (LAUNCHKEY_DISPLAY_MSG_SIZE - LAUNCHKEY_DISPLAY_SYSEX_OVERHEAD)

#define constrain(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))


extern int midi_current_step;
extern int style_section;
extern int old_style;
extern int active_strum_pattern;
extern int active_neck_pos;
extern int basic_chord;
extern int advanced_chord;
extern int style_group;
extern style_change_requested;

extern uint8_t sample_drum_velocity;
extern uint8_t sample_bass_velocity;
extern uint8_t sample_chord_velocity;
extern uint8_t worship_pad_velocity;

extern uint8_t sampler_old_drum_note;
extern uint8_t sampler_old_bass_note;
extern uint8_t sampler_old_chord_note;

extern uint8_t midi_guitar_volume;

extern bool style_started;
extern bool enable_ample_guitar;
extern bool enable_midi_drums;
extern bool enable_seqtrak;
extern bool enable_chord_track;
extern bool enable_bass_track;
extern bool enable_modx;
extern bool enable_sp404mk2;
extern bool enable_mpc_sample;
extern bool enable_mpx_looper;
extern bool enable_nanobox_tangerine;
extern bool enable_wav_trigger_pro;
extern bool style_end_requested;
extern bool style_end_started;
extern bool preferences_changed;

extern uint8_t but0; 
extern uint8_t but1;
extern uint8_t but2; 
extern uint8_t but3; 
extern uint8_t but4; 
extern uint8_t but6; 
extern uint8_t mbut0;

extern uint8_t dpad_down;
extern uint8_t logo;
extern uint8_t starpower;
extern uint8_t pitch;

extern uint8_t green;
extern uint8_t red;
extern uint8_t yellow;
extern uint8_t orange;
extern uint8_t blue;
extern bool joy_up; 
extern uint8_t joystick_up;

static uint32_t old_p1 = 0;
static uint32_t old_p2 = 0;
static uint32_t old_p3 = 0;
static uint32_t old_p4 = 0;

bool wav_trigger_pro_connected = false;
bool launchkey_connected = false;
bool launchkey_daw_mode = false;
bool irig_pro_connected = false;
bool mute_midi_controller = false;

// 128-bit bitmask tracking currently held MIDI notes (one bit per note number).
static uint32_t held_notes_mask[4] = {0};
static int held_note_count = 0;

// MIDI running-status parser state (persists across tuh_midi_rx_cb invocations).
static uint8_t midi_running_status = 0;
static uint8_t midi_data0 = 0;
static int     midi_data_count = 0;

// TinyUSB MIDI host interface index (not a USB device address).
// Set by tuh_midi_mount_cb; 0xFF means no MIDI device is currently mounted.
static uint8_t midi_itf_idx          = 0xFF;
static uint8_t daw_itf_idx          = 0xFF;
// USB device address of the currently mounted MIDI device (from mount_cb_data->daddr).
static uint8_t midi_dev_addr         = 0;
static uint8_t daw_dev_addr         = 0;
// Number of TX (OUT) virtual cables reported by the mounted device.
// Must be >= 2 to use cable 1 for Launchkey DAW / LED / SysEx messages.
static uint8_t launchkey_tx_cable_count = 0;
uint8_t chord1_pad_velocity = 48;
uint8_t chord2_pad_velocity = 32;
uint8_t previous_time = 0;
uint8_t previous_drum_vol = 0;
uint8_t previous_bass_vol = 0;
uint8_t previous_chord_vol = 0;
uint8_t previous_guitar_note = 0;

bool midi_keyboard_connected = false;

void send_ble_midi(uint8_t* midi_data, int len);
void midi_task(void);
void midi_start_stop(bool start);
void midi_send_note(uint8_t command, uint8_t note, uint8_t velocity);
void midi_send_chord_note(uint8_t note, uint8_t velocity);
void midi_send_program_change(uint8_t command, uint8_t code);
void midi_send_control_change(uint8_t command, uint8_t controller, uint8_t value);
void midi_play_chord(bool on, uint8_t p1, uint8_t p2, uint8_t p3);
void midi_play_slash_chord(bool on, uint8_t p1, uint8_t p2, uint8_t p3, uint8_t p4);
void midi_ketron_arr(uint8_t code, bool on);
void midi_ketron_footsw(uint8_t code, bool on);
void midi_seqtrak_pattern(uint8_t pattern);
void midi_seqtrak_mute(uint8_t track, bool mute);
void midi_seqtrak_key(uint8_t key);
void midi_seqtrak_tempo(int tempo);
void midi_seqtrak_arp();
void midi_seqtrak_arp_octave(uint8_t track, int octave);
void midi_modx_tempo(int tempo);
void midi_modx_key(uint8_t key);
void midi_modx_octave(uint8_t octave);
void midi_modx_arp(bool on);
void midi_modx_arp_hold(uint8_t part, bool on);
void midi_modx_arp_realtime(uint8_t part, bool on);
void midi_modx_arp_octave(uint8_t octave);
void dream_set_delay(int tempo);
void sampler_midi_note(uint8_t command, uint8_t note, uint8_t velocity);
void config_mpc_sample();
void sampler_trigger_loop();
void mpc_trigger_loop();
void mpx_trigger_loop();
void sp404_trigger_loop();
void trigger_loop();
void config_nanobox_tangerine();
void config_mpx_looper();
void process_midi_byte(uint8_t b);
void gamepad_bluetooth_handle_data();
void set_tempo(uint8_t tempo);
bool wav_trigger_pro_get_version(char *dst, size_t dst_len);
int wav_trigger_pro_get_num_tracks(void);
bool wav_trigger_pro_track_play_poly(uint16_t track, int16_t gain_db, uint8_t balance, uint16_t attack_ms, int16_t cents, uint8_t flags);
bool wav_trigger_pro_track_get_status(uint16_t track, uint8_t *status);
bool wav_trigger_pro_get_num_active_voices(uint8_t *voices);
bool wav_trigger_pro_track_set_loop(uint16_t track, bool loop);
bool wav_trigger_pro_track_set_lock(uint16_t track, bool lock);
bool wav_trigger_pro_stop_all(void);
bool wav_trigger_pro_track_stop(uint16_t track, uint16_t release_ms);
bool wav_trigger_pro_track_fade(uint16_t track, int16_t gain_db, uint16_t time_ms);
bool wav_trigger_pro_set_volume(uint16_t track, uint8_t cc_value);
bool wav_trigger_pro_send_midi_msg(uint8_t cmd, uint8_t dat1, uint8_t dat2);
bool wav_trigger_pro_load_preset(uint16_t preset);
bool wav_trigger_pro_set_output_gain(int16_t gain_db);
void nanobox_stop_loops();
void wav_trigger_pro_stop_loops();
void config_wav_trigger_pro();
void launchkey_display_text(const char* text, bool is_temp);
void launchkey_set_led(uint8_t msg_type, uint8_t channel, uint8_t index, uint8_t color_id);
void calculate_sampler_matrix_indexed(int target_key, int tempo_index, int *out_track_index, int *out_pitch_cents);
void trigger_indexed_sample(int target_key, int tempo_index);
static void wav_trigger_pro_forward_midi_message(const uint8_t *buffer, uint32_t bufsize);

uint8_t get_arp_template(void);
void midi_n_stream_write(uint8_t itf, uint8_t cable_num, uint8_t *buffer, uint32_t bufsize);
void handle_keyboard_events(uint8_t keycode);

enum {
	// Balanced size: enough to batch multiple MIDI packets per callback while keeping stack usage small.
	HOST_MIDI_RX_BUFFER_SIZE = 48,
};

bool repeating_timer_callback(__unused struct repeating_timer *t) {
    //printf("Repeat at %lld\n", time_us_64());
	uint8_t current_time = 60;
	
	//if (previous) midi_send_note(0x89, previous, 0);
	//midi_send_note(0x99, current, 100);	
	//board_millis();	
	previous_time = current_time;
    return true;
}

bool is_wav_trigger_connected() {
    char version[WAV_TRIGGER_PRO_VERSION_STRING_LEN + 1] = {0};
    return wav_trigger_pro_get_version(version, sizeof(version));
}

//--------------------------------------------------------------------+
//
// core main handle handlers
//
//--------------------------------------------------------------------+

void core1_main() {
	//sleep_ms(10);
	tuh_init(BOARD_TUH_RHPORT);

	while (true) {
		tuh_task(); // tinyusb host task
	}
}

int main() {	
	set_sys_clock_khz(120000, true);
	sleep_ms(10);
	
    stdio_init_all();		

    int rc = pico_led_init();
    hard_assert(rc == PICO_OK);
		
    board_init();	
	
	multicore_reset_core1();
	multicore_launch_core1(core1_main);	
	flash_safe_execute_core_init();	

	tud_init(BOARD_TUD_RHPORT);			
	bluetooth_init();

    //struct repeating_timer timer;	
    //add_repeating_timer_ms(500, repeating_timer_callback, NULL, &timer);
	
	async_timer_init();
	looper_schedule_step_timer();
    note_scheduler_init();
	
	// setup UART0 - M5Stack MIDI
	uart_init(UART_ID, BAUD_RATE);
	gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
	gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);
	uart_set_fifo_enabled(UART_ID, true);
	uart_set_translate_crlf(UART_ID, false);
	sleep_ms(500);	
	
	// setup I2C - WAV Trigger Pro	
	
    i2c_init(I2C_ID, I2C_SPEED_HZ);
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);	
	sleep_ms(500);	
	
	wav_trigger_pro_connected = is_wav_trigger_connected();	
	
	
    while (true) {
		tud_task(); // tinyusb device task		
		
		if (enable_midi_drums) cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, false);			
		
		while (tud_midi_available()) {
			uint8_t buffer[4] = {0};			
			tud_midi_packet_read(buffer);
			
			if (midi_itf_idx != 0xFF) {
				tuh_midi_stream_write(midi_itf_idx, 0, buffer, 4);
				tuh_midi_write_flush(midi_itf_idx);
			}
			
			uart_write_blocking(UART_ID, buffer, 4);
			uart_tx_wait_blocking(UART_ID); 			
		
			cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, true);				
		}

		while (uart_is_readable(UART_ID)) {
			uint8_t ch = uart_getc(UART_ID);
			process_midi_byte(ch);
		}		
		
		note_scheduler_dispatch_pending();

		// Poll for incoming MIDI events from any connected BLE MIDI peripheral.
		// BAO disable BLE for now
		//ble_midi_controller_poll();
		

		if (!launchkey_daw_mode && launchkey_connected && midi_itf_idx != 0xFF) {
			launchkey_daw_mode = true;			
			sleep_ms(500);
			
			uint8_t msg[3];			
			msg[0] = 0x9F;
			msg[1] = 0x0C;
			msg[2] = 0x7F;
			tuh_midi_stream_write(midi_itf_idx, launchkey_tx_cable_count >= 2 ? 1 : 0, msg, 3);
			tuh_midi_write_flush(midi_itf_idx);
		}		

		if (preferences_changed) {
			preferences_changed = false;
			storage_store_preferences();
			
			watchdog_enable(1, 1); 			// force reboot
			while(1); 
		}		
    }
	
    //cancel_repeating_timer(&timer);	
}

//--------------------------------------------------------------------+
//
// Device callbacks
//
//--------------------------------------------------------------------+

void name_received_cb(tuh_xfer_t* xfer) {
    if (xfer->result == XFER_RESULT_SUCCESS) {
        uint16_t* temp_buf = (uint16_t*) xfer->buffer;
        char name[128];

        // Convert the UTF-16 USB string to a standard C-string (UTF-8/ASCII)
        // temp_buf[0] contains the length/type info, actual chars start at index 1
        uint8_t len = (uint8_t)(temp_buf[0] & 0xFF);
        uint8_t char_count = (len - 2) / 2;

        for (uint8_t i = 0; i < char_count; i++) {
            name[i] = (char)temp_buf[i + 1];
        }
        name[char_count] = '\0';
		
		// iRig Keys 2 PRO
		// MPC Sample
		// X-TOUCH MINI
		// nanobox tangerine
		// Launchkey Mini MK4 37 MIDI
		
		if (name[0] == 'i' && name[1] == 'R' && name[2] == 'i' && name[3] == 'g' && name[4] == ' ' && name[5] == 'K' && name[6] == 'e' && name[7] == 'y' && name[8] == 's' && name[9] == ' ' && name[10] == '2' && name[11] == ' ' && name[12] == 'P' && name[13] == 'R' && name[14] == 'O') {		
			// Start/stop 	CC23 (0x17, 0x7F)
			// Next Style	CC22 (0x16, 0x1)
			// Prev Style	CC22 (0x16, 0x16)
			// Volume 		CCXX ((0x0C - 0x13), (0 - 7F)) 
			
			enable_wav_trigger_pro = true;	// assume WAV Trigger Pro is available
			midi_keyboard_connected = true;
			irig_pro_connected = true;
			
			config_wav_trigger_pro();
			cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, true);	
		}
		else
			
		if (name[0] == 'L' && name[1] == 'a' && name[2] == 'u' && name[3] == 'n' && name[4] == 'c' && name[5] == 'h' && name[6] == 'k' && name[7] == 'e'  && name[8] == 'y') {		
			enable_wav_trigger_pro = true;	// assume WAV Trigger Pro is available
			midi_keyboard_connected = true;
			launchkey_connected = true;
			
			config_wav_trigger_pro();;
			cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, true);		
		}
		else
			
		if (name[0] == 'L' && name[1] == 'P' && name[2] == 'K' && name[3] == '2' && name[4] == '5') {		
			enable_wav_trigger_pro = true;	// assume WAV Trigger Pro is available
			midi_keyboard_connected = true;
			
			config_wav_trigger_pro();;
			cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, true);		
		}
		else
			
		if (name[0] == 'M' && name[1] == 'P' && name[2] == 'X' && name[3] == '8') {		
			midi_keyboard_connected = false;
			enable_mpx_looper = true;	
			config_mpx_looper();
			cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, true);		
		}
		else
			
		if (name[0] == 'M' && name[1] == 'P' && name[2] == 'C' && name[3] == ' ' && name[4] == 'S' && name[5] == 'a' && name[6] == 'm' && name[7] == 'p' && name[8] == 'l' && name[9] == 'e') {		
			midi_keyboard_connected = false;
			enable_mpc_sample = true;	
			cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, true);		
		}
		else
			
		if (name[0] == 'X' && name[1] == '-' && name[2] == 'T' && name[3] == 'O' && name[4] == 'U' && name[5] == 'C' && name[6] == 'H' && name[7] == ' ' && name[8] == 'M' && name[9] == 'I' && name[10] == 'N' && name[11] == 'I') {		
			// Dial press    (A) Note On/Off 0x00 - 0x07 (8 buttons)
			// Buttons       (A) Note On/Off 0x08 - 0x17 (16 buttons)
			// Dial volume   (A) CC 0xNN 0x00 - 0x7F (NN = 0x01 - 0x08)
			// Master volume (A) CC 0x09 0x00 - 0x7F
			// Master volume (B) CC 0x0A 0x00 - 0x7F			
			// Dial volume   (A) CC 0xNN 0x00 - 0x7F (NN = 0x0B - 0x12)	
			// Dial press    (B) Note On/Off 0x18 - 0x1F (8 buttons)			
			// Buttons       (B) Note On/Off 0x20 - 0x2F (16 buttons)
			
			midi_keyboard_connected = true;
			cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, true);		
		}
		else

		if (name[0] == 'n' && name[1] == 'a' && name[2] == 'n' && name[3] == 'o' && name[4] == 'b' && name[5] == 'o' && name[6] == 'x' && name[7] == ' ' && name[8] == 't' && name[9] == 'a' && name[10] == 'n' && name[11] == 'g' && name[12] == 'e' && name[13] == 'r' && name[14] == 'i' && name[15] == 'n' && name[16] == 'e') {		
			midi_keyboard_connected = false;
			enable_nanobox_tangerine = true;
			config_nanobox_tangerine();
			cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, true);		
		}
    }
}

void tuh_mount_cb(uint8_t daddr) {
    static uint16_t temp_buf[128]; 

    // Request the Product String (the device name)
    // 0x0409 is the Language ID for English (US)
	cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, false);	
    tuh_descriptor_get_product_string(daddr, 0x0409, temp_buf, sizeof(temp_buf), name_received_cb, 0);
}

void tuh_umount_cb(uint8_t daddr) {
	if (daddr == midi_dev_addr && midi_itf_idx != 0xFF) {
		irig_pro_connected = false;
		launchkey_connected = false;
		launchkey_daw_mode = false;
	}
	cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, false);	
}

void tuh_midi_mount_cb(uint8_t idx, const tuh_midi_mount_cb_t* mount_cb_data) {
	if (midi_itf_idx == 0xFF) {
		midi_itf_idx          = idx;
		midi_dev_addr         = mount_cb_data->daddr;
		launchkey_tx_cable_count = mount_cb_data->tx_cable_count;
	} 
	else 
	
	if (daw_itf_idx == 0xFF) {
		daw_itf_idx = idx;
		daw_dev_addr = mount_cb_data->daddr;
	}
	
	if (enable_mpc_sample) {
		config_mpc_sample();
	}
}

void tuh_midi_umount_cb(uint8_t idx) {
	if (idx == midi_itf_idx) {
		midi_itf_idx          = 0xFF;
		midi_dev_addr         = 0;
		launchkey_tx_cable_count = 0;
		launchkey_connected   = false;
		launchkey_daw_mode    = false;
		irig_pro_connected	  = false;
	}
	cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, false);
}

void tuh_hid_mount_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* desc_report, uint16_t desc_len) {
    uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);

    // Check if the mounted device is a keyboard
    if (itf_protocol == HID_ITF_PROTOCOL_KEYBOARD) {
        //printf("USB Keyboard mounted successfully!\n");
        // Start requesting data events from the device
        tuh_hid_receive_report(dev_addr, instance);
    }
}

void tuh_hid_report_received_cb(uint8_t dev_addr, uint8_t instance, uint8_t const* report, uint16_t len) {
    uint8_t const itf_protocol = tuh_hid_interface_protocol(dev_addr, instance);

    if (itf_protocol == HID_ITF_PROTOCOL_KEYBOARD) {
        hid_keyboard_report_t* kbd_report = (hid_keyboard_report_t*) report;

        // Process up to 6 simultaneous active keypresses (Boot Keyboard Protocol)
        for (int i = 0; i < 6; i++) {
            uint8_t keycode = kbd_report->keycode[i];
            if (keycode != 0) handle_keyboard_events(keycode);
        }

        // Continue listening for subsequent keyboard events
        tuh_hid_receive_report(dev_addr, instance);
    }
}

void tuh_hid_unmount_cb(uint8_t dev_addr, uint8_t instance) {
    //printf("USB Keyboard unmounted.\n");
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen) {
    (void) instance; (void) report_id; (void) report_type; (void) buffer; (void) reqlen;
    return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize) {
    (void) instance; (void) report_id; (void) report_type; (void) buffer; (void) bufsize;
}

//--------------------------------------------------------------------+
//
// Keyboard Controller 
//
//--------------------------------------------------------------------+

void handle_keyboard_events(uint8_t keycode) {
	// keycode contains the raw USB HID scancode (e.g., 0x04 for 'A')
	//printf("Key Pressed Scancode: 0x%02X\n", keycode);
	/*
	------------------------------
	## Alphanumeric Keys (A–Z, 0–9)

	| Key | Hex Scancode | Decimal | | Key | Hex Scancode | Decimal |
	|---|---|---|---|---|---|---|
	| A | 0x04 | 4 | | S | 0x16 | 22 |
	| B | 0x05 | 5 | | T | 0x17 | 23 |
	| C | 0x06 | 6 | | U | 0x18 | 24 |
	| D | 0x07 | 7 | | V | 0x19 | 25 |
	| E | 0x08 | 8 | | W | 0x1A | 26 |
	| F | 0x09 | 9 | | X | 0x1B | 27 |
	| G | 0x0A | 10 | | Y | 0x1C | 28 |
	| H | 0x0B | 11 | | Z | 0x1D | 29 |
	| I | 0x0C | 12 | | 1 (and !) | 0x1E | 30 |
	| J | 0x0D | 13 | | 2 (and @) | 0x1F | 31 |
	| K | 0x0E | 14 | | 3 (and #) | 0x20 | 32 |
	| L | 0x0F | 15 | | 4 (and $) | 0x21 | 33 |
	| M | 0x10 | 16 | | 5 (and %) | 0x22 | 34 |
	| N | 0x11 | 17 | | 6 (and ^) | 0x23 | 35 |
	| O | 0x12 | 18 | | 7 (and &) | 0x24 | 36 |
	| P | 0x13 | 19 | | 8 (and *) | 0x25 | 37 |
	| Q | 0x14 | 20 | | 9 (and () | 0x26 | 38 |
	| R | 0x15 | 21 | | 0 (and )) | 0x27 | 39 |

	------------------------------
	## Navigation, System & Editing Controls

	| Key | Hex Scancode | Decimal |
	|---|---|---|
	| ENTER (Return) | 0x28 | 40 |
	| ESCAPE | 0x29 | 41 |
	| BACKSPACE | 0x2A | 42 |
	| TAB | 0x2B | 43 |
	| SPACEBAR | 0x2C | 44 |
	| CAPS LOCK | 0x39 | 57 |
	| PRINTSCREEN | 0x46 | 70 |
	| SCROLL LOCK | 0x47 | 71 |
	| PAUSE (Break) | 0x48 | 72 |
	| INSERT | 0x49 | 73 |
	| HOME | 0x4A | 74 |
	| PAGE UP | 0x4B | 75 |
	| DELETE (Forward Delete) | 0x4C | 76 |
	| END | 0x4D | 77 |
	| PAGE DOWN | 0x4E | 78 |
	| RIGHT ARROW | 0x4F | 79 |
	| LEFT ARROW | 0x50 | 80 |
	| DOWN ARROW | 0x51 | 81 |
	| UP ARROW | 0x52 | 82 |

	------------------------------
	## Function Keys (F1–F12)

	| Key | Hex Scancode | Decimal | | Key | Hex Scancode | Decimal |
	|---|---|---|---|---|---|---|
	| F1 | 0x3A | 58 | | F7 | 0x40 | 64 |
	| F2 | 0x3B | 59 | | F8 | 0x41 | 65 |
	| F3 | 0x3C | 60 | | F9 | 0x42 | 66 |
	| F4 | 0x3D | 61 | | F10 | 0x43 | 67 |
	| F5 | 0x3E | 62 | | F11 | 0x44 | 68 |
	| F6 | 0x3F | 63 | | F12 | 0x45 | 69 |

	------------------------------
	## Punctuation & Symbols

	| Key | Hex Scancode | Decimal |
	|---|---|---|
	| - (Minus / Underscore) | 0x2D | 45 |
	| = (Equal / Plus) | 0x2E | 46 |
	| [ (Left Bracket / Brace) | 0x2F | 47 |
	| ] (Right Bracket / Brace) | 0x30 | 48 |
	| \ (Backslash / Pipe) | 0x31 | 49 |
	| ; (Semicolon / Colon) | 0x33 | 51 |
	| ' (Apostrophe / Quote) | 0x34 | 52 |
	| ` (Grave Accent / Tilde) | 0x35 | 53 |
	| , (Comma / Less Than) | 0x36 | 54 |
	| . (Period / Greater Than) | 0x37 | 55 |
	| / (Slash / Question Mark) | 0x38 | 56 |

	------------------------------
	## Numeric Keypad (NumPad)

	| Key | Hex Scancode | Decimal | | Key | Hex Scancode | Decimal |
	|---|---|---|---|---|---|---|
	| Num Lock | 0x53 | 83 | | Keypad 6 | 0x5E | 94 |
	| Keypad / | 0x54 | 84 | | Keypad 7 | 0x5F | 95 |
	| Keypad * | 0x55 | 85 | | Keypad 8 | 0x60 | 96 |
	| Keypad - | 0x56 | 86 | | Keypad 9 | 0x61 | 97 |
	| Keypad + | 0x57 | 87 | | Keypad 0 | 0x62 | 98 |
	| Keypad ENTER | 0x58 | 88 | | Keypad . | 0x63 | 99 |
	| Keypad 1 | 0x59 | 89 | | Non-US \ | 0x64 | 100 |
	| Keypad 2 | 0x5A | 90 | | Keypad = | 0x67 | 103 |
	| Keypad 3 | 0x5B | 91 | | | | |
	| Keypad 4 | 0x5C | 92 | | | | |
	| Keypad 5 | 0x5D | 93 | | | | |

	------------------------------
	## ⚠️ A Note on Modifier Keys (Shift, Ctrl, Alt, Gui)
	Modifier keys are unique. Standard keyboards do not report modifiers inside the 6-key keycode[] array. Instead, they are reported as a single bitmask byte inside the hid_keyboard_report_t struct (usually mapped under report->modifier).
	If you need to evaluate them, test the bits against these constant definitions:

	* Left CTRL: 0x01 (Bit 0)
	* Left SHIFT: 0x02 (Bit 1)
	* Left ALT: 0x04 (Bit 2)
	* Left GUI (Windows/Cmd Key): 0x08 (Bit 3)
	* Right CTRL: 0x10 (Bit 4)
	* Right SHIFT: 0x20 (Bit 5)
	* Right ALT (AltGr): 0x40 (Bit 6)
	* Right GUI: 0x80 (Bit 7)

	------------------------------	
	*/
	
	bool trigger_sampler = false; 
	but1 = 0; but0 = 0; but2 = 0; but3 = 0;  but4 = 0; green = 0; red = 0; blue = 0; yellow = 0; orange = 0; starpower = 0; pitch = 0; logo = 0;
				
	if (keycode == 40 || keycode == 44 || keycode == 45) { 											
		mbut0 = 1; 										// start/stop
		gamepad_bluetooth_handle_data();				
	}
	else
		
	if (keycode == 79 || (keycode == 46 && style_started)) { 		// -> key - next style	[plus/=]				
		dpad_down = 1; 			 					
		gamepad_bluetooth_handle_data();				
	}
	else

	if (keycode == 80 || (keycode == 39 && style_started)) {								
		dpad_down = 1; but4 = 1; 									// <- key - prev style [0/)]
		gamepad_bluetooth_handle_data();				
	}
	else
	
	if (keycode == 81 || (keycode == 46 && !style_started)) {	// next style group
		style_group = style_group + 1;
		if (style_group > 20) style_group = 0;
		trigger_sampler	 = true;		
	}
	else
		
	if (keycode == 82 || (keycode == 39 && !style_started)) {	// previous style group
		style_group = style_group - 1;
		if (style_group < 0) style_group = 20;	
		trigger_sampler	 = true;
	}	
	
	if (trigger_sampler) 
	{
		if (enable_wav_trigger_pro) {
			sampler_midi_note(0x9F, 36 + style_group, 127);	 // select and load preset
		} 									
		else

		if (enable_nanobox_tangerine) {
			midi_send_program_change(0xCF, style_group + 2); // select preset on channel 16 and skip both 1010 pianos	
		}	
	}
}

//--------------------------------------------------------------------+
//
// Chord detection helpers 
//
//--------------------------------------------------------------------+

static void chord_note_on(uint8_t note) {
    uint32_t bit = 1u << (note & 31);
    if (!(held_notes_mask[note >> 5] & bit)) {
        held_notes_mask[note >> 5] |= bit;
        held_note_count++;
    }
}

static void chord_note_off(uint8_t note) {
    uint32_t bit = 1u << (note & 31);
    if (held_notes_mask[note >> 5] & bit) {
        held_notes_mask[note >> 5] &= ~bit;
        if (held_note_count > 0) held_note_count--;
    }
}

static int chord_collect_pcs(uint8_t pcs[12], uint8_t *lowest) {
	// Collect unique pitch classes of all held notes into pcs[]; set *lowest to
	// the lowest held MIDI note number.  Returns the number of unique pitch classes.
	
    int count = 0;
    *lowest = 255;
    for (int n = 0; n < 128; n++) {
        if (!(held_notes_mask[n >> 5] & (1u << (n & 31)))) continue;
        if (*lowest == 255) *lowest = (uint8_t)n;
        uint8_t pc = (uint8_t)(n % 12);
        bool dup = false;
        for (int i = 0; i < count; i++) {
            if (pcs[i] == pc) { dup = true; break; }
        }
        if (!dup && count < 12) pcs[count++] = pc;
    }
    return count;
}

static bool triad_match(uint8_t root, const uint8_t pcs[3], uint8_t i0, uint8_t i1) {
	// Returns true when pcs[0..2] is exactly the set {root, root+i0, root+i1} mod 12.	
    uint8_t t1 = (root + i0) % 12;
    uint8_t t2 = (root + i1) % 12;
    int found = 0;
    for (int i = 0; i < 3; i++) {
        if (pcs[i] == root || pcs[i] == t1 || pcs[i] == t2) found++;
    }
    return found == 3;
}

static void chord_detect(void) {
	// Analyse the currently held notes and print the detected chord whenever 3 or
	// 4 notes are pressed simultaneously.  Recognised chord types:
	//   3-note: major {0,4,7}, minor {0,3,7}, sus4 {0,5,7}
	//   4-note: maj7  {0,4,7,11}; falls back to a slash-chord triad + bass note.
	
    if (held_note_count < 3) return;

    uint8_t pcs[12];
    uint8_t lowest = 255;
    int n = chord_collect_pcs(pcs, &lowest);
    if (n < 3 || n > 4) return;

    const char *chord_name = NULL;
    uint8_t chord_root = 255;

    if (n == 3) {
        // Interval tables for triad shapes
        static const uint8_t ti0[3] = { 4, 3, 5 };   // major, minor, sus4
        static const uint8_t ti1[3] = { 7, 7, 7 };
        static const char *const tn[3] = { "maj", "min", "sus4" };
        for (int t = 0; t < 3 && !chord_name; t++) {
            for (int r = 0; r < 3 && !chord_name; r++) {
                if (triad_match(pcs[r], pcs, ti0[t], ti1[t])) {
                    chord_root = pcs[r];
                    chord_name = tn[t];
                }
            }
        }
    } else { // n == 4
        // Try maj7: {root, root+4, root+7, root+11}
        for (int r = 0; r < 4 && !chord_name; r++) {
            uint8_t t1 = (pcs[r] + 4)  % 12;
            uint8_t t2 = (pcs[r] + 7)  % 12;
            uint8_t t3 = (pcs[r] + 11) % 12;
            int found = 0;
            for (int i = 0; i < 4; i++) {
                if (pcs[i] == pcs[r] || pcs[i] == t1 ||
                    pcs[i] == t2      || pcs[i] == t3) found++;
            }
            if (found == 4) { chord_root = pcs[r]; chord_name = "maj7"; }
        }
        // Fallback: try a triad within any 3-note subset (slash chord)
        if (!chord_name) {
            static const uint8_t si0[3] = { 4, 3, 5 };
            static const uint8_t si1[3] = { 7, 7, 7 };
            static const char *const sn[3] = { "maj", "min", "sus4" };
            for (int skip = 0; skip < 4 && !chord_name; skip++) {
                uint8_t sub[3]; int si = 0;
                for (int j = 0; j < 4; j++) if (j != skip) sub[si++] = pcs[j];
                for (int t = 0; t < 3 && !chord_name; t++) {
                    for (int r = 0; r < 3 && !chord_name; r++) {
                        if (triad_match(sub[r], sub, si0[t], si1[t])) {
                            chord_root = sub[r];
                            chord_name = sn[t];
                        }
                    }
                }
            }
        }
    }

    if (chord_name && lowest != 255) {
        // Map chord type to advanced_chord type nibble (pico_bluetooth.c scheme):
        //   0 = major  (also used for maj7, which has no dedicated sampler type)
        //   1 = minor
        //   2 = sus4
        uint8_t type = 0;
        if (strcmp(chord_name, "min") == 0)  type = 1;
        else if (strcmp(chord_name, "sus4") == 0) type = 2;

        // Encode advanced_chord: high byte = root (1-based), middle nibble =
        // bass (1-based), low nibble = type.  Mirrors the pico_bluetooth.c scheme.
        uint8_t root_1based = chord_root + 1;
        uint8_t bass_1based = (lowest % 12) + 1;
        advanced_chord = (root_1based * 256) + (bass_1based * 16) + type;
		trigger_loop();
    }
}

void trigger_loop() {

	if (enable_mpx_looper) 	{					
		mpx_trigger_loop();
	}
	else
	
	if (enable_mpc_sample) {
		mpc_trigger_loop();
	} 
	else 
	
	if (enable_nanobox_tangerine || enable_wav_trigger_pro) {
		sampler_trigger_loop();
	}		
	else 
	
	if (enable_sp404mk2) {
		sp404_trigger_loop();
	}	
}

void process_midi_byte(uint8_t b) {	
	uint8_t buffer[1];
	//bool forward_midi_event = true;
	
	if (b & 0x80) {
		// Status byte.
		if (b >= 0xF8) {
			// Real-time message (single byte); does not affect running status.
			return;
		}
		if (b >= 0xF0) {
			// System Common message; cancels running status per MIDI spec.
			midi_running_status = 0;
			midi_data_count = 0;
			return;
		}
		// Channel message: update running status; reset data accumulator.
		midi_running_status = b;
		unsigned char channel_zero_indexed = midi_running_status & 0x0F;
		midi_data_count = 0;
	} else {
		// Data byte — only process Note On / Note Off messages.
		uint8_t cmd = midi_running_status & 0xF0;
		
		if (cmd == 0x80 || cmd == 0x90) 
		{
			if (midi_data_count == 0) {
				midi_data0 = b;        // first data byte: note number
				midi_data_count = 1;
			} else {
				// Second data byte: velocity.  Complete the message.
				uint8_t note     = midi_data0;
				uint8_t velocity = b;
				midi_data_count  = 0;  // ready for next running-status pair				
				bool note_on = (cmd == 0x90) && (velocity > 0);
					
				if (style_started) 
				{					
					if ((note >= 0x60 && note <= 0x77))	{
						b = 0; // make note silent on midi synth

						if (note_on) {						
							but1 = 0; but0 = 0; but2 = 0; but3 = 0;  but4 = 0; but6 = 0; starpower = 0; dpad_down = 0; pitch = 0;
							green = 0; red = 0; blue = 0; yellow = 0; orange = 0;
						
							if (note >= 0x70 && note <= 0x77) {	
								dpad_down = 1; 							
								
								if (note == 0x70) {but1 = 1;}
								if (note == 0x71) {but0 = 1;}
								if (note == 0x72) {but2 = 1;}
								if (note == 0x73) {but3 = 1;}	
								if (note == 0x74) {but1 = 1;  but0 = 1;}
								if (note == 0x75) {but0 = 1;  but2 = 1;}
								if (note == 0x76) {but2 = 1;  but3 = 1;}
								if (note == 0x77) {but3 = 1;  but4 = 1;}								
								
								gamepad_bluetooth_handle_data();							
							}
							else
								
							if (note >= 0x60 && note <= 0x67) {
								
								if (note == 0x63) {
									mute_midi_controller = !mute_midi_controller;
								
								} else {																
									but6 = 1;
									
									if (note == 0x60) {but1 = 1; but3 = 1;}	// toggle mute drums
									if (note == 0x61) {but0 = 1; but3 = 1;}	// toggle mute bass										
									if (note == 0x62) {but1 = 1; but2 = 1;}	// toggle mute chords
									if (note == 0x64) {but1 = 1; but4 = 1;}	// toggle mute worship pads
									
									gamepad_bluetooth_handle_data();
								}									
							}																
						}
					}
					else {
						if (note_on) {
							chord_note_on(note);
						} else {
							chord_note_off(note);
						}
						
						chord_detect();						
					}
				}
				else 
					
				if (note_on) 
				{					
					if (style_end_requested) {
						style_end_requested = false;
						
						if (enable_nanobox_tangerine) {
							style_end_started = true;	
							sampler_midi_note(0x94, END1, sample_drum_velocity);
							nanobox_stop_loops();						
						} 
						else 
						
						if (enable_wav_trigger_pro) {
							wav_trigger_pro_stop_loops();						
							sampler_midi_note(0x94, END1, sample_drum_velocity); // not a loop								
						}
					}
					else
						
					if (style_end_started) {
						style_end_started = false;					
						sampler_midi_note(0x94, END1, sample_drum_velocity);	
					}
					else		// use pad keys to load a new style from SD Card
						
					if (launchkey_daw_mode && (note >= 0x60 && note <= 0x77) && (enable_nanobox_tangerine || enable_wav_trigger_pro)) {
						//forward_midi_event = false;
						
						if (note >= 0x60 && note <= 0x67) {							// launchkey top roww
							style_group = note - 0x60;
						}
						else
							
						if (note >= 0x70 && note <= 0x77) {							// launchkey bottom row
							style_group = note - 0x70 + 8;								
						}

						if (enable_wav_trigger_pro) {
							sampler_midi_note(0x9F, 36 + style_group, 127);	 // select and load preset
						} 									
						else

						if (enable_nanobox_tangerine) {
							midi_send_program_change(0xCF, style_group + 2); // select preset on channel 16 and skip both 1010 pianos	
						}
					}
				}					
			}
		}
		else
			
		if (cmd == 0xB0) 
		{				
			if (midi_data_count == 0) {
				midi_data0 = b;        			// first data byte: cc command
				midi_data_count = 1;
			} else {						
				uint8_t cc_cmd	= midi_data0;	
				uint8_t cc_value = b;			// Second data byte: value.  Complete the message.
				midi_data_count  = 0; 			// ready for next running-status pair
				
				if (cc_cmd == 0x73 && cc_value == 0x7F && launchkey_daw_mode) {
					but1 = 0; but0 = 0; but2 = 0; but3 = 0;  but4 = 0; green = 0; red = 0; blue = 0; yellow = 0; orange = 0;					
					mbut0 = 1; logo = 0;										// start/stop
					gamepad_bluetooth_handle_data();

					if (style_started) launchkey_set_led(0x90, 0, 36, 45);
					if (!style_started) launchkey_set_led(0x90, 2, 37, 5);
					launchkey_display_text("Jamin Controller", true); 				
				}				
				else

				if (cc_cmd == 0x75 && cc_value == 0x7F && launchkey_daw_mode) {
					but1 = 0; but0 = 0; but2 = 0; but3 = 0;  but4 = 0; green = 0; red = 0; blue = 0; yellow = 0; orange = 0;					
					joy_up = true; joystick_up = 0;								// fill
					gamepad_bluetooth_handle_data();				
				}	
				else

				if (cc_cmd == 0x6A && cc_value == 0x7F && launchkey_daw_mode) {
					but1 = 0; but0 = 0; but2 = 0; but3 = 0;  but4 = 0; green = 0; red = 0; blue = 0; yellow = 0; orange = 0;					
					dpad_down = 1; starpower = 0;			// next style					
					gamepad_bluetooth_handle_data();				
				}
				else

				if (cc_cmd == 0x6B && cc_value == 0x7F && launchkey_daw_mode) {
					but1 = 0; but0 = 0; but2 = 0; but3 = 0;  but4 = 1; green = 0; red = 0; blue = 0; yellow = 0; orange = 0;										
					dpad_down = 1; starpower = 0;			// prev style
					gamepad_bluetooth_handle_data();				
				}
				else

				if ((cc_cmd == 0x33 || cc_cmd == 0x34) && cc_value == 0x7F && launchkey_daw_mode && !style_started) {

					if (cc_cmd == 0x33) {							// next style group
						style_group = style_group + 1;
						if (style_group > 20) style_group = 0;
					}
					else
						
					if (cc_cmd == 0x34) {							// previous style group
						style_group = style_group - 1;
						if (style_group < 0) style_group = 20;								
					}

					if (enable_wav_trigger_pro) {
						sampler_midi_note(0x9F, 36 + style_group, 127);	 // select and load preset
					} 									
					else

					if (enable_nanobox_tangerine) {
						midi_send_program_change(0xCF, style_group + 2); // select preset on channel 16 and skip both 1010 pianos	
					}				
				}				
				else	
					
				if (cc_cmd == 0x17 && cc_value == 0x7F && irig_pro_connected) {	// data button press
				
					if (held_note_count < 3) {						// start/stop
						mbut0 = 1; logo = 0;
						gamepad_bluetooth_handle_data();
					
					} else {										// fill
						joy_up = true; joystick_up = 0;
						gamepad_bluetooth_handle_data();									
					}
				}
				else

				if (cc_cmd == 0x16 && irig_pro_connected) {						// data button dial
				
					if (style_started) {
						if (cc_value == 0x1) {									// next style
							dpad_down = 1; starpower = 0;	
							gamepad_bluetooth_handle_data();
						}
						else
							
						if (cc_value == 0x7F) {									// previous style
							dpad_down = 1; starpower = 0; orange = 0; but4 = 1;
							gamepad_bluetooth_handle_data();								
						}
					} else {
						
						if (cc_value == 0x1) {							// next style group
							style_group = style_group + 1;
							if (style_group > 20) style_group = 0;
						}
						else
							
						if (cc_value == 0x7F) {							// previous style group
							style_group = style_group - 1;
							if (style_group < 0) style_group = 20;								
						}

						if (enable_wav_trigger_pro) {
							sampler_midi_note(0x9F, 36 + style_group, 127);	 // select and load preset
						} 									
						else

						if (enable_nanobox_tangerine) {
							midi_send_program_change(0xCF, style_group + 2); // select preset on channel 16 and skip both 1010 pianos	
						}																		
					}
				}
				else	// These encoder values are for iRig or SMC-PAD or x-touch-mini or launchkey
					
				if (cc_cmd == 0x0C || cc_cmd == 0x1E || cc_cmd == 0x01 || cc_cmd == 0x15) {			// drum volume 
				
					if (enable_wav_trigger_pro) 
					{ 
						if (cc_value != previous_drum_vol) {
							previous_drum_vol = cc_value;
							
							if (sampler_old_drum_note != 255) {							
								//uint16_t track_no = (204 * style_group) + 97 + sampler_old_drum_note - 36;
								//wav_trigger_pro_set_volume(track_no, cc_value);
								sample_drum_velocity = cc_value;
							}
	
						}
					} else {
						sample_drum_velocity = cc_value;
					}
				}
				else
					
				if (cc_cmd == 0x0D || cc_cmd == 0x1F || cc_cmd == 0x02 || cc_cmd == 0x16) {			// bass volume
				
					if (enable_wav_trigger_pro) 
					{ 
						if (cc_value != previous_bass_vol) {
							previous_bass_vol = cc_value;
							
							if (sampler_old_bass_note != 255) {
								//uint16_t track_no = (204 * style_group) + 180 + sampler_old_bass_note - 36;
								//wav_trigger_pro_set_volume(track_no, cc_value);
								sample_bass_velocity = cc_value;								
							}								
	
						}
					} else {				
						sample_bass_velocity = cc_value;
					}
				}
				else

				if (cc_cmd == 0x0E || cc_cmd == 0x20 || cc_cmd == 0x03|| cc_cmd == 0x17) {			// chord volume
				
					if (enable_wav_trigger_pro) 
					{ 
						if (cc_value != previous_chord_vol) {
							previous_chord_vol = cc_value;
							
							if (sampler_old_chord_note != 255) {							
								//uint16_t track_no = (204 * style_group) + 108 + sampler_old_chord_note - 36;
								//wav_trigger_pro_set_volume(track_no, cc_value);		
								sample_chord_velocity = cc_value;								
							}
						}
					} else {				
						sample_chord_velocity = cc_value;
					}
				}
				else

				if (cc_cmd == 0x0F || cc_cmd == 0x21 || cc_cmd == 0x04|| cc_cmd == 0x18) {			// midi guitar volume
				
					if (cc_value != midi_guitar_volume) {				
						midi_guitar_volume = cc_value;	

						if (enable_wav_trigger_pro) {
							//uint16_t track_no = (204 * style_group) + previous_guitar_note;
							//wav_trigger_pro_set_volume(track_no, cc_value);	
							
						} else {
							midi_send_control_change(0xB0, 7, midi_guitar_volume);					
						}
					}
				}	
				else

				if (cc_cmd == 0x10 || cc_cmd == 0x22 || cc_cmd == 0x05 || cc_cmd == 0x19) {			// worship pad volume
					worship_pad_velocity = cc_value;
				}
				else

				if (cc_cmd == 0x11 || cc_cmd == 0x23 || cc_cmd == 0x06 || cc_cmd == 0x1A) {			// chord1 & chord2 pad volumes
					chord1_pad_velocity = cc_value;
					chord2_pad_velocity = cc_value;					
				}
				else

				if (cc_cmd == 0x12 || cc_cmd == 0x24 || cc_cmd == 0x07 || cc_cmd == 0x1B) {			// tempo
					uint8_t tempo = 60 + (cc_value / 127 * 80);
					set_tempo(tempo);
				}
				else

				if (cc_cmd == 0x08) {												// unused

				}				
				else

				if (cc_cmd == 0x13 || cc_cmd == 0x25 || cc_cmd == 0x09 || cc_cmd == 0x1C) {			// master volume
					uint8_t lead_vol = cc_value;					
					midi_send_control_change(0xB0, 7, lead_vol);
					
					if (!enable_wav_trigger_pro) {
						midi_send_control_change(0xB1, 7, lead_vol);
						midi_send_control_change(0xB2, 7, lead_vol);					
						midi_send_control_change(0xB9, 7, lead_vol);	
					}						
				}				
			}						
			
		} else {
			// Other channel messages: Program Change (0xC0) and Channel
			// Pressure (0xD0) carry one data byte; all others carry two.
			uint8_t expected = ((cmd == 0xC0) || (cmd == 0xD0)) ? 1 : 2;
			midi_data_count++;
			if (midi_data_count >= expected) midi_data_count = 0;
		}
	}

	buffer[0] = b;	
	tud_midi_n_stream_write(0, 0, buffer, 1);

	if (!enable_mpx_looper && !mute_midi_controller) { 	// filter midi events from mpx pads	to midi synth			
		uart_write_blocking(UART_ID, buffer, 1);
		uart_tx_wait_blocking(UART_ID);			
	}	
	
}

void tuh_midi_rx_cb(uint8_t idx, uint32_t xferred_bytes) {
	if (xferred_bytes == 0) return;

	uint8_t buffer[HOST_MIDI_RX_BUFFER_SIZE];
	uint8_t cable_num = 0;
	uint32_t bytes_read = 0;

	while ((bytes_read = tuh_midi_stream_read(idx, &cable_num, buffer, sizeof(buffer))) > 0) 
	{			
		for (uint32_t i=0; i<bytes_read; i++) 
		{
			if (enable_mpc_sample || enable_sp404mk2 || enable_nanobox_tangerine || enable_wav_trigger_pro) {
				// Parse the raw MIDI byte stream to track note on/off events.
				process_midi_byte(buffer[i]);
			}
		}
	
		cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, true);		
	}
}

void tuh_midi_tx_cb(uint8_t idx, uint32_t xferred_bytes) {
	(void) idx;
	(void) xferred_bytes;
}

void tud_mount_cb(void) {
  cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, true);
}

void tud_umount_cb(void) {
  cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, false);
}

void tud_suspend_cb(bool remote_wakeup_en) {
	// Invoked when usb bus is suspended
	// remote_wakeup_en : if host allow us  to perform remote wakeup
	// Within 7ms, device must draw an average of current less than 2.5 mA from bus	
  (void) remote_wakeup_en;
  cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, false);
}

void tud_resume_cb(void) {
	// Invoked when usb bus is resumed	
	cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, true);
}

//--------------------------------------------------------------------+
//
// MIDI Tasks
//
//--------------------------------------------------------------------+

void dream_set_delay(int tempo) {
	uint8_t rate = (60000 / tempo / 128 / 4) % 128;
	int check_sum = (rate % 128);
	check_sum = 128 - check_sum;
	if (check_sum == 128) check_sum = 0;
	
	uint8_t msg[11];	
	msg[0] = 0xF0;
	msg[1] = 0x41;
	msg[2] = 0x00;   
	msg[3] = 0x42;
	msg[4] = 0x12;
	msg[5] = 0x40; 
	msg[6] = 0x01;
	msg[7] = 0x35;
	msg[8] = rate;
	msg[9] = check_sum;	
	msg[10] = 0xF7;	
	midi_n_stream_write(0, 0, msg, 11);	

	//msg[7] = 0x3D;
	//midi_n_stream_write(0, 0, msg, 11);		
}

void midi_modx_key(uint8_t key) {
	if (!enable_modx) return;
	
	uint8_t msg[12];	
	msg[0] = 0xF0;
	msg[1] = 0x43;
	msg[2] = 0x10;   
	msg[3] = 0x7F;
	msg[4] = 0x1C;
	msg[5] = 0x0D; 
	msg[6] = 0x00;
	msg[7] = 0x00;
	msg[8] = 0x02;
	msg[9] = 0x00;	
	msg[10] = 0x40 + key;
	msg[11] = 0xF7;
	
	midi_n_stream_write(0, 0, msg, 12);	
}

void midi_modx_arp_octave(uint8_t octave) {
	if (!enable_modx) return;	
	
	uint8_t msg[12];	
	msg[0] = 0xF0;
	msg[1] = 0x43;
	msg[2] = 0x10;   
	msg[3] = 0x7F;
	msg[4] = 0x1C;
	msg[5] = 0x0D; 
	msg[6] = 0x00;
	msg[7] = 0x00;
	msg[8] = 0x02;
	msg[9] = 0x02;	
	msg[10] = 0x40 + octave;
	msg[11] = 0xF7;	
	
	midi_n_stream_write(0, 0, msg, 12);		
}

void midi_modx_arp(bool on) {
	if (!enable_modx) return;	
		
	uint8_t msg[12];	
	msg[0] = 0xF0;
	msg[1] = 0x43;
	msg[2] = 0x10;   
	msg[3] = 0x7F;
	msg[4] = 0x1C;
	msg[5] = 0x0D; 
	msg[6] = 0x06;
	msg[7] = 0x00;
	msg[8] = 0x01;
	msg[9] = 0x09;	
	msg[10] = on ? 1 : 0;
	msg[11] = 0xF7;
	
	midi_n_stream_write(0, 0, msg, 12);		
}

void midi_modx_arp_hold(uint8_t part, bool on) {
	if (!enable_modx) return;	
		
	uint8_t msg[13];	
	msg[0] = 0xF0;
	msg[1] = 0x43;
	msg[2] = 0x10;   
	msg[3] = 0x7F;
	msg[4] = 0x1C;
	msg[5] = 0x0D; 
	msg[6] = 0x10 + part;
	msg[7] = 0x00;
	msg[8] = 0x06;
	msg[9] = 0x00;	
	msg[10] = 0x00;
	msg[11] = on ? 2 : 1;
	msg[12] = 0xF7;
	
	midi_n_stream_write(0, 0, msg, 13);		
}

void midi_modx_arp_realtime(uint8_t part, bool on) {
	if (!enable_modx) return;	
		
	uint8_t msg[13];	
	msg[0] = 0xF0;
	msg[1] = 0x43;
	msg[2] = 0x10;   
	msg[3] = 0x7F;
	msg[4] = 0x1C;
	msg[5] = 0x0D; 
	msg[6] = 0x10 + part;
	msg[7] = 0x00;
	msg[8] = 0x06;
	msg[9] = 0x10;	
	msg[10] = 0x00;
	msg[11] = on ? 0 : 1;
	msg[12] = 0xF7;
	
	midi_n_stream_write(0, 0, msg, 13);		
}

void midi_modx_tempo(int tempo) {
	if (!enable_modx) return;	
	
	uint8_t msg[13];	
	msg[0] = 0xF0;
	msg[1] = 0x43;
	msg[2] = 0x10;   
	msg[3] = 0x7F;
	msg[4] = 0x1C;
	msg[5] = 0x0D; 
	msg[6] = 0x06;
	msg[7] = 0x00;
	msg[8] = 0x02;
	msg[9] = 0x1E;	
	msg[10] = (int) (tempo / 128);
	msg[11] = tempo % 128;
	msg[12] = 0xF7;
	
	midi_n_stream_write(0, 0, msg, 13);	
}

void midi_seqtrak_arp_octave(uint8_t track, int octave) {
	if (!enable_seqtrak) return;	
	
	uint8_t msg[11];	
	msg[0] = 0xF0;
	msg[1] = 0x43;
	msg[2] = 0x10;   
	msg[3] = 0x7F;
	msg[4] = 0x1C;
	msg[5] = 0x0C; 
	msg[6] = 0x31;
	msg[7] = 0x50 + track;
	msg[8] = 0x1C;
	msg[9] = 0x40 + octave;	// 0x3D - 0x43 (-3 to +3)
	msg[10] = 0xF7;
	
	midi_n_stream_write(0, 0, msg, 11);	
}

void midi_seqtrak_tempo(int tempo) {
	if (!enable_seqtrak) return;	
	
	uint8_t msg[12];	
	msg[0] = 0xF0;
	msg[1] = 0x43;
	msg[2] = 0x10;   
	msg[3] = 0x7F;
	msg[4] = 0x1C;
	msg[5] = 0x0C; 
	msg[6] = 0x30;
	msg[7] = 0x40;
	msg[8] = 0x76;
	msg[9] = (int) (tempo / 128);
	msg[10] = tempo % 128;
	msg[11] = 0xF7;
	
	midi_n_stream_write(0, 0, msg, 12);	
}

void midi_seqtrak_key(uint8_t key) {
	if (!enable_seqtrak) return;	
	
	uint8_t msg[11];	
	msg[0] = 0xF0;
	msg[1] = 0x43;
	msg[2] = 0x10;   
	msg[3] = 0x7F;
	msg[4] = 0x1C;
	msg[5] = 0x0C; 
	msg[6] = 0x30;
	msg[7] = 0x40;
	msg[8] = 0x7F;
	msg[9] = key;
	msg[10] = 0xF7;
	
	midi_n_stream_write(0, 0, msg, 11);	
}

void midi_seqtrak_mute(uint8_t track, bool mute) {
	if (!enable_seqtrak) return;	
	
	uint8_t msg[11];	
	msg[0] = 0xF0;
	msg[1] = 0x43;
	msg[2] = 0x10;   
	msg[3] = 0x7F;
	msg[4] = 0x1C;
	msg[5] = 0x0C; 
	msg[6] = 0x30;
	msg[7] = 0x50 + track;
	msg[8] = 0x29;
	msg[9] = mute ? 0x7D : 0;
	msg[10] = 0xF7;
	
	midi_n_stream_write(0, 0, msg, 11);	
}

void midi_seqtrak_pattern(uint8_t pattern) {
	if (!enable_seqtrak) return;	
	
	uint8_t msg[11];	
	msg[0] = 0xF0;
	msg[1] = 0x43;
	msg[2] = 0x10;   
	msg[3] = 0x7F;
	msg[4] = 0x1C;
	msg[5] = 0x0C; 
	msg[6] = 0x30;

	msg[8] = 0x0F;
	msg[9] = pattern;
	msg[10] = 0xF7;
				
	for (int i=0; i<7; i++) {						
		msg[7] = 0x50 + i;
		midi_n_stream_write(0, 0, msg, 11);	
	}	
}

void midi_seqtrak_arp() {
	if (!enable_seqtrak) return;	
	
	// config bass arp on track 8 and 10		
	uint8_t template = get_arp_template();
									
	midi_send_control_change(0xB7, 27, active_neck_pos == 2 ? 8 : 2);		// Use Arp type UP only for bass
	midi_send_control_change(0xB7, 28, 127); 								// Arp Gate always 200%
	midi_send_control_change(0xB7, 29, style_section % 2 == 0 ? 9 : 6); 	// Arp Speed 25% (ARRA/ARRC) or 50% (ARRB/ARRD) 
	midi_seqtrak_arp_octave(7, -2);											// set octave to -2 for bass
	
	midi_send_control_change(0xB9, 27, template);	
	midi_send_control_change(0xB9, 28, 127); 							
	midi_send_control_change(0xB9, 29, style_section % 2 == 0 ? 6 : 9); 	// flip arp speed for bass and keys			
}

uint8_t get_arp_template(void) {
	if (active_strum_pattern == -1) return 15;																							// no chord data, use pattern data
	if (active_strum_pattern == 0) 	return style_section % 2 == 0 ? 13 : 14;															// strum - use chord 
	if (active_strum_pattern == 1) 	return style_section % 2 == 0 ? 6 : 7;																// strum/bass - use random 
	if (active_strum_pattern == 2) 	return style_section % 2 == 0 ? 2 : 3;																// arp1 - use up 
	if (active_strum_pattern == 3) 	return style_section % 2 == 0 ? 4 : 5;																// arp2 - use down
	if (active_strum_pattern == 4) 	return style_section % 2 == 0 ? (active_neck_pos == 1 ? 8 : 9) : (active_neck_pos == 2 ? 10: 11);	// arp3 - use up/down
	return 15;	
}

void sampler_midi_note(uint8_t command, uint8_t note, uint8_t velocity) {
	uint8_t msg[3];	
	
	msg[0] = command;
	msg[1] = note;
	msg[2] = velocity;   

	if (enable_wav_trigger_pro && midi_keyboard_connected) {	// WAV trigger Pro cannot be on USB Host. Something else is
		wav_trigger_pro_forward_midi_message(msg, 3);
	} else {
		midi_n_stream_write(0, 0, msg, 3);	// includes sampler connected by UART0 MIDI	
	}
}

void midi_send_note(uint8_t command, uint8_t note, uint8_t velocity) {
	previous_guitar_note = note;
	uint8_t channel = 0;	
	
	if (enable_seqtrak) {
		channel = 8;
	}
	else
		
	if (enable_nanobox_tangerine) {	
		channel = 0;
	}
	else
		
	if (enable_wav_trigger_pro) {
		channel = 0;
	}
	else
		
	if (enable_sp404mk2) {
		return;
	}
	else
		
	if (enable_mpc_sample) {
		return;
	}
	else
		
	if (enable_mpx_looper) {
		return;
	}
	
	uint8_t msg[3];	
	
	msg[0] = command + channel;
	msg[1] = note;
	msg[2] = velocity;	
		
	midi_n_stream_write(0, 0, msg, 3);			
}

void midi_send_control_change(uint8_t command, uint8_t controller, uint8_t value) {
	uint8_t msg[3];	
	
	msg[0] = command;
	msg[1] = controller;
	msg[2] = value;   
		
	midi_n_stream_write(0, 0, msg, 3);			
}

void midi_send_program_change(uint8_t command, uint8_t code) {
	uint8_t msg[2];	
	
	msg[0] = command;
	msg[1] = code;
		
	midi_n_stream_write(0, 0, msg, 2);		
}

void midi_start_stop(bool start) {
	uint8_t msg[1];	
	msg[0] = start ? 0xFA : 0xFC;	

	midi_n_stream_write(0, 0, msg, 1);	
	msg[0] = 0xF8;	
	midi_n_stream_write(0, 0, msg, 1);			
}

void midi_yamaha_start_stop(int8_t code, bool on) {
	uint8_t msg[6];	
	msg[0] = 0xF0;
	msg[1] = 0x43; 
	msg[2] = 0x60;
	msg[3] = code; 
	msg[4] = on ? 0x7F : 0x00;	
	msg[5] = 0xF7;

	midi_n_stream_write(0, 0, msg, 6);	
}

void midi_yamaha_arr(uint8_t code, bool on) {
	if (enable_seqtrak) return;
	
	uint8_t msg[7];	
	msg[0] = 0xF0;
	msg[1] = 0x43;
	msg[2] = 0x7E;   
	msg[3] = 0x00;
	msg[4] = code; 
	msg[5] = on ? 0x7F : 0x00;
	msg[6] = 0xF7;

	midi_n_stream_write(0, 0, msg, 7);	
}

void midi_ketron_arr(uint8_t code, bool on) {
	if (enable_seqtrak) return;
	
	uint8_t msg[8];	
	msg[0] = 0xF0;
	msg[1] = 0x26;
	msg[2] = 0x79;   
	msg[3] = 0x05;
	msg[4] = 0x00;
	msg[5] = code; 
	msg[6] = on ? 0x7F : 0x00;
	msg[7] = 0xF7;

	midi_n_stream_write(0, 0, msg, 8);	
}

void midi_ketron_footsw(uint8_t code, bool on) {
	if (enable_seqtrak) return;
	
	uint8_t msg[8];		
	msg[0] = 0xF0;
	msg[1] = 0x26;
	msg[2] = 0x7C;   
	msg[3] = 0x05;
	msg[4] = 0x01;
	msg[5] = 0x55 + code; 
	msg[6] = on ? 0x7F : 0x00;
	msg[7] = 0xF7;

	midi_n_stream_write(0, 0, msg, 8);	
}

void midi_send_chord_note(uint8_t note, uint8_t velocity) {
	uint8_t command = 0x90;
	uint8_t msg[3];	
	
	msg[0] = command + 3;
	msg[1] = note;
	msg[2] = velocity;   

	if (enable_seqtrak) 
	{
		if (enable_bass_track) {
			msg[0] = command + 7;						// AWM2 Synth (CH8)
			midi_n_stream_write(0, 0, msg, 3);	
		}
		
		if (enable_chord_track) {
			msg[0] = command + 9;						// DX Synth (CH10)
			midi_n_stream_write(0, 0, msg, 3);	
		}				
		
	} else {
		
		if (!enable_mpc_sample && !enable_sp404mk2 && !enable_mpx_looper && !enable_wav_trigger_pro) {
			midi_n_stream_write(0, 0, msg, 3);	// CH 4

			if (!enable_ample_guitar && !enable_modx && active_strum_pattern != 0 && active_strum_pattern != 1) {	// MIDI arpeggios only
				msg[0] = command + 2;
				msg[1] = note + 24;				
				
				if (velocity > 0 ) {				// (respect note off)
					msg[2] = chord2_pad_velocity;  
				}
				midi_n_stream_write(0, 0, msg, 3);	// CH 3	 

				msg[0] = command + 1;
				msg[1] = note + 36;					
				
				if (velocity > 0 ) {				// (respect note off)
					msg[2] = chord1_pad_velocity;  				
				}
				midi_n_stream_write(0, 0, msg, 3);	// CH 2						
			}
		}
	}	
}

void midi_play_chord(bool on, uint8_t p1, uint8_t p2, uint8_t p3) {
	
	if (!enable_ample_guitar || (enable_ample_guitar && active_strum_pattern == 0))
	{
		if (on) {
			
			if (enable_ample_guitar || enable_modx) {	// squeeze into C1 - B2
				p1 = (p1 % 12) + 36;
				p2 = (p2 % 12) + ((p2  % 12) <  (p1 % 12) ? 48 : 36);
				p3 = (p3 % 12) + ((p3  % 12) <  (p1 % 12) ? 48 : 36);
			}
			
			if (enable_seqtrak && basic_chord > 0 && (active_strum_pattern == 0 || style_started)) {	// sampler only with strum mode or full band
				uint8_t msg[3] = {0x9A, basic_chord + 59, 127};
				midi_n_stream_write(0, 0, msg, 3);
			}
			
			midi_send_chord_note( p1, enable_ample_guitar ? 100 : 127);
			midi_send_chord_note( p2, enable_ample_guitar ? 100 : 127);
			midi_send_chord_note( p3, enable_ample_guitar ? 100 : 127);		
			
			old_p1 = p1;
			old_p2 = p2;
			old_p3 = p3;						
		
			
		} else {
			midi_send_chord_note( old_p1, 0);
			midi_send_chord_note( old_p2, 0);
			midi_send_chord_note( old_p3, 0);
			midi_send_chord_note( old_p4, 0);

			if (enable_seqtrak && basic_chord > 0 && (active_strum_pattern == 0 || style_started)) {
				uint8_t msg[3] = {0x8A, basic_chord + 59, 0};
				midi_n_stream_write(0, 0, msg, 3);
			}				
		}
	}
}

void midi_play_slash_chord(bool on, uint8_t p1, uint8_t p2, uint8_t p3, uint8_t p4)  {	

	if (!enable_ample_guitar || (enable_ample_guitar && active_strum_pattern == 0))
	{	
		if (on) {

			if (enable_ample_guitar || enable_modx) {	// squeeze into C1 - B2
				p1 = (p1 % 12) + 36;
				p2 = (p2 % 12) + ((p2  % 12) <  (p1 % 12) ? 48 : 36);
				p3 = (p3 % 12) + ((p3  % 12) <  (p1 % 12) ? 48 : 36);
				p4 = (p4 % 12) + ((p4  % 12) <  (p1 % 12) ? 48 : 36);
			}
			
			if (enable_seqtrak && basic_chord > 0 && (active_strum_pattern == 0 || style_started)) {
				uint8_t msg[3] = {0x9A, basic_chord + 59, 127};
				midi_n_stream_write(0, 0, msg, 3);
			}			
			
			midi_send_chord_note( p1, enable_ample_guitar ? 100 : 127);
			midi_send_chord_note( p2, enable_ample_guitar ? 100 : 127);
			midi_send_chord_note( p3, enable_ample_guitar ? 100 : 127);
			midi_send_chord_note( p4, enable_ample_guitar ? 100 : 127);	

			old_p1 = p1;
			old_p2 = p2;
			old_p3 = p3;		
			old_p4 = p4;
			
			
		} else {
			midi_send_chord_note( old_p1, 0);
			midi_send_chord_note( old_p2, 0);
			midi_send_chord_note( old_p3, 0);		
			midi_send_chord_note( old_p4, 0);	

			if (enable_seqtrak && basic_chord > 0 && (active_strum_pattern == 0 || style_started)) {
				uint8_t msg[3] = {0x8A, basic_chord + 59, 0};
				midi_n_stream_write(0, 0, msg, 3);
			}				
		}
	}
}

void midi_n_stream_write(uint8_t itf, uint8_t cable_num, uint8_t *buffer, uint32_t bufsize) {
	tud_midi_n_stream_write(itf, cable_num, buffer, bufsize);
	
	if (!midi_keyboard_connected) 	// don't send control events to midi keyboard
	{
		if (midi_itf_idx != 0xFF) {
			tuh_midi_stream_write(midi_itf_idx, cable_num, buffer, bufsize);
			tuh_midi_write_flush(midi_itf_idx);
		}
	}

	uart_write_blocking(UART_ID, buffer, bufsize);
	uart_tx_wait_blocking(UART_ID);
	
	wav_trigger_pro_forward_midi_message(buffer, bufsize);
}

void send_ble_midi(uint8_t* midi_data, int len) {
	/**
	* @brief Send a MIDI 1.0 byte stream to the connected BLE MIDI peripheral.
	*
	* The buffer must contain a complete MIDI message without running status
	* (i.e. each message starts with a full status byte).  The library will
	* timestamp and encode the bytes into a BLE-MIDI 1.0 packet and transmit it
	* when the connection allows.
	*
	* @param midi_data Pointer to the MIDI byte stream.
	* @param len       Number of bytes to send (must fit in a uint8_t, max 255).
	*/	
	if (midi_data == NULL || len <= 0 || len > 255) return;
	if (!ble_midi_controller_is_ready()) return;
	ble_midi_client_stream_write((uint8_t)len, midi_data);
}

//--------------------------------------------------------------------+
//
// WAV Trigger Pro
//
//--------------------------------------------------------------------+

void calculate_sampler_matrix_indexed(int target_key, int tempo_index, int *out_track_index, int *out_pitch_cents) {
	/**
	 * Calculates the exact track index and hardware pitch adjustment using a tempo index.
	 * Bypasses all complex math or approximations for instant execution on the RP2040/RP2350.
	 *
	 * @param target_key       Target musical key index (0 = C, 1 = C#, ..., 11 = B)
	 * @param tempo_index      Target tempo index (-5 to +5, where 0 is original tempo)
	 * @param out_track_index  Pointer to store the resolved file index (0 to 11)
	 * @param out_pitch_cents  Pointer to store the final hardware pitch shift (-700 to +700 cents)
	 */
	 
    // 1. Enforce strict boundaries on the incoming tempo index argument
    if (tempo_index > 5)  tempo_index = 5;
    if (tempo_index < -5) tempo_index = -5;

    // 2. Map the index directly to cents using a fixed 100 cents per step.
    // Index step matches physical sample variant intervals, making math exact.
    int tempo_cents = tempo_index * 100;

    // 3. Round to the nearest semitone step (which equals the tempo index itself)
    int semitone_steps = tempo_index;

    // 4. Calculate the base file index (0-11) needed to counteract the tempo pitch shift.
    // Formulate as: (Target Key - Semitone Steps) Modulo 12
    int file_idx = (target_key - semitone_steps) % 12;
    if (file_idx < 0) {
        file_idx += 12; // Correct negative wrapping in C modulo operations
    }

    // 5. Calculate the remaining micro-tuning pitch adjustment required by the hardware.
    // Because tempo cents align perfectly with the file boundaries, this naturally resolves to 0.
    int pitch_offset_cents = tempo_cents - ((target_key - file_idx) * 100);

    // Correct edge-case wrapping bounds if the modulo wrapped across the octave boundary
    if (pitch_offset_cents > 700)  pitch_offset_cents -= 1200;
    if (pitch_offset_cents < -700) pitch_offset_cents += 1200;

    // 6. Pass the clean, calculated steps back to the playback routine
    *out_track_index = file_idx;
    *out_pitch_cents = pitch_offset_cents;
}

void trigger_indexed_sample(int target_key, int tempo_index) {
    int file_index = 0;
    int pitch_cents = 0;

    // Run the indexed matrix calculation
    calculate_sampler_matrix_indexed(target_key, tempo_index, &file_index, &pitch_cents);

    // Assuming your chromatic sample set begins at track 0001.wav on the SD card
    uint16_t track_to_play = 1 + file_index;

    // Direct library execution via I2C
    wav_trigger_pro_track_play_poly(
        track_to_play, 
        0,                                // 0dB (Unity Gain)
        WAV_TRIGGER_PRO_BALANCE_MID,      // Center Balance (64)
        0,                                // Immediate Attack (0ms)
        (int16_t)pitch_cents,             // Calculated pitch adjustment
        0                                 // Default flags
    );
}

static bool wav_trigger_pro_write_command(uint8_t cmd, const uint8_t *payload, size_t payload_len) {
	if (payload == NULL && payload_len > 0) return false;
	if (payload_len > WAV_TRIGGER_PRO_MAX_PAYLOAD_LEN) return false;

	uint8_t buffer[WAV_TRIGGER_PRO_MAX_MESSAGE_LEN];
	buffer[0] = cmd;

	if (payload_len > 0) {
		memcpy(&buffer[1], payload, payload_len);
	}

	return i2c_write_blocking(I2C_ID, WAV_TRIGGER_PRO_ADDR, buffer, payload_len + 1, false) == (int)(payload_len + 1);
}

static bool wav_trigger_pro_read_response(uint8_t *buffer, size_t len) {
	if (buffer == NULL || len == 0) return false;
	return i2c_read_blocking(I2C_ID, WAV_TRIGGER_PRO_ADDR, buffer, len, false) == (int)len;
}

static uint16_t wav_trigger_pro_pack_signed_16bit(int16_t value) {
	// Match WAVTriggerPro.cpp, which casts signed values to unsigned short before
	// splitting them into little-endian bytes for transport over Qwiic.
	return (uint16_t)value;
}

static uint16_t wav_trigger_pro_unpack_uint16(const uint8_t *bytes) {
	if (bytes == NULL) return 0;
	return (uint16_t)(((uint16_t)bytes[1] << 8) | bytes[0]);
}

static void wav_trigger_pro_pack_uint16(uint8_t *bytes, uint16_t value) {
	if (bytes == NULL) return;
	bytes[0] = (uint8_t)value;
	bytes[1] = (uint8_t)(value >> 8);
}

static void wav_trigger_pro_pack_int16(uint8_t *bytes, int16_t value) {
	if (bytes == NULL) return;
	wav_trigger_pro_pack_uint16(bytes, wav_trigger_pro_pack_signed_16bit(value));
}

static bool wav_trigger_pro_can_send_midi_message(const uint8_t *buffer, uint32_t bufsize) {
	if (!wav_trigger_pro_connected || buffer == NULL) return false;
	if (bufsize < 2 || bufsize > 3) return false;

	// MIDI status bytes must have bit 7 set so we don't forward data bytes as
	// standalone commands over the Qwiic API.
	if ((buffer[0] & MIDI_STATUS_BYTE_MASK) == 0) return false;

	if (bufsize == 2) {
		uint8_t command = (uint8_t)(buffer[0] & MIDI_COMMAND_MASK);
		return command == MIDI_PROGRAM_CHANGE || command == MIDI_CHANNEL_PRESSURE;
	}

	return true;
}

static void wav_trigger_pro_forward_midi_message(const uint8_t *buffer, uint32_t bufsize) {
	if (!wav_trigger_pro_can_send_midi_message(buffer, bufsize)) return;
	if (bufsize < 2) return;

	uint8_t dat1 = buffer[1];
	// Only Program Change / Channel Pressure messages should arrive here with
	// two bytes, so a zero dat2 preserves the library's fixed 3-byte API.
	uint8_t dat2 = (bufsize >= 3) ? buffer[2] : 0;
	wav_trigger_pro_send_midi_msg(buffer[0], dat1, dat2);
}

bool wav_trigger_pro_get_version(char *dst, size_t dst_len) {
	if (dst == NULL || dst_len == 0) return false;
	if (!wav_trigger_pro_write_command(CMD_GET_VERSION, NULL, 0)) return false;

	sleep_ms(WAV_TRIGGER_PRO_RESPONSE_DELAY_MS);

	uint8_t version[WAV_TRIGGER_PRO_VERSION_STRING_LEN];
	if (!wav_trigger_pro_read_response(version, sizeof(version))) return false;

	size_t copy_len = sizeof(version);
	size_t max_copy_len = dst_len - 1;
	if (max_copy_len < copy_len) copy_len = max_copy_len;

	memcpy(dst, version, copy_len);
	dst[copy_len] = '\0';
	return true;
}

int wav_trigger_pro_get_num_tracks(void) {
	if (!wav_trigger_pro_write_command(CMD_GET_NUM_TRACKS, NULL, 0)) return -1;

	sleep_ms(WAV_TRIGGER_PRO_RESPONSE_DELAY_MS);

	uint8_t response[2];
	if (!wav_trigger_pro_read_response(response, sizeof(response))) return -1;

	return (int)wav_trigger_pro_unpack_uint16(response);
}

bool wav_trigger_pro_track_play_poly(uint16_t track, int16_t gain_db, uint8_t balance, uint16_t attack_ms, int16_t cents, uint8_t flags) {
	uint8_t payload[10];

	wav_trigger_pro_pack_uint16(&payload[0], track);
	wav_trigger_pro_pack_int16(&payload[2], gain_db);
	payload[4] = balance;
	wav_trigger_pro_pack_uint16(&payload[5], attack_ms);
	wav_trigger_pro_pack_int16(&payload[7], cents);
	payload[9] = flags;

	return wav_trigger_pro_write_command(CMD_TRACK_PLAY_POLY, payload, sizeof(payload));
}

bool wav_trigger_pro_track_get_status(uint16_t track, uint8_t *status) {
	if (status == NULL) return false;

	uint8_t payload[2];
	wav_trigger_pro_pack_uint16(payload, track);

	if (!wav_trigger_pro_write_command(CMD_GET_TRACK_STATUS, payload, sizeof(payload))) return false;

	sleep_ms(WAV_TRIGGER_PRO_RESPONSE_DELAY_MS);
	return wav_trigger_pro_read_response(status, 1);
}

bool wav_trigger_pro_get_num_active_voices(uint8_t *voices) {
	if (voices == NULL) return false;
	if (!wav_trigger_pro_write_command(CMD_GET_NUM_ACTIVE_VOICES, NULL, 0)) return false;

	sleep_ms(WAV_TRIGGER_PRO_RESPONSE_DELAY_MS);
	return wav_trigger_pro_read_response(voices, 1);
}

bool wav_trigger_pro_track_set_loop(uint16_t track, bool loop) {
	uint8_t payload[3];
	wav_trigger_pro_pack_uint16(payload, track);
	payload[2] = loop ? 1u : 0u;

	return wav_trigger_pro_write_command(CMD_TRACK_SET_LOOP, payload, sizeof(payload));
}

bool wav_trigger_pro_track_set_lock(uint16_t track, bool lock) {
	uint8_t payload[3];
	wav_trigger_pro_pack_uint16(payload, track);
	payload[2] = lock ? 1u : 0u;

	return wav_trigger_pro_write_command(CMD_TRACK_SET_LOCK, payload, sizeof(payload));
}

bool wav_trigger_pro_stop_all(void) {
	return wav_trigger_pro_write_command(CMD_STOP_ALL, NULL, 0);
}

bool wav_trigger_pro_track_stop(uint16_t track, uint16_t release_ms) {
	uint8_t payload[4];
	wav_trigger_pro_pack_uint16(&payload[0], track);
	wav_trigger_pro_pack_uint16(&payload[2], release_ms);

	return wav_trigger_pro_write_command(CMD_TRACK_STOP, payload, sizeof(payload));
}

bool wav_trigger_pro_set_volume(uint16_t track, uint8_t cc_value) {
	//float calc_db = 20.0 * log10((float)cc_value / 127.0);	
	//int target_gain = constrain((int)calc_db, -80, 0);
	int16_t target_gain = (80 * (float)(cc_value / 127.0)) - 80;
	return wav_trigger_pro_track_fade(track, target_gain, 0);	
}

bool wav_trigger_pro_track_fade(uint16_t track, int16_t gain_db, uint16_t time_ms) {
	/*
	uint8_t payload[6];

	wav_trigger_pro_pack_uint16(&payload[0], track);
	wav_trigger_pro_pack_int16(&payload[2], gain_db);
	wav_trigger_pro_pack_uint16(&payload[4], time_ms);
	*/
	
	uint8_t txbuf[7];
	unsigned short tmp16 = (unsigned short)gain_db;	

	txbuf[0] = CMD_TRACK_FADE;
    txbuf[1] = (uint8_t)track;
    txbuf[2] = (uint8_t)(track >> 8);
    txbuf[3] = (uint8_t)tmp16;
    txbuf[4] = (uint8_t)(tmp16 >> 8);
	txbuf[5] = (uint8_t)time_ms;
	txbuf[6] = (uint8_t)(time_ms >> 8);	
	return i2c_write_blocking(I2C_ID, WAV_TRIGGER_PRO_ADDR, txbuf, 7, false) == (int)(7);
}

bool wav_trigger_pro_send_midi_msg(uint8_t cmd, uint8_t dat1, uint8_t dat2) {
	// MIDI status bytes must have bit 7 set; reject data bytes here to keep the
	// Qwiic MIDI bridge aligned with standard 3-byte channel messages.
	if ((cmd & MIDI_STATUS_BYTE_MASK) == 0) return false;

	// Standard MIDI data bytes are 7-bit values, so clear bit 7 before sending
	// them through the WAV Trigger Pro's 3-byte MIDI message command.
	uint8_t payload[3] = {
		cmd,
		(uint8_t)(dat1 & MIDI_DATA_BYTE_MASK),
		(uint8_t)(dat2 & MIDI_DATA_BYTE_MASK),
	};

	return wav_trigger_pro_write_command(CMD_MIDI_MSG, payload, sizeof(payload));
}

bool wav_trigger_pro_load_preset(uint16_t preset) {
	uint8_t payload[2];
	wav_trigger_pro_pack_uint16(payload, preset);

	return wav_trigger_pro_write_command(CMD_LOAD_PRESET, payload, sizeof(payload));
}

bool wav_trigger_pro_set_output_gain(int16_t gain_db) {
	uint8_t payload[2];
	wav_trigger_pro_pack_int16(payload, gain_db);

	return wav_trigger_pro_write_command(CMD_SET_OUTPUT_GAIN, payload, sizeof(payload));
}

//--------------------------------------------------------------------+
//
// WAV Trigger Pro
//
//--------------------------------------------------------------------+

void launchkey_set_led(uint8_t msg_type, uint8_t channel, uint8_t index, uint8_t color_id) {
	/**
	 * 1. Change LED Color on Pads / Buttons
	 * Novation MK4 surfaces process color indexing using specific MIDI Channels:
	 * Channel 1 (0x00) = Static Color
	 * Channel 2 (0x01) = Flashing Color
	 * Channel 3 (0x02) = Pulsing Color
	 *
	 * @param msg_type  0x90 for Pads (Note On), 0xB0 for Buttons (Control Change)
	 * @param channel   0 to 2 (Static, Flashing, Pulsing)
	 * @param index     The Note or CC ID matching the physical Pad/Button
	 * @param color_id  Velocity / value mapping to Novation's color palette lookup index (0-127)
	 */	
	
	if (daw_itf_idx != 0xFF) {
		uint8_t status_byte = (msg_type & 0xF0) | (channel & 0x0F);
		
		uint8_t msg[3];	
		msg[0] = status_byte;
		msg[1] = index;
		msg[2] = color_id;
	
		tuh_midi_stream_write(daw_itf_idx, 0, msg, 3);
		tuh_midi_write_flush(daw_itf_idx);
	}	
}

void launchkey_display_text(const char* text, bool is_temp) {
	/**
	 * 2. Display Text on OLED Screen
	 * Novation devices handle display updates using Manufacturer SysEx commands.
	 * SysEx Structure: [0xF0, 0x00, 0x20, 0x29, 0x02, 0x0F, <Sub-ID>, <Display Type>, <Characters...>, 0xF7]
	 *
	 * @param text       ASCII null-terminated string to print on screen
	 * @param is_temp    true = temporary alert window, false = stationary display string
	 */	
	 
    // Novation Header: Sysex Open, Manufacturer ID (00 20 29), Launchkey Family ID
    uint8_t sysex_header[] = { 0xF0, 0x00, 0x20, 0x29, 0x02, 0x13 }; //use 0x14 for full keys
    
    // MK4 Custom Command Sub-IDs (0x04 for display target handling)
    uint8_t cmd_sub_id = 0x04; 
    
    // Display Mode: 0x01 for Temporary Popup Message, 0x00 for Stationary Default string
    uint8_t display_mode = is_temp ? 0x01 : 0x00;
    uint8_t msg[LAUNCHKEY_DISPLAY_MSG_SIZE];
		
    // Send Header
    for (size_t i = 0; i < sizeof(sysex_header); i++) {
        msg[i] = sysex_header[i];
    }
    
    // Send Display Sub-command properties
    msg[6] = cmd_sub_id;
    msg[7] = display_mode;
    
    // Stream ASCII Text characters payload (Limit safely to 16-32 characters depending on UI layout)
    size_t len = strlen(text);
	if (len > LAUNCHKEY_MAX_TEXT_LEN) len = LAUNCHKEY_MAX_TEXT_LEN;
	
    for (size_t i = 0; i < len; i++) {
        // Enforce valid safe-range 7-bit ASCII text transmission
        if (text[i] < 128) {
            msg[8 + i] = (uint8_t)text[i];
        }
    }
    
    // Send End of Exclusive byte (EOX)
    msg[8 + len] = 0xF7;

	if (daw_itf_idx != 0xFF) {
		tuh_midi_stream_write(daw_itf_idx, 0, msg, LAUNCHKEY_DISPLAY_SYSEX_OVERHEAD + len);
		tuh_midi_write_flush(daw_itf_idx);
	}	
}
