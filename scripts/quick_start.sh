#!/bin/bash
# Quick start script for SavvyCAN automation with serial monitoring

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "=========================================="
echo "SavvyCAN Automation Quick Start"
echo "=========================================="
echo ""

# Check prerequisites
echo "Checking prerequisites..."

# Check for xdotool
if ! command -v xdotool &> /dev/null; then
    echo "❌ xdotool not found. Installing..."
    sudo apt update
    sudo apt install -y xdotool wmctrl
else
    echo "✅ xdotool found"
fi

# Check for Python packages
if ! python3 -c "import serial" 2>/dev/null; then
    echo "❌ pyserial not found. Installing..."
    pip install pyserial
else
    echo "✅ pyserial found"
fi

if ! python3 -c "import pyautogui" 2>/dev/null; then
    echo "⚠️  pyautogui not found (optional, for screenshots). Installing..."
    pip install pyautogui python3-xlib || echo "  (Installation failed, continuing anyway)"
else
    echo "✅ pyautogui found"
fi

# Check DISPLAY
if [ -z "$DISPLAY" ]; then
    echo "⚠️  DISPLAY not set. Attempting to set..."
    if [ -f /mnt/wslg/.X11-unix/X0 ] || [ -S /tmp/.X11-unix/X0 ]; then
        export DISPLAY=:0
        echo "✅ Set DISPLAY=:0 (WSLg)"
    else
        # Try X11 forwarding
        export DISPLAY=$(cat /etc/resolv.conf | grep nameserver | awk '{print $2}'):0.0
        echo "✅ Set DISPLAY for X11 forwarding"
    fi
else
    echo "✅ DISPLAY is set: $DISPLAY"
fi

# Check for serial port
if [ -z "$1" ]; then
    PORT="/dev/ttyACM0"
    echo "Using default port: $PORT"
else
    PORT="$1"
    echo "Using specified port: $PORT"
fi

if [ ! -e "$PORT" ]; then
    echo "⚠️  Warning: Serial port $PORT not found"
    echo "   Available ports:"
    ls -l /dev/ttyACM* /dev/ttyUSB* 2>/dev/null || echo "   (none found)"
    echo ""
    echo "   Make sure your Arduino is connected and attached to WSL:"
    echo "   (In Windows PowerShell as Admin: usbipd attach --busid <BUSID> --wsl)"
    read -p "Continue anyway? (y/N) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
else
    echo "✅ Serial port found: $PORT"
fi

echo ""
echo "=========================================="
echo "Starting SavvyCAN with serial monitor..."
echo "=========================================="
echo ""

# Run the orchestrator script
cd "$PROJECT_DIR"
python3 "$SCRIPT_DIR/run_savvycan_with_monitor.py" \
    --port "$PORT" \
    --connect \
    --log "can_traffic_$(date +%Y%m%d_%H%M%S).log"










