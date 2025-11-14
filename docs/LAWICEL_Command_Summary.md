# LAWICEL Command Summary

This document provides a comprehensive summary of LAWICEL SLCAN protocol commands implemented in the arduino-canbus-monitor project.

See protocol definition here http://www.can232.com/docs/can232_v3.pdf and here http://www.can232.com/docs/canusb_manual.pdf

## Command Support Status

Commands not supported/not implemented:
- s, W, M, m, U.

Commands modified:
- S - supports not declared 83.3 rate
- U - supports `U[CR]` query and only reconfigures the UART after acknowledging so the host can change baud
- Z - supports runtime `Z[CR]` query for the standard 2-byte (60s) timestamp toggle and stores the choice in EEPROM per LAWICEL docs
- Power-on state matches LAWICEL adapters (bus closed until `Sn` + `O`/`L`).

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
| 'U' | YES         | Un[CR] / U[CR]       | Setup UART with a new baud rate where n is 0-6. `U` without argument reports the current selection and the firmware switches baud only after acknowledging. |
| 'V' | YES         | v[CR]                | Get Version number of both CAN232 hardware and software |
| 'v' | YES         | V[CR]                | Get Version number of both CAN232 hardware and software |
| 'N' | YES         | N[CR]                | Get Serial number of the CAN232. |
| 'Z' | YES         | Zn[CR] / Z[CR]       | Standard LAWICEL timestamp control (`Z1` on, `Z0` off). `Z` without argument reports the active mode and the setting is saved to EEPROM. |
| 'Q' | YES  todo   | Qn[CR]               | Auto Startup feature (from power on). |

## Power-On Defaults
- Starts with the CAN channel closed, requiring `O`/`L` to begin traffic.
- `Sn` must be issued after reset; without it `O`/`L` return BEL.
- Auto-startup remains off by default; serial responses are available immediately.
