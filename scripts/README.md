# Automation Scripts

This directory contains scripts for automating SavvyCAN and monitoring serial output from the Arduino CAN bus monitor.

## Prerequisites

Install required tools and Python packages:

```bash
# Install GUI automation tools
sudo apt update
sudo apt install -y xdotool wmctrl

# Install Python dependencies
pip install pyserial pyautogui python3-xlib
```

Ensure WSLg or X11 forwarding is set up (see `../docs/WSL_GUI_AUTOMATION.md`).

## Scripts

### `serial_monitor.py`

Monitors serial output from the Arduino device in real-time.

**Usage:**
```bash
# Basic usage (auto-detect port)
python scripts/serial_monitor.py

# Specify port and baud rate
python scripts/serial_monitor.py --port /dev/ttyACM0 --baud 115200

# Log output to file
python scripts/serial_monitor.py --log serial_output.log

# Filter lines (only show lines containing "ERROR" or "RX")
python scripts/serial_monitor.py --filter ERROR --filter RX

# No timestamps
python scripts/serial_monitor.py --no-timestamp
```

**Options:**
- `--port, -p`: Serial port path (default: auto-detect)
- `--baud, -b`: Baud rate (default: 115200)
- `--log, -l`: Log file path (optional)
- `--no-timestamp`: Don't add timestamps
- `--filter`: Filter lines (can be repeated)

### `savvycan_automation.py`

Launches and automates SavvyCAN GUI application.

**Usage:**
```bash
# Launch SavvyCAN
python scripts/savvycan_automation.py

# Launch and connect to serial port
python scripts/savvycan_automation.py --connect --port /dev/ttyACM0

# Take screenshot
python scripts/savvycan_automation.py --screenshot savvycan.png

# Specify AppImage path
python scripts/savvycan_automation.py --appimage /path/to/SavvyCAN.AppImage
```

**Options:**
- `--appimage`: Path to SavvyCAN AppImage (default: auto-detect)
- `--port`: Serial port to connect (default: /dev/ttyACM0)
- `--baud`: Baud rate (default: 115200)
- `--connect`: Attempt to connect to serial port after launch
- `--screenshot`: Take screenshot and save to file

**Python API:**
```python
from scripts.savvycan_automation import SavvyCANAuto

# Launch SavvyCAN
savvycan = SavvyCANAuto()
savvycan.launch()

# Activate window
savvycan.activate_window()

# Click at coordinates
savvycan.click(100, 200)

# Type text
savvycan.type_text("Hello")

# Send keyboard shortcut
savvycan.send_key("ctrl+o")

# Connect to serial port
savvycan.connect_serial("/dev/ttyACM0", 115200)

# Take screenshot
savvycan.screenshot("screenshot.png")

# Close
savvycan.close()
```

### `run_savvycan_with_monitor.py` ⭐ **Recommended**

Launches SavvyCAN and monitors serial output simultaneously. This is the main script for agent automation.

**Usage:**
```bash
# Launch SavvyCAN and monitor serial (auto-detect port)
python scripts/run_savvycan_with_monitor.py

# Specify port and connect SavvyCAN automatically
python scripts/run_savvycan_with_monitor.py --port /dev/ttyACM0 --connect

# Log serial output to file
python scripts/run_savvycan_with_monitor.py --log serial.log

# Filter serial output
python scripts/run_savvycan_with_monitor.py --filter ERROR --filter RX

# Launch SavvyCAN only (no serial monitor)
python scripts/run_savvycan_with_monitor.py --no-monitor
```

**Options:**
- `--port, -p`: Serial port (default: auto-detect)
- `--baud, -b`: Baud rate (default: 115200)
- `--appimage`: Path to SavvyCAN AppImage (default: auto-detect)
- `--log`: Log serial output to file
- `--filter`: Filter serial lines (can be repeated)
- `--connect`: Attempt to connect SavvyCAN to serial port
- `--no-monitor`: Don't start serial monitor

**Example Output:**
```
Using serial port: /dev/ttyACM0
Launching SavvyCAN...
Launching SavvyCAN from /home/erikm/SavvyCAN_binary/SavvyCAN-x86_64.AppImage...
Waiting 5s for SavvyCAN window...
Found SavvyCAN window: 12345678
Starting serial monitor...
================================================================================
SavvyCAN is running. Serial monitor is active.
Press Ctrl+C to stop.
================================================================================

[12:34:56.789] V1013
[12:34:56.890] N123
[12:34:57.123] t12345678
...
```

## Agent Integration

For AI agents (like Cursor's assistant), you can use these scripts to:

1. **Launch and control SavvyCAN:**
   ```python
   from scripts.savvycan_automation import SavvyCANAuto
   savvycan = SavvyCANAuto()
   savvycan.launch()
   savvycan.connect_serial("/dev/ttyACM0")
   ```

2. **Monitor serial output:**
   ```python
   from scripts.serial_monitor import monitor_serial
   monitor_serial("/dev/ttyACM0", log_file="output.log")
   ```

3. **Run both together:**
   ```bash
   python scripts/run_savvycan_with_monitor.py --connect --log can_traffic.log
   ```

## Troubleshooting

### "xdotool: command not found"
```bash
sudo apt install -y xdotool
```

### "DISPLAY not set"
```bash
# For WSLg
export DISPLAY=:0

# For X11 forwarding
export DISPLAY=$(cat /etc/resolv.conf | grep nameserver | awk '{print $2}'):0.0
```

### "Permission denied" on serial port
```bash
sudo usermod -a -G dialout $USER
newgrp dialout
```

### SavvyCAN window not found
- Ensure WSLg or X11 is working: `xeyes` should show a window
- Wait longer for window to appear (increase wait time in script)
- Check if SavvyCAN is already running: `xdotool search --name "SavvyCAN"`

### Serial port not detected
- Verify device is attached: `ls -l /dev/ttyACM*`
- Check Windows USB attachment: `usbipd list` (in Windows PowerShell as Admin)
- Set port explicitly: `--port /dev/ttyACM0`

## Examples

### Example 1: Monitor CAN traffic while using SavvyCAN

```bash
# Terminal 1: Launch SavvyCAN with serial monitor
python scripts/run_savvycan_with_monitor.py --connect --log can_traffic.log

# The script will:
# - Launch SavvyCAN GUI
# - Connect it to /dev/ttyACM0
# - Monitor serial output in the terminal
# - Log all output to can_traffic.log
```

### Example 2: Automated testing script

```python
#!/usr/bin/env python3
"""Example: Automated CAN bus testing with SavvyCAN"""

from scripts.savvycan_automation import SavvyCANAuto
from scripts.serial_monitor import monitor_serial
import time
import multiprocessing

def test_sequence():
    # Launch SavvyCAN
    savvycan = SavvyCANAuto()
    savvycan.launch()
    time.sleep(3)
    
    # Connect to device
    savvycan.connect_serial("/dev/ttyACM0", 115200)
    time.sleep(2)
    
    # Take screenshot of initial state
    savvycan.screenshot("test_start.png")
    
    # Monitor serial in background
    monitor_proc = multiprocessing.Process(
        target=monitor_serial,
        args=("/dev/ttyACM0", 115200, "test.log", None)
    )
    monitor_proc.start()
    
    # Perform test actions
    time.sleep(10)  # Run test for 10 seconds
    
    # Take final screenshot
    savvycan.screenshot("test_end.png")
    
    # Cleanup
    monitor_proc.terminate()
    savvycan.close()

if __name__ == "__main__":
    test_sequence()
```

## See Also

- `../docs/WSL_GUI_AUTOMATION.md` - Complete WSL GUI automation guide
- `../README.md` - Main project documentation
- `../WSL_USB_SETUP.md` - USB device setup for WSL


