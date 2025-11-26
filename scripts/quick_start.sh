#!/bin/bash
# Minimal quick-start helper that launches the serial monitor with sane defaults.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
DEFAULT_PORT="/dev/ttyACM0"
DEFAULT_BAUD=115200

echo "=========================================="
echo "Serial Monitor Quick Start"
echo "=========================================="
echo ""

# Check for pyserial
if ! python3 -c "import serial" 2>/dev/null; then
    echo "Installing pyserial..."
    pip install --quiet pyserial
fi

# Detect port
PORT="${1:-$DEFAULT_PORT}"
if [ ! -e "$PORT" ]; then
    echo "⚠️  Warning: Serial port $PORT not found."
    ls -l /dev/ttyACM* /dev/ttyUSB* 2>/dev/null || true
    read -p "Continue anyway? (y/N) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

LOG_PATH="$PROJECT_DIR/serial_monitor_$(date +%Y%m%d_%H%M%S).log"

echo "Starting serial monitor on $PORT @ ${DEFAULT_BAUD} baud"
echo "Logging to $LOG_PATH"
python3 "$SCRIPT_DIR/serial_monitor.py" \
    --port "$PORT" \
    --baud "$DEFAULT_BAUD" \
    --log "$LOG_PATH"




