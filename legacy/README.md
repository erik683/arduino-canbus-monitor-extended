# Legacy LCD Options

The firmware ships with a lightweight I²C LCD implementation inside `src/lcd_display.*`.  
If you ever want to resurrect the original HD44780 (parallel) diagnostics display that
predated the current version, drop the `legacy/lcd_diagnostics.*` files back into `src/`
and include `lcd_diagnostics.h` from `arduino-canbus-monitor.ino`.

## When to use this archive

- **Parallel LCD shields** wired directly to D4–D7 plus RS/E pins instead of the I²C backpack.
- **Verbose diagnostics** that show CAN open/close events and incoming frames, instead of only
  the frames-per-second counter offered by the current `LcdDisplay`.
- **Bench testing** where the I²C hardware is unavailable but the HD44780 board is already soldered.

## Drop-in instructions

1. Copy the archived files into the active source tree:
   ```bash
   cp legacy/lcd_diagnostics.* src/
   ```
2. In `src/arduino-canbus-monitor.ino` add:
   ```cpp
   #include "lcd_diagnostics.h"
   ```
3. Initialize it just after `Serial.begin()`:
   ```cpp
   LcdDiagnostics::begin();
   LcdDiagnostics::showSplash();
   ```
4. Optionally invoke the other helpers (`showSerialReady`, `showCanReady`,
   `showFrame`, etc.) at the appropriate points in your workflow.

If you later switch back to the I²C LCD (or no LCD at all) just delete the copied files from `src/`
again. Keeping the drop-in copies here preserves the proven implementation without forcing it
into every build.
