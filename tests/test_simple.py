#!/usr/bin/env python3
"""Minimal quick-check script for a handful of LAWICEL commands."""

import argparse
import sys
import time

import serial

from tests import slcan_smoke

DEFAULT_BAUD = 115200


def test_simple(port: str | None, baud: int) -> int:
    resolved_port = slcan_smoke._detect_port(port)
    try:
        ser = serial.Serial(resolved_port, baud, timeout=1.0, write_timeout=1.0)
    except serial.SerialException as exc:
        print(f"Error opening {resolved_port}: {exc}")
        return 1

    print(f"Connected to {resolved_port} at {baud} baud")
    time.sleep(2)  # allow auto-reset
    ser.reset_input_buffer()
    ser.reset_output_buffer()

    test_commands = [
        ("Version", b"V\r"),
        ("Debug toggle", b"@DBG0\r"),
        ("Percent command", b"%EXT0\r"),
    ]

    for name, cmd in test_commands:
        print(f"\nTesting {name}: {cmd!r}")
        ser.write(cmd)
        ser.flush()
        time.sleep(0.2)
        raw_response = ser.read(64)
        response = raw_response.decode("ascii", errors="ignore").strip()
        print(f"Raw: {raw_response!r}")
        print(f"Decoded: '{response}'")
        if not response:
            print("  (No response - command may not be recognized)")

    ser.close()
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", help="Serial device (defaults to auto-detect).")
    parser.add_argument("--baud", type=int, default=DEFAULT_BAUD, help="Host baud rate.")
    args = parser.parse_args(argv)
    return test_simple(args.port, args.baud)


if __name__ == "__main__":
    raise SystemExit(main())
