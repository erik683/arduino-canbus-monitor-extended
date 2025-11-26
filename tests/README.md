# Regression Harness

The `tests/` directory contains all test-related files, scripts, logs, and documentation for the Arduino CAN bus monitor project.

## Test Directory Structure

```
tests/
├── slcan_smoke.py              # Main regression test suite (LAWICEL/SLCAN commands)
├── slcan_smoke_full.py         # Thin alias that delegates to slcan_smoke
├── loopback_jitter_test.py     # CAN bus timing accuracy tests
├── run_full_suite.py           # Full test suite runner
├── test_simple.py              # Simple test utilities
├── test_serial.py              # Serial communication tests
├── test_can_status.py          # CAN status and diagnostics tests
├── test_diagnostics.py         # Diagnostic command tests
├── blink.ino                   # Arduino blink test sketch
├── platformio.ini              # PlatformIO configuration for test builds
├── irl_validation_report.md    # In-real-life validation test report
├── manual_irl_testing.md       # Manual testing procedures
└── logs/                       # Test execution logs
    ├── field-suite_*.log       # Field test suite logs
    └── irl_validation_*.log    # IRL validation logs
```

## Main Test Suites

### SLCAN Smoke Tests

The `tests/slcan_smoke.py` script executes a lightweight set of LAWICEL/SLCAN commands against a connected board to confirm the firmware still behaves as expected.

## Jitter Testing

The `tests/loopback_jitter_test.py` script measures CAN bus timing accuracy by injecting periodic frames at a known rate and analyzing inter-arrival jitter. This is useful for validating real-time performance and timing precision.

### Jitter Test Requirements

- CAN device that supports loopback mode, OR
- CAN bus setup where transmitted frames can be received back (loopback cable or bus with echo)
- Stable timing requirements (no other high-priority interrupts during test)

### Jitter Test Usage

```bash
# Basic jitter test with defaults (50ms period, 20 frames, 5ms threshold)
python tests/loopback_jitter_test.py --port /dev/ttyACM0

# Custom timing parameters
python tests/loopback_jitter_test.py --port /dev/ttyACM0 --period 25.0 --frames 50 --jitter-threshold 2.0

# High-precision test with extended frame
python tests/loopback_jitter_test.py --port COM5 --period 10.0 --frame-id 1ABCDEF0 --bitrate 8
```

### Jitter Test Arguments

| Flag | Description |
|------|-------------|
| `--port` | Serial device name. Defaults to auto-detection. |
| `--baud` | UART speed (default `115200`). |
| `--period` | Frame period in milliseconds (default `50.0`). |
| `--frames` | Number of frames to send (default `20`). |
| `--frame-id` | CAN frame ID in hex (default `123`). |
| `--frame-data` | Frame data in hex (default `DEADBEEF`). |
| `--jitter-threshold` | Maximum allowed jitter in milliseconds (default `5.0`). |
| `--bitrate` | CAN bitrate setting (S command, default `6` = 500kbps). |

### What the Jitter Test Measures

1. **Periodic Transmission**: Sends CAN frames at precise intervals using `time.sleep()`
2. **Timestamp Capture**: Enables SLCAN timestamps (`Z1`) to record device receive times
3. **Inter-arrival Calculation**: Computes time between consecutive frame receptions
4. **Jitter Analysis**: Measures deviation from expected period
5. **Threshold Assertion**: Fails if maximum jitter exceeds configured threshold

The test outputs statistics including max jitter, average jitter, and standard deviation for detailed analysis.

## Additional Test Scripts

The tests directory includes several additional test scripts for specific functionality:

- **`test_simple.py`**: Quick sanity checks for a few LAWICEL commands (auto-detects port)
- **`test_serial.py`**: Serial communication and UART interface tests (auto-detects port)
- **`test_can_status.py`**: CAN bus status monitoring and diagnostics tests (auto-detects port)
- **`test_diagnostics.py`**: Diagnostic command and telemetry tests (auto-detects port)

These scripts provide focused testing for specific subsystems and can be run independently or as part of the full test suite.

## Test Logs and Reports

Test execution logs are stored in the `logs/` subdirectory:
- **Field suite logs**: `field-suite_YYYYMMDD_HHMMSS.log` - Results from field testing sessions
- **IRL validation logs**: `irl_validation_YYYYMMDD_HHMMSS.log` - In-real-life validation test logs

The `irl_validation_report.md` file contains a comprehensive report of in-real-life validation testing results.

## Requirements

- Python 3.9+
- [`pyserial`](https://pypi.org/project/pyserial/) (`pip install pyserial`)

## Live Bus Requirements

The regression harness now validates every implemented LAWICEL command plus the custom diagnostics, so it needs a realistic environment:

- Connect the adapter to an active **125 kbps** CAN bus with at least one other node generating frames continuously.
- Ensure the partner node acknowledges outgoing frames so `t/T/r/R` tests can observe TX counter changes.
- Provide at least a few frames per second so autopoll (`X1`) tests can drain the RX queue within ~5 seconds.
- If the bus ever goes idle, the suite will fail with a descriptive timeout—restore live traffic and re-run.

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
| `--verbose` | Print each LAWICEL command/response for troubleshooting (noisy). |
| `--enable-debug` | Send `@DBG1` once at startup to leave firmware debug logging enabled for the run. |
| `--allow-debug-chatter` | Allow extra debug text on the serial line without treating it as a failure. |

The harness automatically closes the CAN channel before and after the suite runs to avoid leaving the MCP2515 in an active state.

### Running With/Without Debug Output

- **Verbose debug run** (prints commands/responses and enables firmware debug logging):
  ```bash
  python tests/slcan_smoke.py --port /dev/ttyACM0 --verbose --enable-debug
  ```
- **Debug-tolerant without enabling debug** (accepts existing debug chatter but keeps firmware quiet):
  ```bash
  python tests/slcan_smoke.py --allow-debug-chatter
  ```
- **Strict LAWICEL mode** (fails on any debug chatter and asserts debug is off):
  ```bash
  python tests/slcan_smoke.py --test strict_protocol_no_debug
  ```

### Device Reset Helper

Some protocol guarantees only hold immediately after the MCU boots (for example, ensuring `O` fails until a bitrate is selected). The harness exposes `SlcanHarness.reset_device()`, which momentarily closes and reopens the serial port—triggering the Arduino’s auto-reset circuitry—and flushes any stale serial data. Tests that rely on cold-boot defaults should invoke this helper before issuing commands.

### Host Baud Helper

Tests that exercise the `U` command need to follow the firmware to the newly selected UART speed. Use `SlcanHarness.set_host_baud(baud)` immediately after issuing `Un` to retune the host-side serial port without resetting the MCU.

## Coverage

Every LAWICEL command that the firmware implements now has a corresponding real-world test:

- **Core control**: `S`, `O`, `L`, `C`, `U`, `V/v`, `N` (`test_bitrate_rules`, `test_listen_mode_receives_frames`, `test_uart_speed_change`, etc.).
- **Transmit path**: `t`, `T`, `r`, `R` update runtime stats in `test_transmit_data_frames_increment_stats` and `test_transmit_rtr_frames_increment_stats`.
- **Receive path**: `X` (autopoll) is validated by `test_autopoll_stream`. Timestamping (`Z`) is covered by `test_timestamped_frames_include_counter`.
- **Diagnostics & telemetry**: `F`, `i`, custom `@DBGn`, and `i` snapshots are exercised via `test_flags_format`, `test_info_snapshot`, and `test_debug_toggle`.
- **Persistence & EEPROM-backed settings**: `Z`, `Q`, and `Q2` are covered by `test_timestamp_persistence`, `test_autostart_persistence`, and `test_autostart_listen_persistence`.
- **Filtering knobs**: Hardware filter mode plus acceptance code/mask (`W`, `M`, `m`) have round-trip tests that ensure arguments stick only while the channel is closed.
- **Protocol hygiene**: `strict_protocol_no_debug` asserts that, with debug disabled and autopoll off, the adapter emits only spec-compliant LAWICEL responses (no stray debug chatter).

Use `python tests/slcan_smoke.py --list-tests` to see the exact names; run individual cases with `--test name`.

## Adding New Tests

1. Implement a function that accepts a `SlcanHarness` instance and raises `RegressionFailure` on error.
2. Append `(name, function)` to the `TESTS` list inside `tests/slcan_smoke.py`.
3. Document any hardware prerequisites (loopbacks, fixtures) in the function docstring so future runs remain deterministic.

## Future Work (Medium/Low Priority)

### Medium Priority Improvements

1. **Refactor field-kit runner**: The `run_full_suite.py` script currently reimplements test execution instead of using `slcan_smoke.run_tests()`. Refactor to delegate to the main test runner and add progress reporting/logging as a wrapper.


3. **Configurable timeouts**: Make test timeouts (currently hardcoded at 5 seconds for frame waiting) configurable via command-line arguments to handle different bus speeds and traffic patterns.

### Low Priority Improvements

4. **Add malformed command syntax tests**: Test negative cases for command parsing:
   - Invalid hex characters in frame data
   - Missing or extra parameters
   - Commands with wrong case sensitivity
   - Commands sent while in invalid states

5. **Performance benchmarks**: Add optional performance testing mode that measures:
   - Frame throughput (frames/second)
   - Command response latency
   - Buffer drain time under load
   - UART baud rate change timing

6. **Concurrent command handling**: Test scenarios with rapid command sequences or overlapping operations to verify thread safety and state consistency.

### Documentation Gaps

1. **Test dependencies**: Document which tests require live CAN traffic vs. loopback scenarios:
   - Tests needing live traffic: `test_autopoll_stream`, `test_transmit_*_increment_stats`
   - Tests that can run offline: `test_version`, `test_serial_number`, `test_close_idempotent`, etc.

2. **Test ordering independence**: Verify and document that tests can run in any order without affecting each other. Current cleanup logic should ensure isolation.

3. **Environment setup troubleshooting**: Expand the troubleshooting guide for common failures:
   - Bus idle detection and resolution
   - Port enumeration issues on different platforms
   - MCP2515 hardware fault detection
   - Frame acknowledgment verification procedures
