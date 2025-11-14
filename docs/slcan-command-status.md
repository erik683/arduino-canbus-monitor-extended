# SLCAN / LAWICEL Command Coverage

This matrix captures the current implementation status of every command defined in the LAWICEL CAN232 v1.3 / SLCAN protocol. It is derived from the constants in `src/can-232.h:33` and the handlers in `src/can-232.cpp`.

| Command | Description | Status | Notes |
|---------|-------------|--------|-------|
| `S` | Set standard bitrate (Sn) | **Implemented** | Accepts the predefined CAN rates via `lw232CanBaudRates`. Only works when the channel is closed and must be issued after reset before `O`/`L` succeed. |
| `s` | Set bitrate via raw BTR registers | **Not Implemented** | Command immediately returns error (see `parseAndRunCommand` case `LW232_CMD_SETUP_BTR`). |
| `O` | Open CAN channel (normal mode) | **Implemented** | Calls `openCanBus()` then transitions to `LW232_STATUS_CAN_OPEN_NORMAL`. Duplicate opens are idempotent. |
| `L` | Open CAN channel (listen-only) | **Implemented** | Uses the same `openCanBus()` path but ends in `LW232_STATUS_CAN_OPEN_LISTEN`. |
| `C` | Close CAN channel | **Implemented** | Marks the channel closed and now triggers `LcdDiagnostics::showCanClosed()` when the LCD feature is compiled in. |
| `t` | Transmit 11-bit data frame | **Implemented** | Parses frame payload, sends via `sendMsgBuf`, optional autopoll shortcut. |
| `T` | Transmit 29-bit data frame | **Implemented** | Fully supported; autopoll returns `Z` for large frame acknowledgement. |
| `r` | Transmit 11-bit RTR frame | **Implemented** | Sends remote request frame; autopoll path mirrors 29-bit variant. |
| `R` | Transmit 29-bit RTR frame | **Implemented** | Present; returns lowercase `z` when autopoll is enabled (per historical behaviour). |
| `P` | Poll single received frame | **Implemented** | Only operates when the channel is open and autopoll is disabled. |
| `A` | Poll all pending frames | **Implemented** | Same constraints as `P`, loops until FIFO drains. |
| `F` | Read status flags | **Implemented** | Emits the LAWICEL `Fnxx` bitmap (RX/TX queue full, EI, DOI, EPI, ALI, BEI) and rejects queries while the CAN channel is closed. |
| `X` | Toggle autopoll | **Implemented** | `X1` enables continuous polling, `X0` restores manual mode. |
| `W` | Filter mode | **Not Implemented** | Always returns error. |
| `M` | Acceptance code | **Not Implemented** | Placeholder only; MCP2515 hardware filtering is not wired up yet. |
| `m` | Acceptance mask | **Not Implemented** | Same as above. |
| `U` | Change UART baud | **Implemented** | Accepts range 0–6, supports `U` queries, and reinitializes Serial after acknowledging so hosts can retune to the new speed. |
| `V` / `v` | Firmware version | **Implemented** | Returns `LW232_LAWICEL_VERSION_STR`. |
| `N` | Serial number query | **Implemented** | Responds with `LW232_LAWICEL_SERIAL_NUM`. |
| `Z` | Timestamp control | **Implemented** | Matches LAWICEL behaviour: `Z1` enables the 2-byte/60s timestamp, `Z0` disables it, bare `Z` reports the current mode even while CAN is open, and the setting is persisted in EEPROM. |
| `Q` | Auto-startup | **Not Implemented** | Placeholder comment indicates future non-volatile support. |

## Gaps and Follow-Ups
1. **Hardware Filtering (`W/M/m`)** – A planned enhancement for Phase 3. We need MCP2515 register writes (after closing the channel) to honor LAWICEL expectations.
2. **Raw Bitrate (`s`)** – Could map to `MCP_CAN::begin(mask, freq)` API for advanced users who require custom timing.
3. **Autostart (`Q`)** – Requires a persistent configuration store (EEPROM) plus boot-time logic. Currently returns error.
4. **UART Speed (`U`)** – Verify that the current implementation genuinely reconfigures the `Serial` port before advertising full support.

This document should be updated whenever we add or change command behaviour so that downstream consumers know what to expect.
