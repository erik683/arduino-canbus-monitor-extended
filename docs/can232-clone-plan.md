# CAN232 Clone Implementation Plan

Author: Codex (self-notes)
Last updated: `a3e6a493c430eff82fbd4c6f8be7c917954c470d`

## Repository Snapshot (pre-work)

````text
HEAD: a3e6a493c430eff82fbd4c6f8be7c917954c470d

git status -sb:
## master...origin/master [ahead 2]
 M .gitignore
 M .vscode/extensions.json
 M .vscode/settings.json
 M README.md
 D docs/README.md
 D docs/overview.md
 M platformio.ini
 M src/arduino-canbus-monitor.ino
 M src/can-232.cpp
?? .cache/
?? .clang-format
?? .clangd
?? .cursorignore
?? docs/LAWICEL_CAN232_Manual.pdf
?? docs/LAWICEL_CANUSB_Manual.pdf
?? docs/MCP2515-CAN-Controller-with-SPI-20001801J.pdf
?? docs/slcan-command-status.md
?? src/runtime_stats.cpp
?? src/runtime_stats.h
?? tests/
````

This snapshot is the baseline for all CAN232 parity work. No files should be restored/cleaned without consulting the user.

## Objective

Make the Arduino-based monitor indistinguishable from an official LAWICEL CAN232 to any host software by implementing the full v1.3 protocol (commands, responses, timing rules, and persistence semantics) while retaining our MCP2515 hardware backend.

## Constraints & Assumptions

- Hardware: Arduino Uno + MCP2515 shield, 16 MHz oscillator (per `platformio.ini`).  
- Protocol references: `docs/LAWICEL_CAN232_Manual.pdf` (primary), MCP2515 datasheet `docs/MCP2515-CAN-Controller-with-SPI-20001801J.pdf`.  
- Existing deviations (`README.md` table) must disappear or be re-gated behind opt-in defines.  
- `tests/slcan_smoke.py` is our fast regression harness; extend it instead of crafting ad-hoc test sketches.  
- EEPROM writes should be minimized (wear) → cache the last stored values and only write on change.

## Work Breakdown

1. **Housekeeping & Shared Utilities**  
   - Extract helper functions for EEPROM read/write (`src/runtime_stats.{h,cpp}` already exists—reuse or introduce `settings_store.*`).  
   - Add a `LawicelStatus` struct (bitfields) to centralize flag composition; keep it in `src/can-232.cpp` near exec path.

2. **Canonical Metadata Responses (`V`, `v`, `N`)**  
   - Mirror CAN232 format (`V0103` style). Consider deriving firmware rev from `platformio.ini` version define.  
   - Store serial number in EEPROM (default `NA000`). Provide compile-time override for developers.  
   - Update smoke test to assert prefix + length.

3. **`F` Command Rework**  
   - Map MCP2515 registers (`EFLG`, `TEC`, `REC`) into Lawicel bits per manual §5.4.  
   - Provide fallback/extended info via optional `F1` (if supported).  
   - Unit-test conversion logic using host-side helper invoked via `pio test` or simple desktop harness.

4. **`Qn` Auto-Start Persistence** – *Partial*
   - Enforce "only when CAN closed" rule; respond with current mode when invoked without parameter (per manual). ✓
   - Store mode + desired CAN speed + timestamp setting in EEPROM block. ❌ (Logic implemented, EEPROM persistence pending)
   - On boot: read block, validate checksum, automatically issue `O`/`L` command path if requested. ❌
   - Extend smoke test: set mode, reset board (toggle DTR), ensure it reopens channel. ❌

5. **Filter Mode Toggle (`Wn`)** – *Not Implemented*
   - `W0`: dual filter, `W1`: single filter (common mask). Translate to MCP2515 `RXBnCTRL`, `RXFnX`, `RXMnX`.
   - Provide readback on bare `W` (Lawicel allows).
   - Validate that command is rejected while CAN open. Add tests using loopback frames to confirm filtering difference.

6. **Acceptance Code/Mask (`Mxxxxxxxx`, `mxxxxxxxx`)** – *Not Implemented*
   - Parse eight hex nibbles → 32-bit value, align with MCP2515 register order (SIDH/SIDL/EID8/EID0).
   - Support both STD (11-bit) and EXT (29-bit) usage; manual describes expectation (std bits left-justified).
   - Restrict to CAN-closed state; write to both RXF0/RXF1 (and optionally RXF2/3 for completeness).
   - Provide friendly error message (`BEL`) when channel open or hex malformed.

7. **Custom Bit Timing (`sxxyy`)** – *Not Implemented*
   - Accept two bytes (`xx` = BTR0, `yy` = BTR1). Translate to MCP2515 CNF1/2/3 (SJW, BRP, PHSEG).
   - Validate BRP within MCP2515 limits, ensure sample point between 50–90 %.
   - Lawicel expects this to implicitly close channel, program timing, then wait for `O`.
   - Add regression test: close channel, send `s031C` (125 kbps example), reopen, confirm `S4` blocked until close.

8. **Timestamp Consistency (`Z`)** ✅  
   - Firmware now sticks to the LAWICEL 2-byte/60s timer, supports the bare `Z` query, and persists the selection in EEPROM just like the CANUSB reference devices.  

9. **Testing & Validation**  
   - Expand `tests/slcan_smoke.py` to cover each implemented command (subtests for error cases).  
   - Add hardware-in-the-loop sanity checklist (manual steps) to repo docs.  
   - Before flashing, run `pio run`, `pio device monitor`, then `python tests/slcan_smoke.py --port ...`.

10. **Documentation & Release Notes**  
    - Update `README.md` command table (all entries “YES”).  
    - Summarize EEPROM behavior + new config options.  
    - Tag repo (e.g., `v1.0-can232-clone`) once verified.

## Risks & Mitigations

| Risk | Impact | Mitigation |
| --- | --- | --- |
| Incorrect MCP2515 register mapping for masks | Filters silently fail | Build small desktop tool to send known frames and verify acceptance before merging |
| EEPROM wear due to repeated `Q/M/m/Z` writes | Reduced MCU lifespan | Only write when value changes; gate behind `LW232_EEPROM_DIRTY` flag |
| Custom bit-timing accepts invalid params | Bus lockup | Pre-validate using MCP2515 formulae; fall back to `BEL` |
| Smoke test flakiness due to board reset timing | CI noise | Add generous sleeps + retries around auto-start tests |

## Next Steps

1. **Complete `Q` command EEPROM persistence** (highest priority - currently partial)
2. **Implement `W` filter mode toggle** (medium priority)
3. **Implement `M`/`m` acceptance code/mask** (medium priority)
4. **Implement `s` custom bit timing** (medium priority)
5. **Expand test coverage** for implemented commands
6. Keep CANUSB backlog (`docs/canusb-future-goals.md`) updated as new insights surface.
