#!/usr/bin/env python3
"""
Basic regression harness for the Arduino CAN BUS monitor firmware.

The goal is to exercise critical LAWICEL/SLCAN commands (version, serial number,
bitrate changes, channel open/close, timestamp toggling, and status flags) to
guard against regressions before adding new features.
"""

from __future__ import annotations

import argparse
import os
import sys
import time
from dataclasses import dataclass
from typing import Callable, Iterable, List, Sequence, Tuple

import serial

try:
    from serial.tools import list_ports
except ImportError:  # pragma: no cover  (optional dependency)
    list_ports = None  # type: ignore

UART_BAUD_TABLE: Tuple[int, ...] = (
    230_400,
    115_200,
    57_600,
    38_400,
    19_200,
    9_600,
    2_400,
)


class RegressionFailure(RuntimeError):
    """Raised when the device under test returns an unexpected response."""


def _detect_port(explicit: str | None) -> str:
    """Resolve the serial port to use, falling back to auto-detection."""
    if explicit:
        return explicit
    env = os.getenv("SLCAN_PORT")
    if env:
        return env
    if list_ports is not None:
        ports = list(list_ports.comports())
        if ports:
            return ports[0].device
    # Fallback list for common Linux/macOS/Windows names
    fallback = [
        "/dev/ttyACM0",
        "/dev/ttyUSB0",
        "/dev/tty.usbmodem0",
        "COM3",
        "COM4",
    ]
    for candidate in fallback:
        if os.path.exists(candidate):
            return candidate
    raise SystemExit(
        "Unable to detect a serial port automatically. "
        "Set --port or the SLCAN_PORT environment variable."
    )


@dataclass
class SlcanHarness:
    port: str
    baud: int = 115200
    timeout: float = 1.0

    def __post_init__(self) -> None:
        try:
            self.serial = serial.Serial(
                self.port,
                baudrate=self.baud,
                timeout=self.timeout,
                write_timeout=self.timeout,
            )
        except serial.SerialException as exc:
            raise SystemExit(f"Failed to open {self.port}: {exc}") from exc
        # Allow the MCU to reboot after the port comes up
        time.sleep(1.5)
        self.flush()

    def close(self) -> None:
        try:
            self.serial.close()
        except serial.SerialException:
            pass

    def flush(self) -> None:
        time.sleep(0.05)
        self.serial.reset_input_buffer()
        self.serial.reset_output_buffer()

    def reset_device(self) -> None:
        """Force a full MCU reboot by closing/reopening the serial link."""
        try:
            self.serial.close()
        except serial.SerialException as exc:
            raise SystemExit(f"Failed to close {self.port} during reset: {exc}") from exc
        time.sleep(0.2)
        try:
            self.serial.baudrate = self.baud
        except (serial.SerialException, ValueError) as exc:
            raise SystemExit(f"Failed to set baud {self.baud} on {self.port}: {exc}") from exc
        try:
            self.serial.open()
        except serial.SerialException as exc:
            raise SystemExit(f"Failed to reopen {self.port} during reset: {exc}") from exc
        # Opening a CDC/ACM port toggles DTR, which resets the Arduino bootloader.
        time.sleep(1.5)
        self.flush()

    def set_host_baud(self, baud: int) -> None:
        """Switch the host UART speed without disturbing the MCU state."""
        try:
            self.serial.flush()
            self.serial.baudrate = baud
        except (serial.SerialException, ValueError) as exc:
            raise SystemExit(f"Failed to change host baud to {baud}: {exc}") from exc
        self.baud = baud
        time.sleep(0.1)
        self.flush()

    def _transact(self, command: str, response_timeout: float = 0.5) -> bytes:
        """Send a LAWICEL command string and capture the payload before the CR."""
        if not command.endswith("\r"):
            raise ValueError("Commands must be CR-terminated, e.g. 'V\\r'")
        self.flush()
        self.serial.write(command.encode("ascii"))
        self.serial.flush()
        deadline = time.monotonic() + response_timeout
        raw = bytearray()
        last_read_time = time.monotonic()
        while True:
            if time.monotonic() > deadline:
                raise RegressionFailure(
                    f"Timeout waiting for response to {command!r} (received {raw!r})"
                )
            # Read available bytes
            chunk = self.serial.read(128)
            if chunk:
                raw.extend(chunk)
                last_read_time = time.monotonic()
                # Check if we've received a complete response:
                # 1. Response ends with \r (normal case with data)
                if raw.endswith(b"\r"):
                    break
                # 2. Response is just \r (success with no payload, 0x0D)
                if raw == b"\r":
                    break
                # 3. Error responses send \x07 without CR
                if raw == b"\x07":
                    # Check if more data is available
                    if self.serial.in_waiting == 0:
                        # Small delay to ensure no more data is coming
                        time.sleep(0.01)
                        if self.serial.in_waiting == 0:
                            break
            else:
                # No data available - check if we have a complete error response
                # Error responses send \x07 without CR
                if raw == b"\x07":
                    # Wait a bit to ensure no more data is coming
                    if time.monotonic() - last_read_time > 0.02:
                        break
        # Handle error response (\x07 without CR)
        if raw == b"\x07":
            return b"\x07"
        # Handle success response that's just CR
        if raw == b"\r":
            return b""
        # Normal response ending with CR
        if raw.endswith(b"\r"):
            return bytes(raw[:-1])
        # Should not reach here, but handle gracefully
        raise RegressionFailure(
            f"Incomplete response to {command!r}: {bytes(raw)!r}"
        )

    def expect_ok(self, command: str) -> None:
        payload = self._transact(command)
        if payload not in (b"", b"z", b"Z"):
            raise RegressionFailure(
                f"Expected OK for {command.strip()!r}, got {payload!r}"
            )

    def expect_error(self, command: str) -> None:
        payload = self._transact(command)
        if payload != b"\x07":
            raise RegressionFailure(
                f"Expected error bell for {command.strip()!r}, got {payload!r}"
            )

    def transact(self, command: str) -> bytes:
        """Lower-level helper that returns the payload for data-bearing commands."""
        return self._transact(command)


def ensure_closed(harness: SlcanHarness) -> None:
    """Best-effort channel close without raising when already closed."""
    payload = harness.transact("C\r")
    if payload not in (b"", b"\x07"):
        raise RegressionFailure(f"Unexpected close response: {payload!r}")


def ensure_bitrate_configured(harness: SlcanHarness, rate: str = "4") -> None:
    """Force the LAWICEL device into a known bitrate selection (Sn)."""
    ensure_closed(harness)
    harness.expect_ok(f"S{rate}\r")


# --- Individual tests ----------------------------------------------------- #


def test_version(h: SlcanHarness) -> None:
    payload = h.transact("V\r")
    if not payload.startswith(b"V"):
        raise RegressionFailure(f"Version string malformed: {payload!r}")


def test_serial_number(h: SlcanHarness) -> None:
    payload = h.transact("N\r")
    if not payload.startswith(b"NA"):
        raise RegressionFailure(f"Serial number malformed: {payload!r}")


def test_close_idempotent(h: SlcanHarness) -> None:
    ensure_bitrate_configured(h)
    h.expect_ok("O\r")
    ensure_closed(h)
    # Second close should report an error per LAWICEL spec
    payload = h.transact("C\r")
    if payload != b"\x07":
        raise RegressionFailure("Second close should emit BEL (\x07)")


def test_open_requires_bitrate(h: SlcanHarness) -> None:
    h.reset_device()
    ensure_closed(h)
    h.expect_error("O\r")
    h.expect_ok("S4\r")
    h.expect_ok("O\r")
    ensure_closed(h)


def test_bitrate_rules(h: SlcanHarness) -> None:
    ensure_bitrate_configured(h, "4")  # 125 kbit default
    h.expect_ok("O\r")
    # Should fail because channel is open
    h.expect_error("S5\r")
    ensure_closed(h)
    h.expect_ok("S5\r")  # Now allowed


def test_uart_speed_change(h: SlcanHarness) -> None:
    ensure_closed(h)
    h.expect_ok("U0\r")
    h.set_host_baud(UART_BAUD_TABLE[0])
    payload = h.transact("V\r")
    if not payload.startswith(b"V"):
        raise RegressionFailure(f"Version query failed after UART speed change: {payload!r}")
    h.expect_ok("U1\r")  # Return to default 115200
    h.set_host_baud(UART_BAUD_TABLE[1])
    payload = h.transact("V\r")
    if not payload.startswith(b"V"):
        raise RegressionFailure("Failed to communicate after restoring default UART speed")


def test_timestamp_requires_closed(h: SlcanHarness) -> None:
    ensure_closed(h)
    for mode in ("0", "1"):
        h.expect_ok(f"Z{mode}\r")
    h.expect_error("Z2\r")
    ensure_bitrate_configured(h)
    h.expect_ok("O\r")
    h.expect_error("Z0\r")
    ensure_closed(h)


def test_flags_format(h: SlcanHarness) -> None:
    ensure_closed(h)
    h.expect_error("F\r")
    ensure_bitrate_configured(h)
    h.expect_ok("O\r")
    payload = h.transact("F\r")
    if len(payload) != 3 or payload[0:1] != b"F":
        raise RegressionFailure(f"Unexpected flag payload: {payload!r}")
    try:
        int(payload[1:].decode("ascii"), 16)
    except ValueError as exc:
        raise RegressionFailure(f"Flag bytes are not hex: {payload!r}") from exc


def test_timestamp_query(h: SlcanHarness) -> None:
    ensure_closed(h)
    payload = h.transact("Z\r")
    if payload not in (b"Z0", b"Z1"):
        raise RegressionFailure(f"Unexpected timestamp query payload: {payload!r}")
    h.expect_ok("Z1\r")
    if h.transact("Z\r") != b"Z1":
        raise RegressionFailure("Timestamp query did not report Z1 after enabling")
    h.expect_ok("Z0\r")
    if h.transact("Z\r") != b"Z0":
        raise RegressionFailure("Timestamp query did not report Z0 after disabling")
    ensure_bitrate_configured(h)
    h.expect_ok("O\r")
    if h.transact("Z\r") != b"Z0":
        raise RegressionFailure("Timestamp query should work while CAN is open")
    ensure_closed(h)


def test_timestamp_persistence(h: SlcanHarness) -> None:
    ensure_closed(h)
    h.expect_ok("Z1\r")
    h.reset_device()
    if h.transact("Z\r") != b"Z1":
        raise RegressionFailure("Timestamp mode did not persist as Z1 after reset")
    h.expect_ok("Z0\r")
    h.reset_device()
    if h.transact("Z\r") != b"Z0":
        raise RegressionFailure("Timestamp mode did not revert to Z0 after reset")


TESTS: Sequence[Tuple[str, Callable[[SlcanHarness], None]]] = (
    ("version", test_version),
    ("serial", test_serial_number),
    ("close_idempotent", test_close_idempotent),
    ("open_requires_bitrate", test_open_requires_bitrate),
    ("bitrate_rules", test_bitrate_rules),
    ("uart_speed_change", test_uart_speed_change),
    ("timestamp_requires_closed", test_timestamp_requires_closed),
    ("timestamp_query", test_timestamp_query),
    ("timestamp_persistence", test_timestamp_persistence),
    ("flags_format", test_flags_format),
)


def run_tests(
    harness: SlcanHarness,
    selected: Iterable[str] | None,
    fail_fast: bool,
) -> List[Tuple[str, bool, str]]:
    requested = set(selected or [])
    results: List[Tuple[str, bool, str]] = []
    for name, func in TESTS:
        if requested and name not in requested:
            continue
        try:
            func(harness)
            results.append((name, True, ""))
        except RegressionFailure as exc:
            results.append((name, False, str(exc)))
            if fail_fast:
                break
        finally:
            ensure_closed(harness)
    return results


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", help="Serial device. Defaults to auto-detection.")
    parser.add_argument("--baud", type=int, default=115200, help="Serial baud rate.")
    parser.add_argument("--test", action="append", help="Run only the named test.")
    parser.add_argument(
        "--list-tests", action="store_true", help="List available tests and exit."
    )
    parser.add_argument("--fail-fast", action="store_true", help="Stop on first error.")
    args = parser.parse_args(argv)

    if args.list_tests:
        for name, _ in TESTS:
            print(name)
        return 0

    port = _detect_port(args.port)
    print(f"[INFO] Connecting to {port} @ {args.baud} baud...")
    harness = SlcanHarness(port=port, baud=args.baud)
    try:
        results = run_tests(harness, args.test, args.fail_fast)
    finally:
        ensure_closed(harness)
        harness.close()

    failed = [r for r in results if not r[1]]
    for name, ok, message in results:
        status = "PASS" if ok else "FAIL"
        print(f"[{status}] {name}")
        if message:
            print(f"        {message}")

    if failed:
        print(f"[ERROR] {len(failed)} test(s) failed.")
        return 1
    print(f"[INFO] {len(results)} test(s) passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
