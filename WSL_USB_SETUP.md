# WSL USB Serial Device Setup Guide

This guide explains how to make your USB serial device (Arduino) available in WSL2 permanently.

## Method 1: Using usbipd-win (Recommended)

This is the official Microsoft-recommended solution for sharing USB devices from Windows to WSL2.

### Step 1: Install usbipd-win on Windows

1. Open PowerShell as Administrator on Windows
2. Install via Winget:
   ```powershell
   winget install --interactive --exact dorssel.usbipd-win
   ```
   Or download from: https://github.com/dorssel/usbipd-win/releases

### Step 2: Install USB/IP tools in WSL

In your WSL terminal, run:
```bash
sudo apt update
sudo apt install linux-tools-generic hwdata
sudo update-alternatives --install /usr/local/bin/usbip usbip /usr/lib/linux-tools/*/usbip 20
```

### Step 3: Find your USB device

On Windows (PowerShell as Admin), list USB devices:
```powershell
usbipd list
```

You'll see output like:
```
BUSID  VID:PID    DEVICE                                                        STATE
1-2    2341:0043  USB Serial Device (COM3)                                      Not shared
```

Note the BUSID (e.g., `1-2`) and the COM port.

### Step 4: Attach the device to WSL

On Windows (PowerShell as Admin), attach the device:
```powershell
usbipd attach --busid <BUSID> --wsl
```

Replace `<BUSID>` with your actual BUSID (e.g., `1-2`).

**Note:** You no longer need to specify a WSL distribution. usbipd will automatically select a distribution and make the device available to all WSL 2 distributions.

### Step 5: Verify in WSL

In WSL, check if the device appears:
```bash
ls -l /dev/ttyUSB* /dev/ttyACM*
```

You should see something like `/dev/ttyUSB0` or `/dev/ttyACM0`.

### Step 6: Make it permanent (Auto-attach on WSL start)

#### Option A: Windows Task Scheduler (Recommended)

1. On Windows, open Task Scheduler
2. Create a new task:
   - **General**: Check "Run whether user is logged on or not" and "Run with highest privileges"
   - **Trigger**: "At startup" or "When a specific event is logged" (WSL start)
   - **Action**: Start a program
     - Program: `usbipd.exe`
     - Arguments: `attach --busid <BUSID> --wsl`
   
   Or create a PowerShell script:

   Create `C:\Scripts\attach-usb.ps1`:
   ```powershell
   usbipd attach --busid <BUSID> --wsl
   ```

   Then in Task Scheduler, run:
   - Program: `powershell.exe`
   - Arguments: `-ExecutionPolicy Bypass -File C:\Scripts\attach-usb.ps1`

#### Option B: WSL startup script

Create a script in WSL that runs on startup:

1. Create `/home/erikm/.wsl_startup.sh`:
   ```bash
   #!/bin/bash
   # This will be called from Windows
   ```

2. On Windows, create a startup script that calls:
   ```powershell
   usbipd attach --busid <BUSID> --wsl
   ```

#### Option C: Manual script (Simplest for now)

Create a Windows batch file `attach-usb.bat`:
```batch
@echo off
usbipd attach --busid <BUSID> --wsl
```

Run this batch file as Administrator whenever you start WSL.

### Step 7: Update platformio.ini (if needed)

If your device appears as `/dev/ttyACM0` instead of `/dev/ttyUSB0`, update `platformio.ini`:
```ini
monitor_port = /dev/ttyACM0
```

Or use PlatformIO's auto-detection:
```ini
monitor_port = auto
```

## Method 2: COM Port Forwarding (Alternative)

If usbipd-win doesn't work for your device, you can use COM port forwarding:

1. Install `com0com` or `socat` in WSL
2. Use a Windows service to forward COM3 (example) to a network port
3. Connect from WSL using `socat` or similar

This is more complex and less reliable than usbipd-win.

## Troubleshooting

### Device not appearing in WSL
- Make sure you ran `usbipd attach --busid <BUSID> --wsl` as Administrator
- Check if the device is already attached: `usbipd list`
- Try detaching first: `usbipd detach --busid <BUSID>`

### Permission denied errors
Add your user to the dialout group in WSL:
```bash
sudo usermod -a -G dialout $USER
```
Then log out and back in to WSL.

**PlatformIO reminder:** after adding yourself to `dialout`, either open a fresh WSL shell or run `newgrp dialout` before calling `pio run -t upload`. If you ever need a temporary fix without restarting, you can wrap the command with `sg dialout "pio run -t upload"`, but rebooting the shell is the permanent solution. Also be sure the PlatformIO CLI (`~/.local/bin/pio`) is on your `PATH`; add `export PATH=$HOME/.local/bin:$PATH` to `~/.bashrc` if uploads stop working after a new session.

### Device disappears after WSL restart
This is expected - you need to re-attach. Use one of the permanent solutions above.

### Finding the correct BUSID
If you have multiple USB devices, you can identify your Arduino by:
- The COM port number (if it shows in Device Manager)
- The VID:PID (Arduino Uno is typically `2341:0043` or `2341:0001`)

## Quick Reference

**Attach device:**
```powershell
# Windows (PowerShell as Admin)
usbipd attach --busid <BUSID> --wsl
```

**Detach device:**
```powershell
# Windows (PowerShell as Admin)
usbipd detach --busid <BUSID>
```

**List devices:**
```powershell
# Windows (PowerShell as Admin)
usbipd list
```

**Check in WSL:**
```bash
# WSL
ls -l /dev/ttyUSB* /dev/ttyACM*
dmesg | tail -20  # Check kernel messages
```
