#include "pico_bluetooth.h"
#include "ble_midi_controller.h"

#include <stddef.h>
#include <string.h>

#include <bt/uni_bt.h>
#include <btstack.h>
#include <controller/uni_gamepad.h>
#include "pico/stdlib.h"
#include <pico/cyw43_arch.h>
#include <pico/time.h>
#include <uni.h>
#include <uni_hid_device.h>

#include "debug.h"
#include "sdkconfig.h"
#include "button.h"
#include "looper.h"
#include "storage.h"
#include "ghost_note.h"

#ifndef CONFIG_BLUEPAD32_PLATFORM_CUSTOM
#error "Pico W must use BLUEPAD32_PLATFORM_CUSTOM"
#endif

#define INT1 36
#define ARRA 37
#define FILA 38
#define ARRB 39
#define FILB 40
#define ARRC 41
#define FILC 42
#define ARRD 43
#define FILD 44
#define BRKA 45
#define END1 46

extern looper_status_t looper_status;

bool strum_neutral = true;
bool style_started = false;
bool enable_style_play = false;
bool enable_auto_hold = false;
bool enable_auto_strum = false;
bool enable_stacatto_mode = false;
bool enable_seqtrak = false;
bool enable_dream_midi = false;
bool enable_sp404mk2 = false;
bool enable_mpc_sample = false;
bool enable_mpx_looper = false;
bool enable_mpx_drums = false;
bool enable_nanobox_tangerine = false;
bool enable_wav_trigger_pro = false;
bool enable_synth = false;
bool enable_arranger_mode = false;
bool enable_modx = false;
bool enable_chord_track = true;
bool enable_bass_track = true;
bool enable_drum_track = true;
bool enable_ample_guitar = false;
bool enable_midi_drums = false;
bool enable_worship_pads = false;
bool gamepad_guitar_connected = false;
bool finished_processing = true;
bool style_change_requested = false;
bool style_end_requested = false;
bool style_end_started = false;
bool preferences_changed = false;

uint8_t but0 = 0;
uint8_t but1 = 0;
uint8_t but2 = 0;
uint8_t but3 = 0;
uint8_t but4 = 0; 
uint8_t but5 = 0;
uint8_t but6 = 0;
uint8_t but7 = 0;   
uint8_t but8 = 0;
uint8_t but9 = 0;

uint8_t mbut0 = 0;
uint8_t mbut1 = 0;
uint8_t mbut2 = 0;
uint8_t mbut3 = 0;

uint8_t dpad_left = 0;	
uint8_t dpad_right = 0;
uint8_t dpad_up = 0;
uint8_t dpad_down = 0;

bool joy_up = false;  
bool joy_down = false;  
bool knob_up = false; 
bool knob_down = false; 

uint8_t green = 0;
uint8_t red = 0;
uint8_t yellow = 0;
uint8_t blue = 0;
uint8_t orange = 0;
uint8_t starpower = 0;
uint8_t pitch = 0;
uint8_t song_key = 0;

uint8_t up = 0;
uint8_t down = 0;
uint8_t left = 0;
uint8_t right = 0;	

uint8_t start = 0;
uint8_t menu = 0;
uint8_t logo = 0;
uint8_t config = 0;	

uint8_t joystick_up = 0;
uint8_t joystick_down = 0;  
uint8_t logo_knob_up = 0;  
uint8_t logo_knob_down = 0; 	
uint8_t guitar_pc_code = 26;

int applied_velocity = 100;				// TODO Livelive applied velocity
int active_strum_pattern = 0;	
int active_neck_pos = 2;
int style_section = 0; 
int style_group = 0; 
int old_style = -1;
int ample_old_key = 0;

int basic_chord = 0;
int advanced_chord = 0;
int last_chord_note = 0;
int last_bass_note = 0;
int last_chord_type = 0;

uint8_t sample_drum_velocity = 127;
uint8_t sample_bass_velocity = 127;
uint8_t sample_chord_velocity = 127;
uint8_t worship_pad_velocity = 1;
uint8_t midi_guitar_volume = 127;

uint8_t sampler_drum_note = 0;
uint8_t sampler_chord_note = 0;
uint8_t sampler_bass_note = 0;
uint8_t sampler_old_drum_note = 255;
uint8_t sampler_old_bass_note = 255;
uint8_t sampler_old_chord_note = 255;

uint8_t mpc_drum_note = 0;
uint8_t mpc_chord_note = 0;
uint8_t mpc_bass_note = 0;
uint8_t mpc_old_drum_note = 255;
uint8_t mpc_old_bass_note = 255;
uint8_t mpc_old_chord_note = 255;

uint8_t mpx_sample_note = 0;
uint8_t mpx_old_sample_note = 0;

uint8_t sp404_drum_note = 0;
uint8_t sp404_chord_note = 0;
uint8_t sp404_bass_note = 0;
uint8_t sp404_chord_cmd = 0;
uint8_t sp404_bass_cmd = 0;
uint8_t sp404_old_drum_note = 0;
uint8_t sp404_old_bass_note = 0;
uint8_t sp404_old_chord_note = 0;
uint8_t sp404_old_bass_cmd = 0;
uint8_t sp404_old_chord_cmd = 0;

int transpose = 0; 
int midi_current_step = 0;

uint8_t voice_note = 0;
uint8_t chord_notes[6] = {0};
uint8_t old_midinotes[6] = {0};
uint8_t mute_midinotes[6] = {0};

void midi_modx_arp_octave(uint8_t octave);
void midi_modx_arp(bool on);
void midi_modx_arp_hold(uint8_t part, bool on);
void midi_modx_arp_realtime(uint8_t part, bool on);
void midi_modx_tempo(int tempo);
void midi_modx_key(uint8_t key);
void midi_seqtrak_arp();
void midi_seqtrak_key(uint8_t key);
void midi_seqtrak_tempo(int tempo);
void midi_seqtrak_pattern(uint8_t pattern);
void midi_seqtrak_mute(uint8_t track, bool mute);
void midi_start_stop(bool start);
void midi_send_note(uint8_t command, uint8_t note, uint8_t velocity);
void midi_send_program_change(uint8_t command, uint8_t code);
void midi_send_control_change(uint8_t command, uint8_t controller, uint8_t value);
void midi_play_chord(bool on, uint8_t p1, uint8_t p2, uint8_t p3);
void midi_play_slash_chord(bool on, uint8_t p1, uint8_t p2, uint8_t p3, uint8_t p4);
void midi_ketron_arr(uint8_t code, bool on);
void midi_ketron_footsw(uint8_t code, bool on);
void midi_yamaha_start_stop(uint8_t code, bool on);
void midi_yamaha_arr(uint8_t code, bool on);
void midi_process_state(uint64_t start_us);
void gamepad_bluetooth_handle_data();

void config_guitar(uint8_t mode);
void config_ample_guitar();
void config_arranger();
void config_midi_drums();
void config_modx();
void config_seqtrak();
void config_sp404mk2();
void config_mpc_sample();
void config_style_play();
void config_nanobox_tangerine();
void config_wav_trigger_pro();
void config_mpx_looper();

void play_chord(bool on, bool up);
void clear_chord_notes();
void stop_chord();
void dream_set_delay(int tempo);
void mpc_trigger_loop();
void mpx_trigger_loop();
void sampler_trigger_loop();
void sp404_trigger_loop();
void sampler_midi_note(uint8_t command, uint8_t note, uint8_t velocity);
void nanobox_stop_loops();
void sp404_stop_loops();
void mpc_stop_loops();
void mpx_stop_loops();
void wav_trigger_pro_stop_loops();
void sampler_trigger_fill();
void sampler_trigger_break();
void ble_midi_client_scan_end();
void set_tempo(uint8_t tempo);


int chord_chart[12][3][6] = {
	{{ 3,  3, 2, 0, 1, 0}, {-1,  3, 5, 5, 4, 3}, {-1, -1, 3, 0, 1, 3}},
	{{-1, -1, 3, 1, 2, 1}, {-1, -1, 2, 1, 2, 0}, {-1, -1, 3, 3, 4, 1}},
	{{-1, -1, 0, 2, 3, 2}, {-1, -1, 0, 2, 3, 1}, {-1, -1, 0, 2, 3, 3}},
	{{-1, -1, 5, 3, 4, 3}, {-1, -1, 4, 3, 4, 2}, {-1, -1, 1, 3, 4, 4}},
	{{ 0,  2, 2, 1, 0, 0}, { 0,  2, 2, 0, 0, 0}, { 0,  2, 2, 2, 0, 0}},
	{{ 1,  3, 3, 2, 1, 1}, { 1,  3, 3, 1, 1, 1}, {-1, -1, 3, 3, 1, 1}},
	{{ 2,  4, 4, 3, 2, 2}, { 2,  4, 4, 2, 2, 2}, {-1, -1, 4, 4, 2, 2}},
	{{ 3,  2, 0, 0, 0, 3}, { 3,  5, 5, 3, 3, 3}, {-1, -1, 0, 0, 1, 3}},
	{{ 4,  6, 6, 5, 4, 4}, { 4,  6, 6, 4, 4, 4}, {-1, -1, 1, 1, 2, 4}},
	{{-1,  0, 2, 2, 2, 0}, {-1,  0, 2, 2, 1, 0}, {-1,  0, 2, 2, 3, 0}},
	{{-1,  1, 3, 3, 3, 1}, {-1,  1, 3, 3, 2, 1}, {-1, -1, 3, 3, 4, 1}},
	{{-1,  2, 4, 4, 4, 2}, {-1,  2, 4, 4, 3, 2}, {-1, -1, 4, 4, 5, 2}}
};

uint8_t strum_pattern[14][12][6] = {
	{{6,5,4,3,2,1}, {1,2,3,4,5,6}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}}, 
	{{4,3,2,1,0,0}, {3,2,1,0,0,0}, {4,3,2,1,0,0}, {4,3,2,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}}, 

	// 1-3-1-2-3-1-2-1-3-2
	{{1,0,0,0,0,0}, {3,0,0,0,0,0}, {1,0,0,0,0,0}, {2,0,0,0,0,0}, {3,0,0,0,0,0}, {1,0,0,0,0,0}, {2,0,0,0,0,0}, {1,0,0,0,0,0}, {3,0,0,0,0,0}, {2,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}}, 
	
	// 3-2-4-1-4-2-4
	{{3,0,0,0,0,0}, {2,0,0,0,0,0}, {4,0,0,0,0,0}, {1,0,0,0,0,0}, {4,0,0,0,0,0}, {2,0,0,0,0,0}, {4,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}},	

	// 4-3-4-2-4-3-1-3-2
	{{4,0,0,0,0,0}, {3,0,0,0,0,0}, {4,0,0,0,0,0}, {2,0,0,0,0,0}, {4,0,0,0,0,0}, {3,0,0,0,0,0}, {1,0,0,0,0,0}, {3,0,0,0,0,0}, {2,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}},	
	
	
	// 3-2-1-2-3-4-3-2
	{{3,0,0,0,0,0}, {2,0,0,0,0,0}, {1,0,0,0,0,0}, {2,0,0,0,0,0}, {3,0,0,0,0,0}, {4,0,0,0,0,0}, {3,0,0,0,0,0}, {2,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}}, 
	
	// 3-2-4-2-3-1-3-1
	{{3,0,0,0,0,0}, {2,0,0,0,0,0}, {4,0,0,0,0,0}, {2,0,0,0,0,0}, {3,0,0,0,0,0}, {1,0,0,0,0,0}, {3,0,0,0,0,0}, {1,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}}, 
	
	// 3-2-4-1-2-3-1-2-1
	{{3,0,0,0,0,0}, {2,0,0,0,0,0}, {4,0,0,0,0,0}, {1,0,0,0,0,0}, {2,0,0,0,0,0}, {3,0,0,0,0,0}, {1,0,0,0,0,0}, {2,0,0,0,0,0}, {1,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}}, 

	
	// 3-[2+1]
	{{3,0,0,0,0,0}, {2,1,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}}, 
	
	// 4-[1+2+3]
	{{4,0,0,0,0,0}, {1,2,3,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}}, 
	
	// [3+2+1]-4-[3+2]
	{{3,2,1,0,0,0}, {4,0,0,0,0,0}, {3,2,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}}, 

	
	// [3+2] {2-str.chord}
	{{3,2,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}}, 
	
	// [3+2+1] {3-str.chord}
	{{3,2,1,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}}, 
	
	// [4+3+2] {lower 3-str.chord}
	{{4,3,2,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}, {0,0,0,0,0,0}}, 	
};

uint8_t strum_styles[5][5][16][3] = {	// start action, stop action, velocity
	{
		{{74, 77, 114}, {77, 74, 24}, {78, 77, 90}, {77, 78, 90}, {74, 77, 121}, {77, 74, 91}, {77, 77, 90}, {76, 77, 107}, {77, 76, 82}, {76, 77, 121}, {78, 76, 90}, {77, 78, 90}, {76, 77, 123}, {77, 76, 80}, {78, 77, 90}, {77, 78, 90}},		
		{{64, 83, 91},  {0, 0, 0},    {64, 64, 50}, {0, 0, 0},    {74, 64, 103}, {0, 0, 0},    {79, 74, 71}, {76, 79, 90},  {77, 76, 64}, {76, 77, 89},  {77, 76, 67}, {83, 77, 71}, {74, 83, 101}, {0, 0, 0},    {79, 74, 64}, {83, 79, 40}},	
		{{64, 81, 88},  {0, 0, 0},    {0, 64, 0},   {0, 0, 0},    {78, 0, 26},   {0, 78, 0},   {64, 0, 60},  {0, 0, 0},     {0, 0, 0},    {0, 0, 0},     {64, 0, 90},  {0, 64, 0},   {81, 0, 75},   {0, 0, 0},    {0, 0, 0},    {0, 0, 0}},	
		{{72, 77, 126}, {78, 72, 1},  {78, 78, 58}, {76, 78, 100},{78, 76, 1},   {0, 78, 0},   {72, 0, 123}, {0, 0, 0},     {0, 0, 0},    {0, 0, 0},     {64, 72, 90}, {0, 64, 0},   {72, 0, 109},  {78, 72, 90}, {77, 78, 90}, {77, 77, 64}},	
		{{72, 76, 89},  {0, 0, 0},    {76, 72, 55}, {0, 0, 0},    {77, 76, 100}, {0, 0, 0},    {76, 77, 70}, {0, 0, 0},     {74, 76, 72}, {0, 74, 0},    {76, 0, 65},  {0, 0, 0},    {77, 76, 60},  {0, 0, 0},    {76, 77, 91}, {0, 0, 0}}
	},
	{
		{{72, 76, 100}, {0, 0, 0},    {74, 72, 80}, {76, 74, 60}, {0, 0, 0},     {76, 76, 70}, {0, 0, 0},    {76, 76, 60},  {72, 76, 70}, {0, 0, 0},     {74, 72, 40}, {76, 74, 90}, {74, 76, 50},  {76, 74, 40}, {74, 76, 50}, {76, 74, 60}},
		{{72, 76, 100}, {0, 0, 0},    {74, 72, 40}, {76, 74, 40}, {72, 76, 90},  {76, 72, 30}, {74, 76, 60}, {76, 74, 90},  {78, 76, 60}, {76, 78, 80},  {74, 76, 80}, {76, 74, 50}, {72, 76, 80},  {0, 0, 0},    {74, 72, 60}, {76, 74, 70}},
		{{72, 76, 55},  {0, 0, 0},    {0, 0, 0},    {0, 0, 0},    {77, 72, 100}, {0, 0, 0},    {76, 77, 80}, {0, 0, 0},     {74, 76, 60}, {0, 74, 0},    {76, 0, 60},  {0, 0, 0},    {77, 76, 60},  {0, 0, 0},    {76, 77, 90}, {0, 0, 0}},
		{{72, 76, 90},  {0, 0, 0},    {74, 72, 30}, {76, 74, 60}, {72, 76, 60},  {0, 0, 0},    {76, 72, 70}, {0, 0, 0},     {79, 76, 30}, {83, 79, 40},  {81, 83, 50}, {76, 81, 60}, {72, 76, 70},  {81, 72, 40}, {76, 81, 80}, {0, 0, 0}},
		{{72, 76, 80},  {78, 72, 70}, {72, 78, 60}, {0, 0, 0},    {72, 72, 100}, {81, 72, 60}, {83, 81, 60}, {76, 83, 90},  {72, 76, 60}, {83, 72, 70},  {72, 83, 70}, {0, 0, 0},    {72, 72, 100}, {81, 72, 60}, {83, 81, 60}, {76, 83, 80}},
	},
	{
		{{79, 83, 90},  {79, 79, 50}, {79, 79, 30}, {79, 79, 90}, {79, 79, 30},  {79, 79, 40}, {79, 79, 90}, {79, 79, 50},  {79, 79, 30}, {79, 79, 90},  {79, 79, 30}, {79, 79, 40}, {79, 79, 90},  {77, 79, 90}, {81, 77, 90}, {83, 81, 90}},
		{{72, 71, 80},  {0, 0, 0},    {0, 0, 0},    {76, 72, 50}, {74, 76, 100}, {76, 74, 30}, {74, 76, 60}, {76, 74, 70},  {67, 76, 70}, {0, 0, 0},     {71, 67, 90}, {0, 0, 0},    {64, 71, 80},  {67, 64, 60}, {69, 67, 60}, {71, 69, 80}},
		{{64, 71, 90},  {67, 64, 60}, {69, 67, 90}, {67, 69, 40}, {71, 67, 90},  {67, 71, 50}, {69, 67, 70}, {67, 69, 40},  {77, 67, 30}, {67, 77, 40},  {69, 67, 80}, {0, 0, 0},    {71, 69, 100}, {67, 71, 60}, {69, 67, 90}, {71, 69, 80}},
		{{72, 78, 110}, {0, 0, 0},    {74, 72, 90}, {76, 74, 30}, {74, 76, 70},  {77, 74, 60}, {72, 77, 100},{0, 0, 0},     {0, 0, 0},    {76, 72, 50},  {74, 76, 80}, {0, 0, 0},    {72, 74, 110}, {76, 72, 80}, {74, 76, 90}, {78, 74, 30}},
		{{72, 77, 90},  {77, 72, 10}, {0, 77, 0},   {74, 0, 90},  {77, 74, 1},   {0, 77, 0},   {72, 0, 90},  {77, 72, 10},  {0, 77, 0},   {0, 0, 0},     {77, 0, 70},  {77, 77, 30}, {77, 77, 80},  {76, 77, 90}, {0, 76, 0},   {77, 0, 1}},
	},
	{
		{{77, 77, 90},  {77, 77, 80}, {74, 77, 100},{76, 74, 80}, {74, 76, 90},  {76, 74, 60}, {77, 76, 90}, {77, 77, 70},  {72, 77, 110},{77, 72, 70},  {76, 77, 90}, {77, 76, 70}, {74, 77, 100}, {77, 74, 60}, {77, 77, 90}, {77, 77, 70}},
		{{76, 77, 90},  {77, 76, 80}, {74, 77, 80}, {76, 74, 100},{77, 76, 90},  {76, 77, 80}, {77, 76, 90}, {76, 77, 100}, {77, 76, 1},  {0, 77, 0},    {0, 0, 0},    {0, 0, 0},    {77, 0, 90},   {77, 77, 60}, {77, 77, 90}, {77, 77, 70}},
		{{74, 76, 90},  {77, 74, 50}, {77, 77, 70}, {77, 77, 60}, {72, 77, 90},  {77, 72, 50}, {77, 77, 90}, {76, 77, 100}, {77, 76, 90}, {77, 77, 70},  {77, 77, 80}, {77, 77, 70}, {77, 77, 90},  {77, 77, 50}, {74, 77, 90}, {76, 74, 60}},
		{{77, 77, 90},  {77, 77, 70}, {74, 77, 100},{76, 74, 60}, {72, 76, 90},  {76, 72, 70}, {77, 76, 90}, {77, 77, 60},  {74, 77, 90}, {77, 74, 30},  {74, 77, 100},{77, 74, 50}, {72, 77, 90},  {77, 72, 50}, {77, 77, 90}, {77, 77, 60}},
		{{72, 77, 90},  {77, 72, 80}, {74, 77, 60}, {76, 74, 90}, {72, 76, 90},  {76, 72, 60}, {77, 76, 80}, {76, 77, 60},  {74, 76, 105},{77, 74, 70},  {74, 77, 90}, {77, 74, 80}, {72, 77, 100}, {83, 72, 90}, {74, 83, 90}, {77, 74, 60}},
	},
	{
		{{72, 77, 90},  {77, 72, 90}, {77, 77, 65}, {76, 77, 90}, {77, 76, 90},  {77, 77, 65}, {74, 77, 90}, {76, 74, 70},  {77, 76, 90}, {76, 77, 80},  {77, 76, 60}, {76, 77, 90}, {72, 76, 100}, {77, 72, 80}, {74, 77, 90}, {77, 74, 70}},
		{{72, 76, 90},  {77, 72, 50}, {77, 77, 80}, {77, 77, 70}, {74, 77, 90},  {77, 74, 90}, {77, 77, 60}, {76, 77, 90},  {77, 76, 1},  {0, 77, 0},    {0, 0, 0},    {0, 0, 0},    {72, 0, 90},   {77, 72, 60}, {74, 77, 90}, {76, 74, 70}},
		{{72, 77, 90},  {76, 72, 60}, {77, 76, 80}, {77, 77, 60}, {74, 77, 90},  {77, 74, 90}, {77, 77, 60}, {77, 77, 70},  {74, 77, 100},{77, 74, 1},   {0, 77, 0},   {0, 0, 0},    {0, 0, 0},     {77, 0, 60},  {77, 77, 100},{77, 77, 70}},
		{{77, 77, 90},  {77, 77, 70}, {74, 77, 90}, {76, 74, 50}, {77, 76, 70},  {76, 77, 100},{77, 76, 80}, {77, 77, 60},  {72, 77, 90}, {76, 72, 60},  {74, 76, 70}, {76, 74, 90}, {77, 76, 60},  {76, 77, 90}, {77, 76, 100},{77, 77, 70}},
		{{72, 76, 90},  {77, 72, 50}, {74, 77, 80}, {77, 74, 70}, {72, 77, 80},  {77, 72, 10}, {74, 77, 90}, {76, 74, 60},  {72, 76, 90}, {76, 72, 50},  {77, 76, 60}, {76, 77, 90}, {72, 76, 100}, {77, 72, 70}, {74, 77, 90}, {76, 74, 60}}
	}	
};

// Platform Overrides
static void pico_bluetooth_init(int argc, const char** argv) {
  ARG_UNUSED(argc);
  ARG_UNUSED(argv);
}

static void pico_bluetooth_on_init_complete(void) {
  // Safe to call "unsafe" functions since they are called
  // PICO_INFO("Bluetooth initialization complete.\n");

  // Delete stored BT keys for fresh pairing (helpful for initial connection)
  uni_bt_del_keys_unsafe();

  // Start scanning and autoconnect to supported controllers.
  uni_bt_start_scanning_and_autoconnect_safe();
  // PICO_INFO("Started Bluetooth scanning for new devices.\n");

  uni_property_dump_all();

  // Start the BLE MIDI client scan.  BTStack is now in HCI_STATE_WORKING so
  // ble_midi_controller_scan_begin() can start scanning directly.
  // BAO disable BLE for now
  //ble_midi_controller_scan_begin();

  cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, true);
}

static uni_error_t pico_bluetooth_on_device_discovered(bd_addr_t addr, const char* name, uint16_t cod, uint8_t rssi) {
  	(void) addr;
	(void) rssi;
	
  // You can filter discovered devices here. Return any value different from UNI_ERROR_SUCCESS;
  // @param addr: the Bluetooth address
  // @param name: could be NULL, could be zero-length, or might contain the name.
  // @param cod: Class of Device. See "uni_bt_defines.h" for possible values.
  // @param rssi: Received Signal Strength Indicator (RSSI) measured in dBms. The higher (255) the better.

  //const char* device_name = (name && strlen(name) > 0) ? name : "Unknown";
  // PICO_DEBUG("[BT] Found device: %s (%02X:%02X:%02X:%02X:%02X:%02X) CoD=0x%04X RSSI=%ddBm\n", device_name, addr[0], addr[1], addr[2], addr[3], addr[4], addr[5], cod, rssi);

  // Check if it's a Gamepad controller
  if (name && (strstr(name, "STANDARD GAMEPAD"))) {
    // PICO_INFO("Gamepad controller detected! Attempting connection...\n");
	 cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, true);
  }

  // As an example, if you want to filter out keyboards, do:
  if (((cod & UNI_BT_COD_MINOR_MASK) & UNI_BT_COD_MINOR_KEYBOARD) == UNI_BT_COD_MINOR_KEYBOARD) {
    // PICO_DEBUG("[BT] Ignoring keyboard device\n");
    //return UNI_ERROR_IGNORE_DEVICE;
	 cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, true);	
  }

  return UNI_ERROR_SUCCESS;
}

static void pico_bluetooth_on_device_connected(uni_hid_device_t* d) {
  (void) d;
  gamepad_guitar_connected = true; 	
  // PICO_INFO("Device connected: %s (%02X:%02X:%02X:%02X:%02X:%02X)\n", d->name, d->conn.btaddr[0], d->conn.btaddr[1], d->conn.btaddr[2], d->conn.btaddr[3], d->conn.btaddr[4], d->conn.btaddr[5]);

  // Disable scanning when a device is connected to save power
  uni_bt_stop_scanning_safe();    
  // PICO_DEBUG("[BT] Stopped scanning (device connected)\n"); 
}

static void pico_bluetooth_on_device_disconnected(uni_hid_device_t* d) {
  (void) d;	
  gamepad_guitar_connected = false;	
  // PICO_INFO("Device disconnected: %s (%02X:%02X:%02X:%02X:%02X:%02X)\n", d->name, d->conn.btaddr[0], d->conn.btaddr[1], d->conn.btaddr[2], d->conn.btaddr[3], d->conn.btaddr[4], d->conn.btaddr[5]);

  // Re-enable scanning when a device is disconnected
  uni_bt_start_scanning_and_autoconnect_safe();
  
  cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, true);  
  // PICO_DEBUG("[BT] Restarted scanning (device disconnected)\n");
}

static uni_error_t pico_bluetooth_on_device_ready(uni_hid_device_t* d) {
	(void) d;	
	// You can reject the connection by returning an error.
	cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, false); 
    
	//storage_load_tracks();			
	
	storage_load_preferences();	
	
	enable_style_play = enable_modx || enable_seqtrak || enable_midi_drums || enable_ample_guitar || enable_arranger_mode || enable_nanobox_tangerine;
	
	if (enable_style_play) 		config_style_play();
	if (enable_ample_guitar) 	config_ample_guitar();
	if (enable_arranger_mode) 	config_arranger();
	if (enable_midi_drums) 		config_midi_drums();
	if (enable_modx) 			config_modx();
	if (enable_seqtrak) 		config_seqtrak();	
	if (enable_sp404mk2) 		config_sp404mk2();	
	if (enable_mpc_sample) 		config_mpc_sample();
	if (enable_mpx_looper) 		config_mpx_looper();	
  
  return UNI_ERROR_SUCCESS;
}

static const uni_property_t* pico_bluetooth_get_property(uni_property_idx_t idx) {
  ARG_UNUSED(idx);
  return NULL;
}

static void pico_bluetooth_on_controller_data(uni_hid_device_t* d, uni_controller_t* ctl) { 
	(void) d;
	if (!gamepad_guitar_connected) return;
	
	int8_t axis_x = ctl->gamepad.axis_x / 4;	// nomalise -512 to +512 to -128 to +128
	int8_t axis_y = ctl->gamepad.axis_y / 4;
	int8_t axis_rx = ctl->gamepad.axis_rx / 4;
	int8_t axis_ry = ctl->gamepad.axis_ry / 4;
	
	but0 = (ctl->gamepad.buttons >> 0) & 0x01;
	but1 = (ctl->gamepad.buttons >> 1) & 0x01;
	but2 = (ctl->gamepad.buttons >> 2) & 0x01;
	but3 = (ctl->gamepad.buttons >> 3) & 0x01;
	but4 = (ctl->gamepad.buttons >> 4) & 0x01; 
	but5 = (ctl->gamepad.buttons >> 5) & 0x01; 	
	but6 = (ctl->gamepad.buttons >> 6) & 0x01;   
	but7 = (ctl->gamepad.buttons >> 7) & 0x01;   
	but8 = (ctl->gamepad.buttons >> 8) & 0x01; 	
	but9 = (ctl->gamepad.buttons >> 9) & 0x01;	
	
	dpad_left = ctl->gamepad.dpad & 0x02;	
	dpad_right = ctl->gamepad.dpad & 0x01;
	dpad_up = ctl->gamepad.dpad & 0x04;	
	dpad_down = ctl->gamepad.dpad & 0x08;	
	
	mbut0 = (ctl->gamepad.misc_buttons >> 0) & 0x01;
	mbut1 = (ctl->gamepad.misc_buttons >> 1) & 0x01;
	mbut2 = (ctl->gamepad.misc_buttons >> 2) & 0x01;
	mbut3 = (ctl->gamepad.misc_buttons >> 3) & 0x01;	
	
	joy_up = axis_y > axis_x;  
	joy_down = axis_x > axis_y;  
	knob_up = axis_ry > axis_rx; 
	knob_down = axis_rx > axis_ry; 	
	
	if (knob_up && !but9) 									// bass/drums volume
	{
		if (abs(axis_ry) > abs(axis_rx)) {
			sample_drum_velocity = abs(axis_ry) % 128;
			sample_drum_velocity = sample_drum_velocity > 25 ? sample_drum_velocity : 25;
			style_change_requested = true;
		} else {
			sample_bass_velocity = abs(axis_rx) % 128;			
			sample_bass_velocity = sample_bass_velocity > 25 ? sample_bass_velocity : 25;
		}
	}
	else
		
	if (knob_down && !but9) 								// chords/guitar volume
	{
		if (abs(axis_rx) > abs(axis_ry)) {
			sample_chord_velocity = abs(axis_rx) % 128;				
			sample_chord_velocity = sample_chord_velocity > 64 ? sample_chord_velocity : 64;
		} else {
			midi_guitar_volume = abs(axis_ry) % 128;			
			midi_guitar_volume = midi_guitar_volume > 64 ? midi_guitar_volume : 64;
		}
	}	

	switch (ctl->klass) {
		case UNI_CONTROLLER_CLASS_GAMEPAD:		
			gamepad_bluetooth_handle_data(); 
			break;
		case UNI_CONTROLLER_CLASS_BALANCE_BOARD:
			// DO NOTHING
			break;
		case UNI_CONTROLLER_CLASS_MOUSE:
			// DO NOTHING
			break;
		case UNI_CONTROLLER_CLASS_KEYBOARD:
			// DO NOTHING
			break;
		default:
			//loge("Unsupported controller class: %d\n", ctl->klass);
		break;
  }			
}

void gamepad_bluetooth_handle_data() {
	if (!finished_processing) return;	
	finished_processing = false;
	
	absolute_time_t now = get_absolute_time();
	//uint64_t now_since_boot = to_us_since_boot(now);

	#if IS_PICO_DEBUG
	  static int count = 0;
	  static absolute_time_t last_updated = 0;

	  count++;

	  int64_t elapsed_us = absolute_time_diff_us(last_updated, now);
	  if (elapsed_us >= 1000000) {
		// PICO_DEBUG("[BT] Bluetooth data received: %u\n", count);
		last_updated = now;
		count = 0;
	  }
	#endif
  
	// cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, true);

	uint8_t ketron_code;
	uint8_t yamaha_code;
	bool next_or_previous = false;
			
	if (but1 != green) {									// detect buttons
		green = but1;
	}

	if (but0 != red) {
		red = but0;
	}

	if (but2 != yellow) {
		yellow = but2;
	}

	if (but3 != blue) {
		blue = but3;
	}

	if (but4 != orange) {
		orange = but4;
	}

	if (but6 != pitch) {									// strum selection
		pitch = but6;

		if (but6 && (up || down)) {	// reset transpose
			transpose = 0;
		}
		else
			
		if (red && yellow && blue) 
		{
			if (but6) {
				enable_auto_hold = !enable_auto_hold;
				if (enable_modx) midi_modx_arp_hold(0, enable_auto_hold);	// only control part 1
			}					
		}
		else

		if (green && orange) 								// toggle worship pads/backing tracks
		{	
			if (but6)	
			{				
				if (enable_wav_trigger_pro) {
					enable_worship_pads = !enable_worship_pads;
					
					if (enable_worship_pads) {			// start backing track/worship pads track with midi channel 16 trigger
						sampler_midi_note(0x9F,  56 + transpose, worship_pad_velocity);	
					} else {
						sampler_midi_note(0x9F,  68 + transpose, 127);
					}					
				}			
			}
		}
		else

		if (green && blue) 									// mute/unmute drums
		{
			if (but6) {
				enable_drum_track = !enable_drum_track;
				
				if (enable_mpc_sample) 
				{
					if (!enable_drum_track) {
						if (mpc_old_drum_note != 255) sampler_midi_note(0x94, mpc_old_drum_note, 1);
						mpc_old_drum_note = 255;
					
					} else style_change_requested = true;
				}				
				else 
					
				if (enable_sp404mk2) 
				{
					if (!enable_drum_track) {
						sampler_midi_note(0x90, sp404_old_drum_note, 1);
						sp404_old_drum_note = 0;
						
					} else style_change_requested = true;
				}
				else 
					
				if (enable_nanobox_tangerine) 
				{
					if (!enable_drum_track) {
						if (sampler_old_drum_note != 255) sampler_midi_note(0x94, sampler_old_drum_note, 1);
						sampler_old_drum_note = 255;
					
					} else style_change_requested = true;
				}				
				else 
					
				if (enable_wav_trigger_pro) 
				{
					if (!enable_drum_track) {
						if (sampler_old_drum_note != 255) sampler_midi_note(0x97, sampler_old_drum_note, 127);
						sampler_old_drum_note = 255;
					
					} else style_change_requested = true;
				}				
			}				
		}
		else

		if (red && orange) 
		{
			if (but6) {
				enable_stacatto_mode = !enable_stacatto_mode;
			}
		}
		else

		if (green && yellow) 								// mute/unmute chords
		{
			if (but6) {
				enable_chord_track = !enable_chord_track;
				
				if (enable_mpx_looper) {	// use whammy bar instead to mute/unmute
					
				}
				else 
					
				if (enable_mpc_sample) 
				{
					if (!enable_chord_track) {	
						if (mpc_old_chord_note != 255) sampler_midi_note(0x94, mpc_old_chord_note, 1);					
						mpc_old_chord_note = 255;
					}
				}	
				else 
					
				if (enable_nanobox_tangerine) 
				{
					if (!enable_chord_track) {	
						if (sampler_old_chord_note != 255) sampler_midi_note(0x96, sampler_old_chord_note, 1);					
						sampler_old_chord_note = 255;
					}
				}				
				else 
					
				if (enable_wav_trigger_pro) 
				{
					if (!enable_chord_track) {	
						if (sampler_old_chord_note != 255) sampler_midi_note(0x99, sampler_old_chord_note, 127);					
						sampler_old_chord_note = 255;
					}
				}				
				else 
					
				if (enable_sp404mk2) 
				{
					if (!enable_chord_track) {	
						sampler_midi_note(sp404_old_chord_cmd, sp404_old_chord_note, 1);					
						sp404_old_chord_note = 0;
					}
				}				
			}
		}
		else				
			
		if (red && blue) 									// mute/unmute bass
		{				
			if (but6) {
				enable_bass_track = !enable_bass_track;	

				if (enable_mpc_sample) 
				{
					if (!enable_bass_track) {	
						if (mpc_old_bass_note != 255) sampler_midi_note(0x94, mpc_old_bass_note, 1);					
						mpc_old_bass_note = 255;
					}
				}	
				else
					
				if (enable_nanobox_tangerine) 
				{
					if (!enable_bass_track) {	
						if (sampler_old_bass_note != 255) sampler_midi_note(0x95, sampler_old_bass_note, 1);					
						sampler_old_bass_note = 255;
					}
				}				
				else
					
				if (enable_wav_trigger_pro) 
				{
					if (!enable_bass_track) {	
						if (sampler_old_bass_note != 255) sampler_midi_note(0x98, sampler_old_bass_note, 127);					
						sampler_old_bass_note = 255;
					}
				}				
				else
					
				if (enable_sp404mk2) 
				{
					if (!enable_bass_track) {	
						sampler_midi_note(sp404_old_bass_cmd, sp404_old_bass_note, 1);					
						sp404_old_bass_note = 0;
					}
				}				
			}
		}
		else

		if (yellow && orange) 
		{
			if (but6 && !enable_sp404mk2 && !enable_mpc_sample && !enable_synth && !enable_mpx_looper && !enable_wav_trigger_pro) {
				enable_style_play = !enable_style_play;	// toggle chord generation
			}
		}				
		else

		if (yellow && blue) {
			active_neck_pos = 3;	// High
			
			if (but6) 
			{
				if (enable_arranger_mode) 	{
					midi_send_program_change(0xC0, guitar_pc_code);	// electric jazz guitar on channel 1						
				}
				else
					
				if (enable_seqtrak) 		midi_seqtrak_arp();	
				else						
				if (enable_modx) 			midi_modx_arp_octave(active_neck_pos - 2);	// use neck position to set keyboard octave					
			}				
		}
		else

		if (yellow && red) {
			active_neck_pos = 2;	// Normal
			
			if (but6) 
			{
				if (enable_arranger_mode) 	{
					midi_send_program_change(0xC0, guitar_pc_code);	// electric jazz guitar on channel 1						
				}
				else
					
				if (enable_seqtrak) 		midi_seqtrak_arp();	
				else						
				if (enable_modx) 			midi_modx_arp_octave(active_neck_pos - 2);	// use neck position to set keyboard octave					
			}				
		}
		else
			
		if (green && red) {
			active_neck_pos = 1;	// Low
			
			if (but6) 
			{
				if (enable_arranger_mode) 	{
					midi_send_program_change(0xC0, 33);	// electric bass guitar on channel 1						
					active_strum_pattern = 3;			// force arpeggios, no strumming
				}
				else
					
				if (enable_seqtrak) 		midi_seqtrak_arp();	
				else						
				if (enable_modx) 			midi_modx_arp_octave(active_neck_pos - 2);	// use neck position to set keyboard octave	
			}
		}
		else
			
		if (blue && orange) {
			active_strum_pattern = -1;
			if (but6 && enable_seqtrak) midi_seqtrak_arp();
		}
		else			
						
		if (green) {
			if (active_strum_pattern > 1) stop_chord();   // kill any sustained notes
			
			active_strum_pattern = 0;
			if (but6 && enable_seqtrak) midi_seqtrak_arp();				
			
			if (but6 && enable_ample_guitar && active_neck_pos != 1) {
				midi_send_note(0x90, 86, 127);		// enable chord detection					
				midi_send_note(0x90, 97, 127);		// key switch for strum mode on
				midi_send_control_change(0xB0, 64, 1);		// hold pedal off	
			}
		}
		else
			
		if (yellow) {
			active_strum_pattern = 2;
			if (but6 && enable_seqtrak) midi_seqtrak_arp();
			
			if (but6 && enable_ample_guitar && active_neck_pos != 1) {
				midi_send_note(0x90, 97, 1);		// key switch for strum mode off
				midi_send_control_change(0xB0, 64, 127);		// hold pedal on	
				midi_send_note(0x90, 99, 127);					
			}
		}
		else

		if (blue) {
			active_strum_pattern = 3;
			if (but6 && enable_seqtrak) midi_seqtrak_arp();
			
			if (but6 && enable_ample_guitar && active_neck_pos != 1) {
				midi_send_note(0x90, 97, 1);		// key switch for strum mode off
				midi_send_control_change(0xB0, 64, 127);		// hold pedal on	
				midi_send_note(0x90, 99, 127);						
			}
		}				
		else

		if (red) {
			if (active_strum_pattern > 1) stop_chord();   // kill any sustained notes	
			
			active_strum_pattern = 1;
			
			if (but6 && enable_seqtrak) midi_seqtrak_arp();
			
			if (but6 && enable_ample_guitar && active_neck_pos != 1) {
				midi_send_note(0x90, 97, 1);		// key switch for strum mode off
				midi_send_control_change(0xB0, 64, 1);		// hold pedal off						
			}				
		}
		else

		if (orange) {
			active_strum_pattern = 4;
			
			if (but6 && enable_seqtrak) midi_seqtrak_arp();
			
			if (but6 && enable_ample_guitar && active_neck_pos != 1) {
				midi_send_note(0x90, 97, 1);		// key switch for strum mode off
				midi_send_control_change(0xB0, 64, 127);		// hold pedal on	
				midi_send_note(0x90, 99, 127);						
			}			
		}
		
		if (but6) {
			if (!enable_sp404mk2 && !enable_mpc_sample && !enable_nanobox_tangerine && !enable_wav_trigger_pro && enable_arranger_mode) {
				if (green) midi_send_control_change(0xB3, 9, 1); 		// Melody voice -1
				else if (red) midi_send_control_change(0xB3, 9, 2); 	// Melody voice -2					
				else if (yellow) midi_send_control_change(0xB3, 9, 3); 	// Melody voice -3						
				else if (blue) midi_send_control_change(0xB3, 9, 4); 	// Melody voice -4	
				else if (orange) midi_send_control_change(0xB3, 9, 5); 	// Melody voice -5
			}			

			if (enable_style_play) config_style_play();
			
			if (!green && !red && !yellow && !blue && !orange) {
				enable_auto_strum = !enable_auto_strum;	
				
				if (enable_ample_guitar) 					enable_auto_strum = false;		// clash. ample guitar wins.
				if (style_started && !enable_midi_drums) 	enable_auto_strum = false;		// only midi drums can sync with internal clock. not possible with loopers
				
				stop_chord();											// kill any sustained notes
				
				if (!style_change_requested) 
				{
					if (enable_sp404mk2) {		
						sp404_stop_loops();				
					}
					else

					if (enable_mpc_sample) {
						mpc_stop_loops();						
					}					
					else
						
					if (enable_mpx_looper && mpx_old_sample_note) {
						mpx_stop_loops();				
					}
					else
						
					if (enable_nanobox_tangerine) {
						sampler_midi_note(0x94, sampler_old_drum_note, enable_drum_track ? sample_drum_velocity : 1);					
						nanobox_stop_loops();
					}
					else
						
					if (enable_wav_trigger_pro) {
						wav_trigger_pro_stop_loops();	;						
					}
					
					style_change_requested = true;
				}				
			}
		}
		
		finished_processing = true;
		return;
	}

	if (but7 != song_key)  {								// transpose direct	- handle direct key change (D, E, F, G, A)
		song_key = but7;

		if (but7) {
			transpose = 0;
			
			if (green) 	transpose = 2;		// D
			if (red) 	transpose = 4;		// E
			if (yellow)	transpose = 5;		// F
			if (blue) 	transpose = 7;		// G				
			if (orange) transpose = 9;		// A
		}
		finished_processing = true;		
		return;			
	}

	if (but9 != start)  {									// volume reset
		start = but9;
	
		if (but9) {
			sample_drum_velocity = 127;
			sample_bass_velocity = 120;
			sample_chord_velocity = 90;
			midi_guitar_volume = 127;	
			worship_pad_velocity = 127;			
		}	
		finished_processing = true;	
		return;			
	}						

	if (dpad_up != up) {									// transpose up
		up = dpad_up;
		
		if (dpad_up) {
			transpose++;
			if (transpose > 11) transpose = 0;	
			if (enable_seqtrak) midi_seqtrak_key(transpose);
			//if (enable_modx) 	midi_modx_key(transpose);				
		}
		
		if (enable_mpx_looper)
		{
			if (dpad_up) {
				// do nothing
			}
		}		
		
		finished_processing = true;		
		return;
	}		

	if (mbut0 != logo) {									// start/stop
		logo = mbut0;	
		
		if (enable_midi_drums) 
		{
			if (mbut0) {
				
				if (looper_status.state == LOOPER_STATE_WAITING || looper_status.state == LOOPER_STATE_RECORDING || looper_status.state == LOOPER_STATE_TAP_TEMPO) {
					style_section = 0;	
					looper_status.current_step = 0;	
					
					//if (looper_status.state == LOOPER_STATE_RECORDING) storage_store_tracks();													
					looper_status.state = LOOPER_STATE_PLAYING;
					
					//ghost_parameters_t *params = ghost_note_parameters();						
					//params->ghost_intensity = 0.843;						
				} 
				else 
				
				if (looper_status.state == LOOPER_STATE_PLAYING) {
					looper_status.state = LOOPER_STATE_WAITING;					
				}	
			}					
		} 
		else {
		
			ketron_code = 0x12;		// default start/stop
			yamaha_code = 127;					
			
			if (yellow) {				// INTRO/END-1
				ketron_code = 0x0F;		
				yamaha_code = 0x00;					
			}

			if (red) {
				ketron_code = 0x10;		// INTRO/END-2
				yamaha_code = 0x01;					
			}
			
			if (green) {
				ketron_code = 0x11;		// INTRO/END-3		
				yamaha_code = 0x02;					
			}
			
			if (blue) {
				ketron_code = 0x17;		// TO END
				yamaha_code = 0x01;					
			}
			
			if (orange) {
				ketron_code = 0x35;	// FADE	
				yamaha_code = 0x02;					
			}
			
			if (enable_arranger_mode) midi_ketron_arr(ketron_code, mbut0 ? true : false);
			
			if (!style_started) 
			{
				if (yamaha_code != 127) {
					if (enable_arranger_mode) midi_yamaha_arr(yamaha_code, mbut0 ? true : false);	
				} 
				
				if (mbut0) 
				{					
					if (enable_mpc_sample) {
						mpc_drum_note = 44;
						sampler_midi_note(0x94, mpc_drum_note, enable_drum_track ? sample_drum_velocity : 1);		// .\01\SAMPLE\1-09-085.wav	
						mpc_old_drum_note = mpc_drum_note;
						
						style_change_requested = true;
						style_section = 0;
						
						mpc_chord_note = 255;
						mpc_bass_note = 255;						
					}
					else

					if (enable_mpx_looper && enable_mpx_drums) {
						mpx_sample_note = 36;
						sampler_midi_note(0x99, mpx_sample_note, enable_drum_track ? sample_drum_velocity : 0);	
						mpx_old_sample_note = mpx_sample_note;
						
						style_change_requested = false;
						style_section = 0;					
					}
					else						
						
					if (enable_nanobox_tangerine || enable_wav_trigger_pro) {
						sampler_drum_note = INT1;
						sampler_midi_note(0x94, sampler_drum_note, enable_drum_track ? sample_drum_velocity : 1);	
						sampler_old_drum_note = sampler_drum_note;
						
						sampler_chord_note = 36 + transpose;
						sampler_bass_note = 36 + transpose;							
						sampler_old_chord_note = 255;
						sampler_old_bass_note = 255;
						
						style_change_requested = true;
						style_section = 0;	
					}
					else
						
					if (enable_sp404mk2) {
						// 13	14	15	16	9	10	11	12	5	6	7	8	1	2	3	4
						// C2	C#2	D2	D#2	E2	F2	F#2	G2	G#2	A2	A#2	B2	C3	C#3	D3	D#3
						// 36   37  38  39  40  41  42  43  44  45  46  47  48  49  50  51
						
						sp404_drum_note = 37;
						sampler_midi_note(0x90, sp404_drum_note, enable_drum_track ? sample_drum_velocity : 1);		// .\01\SAMPLE\1-14-085.wav	
						sp404_old_drum_note = sp404_drum_note;
						
						style_change_requested = true;
						style_section = 0;
						
						sp404_chord_note = 0;
						sp404_chord_cmd = 0;
						sp404_bass_note = 0;
						sp404_bass_cmd = 0;						
					}
					else
												
					if (enable_seqtrak) {
						midi_seqtrak_mute(7, false);
						midi_seqtrak_mute(9, false);
						
						midi_start_stop(true);							
					} 
					else
						
					if (enable_modx) {	
						if (green) midi_send_control_change(0xB3, 92, 0); 			// scene n
						else if (red) midi_send_control_change(0xB3, 92, 16); 						
						else if (yellow) midi_send_control_change(0xB3, 92, 32); 				
						else if (blue) midi_send_control_change(0xB3, 92, 48); 	
						else if (orange) midi_send_control_change(0xB3, 92, 64); 	

						midi_modx_arp(true);
					}
					else 
					
					if (enable_arranger_mode) {					// yamaha or ketron
						midi_yamaha_start_stop(0x7A, true);							

						if (!enable_ample_guitar) {
							if (green) midi_send_control_change(0xB3, 3, 1); 		// Fill-1
							else if (red) midi_send_control_change(0xB3, 3, 2); 	// Fill-2						
							else if (yellow) midi_send_control_change(0xB3, 3, 3); 	// Fill-3						
							else if (blue) midi_send_control_change(0xB3, 3, 4); 	// Sync start	
							else if (orange) midi_send_control_change(0xB3, 3, 5); 	// Fade In
							else midi_send_control_change(0xB3, 3, 65); 			// Play
						}
					}
				}
				
			} else {
				if (yamaha_code != 127) {
					if (enable_arranger_mode) midi_yamaha_arr(0x20 + yamaha_code, mbut0 ? true : false);	
				}
				
				if (mbut0) 
				{		
					if (enable_mpx_looper && mpx_old_sample_note) {
						style_end_requested = true;						
					}			
					else
					
					if (enable_mpc_sample) {
						style_end_requested = true;							
					}
					else
						
					if (enable_nanobox_tangerine) {
						style_end_requested = true;						
					}
					else
						
					if (enable_wav_trigger_pro) {
						style_end_requested = true;									 // stop backing track					
					}					
					else
						
					if (enable_sp404mk2) {
						style_end_requested = true;	
					}					
					else
						
					if (enable_seqtrak) {	
						midi_seqtrak_mute(7, true);
						midi_seqtrak_mute(9, true);	
						
						midi_start_stop(false);
						midi_play_chord(false, 0, 0, 0);		
					} 
					else
						
					if (enable_modx) {
						if (green || red || yellow || blue || orange) {
							midi_send_control_change(0xB3, 92, 112);		// scene 8
							sleep_ms(6000);								
						}

						midi_modx_arp(false);						
					}						
					else 
					
					if (enable_arranger_mode) {
						midi_yamaha_start_stop(0x7D, true);		
	
						if (!enable_ample_guitar) {
							if (yellow) midi_send_control_change(0xB3, 3, 66); 		// End-1
							else if (red) midi_send_control_change(0xB3, 3, 67); 	// End-2						
							else if (green) midi_send_control_change(0xB3, 3, 68); 	// End-3						
							else if (blue) midi_send_control_change(0xB3, 3, 69); 	// Sync stop	
							else if (orange) midi_send_control_change(0xB3, 3, 70); // Fade Out
							else midi_send_control_change(0xB3, 3, 127); 			// Stop
						}	
					}						
				}						
			}
		}
		
		if (mbut0) {
			style_started = !style_started;
			cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, style_started);	
		}

		finished_processing = true;
		return;
	}		

	if (dpad_down != starpower) { 							// Style selection
		starpower = dpad_down;	
		next_or_previous = false;
		
		if (dpad_down) {
			old_style = style_section;
		}

		if (green && red) 
		{
			if (dpad_down) {
				style_section = 4;
			}
		}
		else
			
		if (red && yellow) 
		{
			if (dpad_down) {
				style_section = 5;
			}
		}
		else
			
		if (yellow && blue) 
		{
			if (dpad_down) {
				style_section = 6;
			}
		}
		else
			
		if (blue && orange) 
		{
			if (dpad_down) {
				style_section = 7;
			}
		}
		else		
			
		if (green) 
		{
			if (dpad_down) {
				style_section = 0;
			}
		}
		else
			
		if (red) 
		{
			if (dpad_down) {
				style_section = 1;
			}
		}
		else

		if (yellow) 
		{
			if (dpad_down) {
				style_section = 2;
			}
		}				
		else
			
		if (blue) 
		{
			if (dpad_down) {
				style_section = 3;
			}
		}
		else

		if (orange) 						// PREV
		{
			if (dpad_down) {
				next_or_previous = true;
				style_section--;
				if (style_section < 0) style_section = 7;
				if (enable_arranger_mode) midi_send_control_change(0xB3, 14, 127); 		// Previous Style					
			}
		}
		else 
		
		if (dpad_down) {
			next_or_previous = true;			
			style_section++;
			if (style_section > 7) style_section = 0;
			if (enable_arranger_mode) midi_send_control_change(0xB3, 14, 65); 			// Next Style			
		}	

		if (enable_sp404mk2 || enable_mpc_sample || enable_nanobox_tangerine || enable_mpx_looper || enable_wav_trigger_pro)	
		{
			if (dpad_down && style_started) {
				style_change_requested = true;
				
				if (next_or_previous) {			// play fill before style change request
					sampler_trigger_fill();
				}						
			}				
		}			
		else
			
		if (enable_midi_drums)	
		{
			if (dpad_down && looper_status.state == LOOPER_STATE_PLAYING) {	
				//ghost_parameters_t *params = ghost_note_parameters();
				//params->ghost_intensity = 0.843;	
				//storage_store_tracks();						
			}				
		}			
		else		
			
		if (enable_seqtrak) 
		{
			if (dpad_down) 
			{
				if (style_started) {
					midi_seqtrak_arp();
					midi_seqtrak_pattern(style_section % 6);				
				} 
			}
		} 
		else
			
		if (enable_modx) 
		{
			if (dpad_down) {
				uint8_t modx_scenes[8] = {0, 16, 32, 48, 64, 80, 96, 112};
				midi_send_control_change(0xB3, 92, modx_scenes[style_section % 8]);
			}
		}			
		else 
		
		if (enable_arranger_mode) {	
			midi_ketron_arr(3 + (style_section % 4), dpad_down ? true : false);
			midi_yamaha_arr(0x10 + (style_section % 4), dpad_down ? true : false);	
			
			if (dpad_down) {
				if (green) midi_send_control_change(0xB3, 14, 1); 		// Style select -1
				else if (red) midi_send_control_change(0xB3, 14, 2); 	// Style select -2					
				else if (yellow) midi_send_control_change(0xB3, 14, 3); // Style select -3						
				else if (blue) midi_send_control_change(0xB3, 14, 4); 	// Style select -4	
				else if (orange) midi_send_control_change(0xB3, 14, 5); // Style select -5
			}			
		}				

		finished_processing = true;		
		return;			
	}

	if (mbut2 != menu) {									// menu - select registrations/style groups
		if (enable_arranger_mode) midi_ketron_footsw(8, mbut2 ? true : false);						// user defined from footswitch	
		menu = mbut2;

		if (green && blue && orange) 
		{
			if (mbut2) {
				style_group = 20;
			}
		}
		else
			
		if (green && yellow && blue) 
		{
			if (mbut2) {
				style_group = 19;
			}
		}
		else
			
		if (red && blue && orange) 
		{
			if (mbut2) {
				style_group = 18;
			}
		}
		else
			
		if (green && red && yellow) 
		{
			if (mbut2) {
				style_group = 17;
			}
		}
		else
			
		if (red && yellow && blue) 
		{
			if (mbut2) {
				style_group = 16;
			}
		}
		else
			
		if (yellow && blue && orange) 
		{
			if (mbut2) {
				style_group = 15;
			}
		}
		else
			
		if (green && orange) 
		{
			if (mbut2) {
				style_group = 14;
			}
		}
		else

		if (green && blue) 
		{
			if (mbut2) {
				style_group = 13;
			}
		}
		else
			
		if (red && orange) 
		{
			if (mbut2) {
				style_group = 12;	
			}
		}		
		else		

		if (green && yellow) 
		{
			if (mbut2) {
				style_group = 11;	
			}
		}
		else
			
		if (red && blue) 
		{
			if (mbut2) {
				style_group = 10;	
			}
		}
		else

		if (yellow && orange) 
		{
			if (mbut2) {
				style_group = 9;
			}
		}				
		else
			
		if (green && red) 
		{
			if (mbut2) {
				style_group = 8;	
			}
		}
		else

		if (red && yellow) 
		{
			if (mbut2) {
				style_group = 7;
			}
		}				
		else

		if (yellow && blue) 
		{
			if (mbut2) {
				style_group = 6;	
			}
		}
		else

		if (blue && orange) 
		{
			if (mbut2) {
				style_group = 5;
			}
		}

		else
			
		if (green) 
		{
			if (mbut2) {
				style_group = 0;	
			}
		}
		else
			
		if (red) 
		{
			if (mbut2) {
				style_group = 1;	
			}
		}
		else

		if (yellow) 
		{
			if (mbut2) {
				style_group = 2;
			}
		}				
		else

		if (blue) 
		{
			if (mbut2) {
				style_group = 3;	
			}
		}
		else

		if (orange) 
		{
			if (mbut2) {
				style_group = 4;
			}
		}
		
		// Now check what device ia active and act accordingly
		handle_group_change();
					
		finished_processing = true;			
		return;		
	}		

	if (mbut3 != config) {									// config options - select arranger/keyboard/sound module
		config = mbut3;
		
		if (mbut3) 	
		{			
			if (!green && !red && !yellow && !blue && !orange) 
			{
				if (enable_mpx_looper) {
					enable_mpx_drums = !enable_mpx_drums;
				}
			} 			
			else if (green && red && yellow) config_guitar(18);		// Reset Preferences
			else if (red && yellow && blue) config_guitar(16);		// Used
			else if (green && red && blue) config_guitar(15);		// Used			
			else if (yellow && blue && orange) config_guitar(17);	// Save Preferences
			
			else if (green && orange) config_guitar(13);			// Behringer Synth (JT-Micro, UB-1 Micro)
			else if (green && yellow) config_guitar(10);			// Akai MPX Looper			
			else if (red && blue) config_guitar(11);				// Akai MPC Sample
			else if (yellow && orange) config_guitar(12);			// Roland SP-404Mk2
			
			else if (green && blue) config_guitar(19);				// Yamaha SeqTrak
			else if (red && orange) config_guitar(14);				// 1010Music Nanobox Tangerine
			
			else if (green && red) config_guitar(6);				// Acoustic/Electric
			else if (red && yellow) config_guitar(7);				// Guitar Midi FX1
			else if (yellow && blue) config_guitar(8);				// Guitar Midi FX2
			else if (blue && orange) config_guitar(9);				// Guitar Midi Fx3
			
			else if (green) config_guitar(1);						// Ketron	
			else if (red) config_guitar(2);							// Ample
			else if (yellow) config_guitar(3);						// Dream MIDI Drums
			else if (blue) config_guitar(4);						// WAV Trigger Pro
			else if (orange) config_guitar(5);						// MODX				
		}

		finished_processing = true;		
		return;			
	}

	if (joy_up != joystick_up) {							// style control - fill, tempo
		joystick_up = joy_up;
			
		if (green && red) 
		{
			if (joy_up) {
				set_tempo(85);					
			}
		}
		else
			
		if (red && yellow) 
		{
			if (joy_up) {
				set_tempo(95);						
			}
		}
		else

		if (yellow && blue) 
		{
			if (joy_up) {
				set_tempo(105);						
			}
		}
		else
			
		if (blue && orange) 
		{
			if (joy_up) {
				set_tempo(115);					
			}
		}
		else

		if (orange && green) 
		{
			if (joy_up) {
				set_tempo(125);					
			}
		}
		else			
			
		if (green) 
		{
			if (joy_up) {
				set_tempo(80);						
			}
		}
		
		else
			
		if (red) 
		{
			if (joy_up) {
				set_tempo(90);						
			}
		}
		else

		if (yellow) 
		{
			if (joy_up) {
				set_tempo(100);						
			}
		}
		else
			
		if (blue) 
		{
			if (joy_up) {
				set_tempo(110);					
			}
		}
		else	

		if (orange) 
		{
			if (joy_up) {
				set_tempo(120);						
			}
		}
		else					

		if (enable_ample_guitar) {
			midi_send_note(0x90, 24, joy_up ? 127 : 0);						// sustain guitar notes
		} 
		else
			
		if (enable_modx) 
		{
			if (joy_up && style_section > 0 && style_section < 4) {
				uint8_t modx_scenes[8] = {0, 16, 32, 48, 64, 80, 96, 112};
				
				midi_send_control_change(0xB3, 92, modx_scenes[style_section + 3]);
				sleep_ms(1000);				
				midi_send_control_change(0xB3, 92, modx_scenes[style_section]);					
			}
		}
		else
			
		if (enable_mpx_looper) 
		{	
			if (joy_up) {
				// do nothing. drum fills/breaks not available
			}
		}		

		if (enable_arranger_mode && style_started) {
			midi_ketron_arr(0x07 + (style_section % 4), joy_up ? true : false);	// 	Fill
			midi_yamaha_arr(0x10 + (style_section % 4), joy_up ? true : false);				
		}
		else
			
		if (enable_sp404mk2 || enable_mpc_sample || enable_nanobox_tangerine || enable_wav_trigger_pro)	
		{
			if (joy_up && style_started) 
			{
				sampler_trigger_fill();
				style_change_requested = true;						
			}				
		}		
		else if (!enable_ample_guitar) {				
			if (joy_up) {
				midi_send_control_change(0xB3, 14, 6 + (style_section % 4)); 	// Fill
			}
		}
		
		finished_processing = true;		
		return;
	}

	if (joy_down != joystick_down) {						// style control - break, drum beat control
		joystick_down = joy_down;	

		if (red) 
		{
			if (joy_down) 
			{
				if (enable_midi_drums)	{	
					ghost_parameters_t *params = ghost_note_parameters();
					params->ghost_intensity = 0.843;	

					finished_processing = true;
					return;
				}
			}
		}
		else
			
		if (yellow) 
		{
			if (joy_down) 
			{
				if (enable_midi_drums)	{	
					ghost_parameters_t *params = ghost_note_parameters();
					params->ghost_intensity = 0.0;

					finished_processing = true;
					return;
				}
			}
		}
		else			
		
		if (blue) 
		{
			if (joy_down) 
			{
				if (!style_started && enable_midi_drums)	{						
					looper_status.state = LOOPER_STATE_RECORDING;
					
					if (style_group > -1) looper_clear_all_tracks();		// clear static style				
					style_group = -1;
					
					looper_status.current_step = 0;
					
					finished_processing = true;					
					return;
				}
			}
		}
		else

		if (orange) 
		{
			if (joy_down) 
			{
				if (!style_started && enable_midi_drums)	{
					looper_status.state = LOOPER_STATE_TAP_TEMPO;

					finished_processing = true;
					return;	
				}
			}
		}
		else

		if (enable_ample_guitar) {
			midi_send_note(0x90, 26, joy_down ? 127 : 0);	// palm mute guitar notes
		} 
		else
			
		if (enable_arranger_mode && style_started) {			
			midi_ketron_arr(0x0B + (style_section % 4), joy_down ? true : false);	// 	return	
			midi_yamaha_arr(0x18, joy_down ? true : false);				
		}
		else
			
		if (enable_sp404mk2 || enable_mpc_sample || enable_nanobox_tangerine || enable_wav_trigger_pro)	
		{
			if (joy_down && style_started) 
			{				
				sampler_trigger_break();
				style_change_requested = true;						
			}				
		}				
		else if (!enable_ample_guitar) {				
			if (joy_down) {
				midi_send_control_change(0xB3, 14, 10); 	// Break
			}
		}

		finished_processing = true;
		return;			
	}

	if (knob_up != logo_knob_up) {							// unused because of volume control - mpc-sample, sp404mk2
		logo_knob_up = knob_up;	

		finished_processing = true;		
		return;			
	}

	if (knob_down != logo_knob_down) {						// unused because of volume control - mpc-sample, sp404mk2
		logo_knob_down = knob_down;	
			
		finished_processing = true;
		return;			
	}

	if (dpad_right != right) {								// strum up
		right = dpad_right;	

		if (dpad_right) {
			strum_neutral = false;				
			if (!enable_auto_hold) stop_chord();
			play_chord(true, true);
			
		} else {
			strum_neutral = true;
			
			if ((!green && !red && !yellow && !blue && !orange) || active_strum_pattern == 0 || active_strum_pattern == 1 || enable_auto_hold) {
				stop_chord();	// sustain arpeggios only
			}	

			if (looper_status.state == LOOPER_STATE_RECORDING || looper_status.state == LOOPER_STATE_TAP_TEMPO) {
				looper_handle_input_internal_clock(BUTTON_EVENT_CLICK_RELEASE);				
			}						
		}

		if (!style_started) cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, !!dpad_right);	

		finished_processing = true;		
		return;
	}	

	if (dpad_left != left) { 								// Strum down
		left = dpad_left;
		
		if (dpad_left) 	{
			strum_neutral = false;
			if (!enable_auto_hold) stop_chord();			
			play_chord(true, false);
			
		} else {
			strum_neutral = true;
			
			if ((!green && !red && !yellow && !blue && !orange) || active_strum_pattern == 0 || active_strum_pattern == 1 || enable_auto_hold) {
				stop_chord();	// sustain arpeggios only
			}
			
			if (looper_status.state == LOOPER_STATE_RECORDING || looper_status.state == LOOPER_STATE_TAP_TEMPO) {
				looper_handle_input_internal_clock(BUTTON_EVENT_CLICK_RELEASE);				
			}
		}
		
		if (!style_started) cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, !!dpad_left);

		finished_processing = true;		
		return;
	}	

	finished_processing = true;				
}

void handle_group_change() {
	// action a style_group change
	
	if (enable_mpx_looper)
	{
		if (mbut2) {
			// do nothing. single drum group active
		}
	}
	else
		
	if (enable_nanobox_tangerine) 
	{
		if (mbut2)  {				
			midi_send_program_change(0xCF, style_group + 2); // select preset on channel 16 and skip both 1010 pianos					
		}
	}
	else
		
	if (enable_wav_trigger_pro) 
	{
		if (mbut2)  {				
			sampler_midi_note(0x9F, 36 + style_group, 127);			 // select and load preset  
		}
	}		
	else
		
	if (enable_synth) 
	{
		if (mbut2)  {
			midi_send_program_change(0xC0, style_group); // select synth patch
		}
	}		
	else

	if (enable_seqtrak) 
	{
		if (mbut2) {
			midi_send_program_change(0xC0, style_group % 8);	// set PC to project no
			midi_send_control_change(0xB0, 0, 64); 				// MSB 64						
			midi_send_control_change(0xB0, 32, 0); 				// LSB 0
		}
	}
	else 
		
	if (enable_modx) 
	{
		if (mbut2) {
			// TODO - FIX!!!
			midi_send_program_change(0xC0, style_group % 16);	// set PC to performance/set list no						
			midi_send_control_change(0xB0, 0, 62); 				// MSB 62	
			midi_send_control_change(0xB0, 32, 0); 				// LSB 0 Page 1											
		}
	}
	else if (!enable_ample_guitar) {
		if (mbut2) {
			midi_send_control_change(0xB3, 15, style_group + 1);// select style group
		}							
	}
	
}

int compDown(const void *a, const void *b) {
    return (*(uint8_t *)b - *(uint8_t *)a);
}

int compUp(const void *a, const void *b) {
    return (*(uint8_t *)a - *(uint8_t *)b);
}

void config_guitar(uint8_t mode) {
	
	if (mode > 5 && mode < 10 && (enable_nanobox_tangerine || enable_wav_trigger_pro)) {
		// midi guitar control clash with sampler
		// sampler wins. don't send any midi control/program message to channel 1
		return;
	}
		
	
		
	if (mode == 19) {										// SeqTrak			
		enable_seqtrak = !enable_seqtrak;
		enable_style_play = enable_seqtrak;
		
		if (enable_style_play) 	config_style_play();		
		if (enable_seqtrak) 	config_seqtrak();
	}
	else	

	if (mode == 18) {										// Reset Preferences
		enable_arranger_mode 	 = false;
		enable_ample_guitar 	 = false;
		enable_midi_drums 		 = false;
		enable_seqtrak 			 = false;
		enable_modx 			 = false;
		enable_sp404mk2 		 = false;
		enable_mpc_sample 		 = false;
		enable_nanobox_tangerine = false;
		enable_wav_trigger_pro	 = false;
		enable_mpx_looper		 = false;
		enable_mpx_drums		 = false;
		enable_synth 			 = false;
		
		guitar_pc_code			 = 26;
		
		preferences_changed 	 = true;
	}
	else
		
	if (mode == 17) {										// Save Preferences
		preferences_changed = true;
	}
	else
		
	if (mode == 16) {										// unused

	}
	else
		
	if (mode == 15) {										// unused

	}
	else
		
	if (mode == 14) {										// 1010Music Nanobox Tangerine
		enable_nanobox_tangerine = !enable_nanobox_tangerine;
		enable_style_play = enable_nanobox_tangerine;
		
		if (enable_nanobox_tangerine) {
			config_nanobox_tangerine();		
		}
	}
	else
		
	if (mode == 13) {										// Behringer JT-Micro Synth
		enable_synth = !enable_synth;						
		enable_style_play = !enable_synth;			
		
		if (enable_synth)  {
			config_style_play();
		}					
	}	
	else
		
	if (mode == 12) {										// Roland SP404 MK2
		enable_sp404mk2 = !enable_sp404mk2;	
		enable_style_play = !enable_sp404mk2;
		
		if (enable_style_play) 	config_style_play();
		if (enable_sp404mk2) 	config_sp404mk2();
	}
	else
		
	if (mode == 11) {										// Akai MPC Sample
		enable_mpc_sample = !enable_mpc_sample;	
		enable_style_play = !enable_mpc_sample;	
		
		if (enable_style_play) 	config_style_play();		
		if (enable_mpc_sample) 	config_mpc_sample();
	}
	else
		
	if (mode == 10) {										// Akai MPX8 Looper
		enable_mpx_looper = !enable_mpx_looper;
		enable_style_play = !enable_mpx_looper;
		
		if (enable_style_play) 	config_style_play();
		if (enable_mpx_looper) 	config_mpx_looper();					
	}
	else
	
	if (mode == 9) {										// Delay FX	
		enable_dream_midi = true;	
		enable_style_play = true;
		config_style_play();
				
		midi_send_program_change(0xC0, guitar_pc_code);		
		
		midi_send_control_change(0xB0, 80, 7);				// reverb - pan delay
		midi_send_control_change(0xB0, 91, 64);	
		
		midi_send_control_change(0xB0, 81, 0);				// chorus - short delay		
		midi_send_control_change(0xB0, 93, 0);
		
		dream_set_delay(looper_status.bpm);		
	}
	else
		
	if (mode == 8) {										// Chorus + Reverb FX
		enable_dream_midi = true;	
		enable_style_play = true;
		config_style_play();
				
		midi_send_program_change(0xC0, guitar_pc_code);	
		
		midi_send_control_change(0xB0, 80, 4);				// reverb - hall
		midi_send_control_change(0xB0, 91, 64);	
		
		midi_send_control_change(0xB0, 81, 2);				// chorus - 3		
		midi_send_control_change(0xB0, 93, 64);		
	}
	else
		
	if (mode == 7) {										// Reverb FX
		enable_dream_midi = true;	
		enable_style_play = true;
		config_style_play();
			
		midi_send_program_change(0xC0, guitar_pc_code);	
		
		midi_send_control_change(0xB0, 80, 4);				// reverb - hall
		midi_send_control_change(0xB0, 91, 64);	
		
		midi_send_control_change(0xB0, 81, 2);				// chorus - 3		
		midi_send_control_change(0xB0, 93, 0);						
	}
	else
		
	if (mode == 6) {										// toggle guitar from electric to acoustic
		enable_dream_midi = true;	
		enable_style_play = true;	
		config_style_play();
			
		guitar_pc_code = (guitar_pc_code == 26) ? 25 : 26;
		midi_send_program_change(0xC0, guitar_pc_code);	
		
		midi_send_program_change(0xC3, 89);					// warm pad on channel 4 (chords)   
		midi_send_control_change(0xB3, 7, 0); 				// don't play pads by default 			
	}
	else

	if (mode == 1) {  										// Arranger (ketron, giglad)
		enable_arranger_mode = !enable_arranger_mode;
		enable_style_play = enable_arranger_mode;	
		
		if (enable_style_play) 		config_style_play();
		if (enable_arranger_mode) 	config_arranger();						
	}
	else
		
	if (mode == 2) {										// DAW (ample guitar)
		enable_ample_guitar = !enable_ample_guitar; 		// Ample Guitar VST mode
		enable_style_play = enable_ample_guitar;
		if (enable_style_play) config_style_play();		
	
		midi_send_note(0x90, 97, enable_ample_guitar ? 127 : 1);	// set strum mode on by default
		if (enable_ample_guitar) config_ample_guitar();
	}
	else
		
	if (mode == 3) {										// MIDI Ghost drummer		
		if (enable_midi_drums) {
			looper_clear_all_tracks();						// Midi drums looper
		}
		enable_midi_drums = !enable_midi_drums;
		enable_style_play = enable_midi_drums;	
		
		if (enable_style_play) config_style_play();		
		
		if (enable_midi_drums) {
			config_midi_drums();	
		}
	}
	else
		
	if (mode == 4) {										// WAV Trigger Pro
		enable_wav_trigger_pro = !enable_wav_trigger_pro;
		enable_style_play = !enable_wav_trigger_pro;

		if (enable_wav_trigger_pro) {
			config_wav_trigger_pro();		
		}
	}
	else
		
	if (mode == 5) {										// MODX/Montage
		enable_modx = !enable_modx;
		enable_style_play = enable_modx;
		
		if (enable_style_play) 	config_style_play();		
		if (enable_modx) 		config_modx();
	}
}

void config_style_play() {
	midi_play_chord(false, 0, 0, 0);					// reset chord  keys
	
	if (!enable_modx) {
		midi_send_control_change(0xB0, 7, midi_guitar_volume);	
		midi_send_control_change(0xB1, 7, midi_guitar_volume);
		midi_send_control_change(0xB2, 7, midi_guitar_volume);			
			
		midi_send_program_change(0xC1, 89);					// warm pad	(blue)
		midi_send_program_change(0xC2, 99);					// FX4 (orange)	
		midi_send_program_change(0xC3, 89);		

	
		if (active_strum_pattern > 2 || active_strum_pattern == -1) {
			midi_send_control_change(0xB1, 11, 127);			
		}
		
		if (active_strum_pattern > 3 || active_strum_pattern == -1) {
			midi_send_control_change(0xB2, 11, 127);			
		}	
	}		
}

void config_nanobox_tangerine() {
	//midi_send_control_change(0xB4, 11, !enable_nanobox_tangerine ? 127 : 0);  						// silent sample trigger channel 5		
	//midi_send_control_change(0xB5, 11, !enable_nanobox_tangerine ? 127 : 0);  						// silent sample trigger channel 6
	//midi_send_control_change(0xB6, 11, !enable_nanobox_tangerine ? 127 : 0);  						// silent sample trigger channel 7	
	//midi_send_program_change(0xCF, style_group + 2);													// load default selected song
}

void config_wav_trigger_pro() {
	midi_send_control_change(0xB4, 11, 0); 				// silent sample trigger channel 5/8 (drums), 6/9 (bass), 7/10 (chords) and 15 (global)
	midi_send_control_change(0xB5, 11, 0); 
	midi_send_control_change(0xB6, 11, 0); 	
	midi_send_control_change(0xB7, 11, 0);
	midi_send_control_change(0xB8, 11, 0);
	midi_send_control_change(0xB9, 11, 0);
	midi_send_control_change(0xBF, 11, 0);	
	
	midi_send_control_change(0xBF, 7, 127);				
}

void config_mpx_looper() {	
	midi_send_control_change(0xB9, 11, 0);  			// silent sample trigger channel 10	
	midi_send_control_change(0xB9, 7, 0);  				// silent sample trigger channel 10	

	midi_send_program_change(0xC5, guitar_pc_code);						// channel 5 used for guitar melody
	midi_send_control_change(0xB5, 80, 4);				// reverb - hall
	midi_send_control_change(0xB5, 91, 64);	
	midi_send_control_change(0xB5, 81, 2);				// chorus - 3		
	midi_send_control_change(0xB5, 93, 0);		
}

void config_mpc_sample() {	
	midi_send_control_change(0xB4, 11, 0);  						// silent sample trigger channel 5	
	midi_send_program_change(0xC0, guitar_pc_code);					// channel 1 used for guitar melody
}

void config_sp404mk2() {
	for (uint8_t i=0; i<10; i++) midi_send_control_change(0xB0 + i, 11, 0); 	// silence sample trigger channels 1-10
		
	midi_send_program_change(0xCE, guitar_pc_code);							// channel 15 used for guitar melody
	midi_send_control_change(0xBE, 80, 4);				// reverb - hall
	midi_send_control_change(0xBE, 91, 64);	
	midi_send_control_change(0xBE, 81, 2);				// chorus - 3		
	midi_send_control_change(0xBE, 93, 0);			
}

void config_modx() {	
	midi_send_control_change(0xB3, 92, 0);		
}

void config_seqtrak() {	
	// initially mute seqtrak arpeggiator							
	midi_seqtrak_mute(7, true);
	midi_seqtrak_mute(9, true);			
}

void config_midi_drums() {	
	midi_send_program_change(0xC3, 89);		// warm pad on channel 4 (chords)   
	midi_send_control_change(0xB3, 7, 0); 	// don't play pads by default 		
}

void config_arranger() {
	midi_send_program_change(0xC0, guitar_pc_code);	// jazz guitar on channel 1	
	midi_send_control_change(0xB0, 7, midi_guitar_volume);	
	midi_send_control_change(0xB1, 7, midi_guitar_volume);
	midi_send_control_change(0xB2, 7, midi_guitar_volume);	
}

void config_ample_guitar() {
	midi_send_note(0x90, 97, 127);	// set strum mode on by default
	midi_send_note(0x90, 86, 127);		
}

void play_chord(bool on, bool up) {
	uint8_t chord_note = 0;
	uint8_t chord_type = 0;	
	uint8_t bass_note = 0;		
	uint8_t base = 0;	
	bool handled = false;	
		
	base = 24 + transpose;
	
	// --- F/C

	if (yellow && blue && orange && red) {
		basic_chord = 4;	
		advanced_chord = 0x610;
		
		if (enable_style_play) midi_play_slash_chord(on, base - 12, base + 5, base + 9, base + 12);		
		chord_note = (base + 5);	
		bass_note = base - 12;
		handled = true;		
	}
	else

	// --- G/C

	if (yellow && blue && orange && green) 	{
		basic_chord = 5;
		advanced_chord = 0x810;
		
		if (enable_style_play) midi_play_slash_chord(on, base - 12, base + 7, base + 11, base + 14);
		chord_note = (base + 7);	
		bass_note = base - 12;		
		handled = true;		
	}
	else

	// -- B

	if (red && yellow && blue && green) {
		basic_chord = 0;
		advanced_chord = 0xCC0;
		
		if (enable_style_play) midi_play_chord(on, base - 1, base + 3, base + 6);	
		chord_note = (base - 1);		
		handled = true;			
	}
	else

	if (red && yellow && green)     // Ab
	{	
		basic_chord = 0;
		advanced_chord = 0x990;
		
		if (enable_style_play) midi_play_chord(on, base - 4, base, base + 3);
		chord_note = (base - 4);		
		handled = true;				
	}
	else

	if (red && yellow && blue)     // A
	{
		basic_chord = 0;
		advanced_chord = 0xAA0;
		
		if (enable_style_play) midi_play_chord(on, base - 3, base + 13, base + 16);
		chord_note = (base - 3);		
		handled = true;		
	}
	else

	if (blue && yellow && green)     // E
	{
		basic_chord = 0;
		advanced_chord = 0x550;
		
		if (enable_style_play) midi_play_chord(on, base - 8, base + 8, base + 11);
		chord_note = (base - 8);		
		handled = true;		
	}
	else


	if (blue && red && orange)     // Eb
	{
		basic_chord = 0;
		advanced_chord = 0x440;
		
		if (enable_style_play) midi_play_chord(on, base - 9, base + 7, base + 10);
		chord_note = (base - 9);		
		handled = true;		
	}
	else

	if (yellow && blue && orange)    // F/G
	{
		basic_chord = 4;	
		advanced_chord = 0x680;
		
		if (enable_style_play) midi_play_slash_chord(on, base - 17, base + 5, base + 9, base + 12);
		chord_note = (base + 5);
		bass_note = base - 17;			
		handled = true;	
	}
	else

	if (red && yellow)     // Bb
	{
		basic_chord = 7;
		advanced_chord = 0xBB0;
		
		if (enable_style_play) midi_play_chord(on, base - 2, base + 2, base + 5);
		chord_note = (base - 2);		
		handled = true;			
	}
	else

	if (green && yellow)     // Gsus
	{
		basic_chord = 5;
		advanced_chord = 0x882;
		
		if (enable_style_play) midi_play_chord(on, base - 5, base + 12, base + 14);
		chord_note = (base - 5);	
		chord_type = 2;
		handled = true;			
	}
	else

	if (orange && yellow)     // Csus
	{
		basic_chord = 1;	
		advanced_chord = 0x112;
		
		if (enable_style_play) midi_play_chord(on, base, base + 5, base + 7);
		chord_note = (base);	
		chord_type = 2;
		handled = true;				
	}
	else

	if (yellow && blue)    // C/E
	{
		basic_chord = 1;	
		advanced_chord = 0x150;
		
		if (enable_style_play) midi_play_slash_chord(on, base - 20, base, base + 4, base + 7);
		chord_note = (base);
		bass_note = base - 20;	
		handled = true;		
	}
	else

	if (green && red)     // G/B
	{
		basic_chord = 5;
		advanced_chord = 0x8C0;
		
		if (enable_style_play) midi_play_slash_chord(on, base - 13, base + 7, base + 11, base + 14);
		chord_note = (base + 7);
		bass_note = base - 13;			
		handled = true;	
	}
	else

	if (blue && orange)     // F/A
	{
		basic_chord = 4;	
		advanced_chord = 0x6A0;
		
		if (enable_style_play) midi_play_slash_chord(on, base - 15, base + 5, base + 9, base + 12);
		chord_note = (base + 5);
		bass_note = base - 15;			
		handled = true;	
	}
	else

	if (green && blue)     // Em
	{
		basic_chord = 3;
		advanced_chord = 0x551;
		
		if (enable_style_play) midi_play_chord(on, base - 8, base + 7, base + 11);
		chord_note = (base - 8);	
		chord_type = 1;		
		handled = true;		
	}
	else

	if (orange && red)   // Fm
	{
		basic_chord = 0;
		advanced_chord = 0x661;
		
		if (enable_style_play) midi_play_chord(on, base - 7, base + 8, base + 12);
		chord_note = (base - 7);	
		chord_type = 1;
		handled = true;			
	}
	else

	if (green && orange)     // Gm
	{
		basic_chord = 0;
		advanced_chord = 0x881;
		
		if (enable_style_play) midi_play_chord(on, base - 5, base + 10, base + 14);
		chord_note = (base - 5);	
		chord_type = 1;
		handled = true;			
	}
	else

	if (red && blue)     // D
	{
		basic_chord = 0;
		advanced_chord = 0x330;
		
		if (enable_style_play) midi_play_chord(on, base + 2, base + 6, base + 9);
		chord_note = (base + 2);	
		handled = true;			
	}
	else

	if (yellow)    // C
	{
		basic_chord = 1;	
		advanced_chord = 0x110;
		
		if (enable_style_play) midi_play_chord(on, base, base + 4, base + 7);
		chord_note = (base);	
		handled = true;	
	}
	else

	if (blue)      // Dm
	{
		basic_chord = 2;
		advanced_chord = 0x331;
		
		if (enable_style_play) midi_play_chord(on, base + 2, base + 5, base + 9);
		chord_note = (base + 2);	
		chord_type = 1;		
		handled = true;		
	}
	else

	if (orange)   // F
	{
		basic_chord = 4;
		advanced_chord = 0x660;
		
		if (enable_style_play) midi_play_chord(on, base - 7, base + 9, base + 12);
		chord_note = (base - 7);	
		handled = true;		
	}
	else

	if (green)     // G
	{
		basic_chord = 5;
		advanced_chord = 0x880;
		
		if (enable_style_play) midi_play_chord(on, base - 5, base + 11, base + 14);
		chord_note = (base - 5);			
		handled = true;		
	}
	else

	if (red)     // Am
	{
		basic_chord = 6;
		advanced_chord = 0xAA1;
		
		if (enable_style_play) midi_play_chord(on, base - 3, base + 12, base + 16);
		chord_note = (base - 3);	
		chord_type = 1;			
		handled = true;		
	}
	
	int O = 12;
	int D = 2, E = 4, G = 7, A = 9, B = 11;	
	int __6th = E +O*(active_neck_pos+2), __5th = A +O*(active_neck_pos+2), __4th = D +O*(active_neck_pos+2), __3rd = G +O*(active_neck_pos+2), __2nd = B +O*(active_neck_pos+2), __1st = E +O*(active_neck_pos+3);		
	int string_frets[6] = {__6th, __5th, __4th, __3rd, __2nd, __1st};
	uint8_t chord_midinotes[6] = {0};	
	static int seq_index = 0;
	
	int string = 0;
	int notes_count = 0;
	int velocity = 110;	
	bool strum_last_chord = false;

	uint8_t note = 0;
	uint8_t ample_style_notes[8] = {36, 37, 39, 42, 44, 46, 49, 51};	
	//uint8_t ample_string_notes[6] = {47, 45, 43, 41, 40, 38};
	
	if (enable_mpx_looper) 			// trigger chord loop on mpx
	{					
		if (handled && on) 
		{
			if (style_started) {
				mpx_trigger_loop();
			}
			else 
				
			if (style_end_requested) {
				mpx_stop_loops();
				style_end_requested = false;
			}
		}
	}		
	else

	if (enable_mpc_sample)			// trigger chord loop on mpc sample
	{
		if (handled && on) 
		{			
			if (style_started) {
				mpc_trigger_loop();				
			}
			else
				
			if (style_end_requested) {
				sampler_midi_note(0x94, 45, enable_drum_track ? sample_drum_velocity : 1);		// .\01\SAMPLE\1-09-085.wav
				mpc_stop_loops();
				style_end_requested = false;				
			}
		}
	}
	else
				
	if (enable_nanobox_tangerine)	// trigger chord loop on nanobox tangerine
	{
		if (handled && on) 
		{
			if (style_started) {	// chord played
				sampler_trigger_loop();				
			}
			else
				
			if (style_end_requested)  {
				sampler_midi_note(0x94, END1, enable_drum_track ? sample_drum_velocity : 1);
				nanobox_stop_loops();				
				style_end_requested = false;
				style_end_started = true;
			}
			else
				
			if (style_end_started) {
				sampler_midi_note(0x94, END1, enable_drum_track ? sample_drum_velocity : 1);
				style_end_started = false;	
			}			
		}
	}
	else
				
	if (enable_wav_trigger_pro)		// trigger chord loop on w
	{
		if (handled && on) 
		{
			if (style_started) {
				sampler_trigger_loop();				
			}
			else
				
			if (style_end_requested) {
				wav_trigger_pro_stop_loops();						
				sampler_midi_note(0x94, END1, enable_drum_track ? sample_drum_velocity : 1); // not a loop		
				style_end_requested = false;				
			}		
		}
	}
	else
		
	if (enable_sp404mk2) 			// trigger chord loop on sp404 mk2
	{		
		if (handled && on) 
		{	
			if (style_started) {
				sp404_trigger_loop();		
			}
			else
				
			if (style_end_requested) {
				sampler_midi_note(0x90, 40, enable_drum_track ? sample_drum_velocity : 1);		// .\01\SAMPLE\1-09-085.wav
				sp404_stop_loops();
				style_end_requested = false;				
			}		
		}
	}		

	if (handled) {
		last_chord_note = chord_note;
		last_chord_type = chord_type;
		last_bass_note = bass_note;
	}
	
	if (active_strum_pattern > -1) 
	{	
		if (!handled && active_strum_pattern > 0 && active_neck_pos > 1) {	// play strum of last chord for ample guitar arppergio noises and non bass notes
			handled = true;
			strum_last_chord = true;
		}
		
		if (handled) 
		{			
			if (up || (active_strum_pattern == 0) || (strum_last_chord && up)) 
			{	
				if (enable_ample_guitar && active_strum_pattern == 0) 
				{	
					if (style_section != old_style) {
						note = ample_style_notes[style_section % 8] + 24;							
						midi_send_note(0x90, note, 127);				// play style key note
						ample_old_key = note;		
					}						
				} 
				else {	
					int strum_index = active_strum_pattern;
					
					if (active_strum_pattern > 1 && active_neck_pos > 1) {
						strum_index = active_strum_pattern + ((style_section % 4) * 3);							// use 4 style variations to cover arps 3-14
					}
					
					int play_pattern = strum_last_chord ? (active_strum_pattern == 1 ? 2 : 0) : strum_index;	// use strum (0) when playing arpergios and arpegio (2) when playing strum/bass
					
					while (strum_pattern[play_pattern][seq_index][0] == 0 ) {									// ignore empty pattern steps	
						seq_index++;
						if (seq_index > 11) seq_index = 0;
					}

					for (int i=0; i<6; i++) 
					{						
						mute_midinotes[i] = 0;	// reset muted notes
						
						string = 6 - strum_pattern[play_pattern][seq_index][i];
						
						if (string > -1 && string < 6) 
						{
							if (chord_chart[last_chord_note % 12][last_chord_type][string] > -1) {	// ignore unused strings
								chord_midinotes[notes_count] = string_frets[string] + chord_chart[last_chord_note % 12][last_chord_type][string];
								notes_count++;						
							}
						}
					}

					velocity = 110;
					
					if (up) {
						qsort(chord_midinotes, notes_count, sizeof(uint8_t), compUp);			
					} else {
						qsort(chord_midinotes, notes_count, sizeof(uint8_t), compDown);								
					}
				
					for (int n=0; n<notes_count; n++) {
						note = chord_midinotes[(notes_count - 1) - n];											
						if (velocity > 25) velocity = velocity - 10;
										
						if (active_neck_pos == 1) {
							if ((note % 12) > 4) note = note - 12; 	// bass needs another octave lower for bass neck pos
						}						
						
						if (enable_ample_guitar && note < 40) note = note + 12;	
						
						// don't play chord when midi drums is enabled. auto-strum will handle it on beat
						
						if (!enable_midi_drums || active_strum_pattern != 0) {
							midi_send_note(0x90, note, velocity);
						}
						
						old_midinotes[n] = note;
						mute_midinotes[n] = note;							
					}

					if (!up && enable_midi_drums && active_strum_pattern == 0) {
						// play bass note on downstroke with auto-strum
						
						note = ((last_bass_note ? last_bass_note : last_chord_note) % 12) + (O * (active_neck_pos + 1));
						if ((note % 12) > 4) note = note - 12;

						if (!enable_modx && !enable_ample_guitar && !enable_seqtrak && !enable_wav_trigger_pro && !enable_nanobox_tangerine && !enable_synth && active_neck_pos > 1) midi_send_program_change(0xC0, 33);						
						midi_send_note(0x90, note, 120);
						if (!enable_modx && !enable_ample_guitar && !enable_seqtrak && !enable_wav_trigger_pro && !enable_nanobox_tangerine && !enable_synth && active_neck_pos > 1) midi_send_program_change(0xC0, guitar_pc_code);	
						
						old_midinotes[0] = note;						
					}					

					seq_index++;	
					if (seq_index > 11) seq_index = 0;
				}						
				
			} else {
				note = ((last_bass_note ? last_bass_note : last_chord_note) % 12) + (O * (active_neck_pos + 2));
				
				if (!strum_last_chord && (active_strum_pattern == 1 || active_neck_pos == 1)) {
					if ((note % 12) > 4) note = note - 12; 	// bass needs another octave lower for strum pattern 2 or bass
				}
				
				if (enable_ample_guitar && note < 40) note = note + 12;				 
				midi_send_note(0x90, note, velocity);
				old_midinotes[0] = note;				
			}					
		} 
		else 
		{
			if (enable_ample_guitar) 	// noises
			{
				if (active_neck_pos == 1) {	// bass slides
					note = 95;
					if (up) note = 94;
				}					
				else
					
				if (active_strum_pattern == 0) {
					note = 93;
					if (up) note = 94;
				}
				else

				if (active_strum_pattern == 1) {
					note = 95;
					if (up) note = 94;
				}					
				else {	// handled above as strum last chord action

				}
				
				midi_send_note(0x90, note, 100);				
				old_midinotes[0] = note;
			} 
			else 
			{	
				if (enable_midi_drums && looper_status.state == LOOPER_STATE_RECORDING) 
				{					
					if (style_section % 5 == 0) {
						looper_status.current_track = 0;					// Bass Drum
						if (up) looper_status.current_track = 1;			// Snare
						looper_handle_input_internal_clock(BUTTON_EVENT_CLICK_BEGIN);					}
					else
						
					if (style_section % 5 == 1) {
						looper_status.current_track = 2;					// Closed Hit-Hat
						if (up) looper_status.current_track = 5;			// Open Hit-Hat
						looper_handle_input_internal_clock(BUTTON_EVENT_CLICK_BEGIN);						
					}	
					else
						
					if (style_section % 5 == 2) {
						looper_status.current_track = 3;					// Low floor tom
						if (up) looper_status.current_track = 6;			// High tom
						looper_handle_input_internal_clock(BUTTON_EVENT_CLICK_BEGIN);						
					}
					else
						
					if (style_section % 5 == 3) {
						looper_status.current_track = 7;					// crash cymbals
						if (up) looper_status.current_track = 8;			// ride cymbals
						looper_handle_input_internal_clock(BUTTON_EVENT_CLICK_BEGIN);						
					}
					else
						
					if (style_section % 5 == 4) {
						looper_status.current_track = 10;					// Hi Bongo
						if (up) looper_status.current_track = 11;			// Low Bongo
						looper_handle_input_internal_clock(BUTTON_EVENT_CLICK_BEGIN);						
					}					
					
				}
				else
					
				if (active_strum_pattern == 0) 	
				{					
					for (int n=0; n<6; n++) // only mute played notes with strumming up and down
					{
						if (mute_midinotes[n] > 0) {
							note = mute_midinotes[n];
							midi_send_note(0x90, note, 35);	// lower velocity to achieve muted sound
							old_midinotes[n] = note;					
						}
					}					
				}
			}
		}
	} 		
}

void clear_chord_notes() {
	midi_play_chord(false, 0, 0, 0);
	
	if (enable_ample_guitar && ample_old_key) midi_send_note(0x80, ample_old_key, 0);				// stop ample strum		
	ample_old_key = 0;	
}

void stop_chord() {
	if (enable_style_play) clear_chord_notes();		
	
	for (int n=0; n<6; n++) 
	{
		if (old_midinotes[n] > 0) {
			midi_send_note(0x80, old_midinotes[n], 0);	
			old_midinotes[n] = 0;
		}
		
		if (chord_notes[n] > 0) {
			midi_send_note(0x80, chord_notes[n], 0);	
			chord_notes[n] = 0;
		}				
	}

	if (voice_note != 0) midi_send_note(0x80, voice_note, 0);
}

void sampler_trigger_loop() {
	uint8_t sampler_chord = (uint8_t) (advanced_chord / 256);
	uint8_t sampler_bass = (uint8_t) ((advanced_chord % 256) / 16);			
	uint8_t sampler_type = (uint8_t) ((advanced_chord % 256) % 16);
	
	uint8_t bass_tonic = (sampler_bass + transpose - 1) % 12;
	uint8_t chord_tonic = (sampler_chord + transpose - 1) % 12;	
	
	if (sampler_type == 0) {												// Major 
		sampler_bass_note = (bass_tonic + 36) % 128;

		if (bass_tonic != chord_tonic) {									// Bass in root
	
		} 			
		
		// Chords
		
		if (style_section % 2 == 0) {										// C-1
			sampler_chord_note = (chord_tonic + 36) % 128;			
		}									
		else
		if (style_section % 2 == 1) {										// C-2
			sampler_chord_note = (chord_tonic + 72) % 128;						
		}																
	}
	else		
	if (sampler_type == 1) {												// Minor		
		sampler_bass_note = (bass_tonic + 48) % 128;	
		
		if (style_section % 2 == 0) {
			sampler_chord_note = (chord_tonic + 48) % 128;								
		}
		else
		if (style_section % 2 == 1) {
			sampler_chord_note = (chord_tonic + 84) % 128;								
		}
	}
	else		
	if (sampler_type == 2) {												// Sus4	
		sampler_bass_note = (bass_tonic + 36) % 128;		

		if (style_section % 2 == 0) {									
			sampler_chord_note = (chord_tonic + 60) % 128;			
		}									
		else
		if (style_section % 2 == 1) {									
			sampler_chord_note = (chord_tonic + 96) % 128;						
		}		
	}

	if (sampler_old_bass_note != sampler_bass_note || enable_stacatto_mode) 
	{ 
		if (sampler_old_bass_note != 255)
		{
			if (enable_nanobox_tangerine) {
				sampler_midi_note(0x95, sampler_old_bass_note, enable_bass_track ? sample_bass_velocity : 0);
			}
			else
				
			if (enable_wav_trigger_pro) {
				sampler_midi_note(0x98, sampler_old_bass_note, 127);				
			}
		}
		
		sampler_midi_note(0x95, sampler_bass_note, enable_bass_track ? sample_bass_velocity : 0);			
		sampler_old_bass_note = sampler_bass_note;		
	}
	
	if (sampler_old_chord_note != sampler_chord_note || enable_stacatto_mode) 
	{	
		if (sampler_old_chord_note != 255)
		{
			if (enable_nanobox_tangerine) {			
				sampler_midi_note(0x96, sampler_old_chord_note, enable_chord_track ? sample_chord_velocity : 0);	
			}
			else
				
			if (enable_wav_trigger_pro) {
				sampler_midi_note(0x99, sampler_old_chord_note, 127);				
			}
		}
		
		sampler_midi_note(0x96, sampler_chord_note, enable_chord_track ? sample_chord_velocity : 0);		
		sampler_old_chord_note = sampler_chord_note;	
	}		
			
	if (style_change_requested) {
		style_change_requested = false;
		
		uint8_t section = (style_section % 4);
		sampler_drum_note = ARRA;
		
		if (section == 1) sampler_drum_note = ARRB;
		if (section == 2) sampler_drum_note = ARRC;
		if (section == 3) sampler_drum_note = ARRD;		
		
		if (sampler_old_drum_note != sampler_drum_note) 
		{
			if (sampler_old_drum_note != 255) 
			{
				if (enable_nanobox_tangerine) {			
					sampler_midi_note(0x94, sampler_old_drum_note, enable_drum_track ? sample_drum_velocity : 1);
				}
				else
					
				if (enable_wav_trigger_pro) {
					sampler_midi_note(0x97, sampler_old_drum_note, 127);
				}			
			}				
						
			sampler_midi_note(0x94, sampler_drum_note, enable_drum_track ? sample_drum_velocity : 1);
			sampler_old_drum_note = sampler_drum_note;	
		}			
	}	
}

void mpc_trigger_loop() {
	uint8_t mpc_chord = (uint8_t) (advanced_chord / 256);
	uint8_t mpc_bass = (uint8_t) ((advanced_chord % 256) / 16);			
	uint8_t mpc_type = (uint8_t) ((advanced_chord % 256) % 16);
	
	uint8_t bass_tonic = (mpc_bass + transpose - 1) % 12;
	uint8_t chord_tonic = (mpc_chord + transpose - 1) % 12;
	
	uint8_t samples[12][9] = 
	{// BMaj, BRoot, MajA, MajB, Bmin,   MinA,  MinB, Sus4A, Sus4B
		{95,  119,   23,  47, 	107, 	35,  	59,  71, 	83},	// C
		{94,  118,   22,  46, 	106, 	34, 	58,  70, 	82},	// C#
		{97,  121,   25,  49, 	109, 	37,  	61,  73, 	85},	// D
		{96,  120,   24,  48, 	108, 	36,  	60,  72, 	84},	// D#
		{98,  122,   26,  50, 	110, 	38,  	62,  74, 	86},	// E
		{100, 124,   28,  52,  	112,	40,  	64,  76, 	88},	// F
		{99,  123,   27,  51,  	111, 	39,  	63,  75, 	87},	// F#
		{102, 126,   30,  54,  	114,	42,  	66,  78, 	90},	// G
		{101, 125,   29,  53,  	113,	41,  	65,  77, 	89},	// G#
		{92,  116,   20,  44, 	104, 	32, 	56,  68, 	80},	// A
		{91,  115,   19,  43,  	103, 	31, 	55,  67,	79},	// A#
		{93,  117,   21,  45, 	105, 	33, 	57,  69,	81},	// B		
	};	

	if (mpc_type == 0) {												// Bass in major 
		mpc_bass_note = (samples[bass_tonic][0] - 3 + 36) % 128;

		if (bass_tonic != chord_tonic) {												// Bass in root
			mpc_bass_note = (samples[bass_tonic][1] - 3 + 36) % 128;	
		} 			
		
		// Chords
		
		if (style_section % 2 == 0) {									// C-1
			mpc_chord_note = (samples[chord_tonic][2] - 3 + 36) % 128;			
		}									
		else
		if (style_section % 2 == 1) {									// C-2
			mpc_chord_note = (samples[chord_tonic][3] - 3 + 36) % 128;						
		}																
	}
	else		
	if (mpc_type == 1) {												// Bass in minor		
		mpc_bass_note = (samples[bass_tonic][4] - 3 + 36) % 128;	
		
		if (style_section % 2 == 0) {
			mpc_chord_note = (samples[chord_tonic][5] - 3 + 36) % 128;								
		}
		else
		if (style_section % 2 == 1) {
			mpc_chord_note = (samples[chord_tonic][6] - 3 + 36) % 128;								
		}
	}
	else		
	if (mpc_type == 2) {												// Sus4	
		mpc_bass_note = (samples[bass_tonic][0] - 3 + 36) % 128;			

		if (style_section % 2 == 0) {									// Csus-1
			mpc_chord_note = (samples[chord_tonic][7] - 3 + 36) % 128;				
		}									
		else
		if (style_section % 2 == 1) {									// Csus-2
			mpc_chord_note = (samples[chord_tonic][8] - 3 + 36) % 128;
		}		
	}

	
	if (mpc_old_bass_note != mpc_bass_note || enable_stacatto_mode) { 
		if (mpc_old_bass_note != 255) sampler_midi_note(0x94, mpc_old_bass_note, enable_bass_track ? sample_bass_velocity : 1);
		
		sampler_midi_note(0x94, mpc_bass_note, enable_bass_track ? sample_bass_velocity : 5);			
		mpc_old_bass_note = mpc_bass_note;		
	}
	
	if (mpc_old_chord_note != mpc_chord_note || enable_stacatto_mode) {	
		if (mpc_old_chord_note != 255) sampler_midi_note(0x94, mpc_old_chord_note, enable_chord_track ? sample_chord_velocity : 1);	
		
		sampler_midi_note(0x94, mpc_chord_note, enable_chord_track ? sample_chord_velocity : 1);		
		mpc_old_chord_note = mpc_chord_note;	
	}	
	
			
	if (style_change_requested) {
		style_change_requested = false;
		mpc_drum_note = 36 + style_section;		
		
		if (mpc_old_drum_note != mpc_drum_note)
		{		
			if (mpc_old_drum_note != 255) {
				sampler_midi_note(0x94, mpc_old_drum_note, enable_drum_track ? sample_drum_velocity : 1);
			}				

			sampler_midi_note(0x94, mpc_drum_note, enable_drum_track ? sample_drum_velocity : 1);
			mpc_old_drum_note = mpc_drum_note;		
		}			
	}	
}

void sp404_trigger_loop() {
	uint8_t sp404_chord = (uint8_t) (advanced_chord / 256);
	uint8_t sp404_bass = (uint8_t) ((advanced_chord % 256) / 16);			
	uint8_t sp404_type = (uint8_t) ((advanced_chord % 256) % 16);

	// 13	14	15	16	9	10	11	12	5	6	7	8	1	2	3	4
	// C2	C#2	D2	D#2	E2	F2	F#2	G2	G#2	A2	A#2	B2	C3	C#3	D3	D#3
	// 36   37  38  39  40  41  42  43  44  45  46  47  48  49  50  51	
	
	uint8_t pad2midi[16] = {48, 49, 50, 51, 44, 45, 46, 47, 40, 41, 42, 43, 36, 37, 38, 39};
	
	uint8_t samples[12][9][2] = 
	{	// Bmin,   BRoot,    MajA,	 MajB,    SusA,   MinA,     MinB,    BMaj,	Sus4B
		{{7, 9},  {8, 5},  {2, 5},  {3, 13}, {5, 5}, {3, 1},  {4, 9},  {6, 13}, {6, 1}},	// C
		{{7, 8},  {8, 4},  {2, 4},  {3, 12}, {5, 4}, {2, 16}, {4, 8},  {6, 12}, {5, 16}},	// C#
		{{7, 11}, {8, 7},  {2, 7},  {3, 15}, {5, 7}, {3, 3},  {4, 11}, {6, 15}, {6, 3}},	// D
		{{7, 10}, {8, 6},  {2, 6},  {3, 14}, {5, 6}, {3, 2},  {4, 10}, {6, 14}, {6, 2}},	// D#
		{{7, 12}, {8, 8},  {2, 8},  {3, 16}, {5, 8}, {3, 4},  {4, 12}, {6, 16}, {6, 4}},	// E
		{{7, 14}, {8, 10}, {2, 10}, {4, 2},  {5, 10},{3, 6},  {4, 14}, {7, 2},  {6, 6}},	// F
		{{7, 13}, {8, 9},  {2, 9},  {4, 1},  {5, 9}, {3, 5},  {4, 13}, {7, 1},  {6, 5}},	// F#
		{{7, 16}, {8, 12}, {2, 12}, {4, 4},  {5, 12},{3, 8},  {4, 16}, {7, 4},  {6, 8}},	// G
		{{7, 15}, {8, 11}, {2, 11}, {4, 3},  {5, 11},{3, 7},  {4, 15}, {7, 3},  {6, 7}},	// G#
		{{7, 6},  {8, 2},  {2, 2},  {3, 10}, {5, 2}, {2, 14}, {4, 6},  {6, 10}, {5, 14}},	// A
		{{7, 5},  {8, 1},  {2, 1},  {3, 9},  {5, 1}, {2, 13}, {4, 5},  {6, 9},  {5, 13}},	// A#
		{{7, 7},  {8, 3},  {2, 3},  {3, 11}, {5, 3}, {2, 15}, {4, 7},  {6, 11}, {5, 15}},	// B		
	};
	
	uint8_t bass_tonic = (sp404_bass + transpose - 1) % 12;
	uint8_t chord_tonic = (sp404_chord + transpose - 1) % 12;	

	if (sp404_type == 0) {												// Bass in major
		sp404_bass_note = pad2midi[samples[bass_tonic][7][1] - 1];	
		sp404_bass_cmd = 0x90 + samples[bass_tonic][7][0] - 1;							

		if (bass_tonic != chord_tonic) {								// Bass in root
			sp404_bass_note = pad2midi[samples[bass_tonic][1][1] - 1];			
			sp404_bass_cmd = 0x90 + samples[bass_tonic][1][0] - 1;	
		} 			
		
		// Chords
		
		if (style_section % 2 == 0) {									// C-1
			sp404_chord_note = pad2midi[samples[chord_tonic][2][1] - 1];			
			sp404_chord_cmd = 0x90 + samples[chord_tonic][2][0] - 1;	
		}									
		else
		if (style_section % 2 == 1) {									// C-2
			sp404_chord_note = pad2midi[samples[chord_tonic][3][1] - 1];			
			sp404_chord_cmd = 0x90 + samples[chord_tonic][3][0] - 1;			
		}																
	}
	else		
	if (sp404_type == 1) {												// Bass in minor		
		sp404_bass_note = pad2midi[samples[bass_tonic][0][1] - 1];	
		sp404_bass_cmd = 0x90 + samples[bass_tonic][0][0] - 1;	
		
		if (style_section % 2 == 0) {
			sp404_chord_note = pad2midi[samples[chord_tonic][5][1] - 1];			
			sp404_chord_cmd = 0x90 + samples[chord_tonic][5][0] - 1;						
		}
		else
		if (style_section % 2 == 1) {
			sp404_chord_note = pad2midi[samples[chord_tonic][6][1] - 1];			
			sp404_chord_cmd = 0x90 + samples[chord_tonic][6][0] - 1;						
		}
	}
	else		
	if (sp404_type == 2) {												// Sus4	
		sp404_bass_note = pad2midi[samples[bass_tonic][0][1] - 1];	
		sp404_bass_cmd = 0x90 + samples[bass_tonic][0][0] - 1;

		if (style_section % 2 == 0) {
			sp404_chord_note = pad2midi[samples[chord_tonic][4][1] - 1];			
			sp404_chord_cmd = 0x90 + samples[chord_tonic][4][0] - 1;						
		}
		else
		if (style_section % 2 == 1) {
			sp404_chord_note = pad2midi[samples[chord_tonic][8][1] - 1];			
			sp404_chord_cmd = 0x90 + samples[chord_tonic][8][0] - 1;						
		}		
	}

	// TODO
	// make autohold work with sp404
	
	if (sp404_old_bass_note != sp404_bass_note || sp404_old_bass_cmd != sp404_bass_cmd) { 
		if (sp404_old_bass_note != 0) sampler_midi_note(sp404_old_bass_cmd, sp404_old_bass_note, enable_bass_track ? sample_bass_velocity : 1);
		
		sampler_midi_note(sp404_bass_cmd, sp404_bass_note, enable_bass_track ? sample_bass_velocity : 1);
		sp404_old_bass_cmd = sp404_bass_cmd;			
		sp404_old_bass_note = sp404_bass_note;		
	}
	
	if (sp404_old_chord_note != sp404_chord_note || sp404_old_chord_cmd != sp404_chord_cmd) {	
		if (sp404_old_chord_note != 0) sampler_midi_note(sp404_old_chord_cmd, sp404_old_chord_note, enable_chord_track ? sample_chord_velocity : 1);	
		
		sampler_midi_note(sp404_chord_cmd, sp404_chord_note, enable_chord_track ? sample_chord_velocity : 1);
		sp404_old_chord_cmd	= sp404_chord_cmd;		
		sp404_old_chord_note = sp404_chord_note;	
	}

	if (style_change_requested) {
		style_change_requested = false;
		
		if (style_section == 0) 		sp404_drum_note = 48;	
		else if (style_section == 1) 	sp404_drum_note = 49;
		else if (style_section == 2) 	sp404_drum_note = 50;
		else if (style_section == 3) 	sp404_drum_note = 51;
		else if (style_section == 4) 	sp404_drum_note = 44;
		else if (style_section == 5) 	sp404_drum_note = 45;
		else if (style_section == 6) 	sp404_drum_note = 46;
		else if (style_section == 7) 	sp404_drum_note = 47;			
		
		if (sp404_old_drum_note != sp404_drum_note)
		{		
			// 13	14	15	16	9	10	11	12	5	6	7	8	1	2	3	4
			// C2	C#2	D2	D#2	E2	F2	F#2	G2	G#2	A2	A#2	B2	C3	C#3	D3	D#3
			// 36   37  38  39  40  41  42  43  44  45  46  47  48  49  50  51	
			
			if (sp404_old_drum_note > 0) {
				sampler_midi_note(0x90, sp404_old_drum_note, enable_drum_track ? sample_drum_velocity : 1);
			}				

			sampler_midi_note(0x90, sp404_drum_note, enable_drum_track ? sample_drum_velocity : 1);
			sp404_old_drum_note = sp404_drum_note;
		}
		
	}	
}

void mpx_trigger_loop() {			
	//uint8_t mpc_chord = (uint8_t) (advanced_chord / 256);
	uint8_t mpc_type = (uint8_t) ((advanced_chord % 256) % 16);

	if (enable_mpx_drums) 
	{
		if (style_change_requested) {
			style_change_requested = false;
			
			if (style_section == 0) 		mpx_sample_note = 36;			// Variations 1-4 only
			else if (style_section == 1) 	mpx_sample_note = 38;
			else if (style_section == 2) 	mpx_sample_note = 40;
			else if (style_section == 3) 	mpx_sample_note = 41;
			else if (style_section == 4) 	mpx_sample_note = 36;
			else if (style_section == 5) 	mpx_sample_note = 38;
			else if (style_section == 6) 	mpx_sample_note = 40;
			else if (style_section == 7) 	mpx_sample_note = 41;	
					
			if (mpx_old_sample_note != mpx_sample_note)
			{			
				if (mpx_old_sample_note) {
					sampler_midi_note(0x99, mpx_old_sample_note, enable_drum_track ? sample_drum_velocity : 0);
				}				
			
				sampler_midi_note(0x99, mpx_sample_note, enable_drum_track ? sample_drum_velocity : 0);
				mpx_old_sample_note = mpx_sample_note;
			}
		}		
	} else {
		mpx_sample_note = 0;			
		
		if (basic_chord == 1 && mpc_type == 0) mpx_sample_note = 36;		// C
		if (basic_chord == 2 && mpc_type == 1) mpx_sample_note = 43;		// Dm
		if (basic_chord == 3 && mpc_type == 1) mpx_sample_note = 45;		// Em
		if (basic_chord == 4 && mpc_type == 0) mpx_sample_note = 38;		// F
		if (basic_chord == 5 && mpc_type == 0) mpx_sample_note = 40;		// G
		if (basic_chord == 6 && mpc_type == 1) mpx_sample_note = 41;		// Am
		if (basic_chord == 1 && mpc_type == 2) mpx_sample_note = 47;		// Csus
		if (basic_chord == 5 && mpc_type == 2) mpx_sample_note = 48;		// Gsus	

		if ((mpx_sample_note && mpx_old_sample_note != mpx_sample_note) || enable_stacatto_mode) 
		{	
			if (mpx_old_sample_note != 0) {
				sampler_midi_note(0x99, mpx_old_sample_note, enable_chord_track ? sample_chord_velocity : 0);				
			}
			
			sampler_midi_note(0x99, mpx_sample_note, enable_chord_track ? sample_chord_velocity : 0);			
			mpx_old_sample_note = mpx_sample_note;				
		}
		
		if (style_change_requested) {			// no audio drums
			style_change_requested = false;
		}		
	}		
}

void mpc_stop_loops() {	
	if (mpc_old_chord_note != 255) {
		sampler_midi_note(0x94, mpc_old_chord_note, enable_chord_track ? sample_chord_velocity : 1);
		mpc_old_chord_note = 255;						
	}

	if (mpc_old_bass_note != 255) {						
		sampler_midi_note(0x94, mpc_old_bass_note, enable_bass_track ? sample_bass_velocity : 1);
		mpc_old_bass_note = 255;						
	}

	if (mpc_old_drum_note != 255) {
		sampler_midi_note(0x94, mpc_old_drum_note, enable_drum_track ? sample_drum_velocity : 1);
		mpc_old_drum_note = 255;
	}	
}

void mpx_stop_loops() {
	if (mpx_old_sample_note) sampler_midi_note(0x99, mpx_old_sample_note, sample_drum_velocity);		// replay to stop mpx loop		
	mpx_old_sample_note = 0;
}

void sp404_stop_loops() {
	if (sp404_old_chord_note > 0) {
		sampler_midi_note(sp404_old_chord_cmd, sp404_old_chord_note, enable_chord_track ? sample_chord_velocity : 1);
		sp404_old_chord_note = 0;
		sp404_old_chord_cmd = 0;							
	}

	if (sp404_old_bass_note > 0) {						
		sampler_midi_note(sp404_old_bass_cmd, sp404_old_bass_note, enable_bass_track ? sample_bass_velocity : 1);
		sp404_old_bass_note = 0;
		sp404_old_bass_cmd = 0;							
	}

	if (sp404_old_drum_note > 0) {
		sampler_midi_note(0x90, sp404_old_drum_note, enable_drum_track ? sample_drum_velocity : 1);
		sp404_old_drum_note = 0;
	}	
}

void nanobox_stop_loops() {	
	if (sampler_old_chord_note != 255) {
		sampler_midi_note(0x96, sampler_old_chord_note, enable_chord_track ? sample_chord_velocity : 1);
		sampler_old_chord_note = 255;						
	}

	if (sampler_old_bass_note != 255) {						
		sampler_midi_note(0x95, sampler_old_bass_note, enable_bass_track ? sample_bass_velocity : 1);
		sampler_old_bass_note = 255;						
	}

	if (sampler_old_drum_note != 255) {						
		sampler_old_drum_note = 255;
	}	
}

void wav_trigger_pro_stop_loops() {	
	if (sampler_old_chord_note != 255) {
		sampler_midi_note(0x99, sampler_old_chord_note, 127);
		sampler_old_chord_note = 255;						
	}

	if (sampler_old_bass_note != 255) {						
		sampler_midi_note(0x98, sampler_old_bass_note, 127);
		sampler_old_bass_note = 255;						
	}

	if (sampler_old_drum_note != 255) {		// wav trigger pro has loop mode off for end1 drum style	
		sampler_midi_note(0x97, sampler_old_drum_note, 127);	
		sampler_old_drum_note = 255;
	}	
}

void sampler_trigger_fill() {
	
	if (enable_sp404mk2) 
	{
		// 13	14	15	16	9	10	11	12	5	6	7	8	1	2	3	4
		// C2	C#2	D2	D#2	E2	F2	F#2	G2	G#2	A2	A#2	B2	C3	C#3	D3	D#3
		// 36   37  38  39  40  41  42  43  44  45  46  47  48  49  50  51
		
		if (sp404_old_drum_note > 0) {
			sampler_midi_note(0x90, sp404_old_drum_note, enable_drum_track ? sample_drum_velocity : 1);
		}					
		sp404_drum_note = (style_section % 4) == 0 ? 41 : ((style_section % 4) == 1 ? 42 : ((style_section % 4) == 2 ? 43 : 36)); 											// BRK1 & BRKB
		sampler_midi_note(0x90, sp404_drum_note, enable_drum_track ? sample_drum_velocity : 1);		
		sp404_old_drum_note = sp404_drum_note;
	}
	else
		
	if (enable_mpc_sample) 
	{
		if (mpc_old_drum_note != 255) {
			sampler_midi_note(0x94, mpc_old_drum_note, enable_drum_track ? sample_drum_velocity : 1);
		}					
		mpc_drum_note = 36 + (style_section % 4) + 10;											// FILA - FILD
		sampler_midi_note(0x94, mpc_drum_note, enable_drum_track ? sample_drum_velocity : 1);		
		mpc_old_drum_note = mpc_drum_note;					
	}
	else
		
	if (enable_nanobox_tangerine) 
	{
		if (sampler_old_drum_note != 255) {
			sampler_midi_note(0x94, sampler_old_drum_note, enable_drum_track ? sample_drum_velocity : 1);
		}	
		uint8_t section = (style_section % 4);
		sampler_drum_note = FILA;
		
		if (section == 1) sampler_drum_note = FILB;
		if (section == 2) sampler_drum_note = FILC;
		if (section == 3) sampler_drum_note = FILD;					
		
		sampler_midi_note(0x94, sampler_drum_note, enable_drum_track ? sample_drum_velocity : 1);		
		sampler_old_drum_note = sampler_drum_note;					
	}
	else
		
	if (enable_wav_trigger_pro) 
	{
		if (sampler_old_drum_note != 255) {
			sampler_midi_note(0x97, sampler_old_drum_note, 127);
		}	
		uint8_t section = (style_section % 4);
		sampler_drum_note = FILA;
		
		if (section == 1) sampler_drum_note = FILB;
		if (section == 2) sampler_drum_note = FILC;
		if (section == 3) sampler_drum_note = FILD;					
		
		sampler_midi_note(0x94, sampler_drum_note, enable_drum_track ? sample_drum_velocity : 1);		
		sampler_old_drum_note = sampler_drum_note;					
	}					
}

void sampler_trigger_break() {
	
	if (enable_sp404mk2) 
	{
		// 13	14	15	16	9	10	11	12	5	6	7	8	1	2	3	4
		// C2	C#2	D2	D#2	E2	F2	F#2	G2	G#2	A2	A#2	B2	C3	C#3	D3	D#3
		// 36   37  38  39  40  41  42  43  44  45  46  47  48  49  50  51
		
		if (sp404_old_drum_note > 0) {
			sampler_midi_note(0x90, sp404_old_drum_note, enable_drum_track ? sample_drum_velocity : 1);
		}					
		sp404_drum_note = 38 + (style_section % 2); 											// BRK1 & BRKB
		sampler_midi_note(0x90, sp404_drum_note, enable_drum_track ? sample_drum_velocity : 1);		
		sp404_old_drum_note = sp404_drum_note;
	}
	else
		
	if (enable_mpc_sample) 
	{
		if (mpc_old_drum_note != 255) {
			sampler_midi_note(0x94, mpc_old_drum_note, enable_drum_track ? sample_drum_velocity : 1);
		}					
		mpc_drum_note = 36 + (style_section % 2) + 14; 											// BRK1 & BRKB
		sampler_midi_note(0x94, mpc_drum_note, enable_drum_track ? sample_drum_velocity : 1);		
		mpc_old_drum_note = mpc_drum_note;
	}	
	else
		
	if (enable_nanobox_tangerine) 
	{
		if (sampler_old_drum_note != 255) {
			sampler_midi_note(0x94, sampler_old_drum_note, enable_drum_track ? sample_drum_velocity : 1);
		}	
		sampler_drum_note = BRKA;					
		sampler_midi_note(0x94, sampler_drum_note, enable_drum_track ? sample_drum_velocity : 1);		
		sampler_old_drum_note = sampler_drum_note;
	}
	else
		
	if (enable_wav_trigger_pro) 
	{
		if (sampler_old_drum_note != 255) {
			sampler_midi_note(0x97, sampler_old_drum_note, 127);
		}	
		sampler_drum_note = BRKA;					
		sampler_midi_note(0x94, sampler_drum_note, enable_drum_track ? sample_drum_velocity : 1);		
		sampler_old_drum_note = sampler_drum_note;
	}	
}

static void pico_bluetooth_on_oob_event(uni_platform_oob_event_t event, void* data) {
  (void) data;
  switch (event) {
    case UNI_PLATFORM_OOB_GAMEPAD_SYSTEM_BUTTON:
      // Optional: do something when "system" button gets pressed.
	  //logi("my_platform_on_oob_event: system button event: 0x%04x\n", event);
      break;

    case UNI_PLATFORM_OOB_BLUETOOTH_ENABLED:
      // When the "bt scanning" is on / off. Could be triggered by different events
      // Useful to notify the user
      // logi("my_platform_on_oob_event: Bluetooth enabled: %d\n", (bool)(data));
      break;

    default:
      //logi("my_platform_on_oob_event: unsupported event: 0x%04x\n", event);
	  break;
  }
}

struct uni_platform* get_my_platform(void) {
  static struct uni_platform plat = {
      .name = "Pico2 W",
      .init = pico_bluetooth_init,
      .on_init_complete = pico_bluetooth_on_init_complete,
      .on_device_discovered = pico_bluetooth_on_device_discovered,
      .on_device_connected = pico_bluetooth_on_device_connected,
      .on_device_disconnected = pico_bluetooth_on_device_disconnected,
      .on_device_ready = pico_bluetooth_on_device_ready,
      .on_oob_event = pico_bluetooth_on_oob_event,
      .on_controller_data = pico_bluetooth_on_controller_data,
      .get_property = pico_bluetooth_get_property,
  };

  return &plat;
}

void bluetooth_init(void) {
  //PICO_DEBUG("[INIT] Starting Bluetooth initialization...\n");

  // Keep Wi-Fi off but don't fully disable to avoid interfering with Bluetooth
  // cyw43_arch_disable_sta_mode();
  cyw43_arch_disable_ap_mode();

  // Initialise the BLE MIDI client *before* uni_init() so that its BTStack
  // event handlers are registered in the chain before Bluepad32 powers on the
  // BT controller and raises HCI_STATE_WORKING.
  // BAO disable BLE for now
  // ble_midi_controller_init();

  // Must be called before uni_init()
  uni_platform_set_custom(get_my_platform());
  // PICO_DEBUG("[INIT] Custom platform registered\n");

  // Initialize BP32
  uni_init(0, NULL);
  // PICO_INFO("Bluepad32 initialized\n")
}

void midi_process_state(uint64_t start_us) {		// called by looper.c real-timer for auto-strum
	(void) start_us;
	
	int O = 12;
	int D = 2, E = 4, G = 7, A = 9, B = 11;	
	int __6th = E +O*(active_neck_pos+2), __5th = A +O*(active_neck_pos+2), __4th = D +O*(active_neck_pos+2), __3rd = G +O*(active_neck_pos+2), __2nd = B +O*(active_neck_pos+2), __1st = E +O*(active_neck_pos+3);		
	uint8_t auto_chord_midinotes[6] = {0};
	
	for (int i=0; i<6; i++) {
		auto_chord_midinotes[i] = mute_midinotes[i];
	}
	
	midi_current_step = (midi_current_step + 1) % 128; // 8 bars of of 16 (1/16) beats per bar
	
	if ((enable_midi_drums || (enable_auto_strum && !style_started)) && active_strum_pattern == 0 && (enable_auto_hold || !strum_neutral)) {
		uint8_t start_action = strum_styles[style_group % 5][style_section % 5][midi_current_step % 16][0];
		uint8_t stop_action = strum_styles[style_group % 5][style_section % 5][midi_current_step % 16][1];
		uint8_t velocity = strum_styles[style_group % 5][style_section % 5][midi_current_step % 16][2];
				
		// stop chord strum notes
		
		if (stop_action == 72 || stop_action == 74 || stop_action == 76 || stop_action == 79 || stop_action == 81 || stop_action == 83) 
		{		
			for (int n=0; n<6; n++) {
				midi_send_note(0x80, chord_notes[n], 0);
			}
			
		} else {
			midi_send_note(0x80, voice_note, 0);	// stop voice note
		}
		
		// play string 1 -6

		if (start_action == 62) {
			voice_note = __6th + chord_chart[last_chord_note % 12][last_chord_type][0];
			midi_send_note(0x90, voice_note, velocity);
		}
		else
			
		if (start_action == 64) {
			voice_note = __5th + chord_chart[last_chord_note % 12][last_chord_type][1];
			midi_send_note(0x90, voice_note, velocity);
		}
		else
			
		if (start_action == 65) {
			voice_note = __4th + chord_chart[last_chord_note % 12][last_chord_type][2];
			midi_send_note(0x90, voice_note, velocity);
		}
		else
			
		if (start_action == 67) {
			voice_note = __3rd + chord_chart[last_chord_note % 12][last_chord_type][3];
			midi_send_note(0x90, voice_note, velocity);
		}
		else
			
		if (start_action == 69) {
			voice_note = __2nd + chord_chart[last_chord_note % 12][last_chord_type][4];
			midi_send_note(0x90, voice_note, velocity);
		}
		else
			
		if (start_action == 71) {
			voice_note = __1st + chord_chart[last_chord_note % 12][last_chord_type][5];
			midi_send_note(0x90, voice_note, velocity);				
		}
		else

		// play chord strum up notes	
		
		if (start_action == 76 || start_action == 83) {
			qsort(auto_chord_midinotes, 6, sizeof(uint8_t), compUp);
			
			if (start_action == 83) {	// mute
				if (!enable_modx && !enable_seqtrak && !enable_synth && !enable_ample_guitar && !enable_wav_trigger_pro && !enable_nanobox_tangerine) midi_send_program_change(0xC0, 28);
			}
			
			for (int n=0; n<6; n++) {
				midi_send_note(0x90, auto_chord_midinotes[n], velocity);
				chord_notes[n] = auto_chord_midinotes[n];
				if (velocity > 25) velocity = velocity - 10;
			}
			
			if (start_action == 83) {	// normal
				if (!enable_modx && !enable_seqtrak && !enable_synth && !enable_ample_guitar && !enable_wav_trigger_pro && !enable_nanobox_tangerine) midi_send_program_change(0xC0, guitar_pc_code);
			}			
		} 
		else
			
		// play chord strum down notes		

		if (start_action == 72 || start_action == 74 || start_action == 79 || start_action == 81) {
			qsort(auto_chord_midinotes, 6, sizeof(uint8_t), compDown);
			
			if (start_action == 79 || start_action == 81) {	// mute
				if (!enable_modx && !enable_seqtrak && !enable_synth && !enable_ample_guitar && !enable_wav_trigger_pro && !enable_nanobox_tangerine) midi_send_program_change(0xC0, 28);
			}
			
			for (int n=0; n<6; n++) {
				midi_send_note(0x90, auto_chord_midinotes[n], velocity);
				chord_notes[n] = auto_chord_midinotes[n];	
				if (velocity > 25) velocity = velocity - 10;			
			}
			
			if (start_action == 79 || start_action == 81) {	// normal
				if (!enable_modx && !enable_seqtrak && !enable_synth && !enable_ample_guitar && !enable_wav_trigger_pro && !enable_nanobox_tangerine) midi_send_program_change(0xC0, guitar_pc_code);
			}			
		}
		else
		
		// play voice note		

		if (start_action == 77 || start_action == 78) {	
			voice_note = (last_chord_note % 12) + (O * (active_neck_pos + 2)); // auto_chord_midinotes[0];
			midi_send_note(0x90, voice_note, velocity);
		}		
	}
}

void set_tempo(uint8_t tempo) {
	if (enable_dream_midi)  dream_set_delay(tempo);	
	if (enable_midi_drums) 	looper_update_bpm(tempo);
	if (enable_auto_strum) 	looper_update_bpm(tempo);				
	if (enable_seqtrak) 	midi_seqtrak_tempo(tempo);
	if (enable_modx) 		midi_modx_tempo(tempo);	
}