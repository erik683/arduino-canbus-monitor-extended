# Regression Harness

The `tests/slcan_smoke.py` script executes a lightweight set of LAWICEL/SLCAN commands against a connected board to confirm the firmware still behaves as expected.

## Requirements

- Python 3.9+
- [`pyserial`](https://pypi.org/project/pyserial/) (`pip install pyserial`)

## Usage

```bash
# Option 1: specify a port explicitly
python tests/slcan_smoke.py --port /dev/ttyACM0

# Option 2: rely on auto-detection using the first available serial device
python tests/slcan_smoke.py

# Option 3: target a Windows COM port
python tests/slcan_smoke.py --port COM5
```

Environment variable `SLCAN_PORT` can override the auto-detected port.

### Arguments

| Flag | Description |
|------|-------------|
| `--port` | Serial device name. Defaults to `SLCAN_PORT` or the first enumerated port. |
| `--baud` | UART speed (default `115200`). |
| `--test NAME` | Run a specific test (repeat flag for multiple). |
| `--list-tests` | Print the available test names and exit. |
| `--fail-fast` | Stop immediately when a test fails. |

The harness automatically closes the CAN channel before and after the suite runs to avoid leaving the MCP2515 in an active state.

### Device Reset Helper

Some protocol guarantees only hold immediately after the MCU boots (for example, ensuring `O` fails until a bitrate is selected). The harness exposes `SlcanHarness.reset_device()`, which momentarily closes and reopens the serial port—triggering the Arduino’s auto-reset circuitry—and flushes any stale serial data. Tests that rely on cold-boot defaults should invoke this helper before issuing commands.

### Host Baud Helper

Tests that exercise the `U` command need to follow the firmware to the newly selected UART speed. Use `SlcanHarness.set_host_baud(baud)` immediately after issuing `Un` to retune the host-side serial port without resetting the MCU.

## Adding New Tests

1. Implement a function that accepts a `SlcanHarness` instance and raises `RegressionFailure` on error.
2. Append `(name, function)` to the `TESTS` list inside `tests/slcan_smoke.py`.
3. Document any hardware prerequisites (loopbacks, fixtures) in the function docstring so future runs remain deterministic.
