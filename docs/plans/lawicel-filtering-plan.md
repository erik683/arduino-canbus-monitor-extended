# Lawicel/slcan Hardware Filtering Plan (MCP2515)

Scope: Implement everything the MCP2515 can do for RX filtering that is safe under Lawicel/slcan framing and won’t upset SavvyCAN in “LAWICEL / slcan Serial Mode.” No frame format changes; only command semantics W/M/m and internal behavior.

- **Honor both masks + six filters**: Map ACn/AMn bytes (Lawicel M/m) into RXM0/RXM1 and RXF0-5 with correct std/ext decoding; keep W0/W1 selecting dual vs single mask usage.
- **Rollover predictability**: Keep RXB0 rollover disabled (Lawicel expectation) and set it explicitly so filtering remains deterministic.
- **Apply on controller init/mode switch**: Reprogram masks/filters after every `begin()` or mode change; keep W/M/m query paths working (echo current values).
- **Slcan-compliant errors**: Reject invalid arguments while channel open or bad payloads with the standard `BEL` error (no custom framing).
- **Debug-only visibility**: Add a lightweight debug flag/log path to surface MCP2515 filter programming failures without altering serial protocol.

Verification checklist (for implementation phase):
- Flood test on hardware to confirm only allowed IDs pass; ensure serial framing unchanged.
- Run `tests/slcan_smoke.py` (Lawicel mode) to guard regressions.
- Quick SavvyCAN Lawicel-mode session to confirm stability and auto-poll behavior with filters applied.
