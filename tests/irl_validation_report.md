# IRL Validation Report (Current Command Set)

**Date:** November 22, 2025  
**Device:** Arduino UNO with MCP2515 CAN shield  
**Serial Port:** /dev/ttyACM0  
**Baud Rate:** 115200  
**Firmware Version:** V1013  
**Serial Number:** NA666

## Executive Summary
- Core LAWICEL commands validated on hardware: `S`, `O`, `L`, `C`, `t/T/r/R`, `F`, `X`, `W`, `M`, `m`, `Z`, `Q`, `U`, `V/v`, `N`, `i`, `@DBGn`.
- Autopoll streaming (`X1`) and timestamping (`Z1`) operate reliably with continuous bus traffic.
- Runtime stats (`i`) reflect RX/TX activity and debug flag status; EEPROM-backed settings (`Z`, `Q`) persist across resets.

## Test Environment
- Arduino UNO R3 + MCP2515 CAN shield on a 125 kbps live bus
- Host: Linux, pyserial for manual commands
- Serial: `/dev/ttyACM0` @ 115200 baud

## Highlights
- **Bitrate and channel control:** `S4` followed by `O`/`L` opens cleanly; `C` closes without side effects.
- **Frame TX/RX:** Standard and extended frames increment TX counters; listen mode blocks TX as expected.
- **Autopoll streaming:** With `X1` set prior to opening, frames stream continuously; disabling with `X0` silences output after close.
- **Timestamps:** `Z1` appends timestamp suffixes; `Z0` removes them. Preference persists after reset.
- **Runtime stats:** `i` snapshots reflect command/RX/TX counters, uptime, drop/overflow counts, and debug flag.
- **Debug toggle:** `@DBG1`/`@DBG0` flip runtime debug output when built with debug logging enabled.
- **Filters:** `W`, `M`, and `m` round-trip while the channel is closed; values persist as expected.
- **Autostart:** `Q1` auto-opens the bus on reset; `Q0` disables it. `Q2` preserves listen-only auto-open.

## Open Items
- Monitor RX overflow behavior under extreme bus load; consider deeper buffers on larger MCUs.
- Keep UART/bitrate change latency under observation when moving between host baud settings.
