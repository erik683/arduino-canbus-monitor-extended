# GVRET Filtering/Control Plan (MCP2515)

Goal: Capture filtering and related MCP2515 capabilities that are better surfaced via GVRET (binary) without risking Lawicel/slcan compatibility.

Planned GVRET-facing features:
- **Filter routing + rollover control**: Expose RXB0/RXB1 CTRL options (rollover enable, routing bias standard vs extended) via GVRET commands with explicit ACK/NAK.
- **Wake-on-filter + power modes**: Toggle WAKFIL and CANINTE bits, sleep/clock-out controls, and tie wake behavior to filter state.
- **RTR-selective handling**: Allow filters/masks or a toggle to drop/keep RTR frames separately from data frames.
- **TX safety helpers**: One-shot TX, abort-in-flight commands, and TX priority tweaks that interact with filtering use-cases (e.g., quiet sniffing).
- **Custom bit timing + presets**: GVRET command(s) for BTR0/BTR1 plus named presets that also set filter defaults (e.g., “accept only ID set X”).
- **Error/diag surfacing**: Return MCP2515 error counters, arbitration lost capture, and error capture registers through GVRET responses.

Validation/interop expectations:
- Keep Lawicel mode unchanged; GVRET commands gated to GVRET protocol sessions.
- Ensure SavvyCAN GVRET mode recognizes/ignores new command IDs safely; document any extensions.
- Hardware tests: confirm filter routing/rollover behavior under load and sleep/wake scenarios; regression run of existing GVRET logging.
