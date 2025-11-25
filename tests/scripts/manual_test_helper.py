#!/usr/bin/env python3
"""
Manual IRL Testing Helper Script

Assists with manual testing of custom serial commands by providing:
- Serial port connection and command execution
- Response logging and formatting
- Test result tracking
- Command history and repetition

Usage:
    python manual_test_helper.py --port /dev/ttyACM0
    python manual_test_helper.py --port COM3 --baud 500000
"""

import argparse
import serial
import time
from datetime import datetime
from pathlib import Path
from typing import Optional, List, Tuple

class ManualTestHelper:
    """Helper class for manual IRL command testing."""

    def __init__(self, port: str, baud: int = 500000):
        self.port = port
        self.baud = baud
        self.serial: Optional[serial.Serial] = None
        self.log_file: Optional[Path] = None
        self.test_results: List[Tuple[str, str, str]] = []
        self.command_history: List[str] = []

    def connect(self) -> bool:
        """Establish serial connection to the device."""
        try:
            self.serial = serial.Serial(
                self.port,
                baudrate=self.baud,
                timeout=1.0,
                write_timeout=1.0
            )
            print(f"[INFO] Connected to {self.port} @ {self.baud} baud")

            # Initialize device
            self._initialize_device()
            return True
        except serial.SerialException as e:
            print(f"[ERROR] Failed to connect: {e}")
            return False

    def disconnect(self):
        """Close serial connection."""
        if self.serial:
            try:
                # Try to close CAN channel cleanly
                self.serial.write(b"C\r")
                time.sleep(0.1)
                self.serial.close()
                print("[INFO] Disconnected")
            except:
                pass
            self.serial = None

    def _initialize_device(self):
        """Initialize device and verify connectivity."""
        if not self.serial:
            return

        # Flush any pending data
        self.serial.reset_input_buffer()
        self.serial.reset_output_buffer()

        # Try version command
        try:
            self.serial.write(b"V\r")
            time.sleep(0.2)
            if self.serial.in_waiting:
                response = self.serial.read(self.serial.in_waiting)
                print(f"[INFO] Device version: {response.decode('ascii', errors='ignore').strip()}")
        except:
            pass

    def send_command(self, command: str, description: str = "") -> str:
        """Send a command and return the response."""
        if not self.serial:
            return "[ERROR] Not connected"

        if not command.endswith('\r'):
            command += '\r'

        full_command = f"{description}: {command}" if description else command

        try:
            # Log command
            timestamp = datetime.now().strftime("%H:%M:%S")
            print(f"[{timestamp}] >>> {full_command}")

            # Send command
            self.serial.write(command.encode('ascii'))
            self.serial.flush()

            # Read response
            response = self._read_response()
            print(f"[{timestamp}] <<< {response}")

            # Add to history
            self.command_history.append(full_command)

            return response

        except serial.SerialException as e:
            error_msg = f"[ERROR] Serial communication failed: {e}"
            print(error_msg)
            return error_msg

    def _read_response(self, timeout: float = 2.0) -> str:
        """Read response from device."""
        if not self.serial:
            return ""

        response = bytearray()
        start_time = time.monotonic()

        while time.monotonic() - start_time < timeout:
            if self.serial.in_waiting:
                chunk = self.serial.read(1)
                if chunk:
                    if chunk == b'\r':
                        # End of response
                        break
                    response.extend(chunk)
                start_time = time.monotonic()  # Reset timeout on data received
            else:
                time.sleep(0.01)

        # Decode response
        try:
            decoded = response.decode('ascii', errors='replace')
            # Handle BEL character
            decoded = decoded.replace('\x07', '[BEL]')
            return decoded.strip()
        except:
            return f"[BINARY] {response.hex()}"

    def test_sequence(self, commands: List[Tuple[str, str]], name: str):
        """Run a sequence of commands and log results."""
        print(f"\n=== Starting Test Sequence: {name} ===")

        results = []
        for command, description in commands:
            response = self.send_command(command, description)
            results.append((command, response, description))

            # Brief pause between commands
            time.sleep(0.1)

        self.test_results.extend(results)
        print(f"=== Completed Test Sequence: {name} ===\n")

        return results

    def poll_for_frames(self, count: int = 5, use_bulk: bool = False) -> List[str]:
        """Poll for frames and return responses."""
        frames = []
        command = "A" if use_bulk else "P"

        print(f"Polling for {count} frames using '{command}' command...")

        for i in range(count):
            response = self.send_command(command, f"Poll {i+1}")
            frames.append(response)

            if not use_bulk:
                time.sleep(0.5)  # Wait between single polls

        return frames

    def enable_autopoll_monitoring(self, duration: int = 10):
        """Enable autopoll and monitor output for specified duration."""
        print(f"Enabling autopoll monitoring for {duration} seconds...")

        # Enable autopoll
        self.send_command("X1", "Enable autopoll")
        self.send_command("O", "Open channel")

        print("Monitoring autopoll output (Ctrl+C to stop)...")

        try:
            start_time = time.monotonic()
            while time.monotonic() - start_time < duration:
                if self.serial and self.serial.in_waiting:
                    line = self._read_line()
                    if line:
                        timestamp = datetime.now().strftime("%H:%M:%S")
                        print(f"[{timestamp}] AUTOPOLL: {line}")
                time.sleep(0.1)
        except KeyboardInterrupt:
            print("\n[INFO] Autopoll monitoring stopped by user")

        # Disable autopoll
        self.send_command("C", "Close channel")
        self.send_command("X0", "Disable autopoll")

    def _read_line(self) -> str:
        """Read a single line from serial."""
        if not self.serial:
            return ""

        line = bytearray()
        while True:
            if self.serial.in_waiting:
                chunk = self.serial.read(1)
                if chunk == b'\r':
                    break
                line.extend(chunk)
            else:
                break

        try:
            return line.decode('ascii', errors='replace').strip()
        except:
            return f"[BINARY] {line.hex()}"

    def get_device_status(self) -> dict:
        """Get comprehensive device status."""
        status = {}

        # Basic info
        status['version'] = self.send_command("V", "Version")
        status['serial'] = self.send_command("N", "Serial number")

        # Channel state
        status['channel_closed'] = self.send_command("C", "Close channel")

        # Current settings
        status['bitrate'] = self.send_command("S4", "Set bitrate")
        status['channel_open'] = self.send_command("O", "Open channel")
        status['autopoll'] = self.send_command("X", "Autopoll status")
        status['timestamps'] = self.send_command("Z", "Timestamp status")
        status['filter_mode'] = self.send_command("W", "Filter mode")
        status['acceptance_code'] = self.send_command("M", "Acceptance code")
        status['acceptance_mask'] = self.send_command("m", "Acceptance mask")

        # Stats
        status['info'] = self.send_command("i", "Device info/stats")

        # Flags
        status['flags'] = self.send_command("F", "Status flags")

        return status

    def save_log(self, filename: str):
        """Save command history and results to file."""
        log_path = Path(filename)
        with open(log_path, 'w') as f:
            f.write(f"Manual IRL Testing Log - {datetime.now().isoformat()}\n")
            f.write(f"Port: {self.port}, Baud: {self.baud}\n\n")

            f.write("COMMAND HISTORY:\n")
            f.write("-" * 50 + "\n")
            for cmd in self.command_history:
                f.write(f"{cmd}\n")

            f.write("\nTEST RESULTS:\n")
            f.write("-" * 50 + "\n")
            for cmd, resp, desc in self.test_results:
                f.write(f"Command: {cmd}\n")
                f.write(f"Description: {desc}\n")
                f.write(f"Response: {resp}\n")
                f.write("-" * 30 + "\n")

        print(f"[INFO] Log saved to {log_path}")

    def interactive_mode(self):
        """Enter interactive command mode."""
        print("\n=== Interactive Mode ===")
        print("Enter LAWICEL commands (without <CR>). Special commands:")
        print("  'quit' - Exit interactive mode")
        print("  'status' - Show device status")
        print("  'poll' - Poll single frame")
        print("  'pollall' - Poll all frames")
        print("  'autopoll' - Toggle autopoll monitoring")
        print("  'log <filename>' - Save log")
        print()

        while True:
            try:
                cmd = input("CMD> ").strip()

                if cmd.lower() in ('quit', 'exit', 'q'):
                    break
                elif cmd.lower() == 'status':
                    status = self.get_device_status()
                    for key, value in status.items():
                        print(f"  {key}: {value}")
                elif cmd.lower() == 'poll':
                    self.send_command("P", "Manual poll")
                elif cmd.lower() == 'pollall':
                    self.send_command("A", "Bulk poll")
                elif cmd.lower() == 'autopoll':
                    self.enable_autopoll_monitoring(30)
                elif cmd.startswith('log '):
                    filename = cmd[4:].strip()
                    self.save_log(filename)
                elif cmd:
                    self.send_command(cmd, "Interactive")

            except KeyboardInterrupt:
                print("\n[INFO] Interactive mode interrupted")
                break
            except Exception as e:
                print(f"[ERROR] {e}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', required=True, help='Serial port (e.g., /dev/ttyACM0, COM3)')
    parser.add_argument('--baud', type=int, default=500000, help='Baud rate (default: 500000)')
    parser.add_argument('--log', help='Log file to save results')
    parser.add_argument('--interactive', action='store_true', help='Enter interactive mode')

    args = parser.parse_args()

    helper = ManualTestHelper(args.port, args.baud)

    if not helper.connect():
        return 1

    try:
        if args.interactive:
            helper.interactive_mode()
        else:
            # Run some basic tests
            print("Running basic connectivity tests...")

            # Test sequences for each command type
            test_sequences = [
                ("Basic Commands", [
                    ("V", "Version check"),
                    ("N", "Serial number"),
                    ("C", "Close channel"),
                    ("S4", "Set bitrate"),
                    ("O", "Open channel"),
                ]),
                ("Polling Tests", [
                    ("X0", "Disable autopoll"),
                    ("P", "Single frame poll"),
                    ("A", "Bulk frame poll"),
                ]),
                ("Mode Tests", [
                    ("L", "Listen mode"),
                    ("C", "Close"),
                    ("O", "Normal mode"),
                    ("Z1", "Enable timestamps"),
                    ("Z", "Check timestamps"),
                    ("X1", "Enable autopoll"),
                    ("X", "Check autopoll"),
                ]),
                ("Filter Tests", [
                    ("C", "Close channel"),
                    ("W", "Filter mode query"),
                    ("W1", "Set single filter"),
                    ("W", "Verify single filter"),
                    ("W0", "Set dual filter"),
                    ("W", "Verify dual filter"),
                ])
            ]

            for name, commands in test_sequences:
                helper.test_sequence(commands, name)

        if args.log:
            helper.save_log(args.log)

    finally:
        helper.disconnect()


if __name__ == '__main__':
    exit(main())
