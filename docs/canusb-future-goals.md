# CANUSB Clone – Future Goals

Last reviewed: commit `a3e6a493c430eff82fbd4c6f8be7c917954c470d`

The notes below capture the CANUSB-specific protocol gaps and “nice to have” improvements that we can defer until after the CAN232 parity push. Each item references the sections in the official LAWICEL CANUSB manual found under `docs/`.

## High-Value Targets

1. **Lawicel-compliant status flags (`F` command)** – *Complete*  
   - `F` now emits the documented `Fnxx` bitmap and enforces the “channel open” requirement from the CANUSB manual.  
   - Still preserve a debug hook (e.g., `#if ENABLE_CAN_DEBUG_LOGGING`) for raw MCP2515 dumps without surfacing them over LAWICEL responses.  
   - Follow-up: implement optional `F1` extended diagnostics once we spec the payload.

2. **Device identity parity (`V/v/N` commands)**  
   - Mirror the CANUSB version string format, including hardware/firmware rev pairs.  
   - Surface a configurable or EEPROM-backed serial number so multiple adapters enumerate distinctly in PC tools.  
   - Optional stretch goal: expose the USB descriptor strings lawicel tools probe for (even if we remain CDC-ACM underneath).

3. **Auto-start profile (`Qn`)**  
   - Implement persistent storage for the power-on behavior (Normal/ListEN).  
   - Match the manual’s rule set (only allowed while CAN channel closed, echoed on next `Q` query).

4. **Filter/mask programming (`W`, `M`, `m`)**  
   - Translate Lawicel’s dual/single filter terminology to MCP2515 RXF/RXM registers.  
   - Validate extended vs standard mask applicability and document any hardware limitations (e.g., RXB1 mask mirroring).  
   - Provide a `W?` readback helper so host utilities can confirm mode.

5. **Custom bit timing (`s` command)**  
   - Accept raw BTR0/BTR1 bytes, verify them against MCP2515 timing constraints, and program CNF1/2/3 accordingly.  
   - Return `BEL` for invalid combos the controller cannot synthesize, as CANUSB does.

6. **Advanced host niceties**  
   - Honour the CANUSB auto-poll/auto-send timing quirk (immediate `z`/`Z` echoes).  
   - Support the undocumented but widely used `F1` “extended status” query (see CANUSB manual addendum).  
   - Capture statistics (Rx/Tx counters, overrun totals) so diagnostics panes in CANUSB-aware tools populate correctly.

These goals can sit on the backlog until we finish the CAN232 parity work and stabilize the test harness. The intent is to keep them visible so we do not lose track once the immediate milestone ships.
