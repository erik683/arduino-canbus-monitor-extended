# arduino-canbus-monitor [![Build Status](https://api.travis-ci.org/latonita/arduino-canbus-monitor.svg?branch=master)](https://travis-ci.org/latonita/arduino-canbus-monitor) [![Coverity Scan](https://scan.coverity.com/projects/11684/badge.svg)](https://scan.coverity.com/projects/latonita-arduino-canbus-monitor) [![Analytics](https://ga-beacon.appspot.com/UA-99380399-1/welcome-page)](https://github.com/igrigorik/ga-beacon)

CAN BUS monitoring software based on Arduino with Seeduino/ElecFreaks CAN BUS shield based on MCP2515 (Numerous other MCP2515 based CAN BUS modules from ebay and aliexpress work well to).

This software implements CAN ASCII / Serial CAN / SLCAN protocol compatible with Lawicel CAN232/CANUSB.

## PlatformIO Setup

This project is configured for [PlatformIO](https://platformio.org/), a professional development environment for embedded systems.

### Prerequisites
1. Install [PlatformIO IDE for VSCode](https://platformio.org/install/ide) or [PlatformIO CLI](https://docs.platformio.org/en/latest/core/installation.html)
2. Open this project in VS Code with PlatformIO extension

### Building and Uploading
```bash
# Build the project
pio run

# Upload to Arduino (requires device to be attached to WSL)
pio run --target upload

# Monitor serial output (115200 baud)
pio device monitor
```

### Arduino Setup (One-time setup required)
Your Arduino Uno is configured to appear as COM2 in Windows and `/dev/ttyACM0` in WSL.

#### Option 1: Automatic Setup (Recommended)
1. **Run the Windows setup script as Administrator:**
   ```cmd
   # In Windows Command Prompt (as Admin), navigate to project folder and run:
   setup-windows-task.bat
   ```
   This creates a scheduled task that automatically attaches your Arduino to WSL on Windows startup.

2. **For immediate use, run the attachment script:**
   ```cmd
   # In Windows Command Prompt (as Admin):
   attach-arduino.bat
   ```
   Or use PowerShell:
   ```powershell
   # In PowerShell (as Admin):
   .\attach-arduino.ps1
   ```

#### Option 2: Manual Setup
Each time you want to use the Arduino:
1. **In Windows PowerShell (as Administrator):**
   ```powershell
   usbipd attach -i 2341:0043 --wsl
   ```
2. **Verify in WSL:**
   ```bash
   ls -l /dev/ttyACM*
   ```

#### WSL Configuration
The WSL environment is pre-configured. If you get permission errors, run:
```bash
newgrp dialout
```

### Configuration
- **Board**: Arduino Uno (configurable in `platformio.ini`)
- **Libraries**: Seeed-Studio CAN_BUS_Shield (automatically downloaded)
- **Monitor**: 115200 baud rate

### Changing Boards
To use a different Arduino board, edit `platformio.ini`:
```ini
[env:mega2560]
platform = atmelavr
board = megaatmega2560
framework = arduino
```

### Editor IntelliSense (clangd)
The repository includes a `.clangd` file tailored for the Arduino Uno toolchain. Regenerate the compilation database whenever you tweak `platformio.ini`:

```bash
pio run -t compiledb
```

Open the folder in VS Code with the PlatformIO, clangd, Python, and GitLens extensions enabled (see `.vscode/extensions.json`). clangd will automatically consume the freshly generated `compile_commands.json` for accurate diagnostics.

### Regression Tests
Before flashing to a vehicle or sharing firmware, run the lightweight LAWICEL regression harness:

```bash
pip install --upgrade pyserial
python tests/slcan_smoke.py --port /dev/ttyACM0
```

The script auto-detects serial ports when possible and lives alongside usage details in `tests/README.md`. It verifies critical commands such as `S`, `O`, `C`, `Z`, and `F` so protocol regressions are caught early.

## PC Counterpart Software

As for PC counterpart software I personally used and can recommend two tools:

1) [Windows] CANHacker tool v.2.00.01 (by fuchs) to sniff and visualize data on the bus. You can download CANHacker tool from this forum page: http://www.canhack.net/viewforum.php?f=25&sid=ac01d465f19e088cb160cab630561607 (P.S. Looks like canhack.net no longer operating, here is a copy of installation file: [CANHackerV2.00.01.exe](https://github.com/latonita/arduino-canbus-monitor/raw/master/CANHackerV2.00.01.exe))

2) [Windows] CAN-COOL (by MHS Elektronik), open source, but unfortunaly available only in German. Download link: http://www.mhs-elektronik.de/index.php?module=content&action=show&page=can_cool  (Make sure you select RS232 and SL-CAN protocol and then click hardware bus reset icon on a toolbar)

3) [Linux] SLCAN/SocketCAN can be used https://github.com/linux-can/can-utils. See details in the end of this README file

This monitor uses CAN BUS library forked from https://github.com/Seeed-Studio/CAN_BUS_Shield.

Copyright (C) 2015,2016 Anton Viktorov <latonita@yandex.ru>

You can buy me a beer if you like the tool :o)   [![Donate](https://www.paypal.com/en_US/i/btn/btn_donate_LG.gif)](https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=4JPDVHYWUY3LW)


See protocol definition here http://www.can232.com/docs/can232_v3.pdf and here http://www.can232.com/docs/canusb_manual.pdf

Commands not supported/not implemented:  
- s, W, M, m, U.

Commands modified:
-  S - supports not declared 83.3 rate 
-  U - adds a `U[CR]` query and only retunes the UART after acknowledging so the host can switch baud cleanly
-  Z - adds a `Z[CR]` query to report the current 2-byte timestamp mode (LAWICEL-compatible 60s rollover) and persists the setting in EEPROM per LAWICEL spec
-  Power-on behavior mirrors a stock LAWICEL CAN232 (channel remains closed until the host issues `Sn` followed by `O`/`L`).
  
| CMD | IMPLEMENTED | SYNTAX               | DESCRIPTION |
|-----|-------------|----------------------|-------------|
| 'S' | YES+        | Sn[CR]               | Setup with standard CAN bit-rates where n is 0-8.<br>S0 10Kbit          S4 125Kbit         S8 1Mbit<br>S1 20Kbit          S5 250Kbit         S9 83.3Kbit<br>S2 50Kbit          S6 500Kbit<br>S3 100Kbit         S7 800Kbit |
| 's' | -           | sxxyy[CR]            | Setup with BTR0/BTR1 CAN bit-rates where xx and yy is a hex value. |
| 'O' | YES         | O[CR]                | Open the CAN channel in normal mode (sending & receiving). |
| 'L' | YES         | L[CR]                | Open the CAN channel in listen only mode (receiving). |
| 'C' | YES         | C[CR]                | Close the CAN channel. |
| 't' | YES         | tiiildd...[CR]       | Transmit a standard (11bit) CAN frame. |
| 'T' | YES         | Tiiiiiiiildd...[CR]  | Transmit an extended (29bit) CAN frame |
| 'r' | YES         | riiil[CR]            | Transmit an standard RTR (11bit) CAN frame. |
| 'R' | YES         | Riiiiiiiil[CR]       | Transmit an extended RTR (29bit) CAN frame. |
| 'P' | YES         | P[CR]                | Poll incomming FIFO for CAN frames (single poll) |
| 'A' | YES         | A[CR]                | Polls incomming FIFO for CAN frames (all pending frames) |
| 'F' | YES         | F[CR]                | Read Status Flags (LAWICEL `Fnxx` bitmap). |
| 'X' | YES         | Xn[CR]               | Sets Auto Poll/Send ON/OFF for received frames. |
| 'W' | -           | Wn[CR]               | Filter mode setting. By default CAN232 works in dual filter mode (0) and is backwards compatible with previous CAN232 versions. |
| 'M' | -           | Mxxxxxxxx[CR]        | Sets Acceptance Code Register (ACn Register of SJA1000). // we use MCP2515, not supported |
| 'm' | -           | mxxxxxxxx[CR]        | Sets Acceptance Mask Register (AMn Register of SJA1000). // we use MCP2515, not supported |
| 'U' | YES         | Un[CR] / U[CR]       | Setup UART with a new baud rate (0–6). Bare `U` reports the current slot, and the firmware reinitializes Serial after acknowledging so the host can retune. |
| 'V' | YES         | v[CR]                | Get Version number of both CAN232 hardware and software |
| 'v' | YES         | V[CR]                | Get Version number of both CAN232 hardware and software |
| 'N' | YES         | N[CR]                | Get Serial number of the CAN232. |
| 'Z' | YES         | Zn[CR] / Z[CR]       | Toggle LAWICEL-style 2 byte timestamps (`Z1` on, `Z0` off). Bare `Z` reports the active mode, and the choice is saved in EEPROM like the original CANUSB. |
| 'Q' | YES  todo   | Qn[CR]               | Auto Startup feature (from power on). |

### Power-On Defaults
- CAN channel starts closed and stays that way until the host explicitly opens it with `O` (normal) or `L` (listen-only).
- Every reset requires a fresh bitrate selection via `Sn`; `O`/`L` return BEL until a valid bitrate is chosen.
- Auto-startup is disabled, matching LAWICEL units (no unsolicited bus activity).
- The serial port is immediately available, so identification commands (`V`, `N`, etc.) work before the CAN side is configured.

## Linux SLCAN instructions
### Prerequisites
Install `can-utils` first. 
On Ubuntu and other Debian-based distros `can-utils` package is included into standard repositories:
```
sudo apt install can-utils
```
For other distros please follow respective instructions, start from here https://github.com/linux-can/can-utils

### Create CAN device
```
sudo slcan_attach -f -s4 -o /dev/ttyUSB0
sudo slcand -S 115200 /dev/ttyUSB0 can0  
sudo ifconfig can0 up
```
where 115200 is port speed, `/dev/ttyUSB0` - name of your arduino device. can be different 

### To dump running traffic 
```
candump can0
```

### To delete CAN device
```
sudo ifconfig can0 down
sudo killall slcand
```
