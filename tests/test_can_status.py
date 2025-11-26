#!/usr/bin/env python3
"""Lightweight CAN status probe using LAWICEL commands."""

import argparse
import sys
import time

import serial

from tests import slcan_smoke

DEFAULT_BAUD = 115200


def test_can_status(port: str | None, baud: int) -> int:
    resolved_port = slcan_smoke._detect_port(port)
    try:
        ser = serial.Serial(resolved_port, baud, timeout=1.0, write_timeout=1.0)
    except serial.SerialException as exc:
        print(f"Error opening {resolved_port}: {exc}")
        return 1

    print(f"Connected to {resolved_port} at {baud} baud")
    time.sleep(2)

    sequence = [
        ("Version", b"V\r"),
        ("Status before open (F)", b"F\r"),
        ("Open bus (O)", b"O\r"),
        ("Status after open (F)", b"F\r"),
        ("Extended frame debug (#EXT)", b"#EXT\r"),
        ("Extended frame rejection (%EXT1)", b"%EXT1\r"),
        ("Runtime stats (i)", b"i\r"),
    ]

    for label, cmd in sequence:
        ser.write(cmd)
        ser.flush()
        time.sleep(0.15)
        response = ser.read(64).decode("ascii", errors="ignore").strip()
        print(f"{label}: '{response}'")

    ser.close()
    print("CAN status probe completed")
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", help="Serial device (defaults to auto-detect).")
    parser.add_argument("--baud", type=int, default=DEFAULT_BAUD, help="Host baud rate.")
    args = parser.parse_args(argv)
    return test_can_status(args.port, args.baud)


if __name__ == "__main__":
    raise SystemExit(main())
