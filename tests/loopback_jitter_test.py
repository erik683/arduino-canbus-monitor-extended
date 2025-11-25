#!/usr/bin/env python3
"""
CAN Bus Loopback Jitter Test

This script injects periodic CAN frames at a known rate, captures timestamps,
calculates inter-arrival jitter, and asserts it stays within acceptable bounds.

Designed for testing with CAN devices that support loopback mode or when
connected to a CAN bus where transmitted frames can be received back.
"""

from __future__ import annotations

import argparse
import os
import statistics
import sys
import time
from dataclasses import dataclass
from typing import List, Optional, Tuple

import serial

try:
    from serial.tools import list_ports
except ImportError:  # pragma: no cover  (optional dependency)
    list_ports = None  # type: ignore


class TestFailure(RuntimeError):
    """Raised when the jitter test fails."""


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
class CanFrame:
    raw: bytes
    identifier: int
    extended: bool
    remote: bool
    dlc: int
    data: bytes
    timestamp: Optional[int]


@dataclass
class JitterStats:
    num_frames: int
    period_ms: float
    expected_period_ms: float
    inter_arrival_times: List[float]
    jitters: List[float]
    max_jitter_ms: float
    avg_jitter_ms: float
    std_dev_jitter_ms: float


class SlcanHarness:
    def __init__(self, port: str, baud: int = 115200, timeout: float = 1.0):
        try:
            self.serial = serial.Serial(
                port,
                baudrate=baud,
                timeout=timeout,
                write_timeout=timeout,
            )
        except serial.SerialException as exc:
            raise SystemExit(f"Failed to open {port}: {exc}") from exc
        self.baud = baud
        self._await_device_ready()
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

    def _transact(self, command: str, response_timeout: float = 2.5) -> bytes:
        """Send a LAWICEL command and capture the response."""
        if not command.endswith("\r"):
            raise ValueError("Commands must be CR-terminated")

        self.flush()
        self.serial.write(command.encode("ascii"))
        self.serial.flush()

        deadline = time.monotonic() + response_timeout
        raw = bytearray()
        last_read_time = time.monotonic()
        response_started = False

        while True:
            if time.monotonic() > deadline:
                raise TestFailure(f"Timeout waiting for response to {command!r}")

            chunk = self.serial.read(128)
            if chunk:
                raw.extend(chunk)
                last_read_time = time.monotonic()
                response_started = True
                if raw.endswith(b"\r") or raw == b"\r" or raw == b"\x07":
                    if raw == b"\x07" and self.serial.in_waiting:
                        continue
                    break
            else:
                if response_started and time.monotonic() - last_read_time > 0.05:
                    break
                if raw == b"\x07" and time.monotonic() - last_read_time > 0.02:
                    break
                time.sleep(0.01)

        if raw == b"\x07":
            return b"\x07"
        if raw == b"\r":
            return b""
        if raw.endswith(b"\r"):
            return bytes(raw[:-1])
        if raw:
            return bytes(raw)
        raise TestFailure(f"No response received to {command!r}")

    def expect_ok(self, command: str) -> None:
        payload = self._transact(command)
        if payload not in (b"", b"z", b"Z"):
            raise TestFailure(f"Expected OK for {command.strip()!r}, got {payload!r}")

    def transact(self, command: str) -> bytes:
        return self._transact(command)

    def _await_device_ready(self) -> None:
        deadline = time.monotonic() + 3.0
        while time.monotonic() < deadline:
            try:
                self.serial.write(b"V\r")
                self.serial.flush()
            except serial.SerialException:
                return
            poll_start = time.monotonic()
            while time.monotonic() - poll_start < 0.25:
                if self.serial.in_waiting:
                    self.serial.read(self.serial.in_waiting)
                    return
                time.sleep(0.01)
        time.sleep(0.25)

    def read_line(self, timeout: float = 1.0) -> bytes:
        """Read a CR-terminated line."""
        deadline = time.monotonic() + timeout
        raw = bytearray()
        while time.monotonic() < deadline:
            chunk = self.serial.read(1)
            if not chunk:
                continue
            if chunk == b"\r":
                return bytes(raw)
            raw.extend(chunk)
        raise TestFailure("Timeout waiting for serial line.")


def _parse_hex(segment: bytes) -> int:
    if not segment:
        raise TestFailure("Missing hex segment.")
    try:
        return int(segment, 16)
    except ValueError as exc:
        raise TestFailure(f"Invalid hex segment: {segment!r}") from exc


def parse_can_frame(payload: bytes, timestamps_enabled: bool = False) -> CanFrame:
    if not payload:
        raise TestFailure("Empty CAN frame payload.")
    frame_type = payload[0:1]
    if frame_type not in (b"t", b"T", b"r", b"R"):
        raise TestFailure(f"Unexpected frame prefix: {payload!r}")

    extended = frame_type in (b"T", b"R")
    remote = frame_type in (b"r", b"R")
    idx = 1
    id_chars = 8 if extended else 3
    if len(payload) < idx + id_chars + 1:
        raise TestFailure(f"Truncated CAN frame: {payload!r}")
    identifier = _parse_hex(payload[idx : idx + id_chars])
    idx += id_chars

    dlc = _parse_hex(payload[idx : idx + 1])
    if dlc > 8:
        raise TestFailure(f"DLC exceeds 8 bytes in frame: {payload!r}")
    idx += 1

    data = b""
    if not remote:
        data_chars = dlc * 2
        if len(payload) < idx + data_chars:
            raise TestFailure(f"Missing data bytes in frame: {payload!r}")
        data_str = payload[idx : idx + data_chars].decode("ascii")
        data = bytes.fromhex(data_str) if data_str else b""
        idx += data_chars

    timestamp = None
    if timestamps_enabled:
        if len(payload) < idx + 4:
            raise TestFailure(f"Missing timestamp in frame: {payload!r}")
        timestamp = _parse_hex(payload[idx : idx + 4])
        idx += 4

    if len(payload) != idx:
        raise TestFailure(f"Unexpected trailing bytes in frame: {payload!r}")

    return CanFrame(
        raw=payload,
        identifier=identifier,
        extended=extended,
        remote=remote,
        dlc=dlc,
        data=data,
        timestamp=timestamp,
    )


def ensure_closed(harness: SlcanHarness) -> None:
    payload = harness.transact("C\r")
    if payload not in (b"", b"\x07"):
        raise TestFailure(f"Unexpected close response: {payload!r}")


def ensure_bitrate_configured(harness: SlcanHarness, rate: str = "6") -> None:
    ensure_closed(harness)
    harness.expect_ok(f"S{rate}\r")


def ensure_autopoll_mode(harness: SlcanHarness, enabled: bool) -> None:
    ensure_closed(harness)
    target = "1" if enabled else "0"
    harness.expect_ok(f"X{target}\r")
    status = harness.transact("X\r")
    if status != f"X{target}".encode("ascii"):
        raise TestFailure(f"Unexpected autopoll status: {status!r}")


def run_jitter_test(
    harness: SlcanHarness,
    period_ms: float = 50.0,
    num_frames: int = 20,
    frame_id: str = "123",
    frame_data: str = "DEADBEEF",
    jitter_threshold_ms: float = 5.0,
    bitrate: str = "6",
) -> JitterStats:
    """
    Run the jitter test with specified parameters.

    Args:
        harness: SLCAN harness instance
        period_ms: Period between frames in milliseconds
        num_frames: Number of frames to send
        frame_id: CAN frame ID (hex string)
        frame_data: Frame data (hex string)
        jitter_threshold_ms: Maximum allowed jitter in milliseconds
        bitrate: CAN bitrate setting (S command parameter)

    Returns:
        JitterStats with measurement results
    """
    print(f"[INFO] Starting jitter test: {num_frames} frames @ {period_ms}ms period")
    print(f"[INFO] Frame ID: 0x{frame_id}, Data: 0x{frame_data}")
    print(f"[INFO] Bitrate: S{bitrate}, Jitter threshold: {jitter_threshold_ms}ms")

    # Setup
    ensure_autopoll_mode(harness, False)
    ensure_closed(harness)
    harness.expect_ok("Z1\r")  # Enable timestamps
    ensure_bitrate_configured(harness, bitrate)
    harness.expect_ok("O\r")

    # Enable autopoll for receiving frames
    ensure_autopoll_mode(harness, True)
    harness.expect_ok("O\r")

    timestamps = []
    start_time = time.monotonic()

    try:
        for i in range(num_frames):
            # Transmit frame - use 'T' for extended frames (8 hex digits), 't' for standard frames
            frame_prefix = "T" if len(frame_id) == 8 else "t"
            cmd = f"{frame_prefix}{frame_id}{len(frame_data)//2:01X}{frame_data}\r"
            harness.expect_ok(cmd)

            # Wait for the frame to be received
            frame_received = False
            frame_deadline = time.monotonic() + 0.2  # 200ms timeout

            while time.monotonic() < frame_deadline:
                try:
                    frame = harness.read_line(timeout=0.01)
                    if frame and frame.startswith(b"t"):
                        parsed_frame = parse_can_frame(frame, timestamps_enabled=True)
                        if (parsed_frame.identifier == int(frame_id, 16) and
                            parsed_frame.timestamp is not None):
                            timestamps.append(parsed_frame.timestamp)
                            frame_received = True
                            print(f"[DEBUG] Frame {i+1}: timestamp {parsed_frame.timestamp}")
                            break
                except TestFailure:
                    continue

            if not frame_received:
                raise TestFailure(f"Failed to receive frame {i+1} within timeout")

            # Wait for next transmission time
            next_tx_time = start_time + ((i + 1) * period_ms / 1000.0)
            sleep_time = max(0, next_tx_time - time.monotonic())
            if sleep_time > 0:
                time.sleep(sleep_time)

    finally:
        # Cleanup
        ensure_closed(harness)
        ensure_autopoll_mode(harness, False)
        harness.expect_ok("Z0\r")

    if len(timestamps) < 2:
        raise TestFailure("Did not receive enough timestamped frames")

    # Calculate inter-arrival times (assuming timestamps are in milliseconds)
    inter_arrival_times = []
    for i in range(1, len(timestamps)):
        dt = timestamps[i] - timestamps[i-1]
        inter_arrival_times.append(dt)

    # Calculate jitter as deviation from expected period
    jitters = [abs(dt - period_ms) for dt in inter_arrival_times]

    # Compute statistics
    max_jitter = max(jitters)
    avg_jitter = statistics.mean(jitters)
    std_dev_jitter = statistics.stdev(jitters) if len(jitters) > 1 else 0

    stats = JitterStats(
        num_frames=len(timestamps),
        period_ms=period_ms,
        expected_period_ms=period_ms,
        inter_arrival_times=inter_arrival_times,
        jitters=jitters,
        max_jitter_ms=max_jitter,
        avg_jitter_ms=avg_jitter,
        std_dev_jitter_ms=std_dev_jitter,
    )

    print(f"[INFO] Test completed: {len(timestamps)} frames received")
    print(f"[INFO] Inter-arrival times: {[f'{t:.1f}' for t in inter_arrival_times]}")
    print(f"[INFO] Jitters: {[f'{j:.1f}' for j in jitters]}")
    print(f"[INFO] Max jitter: {max_jitter:.1f}ms, Avg jitter: {avg_jitter:.1f}ms, Std dev: {std_dev_jitter:.1f}ms")

    # Assert jitter threshold
    if max_jitter > jitter_threshold_ms:
        raise TestFailure(
            f"Maximum jitter {max_jitter:.1f}ms exceeds threshold {jitter_threshold_ms:.1f}ms"
        )

    print(f"[PASS] Jitter test passed (max jitter: {max_jitter:.1f}ms < threshold: {jitter_threshold_ms:.1f}ms)")
    return stats


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", help="Serial device. Defaults to auto-detection.")
    parser.add_argument("--baud", type=int, default=115200, help="Serial baud rate.")
    parser.add_argument("--period", type=float, default=50.0,
                       help="Frame period in milliseconds (default: 50.0)")
    parser.add_argument("--frames", type=int, default=20,
                       help="Number of frames to send (default: 20)")
    parser.add_argument("--frame-id", default="123",
                       help="CAN frame ID in hex (default: 123)")
    parser.add_argument("--frame-data", default="DEADBEEF",
                       help="Frame data in hex (default: DEADBEEF)")
    parser.add_argument("--jitter-threshold", type=float, default=5.0,
                       help="Maximum allowed jitter in milliseconds (default: 5.0)")
    parser.add_argument("--bitrate", default="6",
                       help="CAN bitrate setting (S command, default: 6 = 500kbps)")

    args = parser.parse_args(argv)

    try:
        port = _detect_port(args.port)
        print(f"[INFO] Connecting to {port} @ {args.baud} baud...")

        harness = SlcanHarness(port=port, baud=args.baud)
        try:
            stats = run_jitter_test(
                harness=harness,
                period_ms=args.period,
                num_frames=args.frames,
                frame_id=args.frame_id,
                frame_data=args.frame_data,
                jitter_threshold_ms=args.jitter_threshold,
                bitrate=args.bitrate,
            )
            print("[PASS] Jitter test completed successfully")
            return 0
        finally:
            harness.close()

    except (TestFailure, SystemExit) as e:
        print(f"[FAIL] {e}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
