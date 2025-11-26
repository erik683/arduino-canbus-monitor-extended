# Automation Scripts

Helper tools for working with the Arduino CAN bus monitor outside of PlatformIO.

## Prerequisites

```bash
pip install pyserial
```

## serial_monitor.py

Real-time serial monitor with optional logging and port auto-detection.

**Usage**
```bash
# Auto-detect port, default 115200 baud
python scripts/serial_monitor.py

# Specify port/baud and log to file
python scripts/serial_monitor.py --port /dev/ttyACM0 --baud 115200 --log serial_output.log
```

**Flags**
- `--port, -p`: Serial port path (default: auto-detect; honors SLCAN_PORT/ARDUINO_PORT)
- `--baud, -b`: Baud rate (default: 115200)
- `--log, -l`: Log file path (optional)
- `--no-timestamp`: Do not prepend timestamps
- `--filter`: Only show lines containing the given string (repeatable)

## quick_start.sh

One-shot helper that starts `serial_monitor.py` with defaults (`/dev/ttyACM0` @ 115200) and writes a timestamped log in the repo root. Pass an explicit port as the first argument to override.




