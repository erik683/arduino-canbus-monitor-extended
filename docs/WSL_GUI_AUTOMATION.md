# WSL GUI Window Automation Guide

This guide explains how to set up GUI support in WSL and enable automation tools to manipulate GUI windows.

## Overview

To allow an agent (automation script, AI assistant, etc.) to manipulate GUI windows opened through WSL, you need:

1. **GUI Support**: WSLg (recommended) or X11 forwarding
2. **Automation Tools**: Command-line tools or Python libraries for window manipulation
3. **Display Configuration**: Proper DISPLAY environment variable setup

## Option 1: WSLg (Recommended for Windows 11 / Windows 10 with updates)

WSLg provides native GUI support without additional X server setup.

### Prerequisites

- **Windows 11** (built-in), or
- **Windows 10** with:
  - WSL version 0.67.6 or later
  - Windows 10 version 19044 or later
  - GPU driver with WDDM 2.9 or later

### Verify WSLg is Working

```bash
# Check if DISPLAY is set automatically
echo $DISPLAY
# Should show something like :0

# Test with a simple GUI app
sudo apt update
sudo apt install -y x11-apps
xeyes  # Should open a window
```

### Enable GUI Automation Tools

#### Install xdotool (Window Manipulation)

```bash
sudo apt update
sudo apt install -y xdotool wmctrl
```

**Basic xdotool commands:**
```bash
# Find window by name
xdotool search --name "SavvyCAN"

# Click at coordinates
xdotool click 1  # Left click
xdotool mousemove 100 200 click 1  # Move and click

# Type text
xdotool type "Hello World"

# Send keyboard shortcuts
xdotool key ctrl+c
xdotool key alt+Tab

# Get window information
xdotool getactivewindow
xdotool getwindowgeometry $(xdotool search --name "SavvyCAN")
```

#### Install Python Automation Libraries

```bash
# Install pyautogui (cross-platform GUI automation)
pip install pyautogui

# For WSL, you may also need:
sudo apt install -y python3-tk python3-dev
pip install python3-xlib  # X11 support for pyautogui
```

**Example Python script:**
```python
import pyautogui
import time

# Move mouse and click
pyautogui.moveTo(100, 200)
pyautogui.click()

# Type text
pyautogui.write('Hello from automation')

# Take screenshot
screenshot = pyautogui.screenshot()
screenshot.save('window.png')

# Find window by image (if you have a reference image)
button = pyautogui.locateOnScreen('button.png')
pyautogui.click(button)
```

## Option 2: X11 Forwarding (Alternative)

If WSLg is not available, use X11 forwarding with an X server on Windows.

### Windows Side Setup

1. **Install VcXsrv or X410:**
   - **VcXsrv** (free): https://sourceforge.net/projects/vcxsrv/
   - **X410** (paid, better performance): Microsoft Store

2. **Configure VcXsrv:**
   - Start XLaunch
   - Select "Multiple windows" or "One large window"
   - Check "Disable access control" (for localhost)
   - Finish

### WSL Side Setup

```bash
# Set DISPLAY to Windows host IP
export DISPLAY=$(cat /etc/resolv.conf | grep nameserver | awk '{print $2}'):0.0

# Make it persistent
echo "export DISPLAY=\$(cat /etc/resolv.conf | grep nameserver | awk '{print \$2}'):0.0" >> ~/.bashrc
source ~/.bashrc

# Test
sudo apt install -y x11-apps
xeyes
```

### Install Automation Tools (Same as WSLg)

```bash
sudo apt install -y xdotool wmctrl
pip install pyautogui python3-xlib
```

## Automation Examples

### Example 1: Automate SavvyCAN with xdotool

```bash
#!/bin/bash
# automate-savvycan.sh

# Launch SavvyCAN (adjust path as needed)
./SavvyCAN-x86_64.AppImage &

# Wait for window to appear
sleep 3

# Find SavvyCAN window
WINDOW_ID=$(xdotool search --name "SavvyCAN" | head -1)

if [ -z "$WINDOW_ID" ]; then
    echo "SavvyCAN window not found"
    exit 1
fi

# Activate window
xdotool windowactivate $WINDOW_ID

# Example: Click on a menu item (adjust coordinates)
xdotool mousemove 50 30 click 1  # File menu
sleep 1
xdotool mousemove 50 80 click 1  # Open option

# Type or send keys
xdotool key ctrl+o  # Open dialog
```

### Example 2: Python Automation Script

```python
#!/usr/bin/env python3
"""
Automate GUI interactions with SavvyCAN or other WSL GUI apps
"""
import pyautogui
import time
import subprocess
import sys

# Safety: fail-safe if mouse goes to corner
pyautogui.FAILSAFE = True
pyautogui.PAUSE = 0.5  # Small delay between actions

def find_and_click_window(window_name):
    """Find window by name and bring it to focus"""
    try:
        # Use xdotool to find window
        result = subprocess.run(
            ['xdotool', 'search', '--name', window_name],
            capture_output=True,
            text=True
        )
        if result.returncode == 0 and result.stdout.strip():
            window_id = result.stdout.strip().split('\n')[0]
            subprocess.run(['xdotool', 'windowactivate', window_id])
            time.sleep(0.5)
            return True
    except Exception as e:
        print(f"Error finding window: {e}")
    return False

def automate_savvycan():
    """Example automation for SavvyCAN"""
    print("Starting SavvyCAN automation...")
    
    # Find and activate window
    if not find_and_click_window("SavvyCAN"):
        print("SavvyCAN window not found. Please launch it first.")
        return False
    
    # Example actions
    print("Taking screenshot...")
    screenshot = pyautogui.screenshot()
    screenshot.save('savvycan_screenshot.png')
    
    # Example: Click File menu (adjust coordinates for your setup)
    print("Clicking File menu...")
    pyautogui.click(50, 30)
    time.sleep(0.5)
    
    # Example: Send keyboard shortcut
    print("Sending Ctrl+O (Open)...")
    pyautogui.hotkey('ctrl', 'o')
    time.sleep(1)
    
    # Example: Type text
    print("Typing filename...")
    pyautogui.write('test.log')
    time.sleep(0.5)
    pyautogui.press('enter')
    
    return True

if __name__ == "__main__":
    automate_savvycan()
```

### Example 3: Using wmctrl (Window Manager Control)

```bash
#!/bin/bash
# wmctrl example

# List all windows
wmctrl -l

# Find window by title
wmctrl -a "SavvyCAN"  # Activate window with this title

# Move and resize window
wmctrl -r "SavvyCAN" -e 0,100,100,800,600
# Format: gravity,x,y,width,height

# Close window
wmctrl -c "SavvyCAN"
```

## Troubleshooting

### DISPLAY not set

```bash
# For WSLg
export DISPLAY=:0

# For X11 forwarding
export DISPLAY=$(cat /etc/resolv.conf | grep nameserver | awk '{print $2}'):0.0

# Verify
echo $DISPLAY
```

### Permission denied errors

```bash
# Allow X11 connections (for X11 forwarding)
xhost +local:

# Or more securely:
xhost +SI:localuser:$(whoami)
```

### Window not found

```bash
# List all windows
xdotool search --class ".*"  # All windows
wmctrl -l  # All windows with titles

# Check window properties
xdotool search --name ".*" getwindowname %@
```

### GUI apps not showing

1. **Verify WSLg is working:**
   ```bash
   echo $DISPLAY
   xeyes  # Should show a window
   ```

2. **Check Windows GPU driver:**
   - Update your GPU driver (NVIDIA/AMD/Intel)
   - Ensure WDDM 2.9+ support

3. **For X11 forwarding:**
   - Ensure VcXsrv/X410 is running
   - Check firewall settings
   - Verify DISPLAY variable

## Advanced: Integration with AI Agents

If you're using an AI agent (like Cursor's AI assistant) to automate GUI interactions:

1. **Create automation scripts** that the agent can call
2. **Use command-line tools** (xdotool, wmctrl) that agents can execute
3. **Provide window coordinates** or use image recognition (pyautogui.locateOnScreen)
4. **Log actions** for debugging:
   ```python
   import logging
   logging.basicConfig(level=logging.INFO)
   pyautogui.PAUSE = 1.0  # Slower for visibility
   ```

## Security Considerations

- **X11 forwarding**: Disable access control only for localhost, not for network access
- **Automation scripts**: Be careful with scripts that can click/type anywhere
- **Screenshots**: Be mindful of sensitive data in screenshots
- **Window access**: Some applications may block automation for security

## Additional Resources

- **xdotool documentation**: https://www.semicomplete.com/projects/xdotool/
- **pyautogui documentation**: https://pyautogui.readthedocs.io/
- **WSLg documentation**: https://github.com/microsoft/wslg
- **VcXsrv**: https://sourceforge.net/projects/vcxsrv/

## Quick Reference

```bash
# Setup (one-time)
sudo apt install -y xdotool wmctrl x11-apps
pip install pyautogui python3-xlib

# Daily use
export DISPLAY=:0  # WSLg
# or
export DISPLAY=$(cat /etc/resolv.conf | grep nameserver | awk '{print $2}'):0.0  # X11

# Find window
xdotool search --name "WindowName"

# Click
xdotool mousemove 100 200 click 1

# Type
xdotool type "text"

# Screenshot (Python)
python3 -c "import pyautogui; pyautogui.screenshot('screen.png')"
```

