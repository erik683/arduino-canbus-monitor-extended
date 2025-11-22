# CODEMAP

## Hidden configuration & metadata
- `.clang-format`
  - Arduino-specific clang-format profile (LLVM base) that fixes indent width to 2 spaces, enforces include ordering, and caps columns at 100 so code stays compact on embedded editors.
- `.clangd`
  - Explicit compile flags for clangd/clang-tidy with Arduino macro defines, include paths for PlatformIO cores, and reference to the local `compile_commands.json` database.
- `.cursorignore`
  - Empty sentinel file; Cursor respects it when expanding the workspace (usually used to skip generated files when populated).
- `.editorconfig`
  - Enforces UTF-8, LF line endings, and space-based indentation rules for C/C++/Arduino sources plus relaxed Markdown settings.
- `.gitignore`
  - Keeps PlatformIO build caches (`.pio/`, `.piolibdeps/`, `.pio-home/`), IDE/OS artifacts, and `testing_export/` out of Git along with standard binary/object files.
- `.travis.yml`
  - CI pipeline that sets up Xvfb, installs Arduino 1.6.5, and verifies `arduino-canbus-monitor.ino` with the Arduino CLI (it can be extended with `sonar-scanner` if uncommented).
- `.snapshots/`
  - PlatformIO snapshot metadata (config/readme/sponsors) used by the documentation/site tooling; contents are referenced instead of listing each file.
- `.vscode/`
  - Recommended VS Code workspace settings and extension list tailored for PlatformIO+Arduino workflows; this folder contains `settings.json` and `extensions.json`.
- `compile_commands.json`
  - PlatformIO-generated compilation database (used by clangd) listings the build commands for each source file when targeting the Uno environment.

## Root documentation & licensing
- `README.md`
  - Project overview, hardware requirements, PlatformIO build/flash instructions, LAWICEL protocol status table, regression test instructions, and links to USB/WSL documentation and credits.
- `LICENSE`
  - The MIT license governing this repository (includes attribution, grant of rights, and warranty disclaimer).
- `CAN_BUS_Shield License.txt`
  - Third-party license text for the Seeed Studio CAN_BUS_Shield library that the firmware relies on.
- `WSL_USB_SETUP.md`
  - Step-by-step WSL2 USB device setup guidance covering `usbipd`, COM port bridging, group permissions, and debugging tips for Windows + WSL.

## Windows/WSL helper scripts
- `ACTIVATE-USB-TASK.bat`
  - Simple batch wrapper that launches the VBScript version of the USB attach task (used by Task Scheduler to attach the Arduino at login).
- `ACTIVATE-USB-TASK.vbs`
  - VBScript shim invoked by scheduled tasks to invisibly run `attach-arduino.bat/ps1` with elevated permissions.
- `attach-arduino.bat`
  - Batch script that auto-attaches the Arduino Uno to WSL via `usbipd attach` while echoing helpful diagnostics and requiring administrator rights.
- `attach-arduino.ps1`
  - PowerShell version with options to list/detach devices, auto-detect based on VID/PID, and attach the Arduino; suitable for scheduled tasks or manual invocation.
- `create-usb-task.ps1`
  - Creates a scheduled task that runs `usbipd attach --busid 1-8 --wsl` at logon with a 30-second delay, ensuring Windows reattaches the Arduino automatically.
- `setup-usb-task.bat`
  - Elevation wrapper that runs `setup-usb-task.ps1` inside PowerShell so the Task Scheduler creation process receives admin rights.
- `setup-usb-task.ps1`
  - Interactive PowerShell script that checks for `usbipd`, prompts for a BUSID if needed, and registers/resets the task (`WSL-Attach-USB-<bus>`) that auto-attaches the Arduino to WSL.
- `setup-windows-task.bat`
  - Prompts the user and uses `schtasks` to register a high-privilege Task Scheduler entry (`Arduino WSL Attach`) that calls `attach-arduino.ps1` at every Windows startup.
- `setup-wsl-arduino.sh`
  - Linux shell helper that adds the user to `dialout`, installs USB/IP tools, ensures PlatformIO is on the PATH, and prints steps for using the Arduino from WSL.
- `update-usb-task.ps1`
  - Updates an existing `WSL-Attach-USB-1-8` scheduled task to the newer `usbipd attach --busid … --wsl` syntax and prints the new action for verification.

## Build & analysis configuration
- `platformio.ini`
  - Primary PlatformIO configuration: Uno default env, 32-frame circular buffer build flag, monitor/upload ports, and a Mega env with identical debug settings; enforces `debug` builds.
- `sonar-project.properties`
  - SonarQube metadata (project key/name/version) pointing sources to the `arduino-canbus-monitor` directory for static analysis.

## Documentation files (`docs/`)
- `docs/_config.yml`
  - Minimal Jekyll config enabling the `jekyll-theme-minimal` theme when rendering the `docs/` site.
- `docs/ARDUINO_UNO_R3.md`
  - Detailed Arduino Uno R3 hardware summary (ATmega328P specs, pinouts, power connectors, interfaces, and reference links).
- `docs/MCP2515_SHIELD.md`
  - Inland/Keyestudio MCP2515 shield datasheet with CAN controller/transceiver specs, pin mappings, wiring guidance, and sample Seeed-studio usage snippet.
- `docs/MCP_SETUP.md`
  - MCP configuration guide describing how Cursor/Codex can expose the local docs folder via `mcp.json` plus pointers to the Arduino/MCP references.
- `docs/MCP2515-CAN-Controller-with-SPI-20001801J.pdf`
  - Microchip MCP2515 stand-alone CAN controller datasheet (SPI commands, register map, timing formulas) used for low-level driver development.
- `docs/LAWICEL_CAN232_Manual.pdf`
  - Lawicel CAN232 v3 manual detailing the ASCII-command protocol, command reference, and RS232 pinouts that the firmware emulates.
- `docs/LAWICEL_CANUSB_Manual.pdf`
  - Lawicel CANUSB v1.3 manual (similar command set) that validates the LAWICEL compliance claims and protocol timing.

## Planning files (`docs/plans/`)
- `docs/plans/ENHANCEMENT_RECOMMENDATIONS.md`
  - Prioritized backlog of reliability, telemetry, and diagnostics improvements (watchdog, logging, filtering, etc.); README now highlights the work already completed so this document can focus on remaining work.
- `docs/plans/lawicel-filtering-plan.md`
  - Draft thinking around LAWICEL filtering configuration; moved here for consistency, content unchanged.

## Source code (`src/`)
- `src/arduino-canbus-monitor.ino`
  - Entry point that initializes serial/CAN, registers the MCP interrupt handler, applies optional custom filters, and forwards `loop`/`serialEvent` to `Can232`.
- `src/can-232.h`
  - Declaration of the `Can232` singleton, LAWICEL command constants, circular RX buffer configuration, EEPROM keys, helper `HexHelper`, and firmware configuration macros.
- `src/can-232.cpp`
  - Implementation of the LAWICEL state machines, buffering, filtering, EEPROM persistence, diagnostic `i` command, auto-polling, and telemetry exposed to `g_canStats`.
- `src/mcp_can.cpp`
  - Seeed Studio MCP_CAN driver: SPI helpers, CAN controller initialization, message I/O, error checking, buffer management, and hardware filter setup.
- `src/mcp_can.h`
  - Declares the `MCP_CAN` class interface (begin/send/read routines, status helpers, mode setters) used by `Can232`.
- `src/mcp_can_dfs.h`
  - Register addresses, bit masks, SPI opcode definitions, baud-rate constants, and return codes for the MCP2515 that back the low-level driver.
- `src/runtime_stats.cpp`
  - Thread-safe implementation of runtime metrics (command/Rx/Tx counts, drops, overflows, timestamps, FPS) consumed by the diagnostics command and telemetry hooks.
- `src/runtime_stats.h`
  - Definition of the `CanRuntimeStats` struct and API (`statsReset`, `statsRecord*`) shared between diagnostics and the `Can232` logic.

## Legacy & support
- `legacy/README.md`
  - Notes on the legacy HD44780 LCD diagnostics module that can be drop-in replaced if required; kept for historical reference.
- `legacy/lcd_diagnostics.cpp`
  - Legacy LCD driver for showing bus stats on a 16×2 display (HD44780/PCF8574) retained for optional diagnostics.
- `legacy/lcd_diagnostics.h`
  - Header for the legacy LCD diagnostics helper (initialization, message formatting, and update helpers).

## Patches & extras
- `patches/arduino-mega-reference.patch`
  - Patch file referencing Mega2560-specific changes (likely updates to PlatformIO config and references) for maintaining compatibility with larger boards.

## Tests
- `tests/README.md`
  - Describes the LAWICEL regression harness, dependencies (`pyserial`), and steps for running `tests/slcan_smoke.py`.
- `tests/slcan_smoke.py`
  - Python script that exercises 12 core LAWICEL commands over a serial port, validating compliance before flashing to production hardware.

