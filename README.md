# Orinayo

[![Build Status](https://github.com/Jus-Be/orinayo-pico/actions/workflows/build.yml/badge.svg)](https://github.com/Jus-Be/orinayo-pico/actions/workflows/build.yml)

Firmware for the **Raspberry Pi Pico 2 W** that acts as a Bluetooth-to-MIDI gateway, turning Bluetooth Classic (HID over L2CAP) and BLE devices into professional MIDI control surfaces. Connect wirelessly to your controller and output MIDI simultaneously over USB (to a DAW) and hardware UART (to a synthesizer or phrase sampler).

https://github.com/user-attachments/assets/bf5d0bad-dd6a-400b-bc92-5dadf5c17ba5

---

## Features

- **Wireless input** — Bluetooth Classic (HID gamepads) and BLE MIDI devices
- **Dual MIDI output** — USB MIDI (TinyUSB) and UART MIDI (31,250 baud) simultaneously
- **Chord generation** — Button combinations map to chords via a 36-chord lookup table (12 roots × major/minor/sus)
- **32-step drum sequencer** — Record and play back patterns across 14 tracks with algorithmic ghost notes and fills
- **Tap tempo** — Set BPM by tapping the controller
- **Pattern persistence** — Looper patterns saved to flash and restored on power-up
- **Multiple operational modes** — Arranger, Ample Guitar VST, MIDI Drums, Yamaha SeqTrak, Yamaha MODX

---

## Supported Devices

### Bluetooth Input
| Device | Protocol |
|--------|----------|
| Guitar Hero-style controllers (CRKD-Gibson, etc.) | Bluetooth Classic HID |
| Generic HID gamepads | Bluetooth Classic HID |
| Liberlive | BLE |
| Sonicake NeoUke | BLE
| Midi Controller | BLE

### MIDI Input/Output
| Interface | Connection |
|-----------|------------|
| USB Client MIDI | USB cable to computer (DAW, etc.) |
| USB Host MIDI | USB cable to Midi compliant devices |
| UART MIDI | UART0 GPIO Pins 0 (TX) and 1 (RX) to SAM2695 from M5Stack|

---

## Operational Modes

Select a mode at runtime using a button combination on the controller — no reflashing needed.

| Mode | Description |
|------|-------------|
| **Arranger** | Send chords and style-section commands to Ketron/Yamaha keyboard arrangers |
| **Ample Guitar** | Trigger guitar VST articulations and strumming modes via key switches |
| **MIDI Drums** | Record and play back MIDI guitar and drum patterns with 14 tracks, ghost notes, and fills |
| **SeqTrak** | Control Yamaha SeqTrak arpeggiator and pattern selection via SysEx |
| **MPX8** | Trigger 8 chord audio loops via MIDI |
| **MPC Sample** | Trigger 124 audio loops via MIDI with independent tracks for drums, bass and other instruments |
| **SP404 Mk2** | Trigger 160 audio loops via MIDI with independent tracks for drums, bass and other instruments |
| **WAV Trigger Pro** | Play a single multi-sampled instrument and trigger audio loops on WAV trigger Pro from SparkFun via MIDI with independent tracks for drums, bass and other instruments |
| **Nanobox Tangerine** | Play up to 4 multi-sampled instruments with effects and trigger audio loops on Nanobox Tangerine from 1010Music via MIDI with independent tracks for drums, bass and other instruments |

---

## Software Requirements to Build
- Rasberry Pico SDK 2.2 with BTstack 1.6.2

## Hardware Requirements

- **Raspberry Pi Pico 2 W** (RP2350 + CYW43439 — the Pico 2 W specifically, not Pico W)
- USB cable for power and USB Client MIDI
- USB cable for USB Host MIDI and supplied power
- A supported Bluetooth input device (see above)
- *(Optional)* 5-pin DIN connector for hardware UART MIDI input/output

---

## Quick Start

### Download Pre-built Firmware

The latest firmware is built automatically on every push via GitHub Actions.

1. Go to the [Actions tab](https://github.com/Jus-Be/orinayo-pico/actions)
2. Open the most recent successful **Build Orinayo** run
3. Download `workspace_artifacts.zip` and extract `Orinayo.uf2`
4. Hold **BOOTSEL** on the Pico 2 W while plugging in USB — it appears as **RPI-RP2**
5. Copy `Orinayo.uf2` to the **RPI-RP2** drive; the Pico reboots automatically
6. Pair your Bluetooth controller and connect a USB MIDI host or UART MIDI device

### Build from Source

```bash
# 1. Clone with submodules
git clone https://github.com/Jus-Be/orinayo-pico.git
cd orinayo-pico
git submodule update --init --recursive

# 2. Set Pico SDK path
export PICO_SDK_PATH=/path/to/pico-sdk

# 3. Build
mkdir build && cd build
cmake ..
make -j$(nproc)

# 4. Flash Orinayo.uf2 to the Pico 2 W
```

Build artifacts (`Orinayo.uf2`, `.elf`, `.bin`, `.hex`) are placed in the `build/` directory.

---

## Documentation

Detailed documentation is available in the [`wiki/`](wiki/README.md) directory:

- [Overview](wiki/1-orinayo-overview.md)
- [Getting Started](wiki/2-getting-started.md)
  - [Hardware Requirements](wiki/2.1-hardware-requirements.md)
  - [Building and Flashing](wiki/2.2-building-and-flashing.md)
  - [Configuration](wiki/2.3-configuration.md)
- [Architecture](wiki/3-architecture.md)
- [Bluetooth Input System](wiki/4-bluetooth-input-system.md)
- [Musical Processing](wiki/5-musical-processing.md)
- [MIDI Output System](wiki/6-midi-output-system.md)
- [Reference](wiki/10-reference.md)
  - [MIDI Implementation Chart](wiki/10.1-midi-implementation-chart.md)
  - [Configuration Reference](wiki/10.2-configuration-reference.md)
  - [Troubleshooting](wiki/10.3-troubleshooting.md)

## Credits/Inspiration
-  [Pico MIDI Looper "Ghost" Edition](https://github.com/oyama/pico-midi-looper-ghost)

## Legal & Disclaimer
This is a general-purpose wireless translation utility.
- **Trademarks:** Any reference to third-party brands is for **compatibility description** only.
- **Patents:** This software utilizes Bluetooth Classic (HID over L2CAP) protocols, USB Class Compliant MIDI behavior and original state-switching logic to provide a unique musical interface.

## License
Licensed under the **Apache License, Version 2.0**