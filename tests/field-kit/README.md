# Field Test Kit

This folder contains everything you need to move onto a Windows laptop and perform a full regression sweep against the UNO on a live CAN bus.

## Contents

- `run_full_suite.py`: Python driver that imports `tests/slcan_smoke.py`, exercises every implemented LAWICEL command/test pair, logs a timestamped summary, and also writes `field-suite.log` by default.
- `field-suite.log` (generated): appended by the script and safe to copy back with the results.
- `field_checklist.md`: step-by-step checklist to diagnose environmental issues, exercise the bus manually, and capture a report that will help later debugging.
- `requirements.txt`: installable dependencies to ensure a clean Windows environment (`pyserial` only for now).

## Requirements

1. **Python 3.9+** – download the Windows installer from python.org and be sure to check “Add Python to PATH”.
2. **Terminal** – run from PowerShell/CMD/Windows Terminal with Administrator rights if USB access is restricted.
3. **Install dependencies**: `pip install -r requirements.txt`.
4. **Hardware** – the UNO must be connected via USB (COM port will appear when `Device Manager` enumerates it) and the CAN transceiver must be wired into a live 125 kbps bus with at least one traffic generator that acknowledges frames.

## Running the Full Suite

1. Open PowerShell in this directory (the entire folder can be copied over to the laptop).
2. Plug in the UNO and confirm which COM port it exposes (e.g., `COM3`).
3. Run the script with:

   ```ps
   python run_full_suite.py --port COM3
   ```

   Use `--baud 500000` if you need a different UART speed and `--fail-fast` if you only want the first failing test. Logs default to `field-suite.log`.

4. After the run completes, copy `field-suite.log` and the console output to your report. The log includes timestamps, pass/fail results, and any error messages for quick triage.

If the run cannot complete because of environment issues (no traffic, wrong bitrate, etc.), follow the steps in `field_checklist.md` while still recording at least the command that failed and any partial output.
