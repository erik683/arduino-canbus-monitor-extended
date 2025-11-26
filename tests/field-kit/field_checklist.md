# Field Troubleshooting Checklist

Use this checklist whenever the Windows field run either reports failures or cannot complete because the environment is misbehaving. It walks through the CAN expectations, how to probe the adapter manually, and the minimum information to add to your follow-up report.

## 1. Verify the physical setup

1. Confirm the UNO enumerates as a serial port in Device Manager (e.g., `COM3`). Record the port name in your report.
2. Ensure the shield is wired into a **live 125 kbps CAN bus** with another node actively transmitting frames. Without traffic the suite intentionally times out.
3. Check that the partner node acknowledges frames (look for RX/TX LEDs if available) so `t/T/r/R` tests can observe incrementing counters.
4. Double-check termination resistors and 5 V/3.3 V power rails to the MCP2515 transceiver; brief noise spikes can look like suite failures.

## 2. Run the suite and watch for symptoms

- Run `python run_full_suite.py --port <COMx>` from this folder.
- Keep the console visible to capture test names that fail; the harness logs every PASS/FAIL.
- If the script times out or hangs on a test (>120 s), note the last reported command. It likely indicates an idle bus (`P/A/X` waiting for traffic) or insufficient acknowledgments.
- After the run, copy `field-suite.log` and any console scrollback into your report.

## 3. When commands fail or time out

| Failure symptom | Probe | Notes for the report |
|----------------|-------|----------------------|
| `P`/`A`/`X` tests timeout | Use a CAN sniffer (SavvyCAN, PCAN) to confirm frames appear and the RX queue drains. Record frame rate and IDs. | Include CAN IDs, parity, and whether frames include timestamps. |
| `O`/`C`/`S` commands error | Run `S4` then `O`, then `C` manually from a serial terminal. Log the responses (`S4\r`, `O\r`, `C\r`) and note whether the bus reports ERR or BUS OFF. | Capture whether `C` returns `\r` or BEL. |
| `F`/`i` diagnostics misbehave | Dump `i\r` output (wrap the responses in quotes). Confirm `i` reports sensible counters (frames_rx/tx increase when traffic exists). | Attach the `i` payload and highlight anomalies like zero frames. |
| `Z` timestamp persistence fails after reset | Record whether `Z1\r` survives a `reset_device()` (close/reopen port). Note whether the timestamp counter resets. | Mention if the device reverts to `Z0` unexpectedly. |
| `Q` autostart doesn't behave | Observe whether the channel opens automatically after a reset when `Q1` is enabled. If not, include the `Q\r` reply both before and after reboot. | Add whether the channel stays closed even though `Q1` was set. |

## 4. Reporting template

Use the following fields when documenting a failure or environmental problem:

```
1. Date / Time (UTC):
2. Windows host name / COM port:
3. Test(s) that failed: <list from log>
4. Console/log excerpt (attach `field-suite.log`):
5. Physical bus details: node count, bitrate, termination, noise?
6. CAN traffic sample: IDs, data bytes, expected vs. observed behavior.
7. Additional notes: auto-start and timestamp settings, whether `SLCAN` commands returned BEL.
8. Next steps taken (reconnect, power cycle, switch bus, etc.):
```

If the environment prevented the test from starting (missing port, no traffic), skip the failure details and focus on steps 1‑5 plus any error text. When you return, we can then correlate the logged data with firmware telemetry to fix the bug.
