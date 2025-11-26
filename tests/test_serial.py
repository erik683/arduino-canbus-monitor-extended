#!/usr/bin/env python3
"""Quick sanity check of core LAWICEL commands and debug toggles."""

import argparse
import sys
import time

import serial

from tests import slcan_smoke

DEFAULT_BAUD = 115200


def test_arduino(port: str | None, baud: int) -> int:
    resolved_port = slcan_smoke._detect_port(port)
    try:
        ser = serial.Serial(resolved_port, baud, timeout=1.0, write_timeout=1.0)
    except serial.SerialException as exc:
        print(f"Error opening {resolved_port}: {exc}")
        return 1

    print(f"Connected to {resolved_port} at {baud} baud")
    time.sleep(2)

    steps = [
        ("version", b"V\r"),
        ("open", b"O\r"),
        ("extended frame rejection on", b"%EXT1\r"),
        ("extended frame rejection off", b"%EXT0\r"),
        ("debug enable", b"@DBG1\r"),
        ("debug disable", b"@DBG0\r"),
        ("runtime stats", b"i\r"),
    ]

    for label, cmd in steps:
        print(f"{label} ({cmd!r})...")
        ser.write(cmd)
        ser.flush()
        response = ser.readline().decode("ascii", errors="ignore").strip()
        print(f"Response: '{response}'")

    print("Listening for CAN traffic for 5 seconds...")
    start_time = time.time()
    while time.time() - start_time < 5:
        if ser.in_waiting:
            data = ser.read(ser.in_waiting).decode("ascii", errors="ignore")
            if data.strip():
                print(f"CAN Data: '{data.strip()}'")
        time.sleep(0.1)

    ser.close()
    print("Test completed")
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", help="Serial device (defaults to auto-detect).")
    parser.add_argument("--baud", type=int, default=DEFAULT_BAUD, help="Host baud rate.")
    args = parser.parse_args(argv)
    return test_arduino(args.port, args.baud)


if __name__ == "__main__":
    raise SystemExit(main())
