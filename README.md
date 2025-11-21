# arduino-canbus-monitor [![Build Status](https://api.travis-ci.org/latonita/arduino-canbus-monitor.svg?branch=master)](https://travis-ci.org/latonita/arduino-canbus-monitor) [![Coverity Scan](https://scan.coverity.com/projects/11684/badge.svg)](https://scan.coverity.com/projects/latonita-arduino-canbus-monitor) [![Analytics](https://ga-beacon.appspot.com/UA-99380399-1/welcome-page)](https://github.com/igrigorik/ga-beacon)

CAN bus monitor for Arduino boards with MCP2515-based shields. Implements CAN ASCII / Serial CAN / SLCAN protocol compatible with Lawicel CAN232/CANUSB v1.3, so it works with tools like SavvyCAN.

## Hardware

- MCU: Arduino Uno by default (Mega2560 profile in `platformio.ini`)
- CAN interface: MCP2515-based shield with TJA1050 or SN65HVD230 transceiver
- Optional telemetry: 16x2 I2C LCD (PCF8574 backpack at address `0x27`). Connect SDA -> A4, SCL -> A5, 5V, and GND. The firmware shows current RX frames per second when a display is attached.

## PlatformIO Setup

Configured for [PlatformIO](https://platformio.org/).

### Prerequisites
1. Install PlatformIO IDE for VS Code or the PlatformIO CLI.
2. Open this project in VS Code with the PlatformIO extension, or work from the CLI in the project folder.

### Building and uploading
```bash
pio run
pio run --target upload
pio device monitor
```
Adjust `monitor_port` and `upload_port` in `platformio.ini` to match your device (defaults to `/dev/ttyACM1` in WSL). Serial baud rate is 115200.

### Arduino setup (WSL + Windows bridge)
The board shows up as a COM port in Windows; Device Manager will list the exact number. In WSL it typically appears as `/dev/ttyACMx`.

#### Option 1: Automatic attach (recommended)
- Run `setup-windows-task.bat` as Administrator to create a startup task that calls `attach-arduino.ps1`.
- For immediate use, run `attach-arduino.bat` or `attach-arduino.ps1` as Administrator.

#### Option 2: Manual attach
Each time you want to use the Arduino:
1. In Windows PowerShell (as Administrator):
   ```powershell
   usbipd attach -i 2341:0043 --wsl
   ```
2. Verify in WSL:
   ```bash
   ls -l /dev/ttyACM*
   ```

#### WSL configuration
If you get permission errors in WSL, run:
```bash
newgrp dialout
```

### Configuration
- Board: Arduino Uno (change env in `platformio.ini`)
- Libraries: Seeed-Studio CAN_BUS_Shield and `liquidcrystal_i2c` for the optional LCD
- Receive buffering defaults to 64 frames (`LW232_RX_BUFFER_SIZE` in `src/can-232.h`). Override with `build_flags = -DLW232_RX_BUFFER_SIZE=128` in `platformio.ini` if you need a deeper queue for heavy traffic.

### Changing boards
To use a different Arduino board, edit `platformio.ini`:
```ini
[env:mega2560]
platform = atmelavr
board = megaatmega2560
framework = arduino
```

### Editor IntelliSense (clangd)
Regenerate the compilation database whenever you tweak `platformio.ini`:
```bash
pio run -t compiledb
```
Open the folder in VS Code with the PlatformIO, clangd, Python, and GitLens extensions (see `.vscode/extensions.json`) or use `arduino-canbus-monitor.code-workspace`. clangd will consume `compile_commands.json` for diagnostics.

### Regression tests
Before flashing to a vehicle or sharing firmware, run the LAWICEL regression harness:
```bash
pip install --upgrade pyserial
python tests/slcan_smoke.py --port /dev/ttyACM0  # set to your device path
```
The suite exercises 12 core LAWICEL commands. See `tests/README.md` for details.

## Runtime Telemetry and LCD Display

- `src/runtime_stats.cpp` tracks command, RX, and TX counters, last activity timestamps, RX buffer drops/overflows, and the current frames-per-second value via the `g_canStats` globals.
- `Can232::updateBusLoad()` updates `currentFramesPerSecond` once per second.
- Send the custom `i[CR]` LAWICEL command after opening the bus to dump a snapshot: `iCCCCRRRRTTTTUUUUddddooooFF` (hex-encoded counters for commands/RX/TX/uptime/drops/overflows plus the current FPS byte). The handler lives in `src/can-232.cpp`.
- `src/lcd_display.cpp` drives an optional 16x2 I2C LCD. Line 1 shows a startup banner or status set via `LcdDisplay::updateStatus()`, while line 2 displays RX FPS. If no LCD is present the firmware runs normally.

## Protocol Coverage

This project implements the LAWICEL CAN232/CANUSB ASCII protocol v1.3 plus a diagnostic `i` command.

### Implementation status

| Command | Status | Syntax | Description |
|---------|--------|--------|-------------|
| S | Full | `Sn[CR]` | Setup CAN bit-rate (n=0-9)<br>- S0=10K, S1=20K, S2=50K, S3=100K, S4=125K<br>- S5=250K, S6=500K, S7=800K, S8=1M, S9=83.3K |
| s | None | `sxxyy[CR]` | Custom bit-rate via BTR0/BTR1 registers |
| O | Full | `O[CR]` | Open CAN channel (normal mode - TX/RX) |
| L | Full | `L[CR]` | Open CAN channel (listen-only mode - RX only) |
| C | Full | `C[CR]` | Close CAN channel |
| t | Full | `tiiildd...[CR]` | Transmit standard 11-bit CAN frame |
| T | Full | `Tiiiiiiiildd...[CR]` | Transmit extended 29-bit CAN frame |
| r | Full | `riiil[CR]` | Transmit standard 11-bit RTR frame |
| R | Full | `Riiiiiiiil[CR]` | Transmit extended 29-bit RTR frame |
| P | Full | `P[CR]` | Poll single frame from RX buffer |
| A | Full | `A[CR]` | Poll all pending frames from RX buffer |
| F | Full | `F[CR]` | Read status flags (returns `Fnxx` bitmap) |
| X | Full | `Xn[CR]` or `X[CR]` | Auto-poll mode (X0=off, X1=on) or query; uses a 64-frame circular RX buffer |
| W | None | `Wn[CR]` | Hardware filter mode (dual/single) |
| M | None | `Mxxxxxxxx[CR]` | Acceptance code register (MCP2515 not wired up) |
| m | None | `mxxxxxxxx[CR]` | Acceptance mask register (MCP2515 not wired up) |
| U | Limited | `Un[CR]` or `U[CR]` | Set/query UART baud rate (n=0-7); 115200 baud recommended |
| V/v | Full | `V[CR]` or `v[CR]` | Get firmware version (V1013) |
| N | Full | `N[CR]` | Get serial number (NA123) |
| Z | Full | `Zn[CR]` or `Z[CR]` | Timestamp mode (Z0=off, Z1=on) or query; persists to EEPROM |
| Q | Full | `Qn[CR]` or `Q[CR]` | Auto-start mode (Q0, Q1, Q2) or query; persists to EEPROM |
| i | Custom | `i[CR]` | Diagnostic snapshot: `iCCCCRRRRTTTTUUUUddddooooFFD` (hex counters + FPS + debug mode). Requires CAN channel open. |
| @ | Custom | `@DBGn[CR]` | Runtime debug toggle (0=off, 1=on); requires compile-time ENABLE_CAN_DEBUG_LOGGING=1 |

### Power-on behavior
- CAN channel: closed by default
- Bit-rate: must be set with `Sn` before opening the channel
- Auto-start: disabled by default (enable with `Q1` or `Q2` to auto-open on boot)
- Serial port: available immediately at 115200 baud
- Timestamp: off by default (enable with `Z1`)

### Extras
- EEPROM settings include corruption recovery and backward compatibility.
- Regression harness in `tests/slcan_smoke.py` covers 12 LAWICEL commands; SavvyCAN and real CAN bus hardware were used during testing.
- RX pipeline uses a shared circular buffer so `P`, `A`, and `X` read from the same queue without dropping bursts (`src/can-232.h`).
- Strict serial parser validates LAWICEL command formatting before touching the MCP2515.
- Bus load telemetry and optional LCD support expose frames-per-second data via `g_canStats`.
- Runtime debug logging can be enabled with `@DBG1[CR]` (requires `ENABLE_CAN_DEBUG_LOGGING=1` at compile time) for troubleshooting CAN bus issues.

### GVRET (experimental)
- Send `@GVRET[CR]` to swap from LAWICEL to the GVRET-compatible binary menu (or build with `-DLW232_DEFAULT_PROTOCOL_MODE=LW232_PROTOCOL_GVRET` to boot directly into it).
- SavvyCAN handshake supported: `0xE7 0xE7` enters binary, then `0xF1` command stream (device info `0x07`, bus params `0x06`, validation `0x09`, time sync `0x01`, bus count `0x0C/0x0D`).
- Bus setup command `0x05` consumes CAN0 baud/enable/listen bits, opens the bus when flagged, and reports back through `0x06` (single bus only). Unsupported buses (CAN1, SWCAN, LIN) are reported as disabled.
- RX frames stream as `[0xF1][0x00][timestamp_us 4][id|ext_bit][len|bus<<4][data...]` with timestamps in microseconds and bus fixed to 0.
- Unsupported on this hardware: digital outputs, single-wire, flow control, and hardware filters/masks (host may probe but nothing is applied).

## PC Software

- Windows: SavvyCAN (recommended), CANHacker v2.00.01 ([archived copy](https://github.com/latonita/arduino-canbus-monitor/raw/master/CANHackerV2.00.01.exe)), and CAN-COOL (select RS232 + SL-CAN protocol).
- Linux: use SLCAN/SocketCAN with the `can-utils` package (see instructions below).

## Linux SLCAN instructions
### Prerequisites
Install `can-utils` first. On Ubuntu and other Debian-based distros:
```
sudo apt install can-utils
```
For other distros please follow respective instructions starting from https://github.com/linux-can/can-utils

### Create CAN device
```
sudo slcan_attach -f -s4 -o /dev/ttyUSB0
sudo slcand -S 115200 /dev/ttyUSB0 can0  
sudo ifconfig can0 up
```
where 115200 is port speed, `/dev/ttyUSB0` is the Arduino device path (adjust as needed).

### To dump traffic 
```
candump can0
```

### To delete CAN device
```
sudo ifconfig can0 down
sudo killall slcand
```

## Additional Documentation

- `WSL_USB_SETUP.md` - detailed WSL2 USB device setup guide
- `ENHANCEMENT_RECOMMENDATIONS.md` - future enhancement ideas and performance improvements
- `GIT_CHEATSHEET.md` - Git workflow reference for contributors
- `tests/README.md` - regression test suite documentation
- `legacy/README.md` - archived LCD diagnostics module (parallel HD44780) for drop-in use

## Credits and License

Original Author: Anton Viktorov <latonita@yandex.ru>  
Repository: https://github.com/latonita/arduino-canbus-monitor

This project uses the CAN BUS library from [Seeed-Studio/CAN_BUS_Shield](https://github.com/Seeed-Studio/CAN_BUS_Shield).

Licensed under The MIT License. See `LICENSE` for details.

Support the original author: [![Donate](https://www.paypal.com/en_US/i/btn/btn_donate_LG.gif)](https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=4JPDVHYWUY3LW)
