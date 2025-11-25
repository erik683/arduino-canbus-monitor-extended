#!/usr/bin/env python3
"""
Serial monitor for Arduino CAN bus monitor device.
Monitors serial output and optionally logs to file.
"""

import argparse
import os
import serial
import sys
import time
from datetime import datetime
from pathlib import Path

try:
    from serial.tools import list_ports
except ImportError:
    list_ports = None


def detect_port(explicit: str | None) -> str:
    """Detect serial port automatically or use explicit value."""
    if explicit:
        return explicit
    
    # Check environment variable
    env_port = os.getenv("SLCAN_PORT") or os.getenv("ARDUINO_PORT")
    if env_port:
        return env_port
    
    # Try auto-detection
    if list_ports is not None:
        ports = list(list_ports.comports())
        for port in ports:
            # Prefer Arduino devices
            if 'Arduino' in port.description or '2341' in port.hwid:
                return port.device
        if ports:
            return ports[0].device
    
    # Fallback to common paths
    for candidate in ["/dev/ttyACM0", "/dev/ttyUSB0", "/dev/ttyACM1"]:
        if Path(candidate).exists():
            return candidate
    
    raise SystemExit(
        "Unable to detect serial port. "
        "Set --port or ARDUINO_PORT environment variable."
    )


def monitor_serial(
    port: str,
    baud: int = 500000,
    log_file: str | None = None,
    timestamp: bool = True,
    filter_lines: list[str] | None = None,
):
    """
    Monitor serial port and print/log output.
    
    Args:
        port: Serial port path
        baud: Baud rate
        log_file: Optional log file path
        timestamp: Add timestamps to output
        filter_lines: List of strings to filter (only show lines containing these)
    """
    try:
        ser = serial.Serial(port, baudrate=baud, timeout=1.0)
        print(f"Connected to {port} at {baud} baud", file=sys.stderr)
        print(f"Monitoring serial output... (Ctrl+C to stop)", file=sys.stderr)
        print("-" * 80, file=sys.stderr)
        
        log_fp = None
        if log_file:
            log_fp = open(log_file, 'a', encoding='utf-8')
            print(f"Logging to: {log_file}", file=sys.stderr)
        
        buffer = ""
        
        try:
            while True:
                if ser.in_waiting:
                    data = ser.read(ser.in_waiting).decode('utf-8', errors='replace')
                    buffer += data
                    
                    # Process complete lines
                    while '\n' in buffer or '\r' in buffer:
                        if '\n' in buffer:
                            line, buffer = buffer.split('\n', 1)
                        else:
                            line, buffer = buffer.split('\r', 1)
                        
                        line = line.strip()
                        if not line:
                            continue
                        
                        # Apply filters if specified
                        if filter_lines:
                            if not any(filt in line for filt in filter_lines):
                                continue
                        
                        # Format output
                        if timestamp:
                            ts = datetime.now().strftime("%H:%M:%S.%f")[:-3]
                            output = f"[{ts}] {line}"
                        else:
                            output = line
                        
                        print(output)
                        
                        if log_fp:
                            log_fp.write(f"{output}\n")
                            log_fp.flush()
                else:
                    time.sleep(0.01)  # Small delay to avoid busy-waiting
                    
        except KeyboardInterrupt:
            print("\nStopping monitor...", file=sys.stderr)
        finally:
            ser.close()
            if log_fp:
                log_fp.close()
                print(f"Log saved to: {log_file}", file=sys.stderr)
                
    except serial.SerialException as e:
        print(f"Error opening serial port {port}: {e}", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(f"Unexpected error: {e}", file=sys.stderr)
        sys.exit(1)


def main():
    parser = argparse.ArgumentParser(
        description="Monitor serial output from Arduino CAN bus monitor"
    )
    parser.add_argument(
        "--port", "-p",
        type=str,
        default=None,
        help="Serial port (default: auto-detect)"
    )
    parser.add_argument(
        "--baud", "-b",
        type=int,
        default=500000,
        help="Baud rate (default: 500000)"
    )
    parser.add_argument(
        "--log", "-l",
        type=str,
        default=None,
        help="Log file path (optional)"
    )
    parser.add_argument(
        "--no-timestamp",
        action="store_true",
        help="Don't add timestamps to output"
    )
    parser.add_argument(
        "--filter",
        type=str,
        action="append",
        help="Filter lines (only show lines containing this string, can be repeated)"
    )
    
    args = parser.parse_args()
    
    port = detect_port(args.port)
    
    monitor_serial(
        port=port,
        baud=args.baud,
        log_file=args.log,
        timestamp=not args.no_timestamp,
        filter_lines=args.filter,
    )


if __name__ == "__main__":
    main()
