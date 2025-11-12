# CANUSB Clone – Future Goals

Last reviewed: commit `c0b7e7496e3ea1ae57f17d018ede2aa51aaca9a5`

The notes below capture the CANUSB-specific protocol gaps and “nice to have” improvements that we can defer until after the CAN232 parity push. Each item references the sections in the official LAWICEL CANUSB manual found under `docs/`.

## High-Value Targets

1. **Lawicel-compliant status flags (`F` command)**  
   - Emit the documented `Fnxx` bitmap instead of forwarding MCP2515 `EFLG`.  
   - Preserve a debug hook (e.g., `#if ENABLE_CAN_DEBUG_LOGGING`) to print native MCP2515 faults without exposing them to the host.

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
