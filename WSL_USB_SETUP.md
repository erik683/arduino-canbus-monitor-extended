# WSL2 USB Serial Device Setup - Detailed Guide

> **Quick Start**: See [README.md](README.md) for basic Arduino setup instructions.

This guide provides detailed troubleshooting and advanced configuration for USB serial devices (Arduino) in WSL2.

## Prerequisites

This project requires your Arduino to be accessible from WSL2. The recommended method is **usbipd-win**, the official Microsoft solution for USB passthrough.

## Installation Steps

### 1. Install usbipd-win on Windows

```powershell
# PowerShell as Administrator
winget install --interactive --exact dorssel.usbipd-win
```

Or download from: https://github.com/dorssel/usbipd-win/releases

### 2. Install USB/IP tools in WSL

```bash
sudo apt update
sudo apt install linux-tools-generic hwdata
sudo update-alternatives --install /usr/local/bin/usbip usbip /usr/lib/linux-tools/*/usbip 20
```

### 3. Identify Your Arduino

**Arduino Uno** typically shows as:
- **VID:PID**: `2341:0043` or `2341:0001`
- **Device**: USB Serial Device (COMx)

List devices in PowerShell (as Admin):
```powershell
usbipd list
```

Example output:
```
BUSID  VID:PID    DEVICE                        STATE
1-2    2341:0043  USB Serial Device (COM3)      Not shared
```

### 4. Configure Permanent Auto-Attach

This project includes automation scripts for permanent Arduino access in WSL.

**This project includes automated setup scripts** - See the "Arduino Setup" section in [README.md](README.md) for:
- `setup-windows-task.bat` - Creates Windows Task Scheduler job for auto-attach
- `attach-arduino.bat` / `attach-arduino.ps1` - Manual attachment scripts

**Manual Task Scheduler Setup:**

If you prefer manual configuration:
1. Open Windows Task Scheduler
2. Create new task with highest privileges
3. Trigger: At startup
4. Action: Run `usbipd attach --busid <BUSID> --wsl` (or use `attach-arduino.bat`)

**PlatformIO Port Configuration:**

Arduino Uno typically appears as `/dev/ttyACM0` in WSL. The `platformio.ini` is already configured, but if needed:
```ini
monitor_port = /dev/ttyACM0  # Or 'auto' for auto-detection
```

## Troubleshooting

### Device not appearing in WSL
- Make sure you ran `usbipd attach --busid <BUSID> --wsl` as Administrator
- Check if the device is already attached: `usbipd list`
- Try detaching first: `usbipd detach --busid <BUSID>`

### Permission denied errors

Add your user to the `dialout` group in WSL:
```bash
sudo usermod -a -G dialout $USER
```

Then either:
- Log out and back in to WSL
- Run `newgrp dialout` in current shell
- Use `sg dialout "pio run -t upload"` for one-time access

**Important**: Ensure PlatformIO CLI is on your PATH:
```bash
# Add to ~/.bashrc if needed
export PATH=$HOME/.local/bin:$PATH
```

### Device disappears after WSL restart

Expected behavior - device must be re-attached after WSL restart. Use the automated Task Scheduler setup to make it permanent.

### Multiple USB devices

Identify your Arduino by:
- **VID:PID**: `2341:0043` (Uno) or `2341:0001` (older Uno)
- **COM port**: Check Windows Device Manager
- **Description**: "USB Serial Device" or "Arduino Uno"

### PlatformIO upload fails

```bash
# Verify device is accessible
ls -l /dev/ttyACM*

# Check permissions
groups | grep dialout

# Check kernel messages
dmesg | tail -20

# Manually attach if needed (Windows PowerShell as Admin)
usbipd attach --busid <BUSID> --wsl
```

## Quick Command Reference

| Task | Command |
|------|---------|
| **List USB devices** | `usbipd list` (Windows, as Admin) |
| **Attach to WSL** | `usbipd attach --busid <BUSID> --wsl` (Windows, as Admin) |
| **Detach from WSL** | `usbipd detach --busid <BUSID>` (Windows, as Admin) |
| **Check device in WSL** | `ls -l /dev/ttyACM*` or `dmesg \| tail` |
| **Build firmware** | `pio run` |
| **Upload firmware** | `pio run -t upload` |
| **Monitor serial** | `pio device monitor` |

---

**For basic setup instructions, see [README.md](README.md).**
