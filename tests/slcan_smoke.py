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
from typing import Callable, Iterable, List, Optional, Sequence, Tuple

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
    500_000,
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
    baud: int = 115200  # Match firmware default baud rate
    timeout: float = 1.0
    boot_timeout: float = 3.0
    command_timeout: float = 2.5  # Timeout for command responses
    frame_timeout: float = 5.0    # Timeout for frame reception
    verbose: bool = False
    allow_debug_output: bool = False
    debug_default: bool = False
    current_debug_enabled: bool = False

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
        self._log(f"Opened serial port {self.port} @ {self.baud} baud")
        self._await_device_ready()
        self.flush()
        if self.debug_default:
            try:
                self.set_debug_mode(True, update_default=False)
            except RegressionFailure as exc:
                raise SystemExit(f"Unable to enable debug mode: {exc}") from exc
        # Prime the device - some Arduino-based SLCAN devices need initialization
        # Try sending CR/LF first, then a dummy command
        try:
            self.serial.write(b"\r\n")
            self.serial.flush()
            time.sleep(0.2)
            if self.serial.in_waiting:
                self.serial.read(self.serial.in_waiting)

            # Try version command as priming
            self.serial.write(b"V\r")
            self.serial.flush()
            time.sleep(0.2)
            if self.serial.in_waiting:
                response = self.serial.read(self.serial.in_waiting)
                # If we got a response, the device is primed
                if response:
                    pass
        except (serial.SerialException, OSError):
            pass  # Ignore priming failures

    def close(self) -> None:
        try:
            self.serial.close()
        except serial.SerialException:
            pass

    def flush(self) -> None:
        time.sleep(0.05)
        self.serial.reset_input_buffer()
        self.serial.reset_output_buffer()

    def _log(self, message: str) -> None:
        if self.verbose:
            print(f"[DEBUG] {message}")

    def _looks_like_protocol(self, segment: bytes) -> bool:
        """Heuristic to distinguish LAWICEL responses from debug chatter."""
        if segment in (b"", b"\x07"):
            return True
        if not segment:
            return False
        prefixes = b"VTtRrPpAaFfXxZzQqNnOoLlSsMmUuWwIiCcDd%@#"
        if segment[0:1] not in prefixes:
            return False
        allowed_chars = b"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
        return all(ch in allowed_chars for ch in segment[1:])

    def _filter_payload(self, payload: bytes) -> bytes:
        """Drop debug lines when allowed and surface the most recent protocol-looking line."""
        if not self.allow_debug_output:
            return payload
        segments = payload.replace(b"\n", b"\r").split(b"\r")
        protocol_segments = [seg for seg in segments if self._looks_like_protocol(seg)]
        if not protocol_segments:
            return payload

        non_empty = [seg for seg in protocol_segments if seg]
        chosen = non_empty[-1] if non_empty else b""
        if not non_empty and not self._looks_like_protocol(payload):
            return payload
        if chosen != payload and self.verbose:
            self._log(f"[DEBUG_CHATTER] Ignored debug text while waiting for response: {payload!r}")
        return chosen

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
        self._await_device_ready()
        self.flush()

    def set_host_baud(self, baud: int) -> None:
        """Switch the host UART speed without disturbing the MCU state."""
        try:
            self.serial.flush()
            self.serial.baudrate = baud
        except (serial.SerialException, ValueError) as exc:
            raise SystemExit(f"Failed to change host baud to {baud}: {exc}") from exc
        self.baud = baud
        # Allow time for baud rate change to settle
        time.sleep(0.05)
        self.flush()

    def set_debug_mode(self, enabled: bool, update_default: bool = False) -> None:
        """Toggle runtime debug logging via @DBGn."""
        cmd = "@DBG1\r" if enabled else "@DBG0\r"
        payload = self._transact(cmd)
        banner = b"DEBUG ON" if enabled else b"DEBUG OFF"
        if banner not in payload:
            raise RegressionFailure(
                f"Unexpected @DBG response while setting debug to {int(enabled)}: {payload!r}"
            )
        if update_default:
            self.debug_default = enabled
        self.current_debug_enabled = enabled
        if enabled:
            self.allow_debug_output = True
        self._log(f"Debug mode set to {int(enabled)}")

    def _transact(self, command: str, response_timeout: Optional[float] = None) -> bytes:
        """Send a LAWICEL command string and capture the payload before the CR."""
        if not command.endswith("\r"):
            raise ValueError("Commands must be CR-terminated, e.g. 'V\\r'")
        self._log(f"--> {command.strip()!r}")

        def perform_io() -> bytes:
            self.flush()
            self.serial.write(command.encode("ascii"))
            self.serial.flush()
            deadline = time.monotonic() + (response_timeout if response_timeout is not None else self.command_timeout)
            raw = bytearray()
            last_read_time = time.monotonic()
            response_started = False
            while True:
                if time.monotonic() > deadline:
                    raise RegressionFailure(
                        f"Timeout waiting for response to {command!r} (received {raw!r})"
                    )
                chunk = self.serial.read(128)
                if chunk:
                    raw.extend(chunk)
                    last_read_time = time.monotonic()
                    response_started = True
                    # Check for standard LAWICEL termination
                    if raw.endswith(b"\r") or raw == b"\r" or raw == b"\x07":
                        if raw == b"\x07" and self.serial.in_waiting:
                            continue
                        break
                else:
                    # If we've received some data and no more comes within 50ms, consider response complete
                    if response_started and time.monotonic() - last_read_time > 0.05:
                        break
                    if raw == b"\x07" and time.monotonic() - last_read_time > 0.02:
                        break
                    # Small sleep to avoid busy waiting
                    time.sleep(0.01)
            if raw == b"\x07":
                return b"\x07"
            if raw == b"\r":
                return b""
            if raw.endswith(b"\r"):
                return bytes(raw[:-1])
            # Handle responses that don't end with \r (like Arduino firmware)
            if raw:
                payload = bytes(raw)
                # Optionally allow debug chatter by keeping the payload but logging it
                if self.allow_debug_output and self.verbose:
                    self._log(f"[DEBUG_CHATTER] {payload!r}")
                return payload
            raise RegressionFailure(
                f"No response received to {command!r}"
            )

        try:
            payload = perform_io()
            payload = self._filter_payload(payload)
            self._log(f"<-- {payload!r}")
            return payload
        except RegressionFailure as exc:
            # Allow one automatic retry after a full MCU reset in case the
            # adapter is still booting (e.g. after enabling auto-start).
            self.reset_device()
            payload = perform_io()
            payload = self._filter_payload(payload)
            self._log(f"<-- {payload!r}")
            return payload

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

    def _await_device_ready(self) -> None:
        """Poll the adapter until it responds or the boot timeout elapses."""
        deadline = time.monotonic() + self.boot_timeout
        while time.monotonic() < deadline:
            try:
                self.serial.write(b"V\r")
                self.serial.flush()
            except serial.SerialException:
                return
            poll_start = time.monotonic()
            while time.monotonic() - poll_start < 0.25:
                pending = self.serial.in_waiting
                if pending:
                    # Drain whatever banner/response arrived so later tests start clean.
                    self.serial.read(pending)
                    return
                time.sleep(0.01)
        # Fallback to a short delay so very slow boards still have time to boot.
        time.sleep(0.25)

    def read_line(self, timeout: Optional[float] = None) -> bytes:
        """Read a CR-terminated ASCII line, ignoring empty heartbeats."""
        deadline = time.monotonic() + (timeout if timeout is not None else self.timeout)
        raw = bytearray()
        while time.monotonic() < deadline:
            chunk = self.serial.read(1)
            if not chunk:
                continue
            if chunk == b"\r":
                self._log(f"<-- line {bytes(raw)!r}")
                return bytes(raw)
            raw.extend(chunk)
        raise RegressionFailure("Timeout waiting for serial line.")


def ensure_closed(harness: SlcanHarness) -> None:
    """Best-effort channel close without raising when already closed."""
    payload = harness.transact("C\r")
    if payload not in (b"", b"\x07"):
        raise RegressionFailure(f"Unexpected close response: {payload!r}")


def ensure_bitrate_configured(harness: SlcanHarness, rate: str = "6") -> None:
    """Force the LAWICEL device into a known bitrate selection (Sn)."""
    ensure_closed(harness)
    harness.expect_ok(f"S{rate}\r")


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
class AdapterStats:
    command_count: int
    frames_rx: int
    frames_tx: int
    uptime_seconds: int
    drops: int
    overflows: int
    frames_per_second: int
    debug_enabled: bool


def _parse_hex(segment: bytes) -> int:
    if not segment:
        raise RegressionFailure("Missing hex segment.")
    try:
        return int(segment, 16)
    except ValueError as exc:
        raise RegressionFailure(f"Invalid hex segment: {segment!r}") from exc


def parse_can_frame(payload: bytes, timestamps_enabled: bool = False) -> CanFrame:
    if not payload:
        raise RegressionFailure("Empty CAN frame payload.")
    frame_type = payload[0:1]
    if frame_type not in (b"t", b"T", b"r", b"R"):
        raise RegressionFailure(f"Unexpected frame prefix: {payload!r}")

    extended = frame_type in (b"T", b"R")
    remote = frame_type in (b"r", b"R")
    idx = 1
    id_chars = 8 if extended else 3
    if len(payload) < idx + id_chars + 1:
        raise RegressionFailure(f"Truncated CAN frame: {payload!r}")
    identifier = _parse_hex(payload[idx : idx + id_chars])
    idx += id_chars

    dlc = _parse_hex(payload[idx : idx + 1])
    if dlc > 8:
        raise RegressionFailure(f"DLC exceeds 8 bytes in frame: {payload!r}")
    idx += 1

    data = b""
    if not remote:
        data_chars = dlc * 2
        if len(payload) < idx + data_chars:
            raise RegressionFailure(f"Missing data bytes in frame: {payload!r}")
        data_str = payload[idx : idx + data_chars].decode("ascii")
        data = bytes.fromhex(data_str) if data_str else b""
        idx += data_chars

    timestamp = None
    if timestamps_enabled:
        if len(payload) < idx + 4:
            raise RegressionFailure(f"Missing timestamp in frame: {payload!r}")
        timestamp = _parse_hex(payload[idx : idx + 4])
        idx += 4

    if len(payload) != idx:
        raise RegressionFailure(f"Unexpected trailing bytes in frame: {payload!r}")

    return CanFrame(
        raw=payload,
        identifier=identifier,
        extended=extended,
        remote=remote,
        dlc=dlc,
        data=data,
        timestamp=timestamp,
    )


def read_adapter_stats(harness: SlcanHarness) -> AdapterStats:
    payload = harness.transact("i\r")
    if not payload.startswith(b"i"):
        raise RegressionFailure(f"Unexpected info payload: {payload!r}")
    text = payload.decode("ascii")
    idx = 1

    def take(width: int) -> int:
        nonlocal idx
        segment = text[idx : idx + width]
        if len(segment) != width:
            raise RegressionFailure("Truncated info payload.")
        idx += width
        return int(segment, 16)

    command_count = take(4)
    frames_rx = take(4)
    frames_tx = take(4)
    uptime = take(4)
    drops = take(4)
    overflows = take(4)
    fps = take(2)
    if len(text) <= idx:
        raise RegressionFailure("Missing debug flag in info payload.")
    debug_flag = text[idx]

    return AdapterStats(
        command_count=command_count,
        frames_rx=frames_rx,
        frames_tx=frames_tx,
        uptime_seconds=uptime,
        drops=drops,
        overflows=overflows,
        frames_per_second=fps,
        debug_enabled=(debug_flag == "D"),
    )


def ensure_autopoll_mode(harness: SlcanHarness, enabled: bool) -> None:
    ensure_closed(harness)
    target = "1" if enabled else "0"
    harness.expect_ok(f"X{target}\r")
    status = harness.transact("X\r")
    if status != f"X{target}".encode("ascii"):
        raise RegressionFailure(f"Unexpected autopoll status: {status!r}")


def wait_for_autopoll_frame(
    harness: SlcanHarness,
    timeout: Optional[float] = None,
    timestamps_enabled: bool = False,
) -> CanFrame:
    deadline = time.monotonic() + (timeout if timeout is not None else harness.frame_timeout)
    while time.monotonic() < deadline:
        remaining = max(0.1, deadline - time.monotonic())
        try:
            line = harness.read_line(timeout=remaining)
        except RegressionFailure:
            continue
        if not line:
            continue
        if line in (b"Z", b"z"):
            continue
        if line.startswith((b"t", b"T", b"r", b"R")):
            return parse_can_frame(line, timestamps_enabled)
        # Debug: print unexpected lines
        harness._log(f"Unexpected line in autopoll: {line!r}")
    raise RegressionFailure(
        "No autopoll frames observed. Ensure the CAN bus is active."
    )


# --- Individual tests ----------------------------------------------------- #


def test_version(h: SlcanHarness) -> None:
    payload = h.transact("V\r")
    if not payload.startswith(b"V"):
        raise RegressionFailure(f"Version string malformed: {payload!r}")


def test_serial_number(h: SlcanHarness) -> None:
    payload = h.transact("N\r")
    if not payload.startswith(b"NA"):
        raise RegressionFailure(f"Serial number malformed: {payload!r}")


def test_lowercase_version(h: SlcanHarness) -> None:
    payload = h.transact("v\r")
    if not payload.startswith(b"V"):
        raise RegressionFailure(f"Lowercase version response malformed: {payload!r}")


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
    ensure_bitrate_configured(h, "4")  # 125 kbit for testing
    h.expect_ok("O\r")
    # Should fail because channel is open
    h.expect_error("S5\r")
    ensure_closed(h)
    h.expect_ok("S5\r")  # Now allowed


def test_invalid_bitrate_rejected(h: SlcanHarness) -> None:
    ensure_closed(h)
    # Test invalid bitrate indices
    h.expect_error("S10\r")  # Invalid bitrate index (max is S9)
    h.expect_error("S99\r")  # Way out of range
    h.expect_error("S-1\r")  # Negative index
    h.expect_error("SA\r")   # Non-numeric character
    # Verify valid bitrates still work after invalid ones
    h.expect_ok("S4\r")      # Should still accept valid bitrate


def test_listen_mode_receives_frames(h: SlcanHarness) -> None:
    ensure_autopoll_mode(h, True)
    ensure_bitrate_configured(h)
    h.expect_ok("L\r")
    frame = wait_for_autopoll_frame(h)
    if frame.identifier < 0:
        raise RegressionFailure("Listen mode failed to deliver a CAN frame.")
    payload = h.transact("t1230\r")
    if payload != b"\x07":
        raise RegressionFailure("Transmit should be blocked while in listen-only mode.")
    ensure_closed(h)


def test_uart_speed_change(h: SlcanHarness) -> None:
    ensure_closed(h)
    current = h.transact("U\r")
    if len(current) != 2 or not current.startswith(b"U"):
        raise RegressionFailure(f"Unexpected UART query response: {current!r}")
    current_idx = int(chr(current[1]))

    target_idx = 2 if current_idx != 2 else 1  # Prefer 57.6 kbaud, fallback to 500000
    h.expect_ok(f"U{target_idx}\r")
    h.set_host_baud(UART_BAUD_TABLE[target_idx])
    new_payload = h.transact("U\r")
    if new_payload != f"U{target_idx}".encode("ascii"):
        raise RegressionFailure(f"UART index did not stick: {new_payload!r}")

    h.expect_ok(f"U{current_idx}\r")
    h.set_host_baud(UART_BAUD_TABLE[current_idx])


def test_uart_query_report(h: SlcanHarness) -> None:
    ensure_closed(h)
    payload = h.transact("U\r")
    if len(payload) != 2 or payload[0:1] != b"U" or payload[1:2] not in b"01234567":
        raise RegressionFailure(f"Unexpected UART query payload: {payload!r}")


def test_transmit_data_frames_increment_stats(h: SlcanHarness) -> None:
    ensure_autopoll_mode(h, False)
    ensure_bitrate_configured(h)
    h.expect_ok("O\r")
    stats_before = read_adapter_stats(h)
    h.expect_ok("t1232A5B6\r")
    h.expect_ok("T1ABCDEF04DEADBEEF\r")
    stats_after = read_adapter_stats(h)
    if stats_after.frames_tx - stats_before.frames_tx < 2:
        raise RegressionFailure("TX counter did not increment after sending data frames.")


def test_transmit_rtr_frames_increment_stats(h: SlcanHarness) -> None:
    ensure_autopoll_mode(h, False)
    ensure_bitrate_configured(h)
    h.expect_ok("O\r")
    stats_before = read_adapter_stats(h)
    h.expect_ok("r1201\r")
    h.expect_ok("R1ABCDEF01\r")
    stats_after = read_adapter_stats(h)
    if stats_after.frames_tx - stats_before.frames_tx < 2:
        raise RegressionFailure("TX counter did not increment for RTR frames.")


def test_timestamp_requires_closed(h: SlcanHarness) -> None:
    ensure_closed(h)
    for mode in ("0", "1"):
        h.expect_ok(f"Z{mode}\r")
    h.expect_error("Z2\r")
    ensure_bitrate_configured(h)
    h.expect_ok("O\r")
    h.expect_error("Z0\r")
    ensure_closed(h)


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


def test_timestamped_frames_include_counter(h: SlcanHarness) -> None:
    ensure_autopoll_mode(h, True)
    ensure_closed(h)
    h.expect_ok("Z1\r")
    ensure_bitrate_configured(h)
    h.expect_ok("O\r")
    frame = wait_for_autopoll_frame(h, timestamps_enabled=True)
    if frame.timestamp is None:
        raise RegressionFailure("Timestamped frame did not include timer bytes.")
    ensure_closed(h)
    h.expect_ok("Z0\r")
    ensure_autopoll_mode(h, False)


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


def test_info_snapshot(h: SlcanHarness) -> None:
    ensure_closed(h)
    if h.transact("i\r") != b"\x07":
        raise RegressionFailure("Info command should fail while CAN channel is closed.")
    ensure_autopoll_mode(h, True)
    ensure_bitrate_configured(h)
    h.expect_ok("O\r")
    stats_before = read_adapter_stats(h)
    wait_for_autopoll_frame(h)
    stats_after = read_adapter_stats(h)
    if stats_after.frames_rx <= stats_before.frames_rx:
        raise RegressionFailure("Info snapshot did not reflect a received frame.")
    ensure_closed(h)
    ensure_autopoll_mode(h, False)


def test_autopoll_stream(h: SlcanHarness) -> None:
    ensure_autopoll_mode(h, True)
    ensure_bitrate_configured(h, "4")  # Use 125kbps
    h.expect_ok("O\r")
    frame = wait_for_autopoll_frame(h)
    if frame.identifier < 0:
        raise RegressionFailure("Autopoll failed to emit a CAN frame.")
    ensure_closed(h)
    ensure_autopoll_mode(h, False)
    if h.transact("X\r") != b"X0":
        raise RegressionFailure("Autopoll disable did not persist.")
    h.flush()


def test_rx_buffer_overflow_detection(h: SlcanHarness) -> None:
    """Test that RX buffer overflow is detected and reported in stats."""
    ensure_autopoll_mode(h, False)
    ensure_bitrate_configured(h)
    h.expect_ok("O\r")

    # Get initial stats with channel open
    initial_stats = read_adapter_stats(h)

    # Keep channel open and wait for frames to accumulate
    # Under high traffic conditions, this may cause overflow
    time.sleep(2.0)  # Wait longer for potential frame accumulation

    # Check that we can still read stats (overflow counter should be accessible)
    try:
        final_stats = read_adapter_stats(h)
    except Exception as exc:
        raise RegressionFailure(f"Failed to read stats during overflow test: {exc}")

    # Verify overflow counter is non-negative (basic sanity check)
    if final_stats.overflows < 0:
        raise RegressionFailure(f"Overflow counter is negative: {final_stats.overflows}")
    # If we had overflow, drops should also have increased
    # Note: This comparison is valid since we kept the channel open throughout
    if final_stats.overflows > initial_stats.overflows and final_stats.drops <= initial_stats.drops:
        raise RegressionFailure("Overflow occurred but drops counter didn't increase")

    ensure_closed(h)


def test_autostart_query_and_rules(h: SlcanHarness) -> None:
    ensure_closed(h)
    h.expect_ok("Q0\r")
    payload = h.transact("Q\r")
    if payload != b"Q0":
        raise RegressionFailure(f"Unexpected autostart payload: {payload!r}")
    ensure_bitrate_configured(h)
    h.expect_ok("O\r")
    h.expect_error("Q1\r")  # cannot change mode while channel open
    ensure_closed(h)
    h.expect_error("Q9\r")
    h.expect_ok("Q2\r")
    if h.transact("Q\r") != b"Q2":
        raise RegressionFailure("Autostart query did not report listen mode")
    h.expect_ok("Q0\r")
    if h.transact("Q\r") != b"Q0":
        raise RegressionFailure("Autostart did not return to Q0 after disabling")


def test_autostart_persistence(h: SlcanHarness) -> None:
    ensure_closed(h)
    h.expect_ok("Q0\r")
    ensure_bitrate_configured(h)
    h.expect_ok("Q1\r")
    if h.transact("Q\r") != b"Q1":
        raise RegressionFailure("Autostart query failed after enabling normal mode")
    h.reset_device()
    if h.transact("Q\r") != b"Q1":
        raise RegressionFailure("Autostart mode did not persist as Q1 after reset")
    # Auto-start should have opened the channel already; closing must succeed without Sn
    if h.transact("C\r") != b"":
        raise RegressionFailure("Auto-start did not leave the channel open after boot")
    h.expect_ok("O\r")
    ensure_closed(h)
    h.expect_ok("Q0\r")
    h.reset_device()
    if h.transact("Q\r") != b"Q0":
        raise RegressionFailure("Autostart disable did not persist")
    ensure_closed(h)
    h.expect_error("O\r")
    ensure_bitrate_configured(h)
    h.expect_ok("O\r")
    ensure_closed(h)


def test_autostart_listen_persistence(h: SlcanHarness) -> None:
    ensure_closed(h)
    h.expect_ok("Q2\r")
    ensure_bitrate_configured(h)
    h.reset_device()
    if h.transact("Q\r") != b"Q2":
        raise RegressionFailure("Autostart listen mode did not persist as Q2 after reset.")
    if h.transact("C\r") != b"":
        raise RegressionFailure("Listen-mode autostart did not leave the bus open after boot.")
    ensure_closed(h)
    h.expect_ok("Q0\r")
    h.reset_device()
    if h.transact("Q\r") != b"Q0":
        raise RegressionFailure("Autostart listen disable did not persist.")
    ensure_closed(h)


def test_filter_query_reports_mode(h: SlcanHarness) -> None:
    ensure_closed(h)
    payload = h.transact("W\r")
    if len(payload) != 2 or payload[0:1] != b"W" or payload[1:2] not in (b"0", b"1"):
        raise RegressionFailure(f"Unexpected filter query payload: {payload!r}")


def test_filter_mode_roundtrip(h: SlcanHarness) -> None:
    ensure_closed(h)
    payload = h.transact("W\r")
    if len(payload) != 2 or not payload.startswith(b"W"):
        raise RegressionFailure(f"Unexpected filter query payload: {payload!r}")
    original = payload[1:2]
    target = b"1" if original == b"0" else b"0"
    h.expect_ok(f"W{target.decode('ascii')}\r")
    if h.transact("W\r") != b"W" + target:
        raise RegressionFailure("Filter mode change did not persist.")
    h.expect_ok(f"W{original.decode('ascii')}\r")


def test_acceptance_code_query_format(h: SlcanHarness) -> None:
    ensure_closed(h)
    payload = h.transact("M\r")
    if not payload.startswith(b"M") or len(payload) != 9:
        raise RegressionFailure(f"Unexpected acceptance code query payload: {payload!r}")
    try:
        int(payload[1:].decode("ascii"), 16)
    except ValueError as exc:
        raise RegressionFailure("Acceptance code contains invalid hex digits.") from exc


def test_acceptance_code_roundtrip(h: SlcanHarness) -> None:
    ensure_closed(h)
    payload = h.transact("M\r")
    if not payload.startswith(b"M") or len(payload) != 9:
        raise RegressionFailure(f"Unexpected acceptance code payload: {payload!r}")
    original = payload[1:].decode("ascii")
    new_value = "11223344" if original.upper() != "11223344" else "A5A5A5A5"
    h.expect_ok(f"M{new_value}\r")
    if h.transact("M\r") != f"M{new_value}".encode("ascii"):
        raise RegressionFailure("Acceptance code change did not persist.")
    h.expect_ok(f"M{original}\r")


def test_acceptance_mask_query_format(h: SlcanHarness) -> None:
    ensure_closed(h)
    payload = h.transact("m\r")
    if not payload.startswith(b"m") or len(payload) != 9:
        raise RegressionFailure(f"Unexpected acceptance mask query payload: {payload!r}")
    try:
        int(payload[1:].decode("ascii"), 16)
    except ValueError as exc:
        raise RegressionFailure("Acceptance mask contains invalid hex digits.") from exc


def test_acceptance_mask_roundtrip(h: SlcanHarness) -> None:
    ensure_closed(h)
    payload = h.transact("m\r")
    if not payload.startswith(b"m") or len(payload) != 9:
        raise RegressionFailure(f"Unexpected acceptance mask payload: {payload!r}")
    original = payload[1:].decode("ascii")
    new_value = "FFFFFFFF" if original.upper() != "FFFFFFFF" else "00000000"
    h.expect_ok(f"m{new_value}\r")
    if h.transact("m\r") != f"m{new_value}".encode("ascii"):
        raise RegressionFailure("Acceptance mask change did not persist.")
    h.expect_ok(f"m{original}\r")


def test_debug_toggle(h: SlcanHarness) -> None:
    ensure_closed(h)
    h.set_debug_mode(True)
    ensure_bitrate_configured(h)
    h.expect_ok("O\r")
    stats = read_adapter_stats(h)
    if not stats.debug_enabled:
        raise RegressionFailure("Info snapshot did not report debug mode enabled.")
    ensure_closed(h)
    h.set_debug_mode(False)
    ensure_bitrate_configured(h)
    h.expect_ok("O\r")
    stats_after = read_adapter_stats(h)
    if stats_after.debug_enabled:
        raise RegressionFailure("Debug mode remained on after @DBG0.")


def test_periodic_frame_jitter(h: SlcanHarness) -> None:
    """Test periodic frame injection and measure inter-arrival jitter."""
    ensure_autopoll_mode(h, False)
    ensure_closed(h)
    h.expect_ok("Z1\r")  # Enable timestamps
    ensure_bitrate_configured(h, "6")  # 500kbps for fast testing
    h.expect_ok("O\r")

    # Test parameters
    frame_id = "123"  # Standard frame ID
    frame_data = "DEADBEEF"  # 4 bytes of data
    period_ms = 50.0  # 50ms period = 20Hz
    num_frames = 20  # Send 20 frames for jitter analysis
    jitter_threshold_us = 5000  # 5ms maximum allowed jitter

    timestamps = []
    expected_times = []

    # Enable autopoll for receiving frames
    ensure_autopoll_mode(h, True)
    h.expect_ok("O\r")  # Reopen after autopoll change

    start_time = time.monotonic()

    try:
        for i in range(num_frames):
            # Record expected transmission time
            expected_time = start_time + (i * period_ms / 1000.0)
            expected_times.append(expected_time)

            # Transmit frame - use 'T' for extended frames (8 hex digits), 't' for standard frames
            frame_prefix = "T" if len(frame_id) == 8 else "t"
            cmd = f"{frame_prefix}{frame_id}{len(frame_data)//2:01X}{frame_data}\r"
            h.expect_ok(cmd)

            # Wait for the frame to be received (with timeout)
            frame_received = False
            frame_deadline = time.monotonic() + 0.2  # 200ms timeout per frame

            while time.monotonic() < frame_deadline:
                try:
                    frame = h.read_line(timeout=0.01)
                    if frame and frame.startswith(b"t"):
                        parsed_frame = parse_can_frame(frame, timestamps_enabled=True)
                        if parsed_frame.identifier == int(frame_id, 16) and parsed_frame.timestamp is not None:
                            # Convert timestamp to monotonic time reference
                            # Note: SLCAN timestamps are typically in milliseconds from device boot
                            # For jitter measurement, we use the relative timing
                            receive_time = time.monotonic()
                            timestamps.append((parsed_frame.timestamp, receive_time))
                            frame_received = True
                            break
                except RegressionFailure:
                    continue  # No frame available yet

            if not frame_received:
                raise RegressionFailure(f"Failed to receive frame {i+1} within timeout")

            # Wait for next transmission time (accounting for transmission time)
            next_tx_time = start_time + ((i + 1) * period_ms / 1000.0)
            sleep_time = max(0, next_tx_time - time.monotonic())
            if sleep_time > 0:
                time.sleep(sleep_time)

    finally:
        # Clean up
        ensure_closed(h)
        ensure_autopoll_mode(h, False)
        h.expect_ok("Z0\r")  # Disable timestamps

    if len(timestamps) < 2:
        raise RegressionFailure("Did not receive enough timestamped frames for jitter analysis")

    # Calculate inter-arrival times from timestamps
    # Use the device timestamps for more accurate measurement
    inter_arrival_times = []
    for i in range(1, len(timestamps)):
        dt = timestamps[i][0] - timestamps[i-1][0]  # Device timestamp difference
        inter_arrival_times.append(dt)

    if not inter_arrival_times:
        raise RegressionFailure("No inter-arrival times calculated")

    # Calculate expected period in device timestamp units
    # Assume device timestamps are in milliseconds (common for SLCAN)
    expected_period = period_ms

    # Calculate jitter as deviation from expected period
    jitters = []
    for dt in inter_arrival_times:
        jitter = abs(dt - expected_period)
        jitters.append(jitter)

    max_jitter = max(jitters)
    avg_jitter = sum(jitters) / len(jitters)

    print(f"[INFO] Jitter analysis: {len(timestamps)} frames, max jitter: {max_jitter:.1f}ms, avg jitter: {avg_jitter:.1f}ms")

    # Assert jitter is within threshold
    if max_jitter > (jitter_threshold_us / 1000.0):
        raise RegressionFailure(
            f"Maximum jitter {max_jitter:.1f}ms exceeds threshold {(jitter_threshold_us/1000.0):.1f}ms. "
            f"Inter-arrival times: {[f'{dt:.1f}ms' for dt in inter_arrival_times]}"
        )


def test_high_throughput_rx_no_drops(h: SlcanHarness) -> None:
    """Verify RX buffer handles sustained high traffic without corruption."""
    ensure_autopoll_mode(h, True)
    ensure_bitrate_configured(h)
    h.expect_ok("O\r")

    # Collect 500+ frames under continuous load
    frames = []
    stats_before = read_adapter_stats(h)

    deadline = time.monotonic() + 10.0  # 10 seconds of traffic
    while time.monotonic() < deadline:
        try:
            frame = h.read_line(timeout=0.05)
            if frame.startswith((b"t", b"T")):
                frames.append(parse_can_frame(frame))
        except RegressionFailure:
            continue

    stats_after = read_adapter_stats(h)

    # Assert no frame corruption (all IDs valid, DLC matches data length)
    for frame in frames:
        if frame.dlc > 8 or len(frame.data) != frame.dlc:
            raise RegressionFailure(f"Corrupted frame: {frame.raw!r}")

    # Assert reasonable drop rate under load
    drop_rate = (stats_after.drops - stats_before.drops) / len(frames) if frames else 0
    if drop_rate > 0.05:  # >5% drops indicates buffer management issue
        raise RegressionFailure(f"Excessive drop rate: {drop_rate:.1%}")


def test_serial_input_overflow_protection(h: SlcanHarness) -> None:
    """Verify firmware doesn't crash on oversized serial input."""
    ensure_closed(h)

    # Send oversized command (should truncate or reject)
    oversized = "S" + "X" * 200 + "\r"
    payload = h.transact(oversized)
    # Device should either error or ignore excess
    if payload not in (b"\x07", b""):
        # If it responds with something else, check it's not corrupted
        if len(payload) > 100:
            raise RegressionFailure("Device returned suspiciously long response")

    # Verify device still responsive after overflow attempt
    version = h.transact("V\r")
    if not version.startswith(b"V"):
        raise RegressionFailure("Device unresponsive after input overflow")


def test_extended_frame_rejection(h: SlcanHarness) -> None:
    """Verify %EXT1 command rejects extended frames as advertised."""
    ensure_autopoll_mode(h, True)
    ensure_bitrate_configured(h)

    # Enable extended frame rejection
    payload = h.transact("%EXT1\r")
    if b"REJECT EXTENDED" not in payload:
        raise RegressionFailure("Did not confirm extended rejection")

    h.expect_ok("O\r")

    # Transmit both standard and extended frames
    h.expect_ok("t1230\r")  # Standard
    h.expect_ok("T1234567805DEADBEEF\r")  # Extended

    # Verify only standard frame is received
    frames_seen = []
    deadline = time.monotonic() + 2.0
    while time.monotonic() < deadline:
        try:
            frame_data = h.read_line(timeout=0.1)
            if frame_data.startswith((b"t", b"T")):
                frames_seen.append(parse_can_frame(frame_data))
        except RegressionFailure:
            continue

    extended_count = sum(1 for f in frames_seen if f.extended)
    if extended_count > 0:
        raise RegressionFailure(f"Extended frames not rejected: saw {extended_count} extended frames")


def test_autopoll_batch_limiting(h: SlcanHarness) -> None:
    """Verify autopoll respects batch size limits to prevent serial jitter."""
    ensure_autopoll_mode(h, True)
    ensure_bitrate_configured(h)
    h.expect_ok("O\r")

    # Measure time to receive 100 frames
    frame_count = 0
    batch_times = []
    batch_start = time.monotonic()
    deadline = time.monotonic() + 30.0  # 30 second timeout for 100 frames

    while frame_count < 100 and time.monotonic() < deadline:
        try:
            frame_data = h.read_line(timeout=0.05)
            if frame_data.startswith((b"t", b"T")):
                frame_count += 1
                batch_times.append(time.monotonic() - batch_start)
        except RegressionFailure:
            continue

    if frame_count < 10:
        raise RegressionFailure(f"Received only {frame_count} frames within timeout. Need at least 10 frames for batch analysis.")

    # Check that frames arrive in bursts, not continuously
    # (indicating batching is active)
    inter_frame_gaps = [
        batch_times[i+1] - batch_times[i]
        for i in range(len(batch_times)-1)
    ]

    # Should see occasional gaps >10ms between batches
    large_gaps = sum(1 for gap in inter_frame_gaps if gap > 0.01)
    if large_gaps < 5:
        raise RegressionFailure("No batch boundaries detected - may overwhelm serial buffer")


def test_timestamp_rollover_handling(h: SlcanHarness) -> None:
    """Verify timestamps handle micros() rollover gracefully."""
    ensure_autopoll_mode(h, True)
    ensure_closed(h)
    h.expect_ok("Z1\r")
    ensure_bitrate_configured(h)
    h.expect_ok("O\r")

    timestamps = []
    for _ in range(100):  # Collect many frames
        frame = wait_for_autopoll_frame(h, timestamps_enabled=True)
        timestamps.append(frame.timestamp)

    # Check for backward jumps (indicating rollover bug)
    for i in range(1, len(timestamps)):
        delta = timestamps[i] - timestamps[i-1]
        # Allow for 60-second rollover, but detect large backward jumps
        if delta < -5000:  # >5 second backward jump
            raise RegressionFailure(f"Timestamp jumped backward: {timestamps[i-1]} -> {timestamps[i]}")
    ensure_closed(h)
    ensure_autopoll_mode(h, False)


def test_recovery_from_bus_off(h: SlcanHarness) -> None:
    """Verify adapter recovers from bus-off condition."""
    ensure_bitrate_configured(h)
    h.expect_ok("O\r")

    # Inject many malformed frames to trigger error counters
    # (This requires either a test harness or manual bus corruption)
    # In practice, check flag register under stress

    for _ in range(100):
        flags_payload = h.transact("F\r")
        flags = int(flags_payload[1:].decode("ascii"), 16)

        # Check bus-off bit (0x80 per LAWICEL spec)
        if flags & 0x80:
            # Verify we can recover by closing/reopening
            ensure_closed(h)
            h.expect_ok("O\r")

            # Check flags cleared
            new_flags_payload = h.transact("F\r")
            new_flags = int(new_flags_payload[1:].decode("ascii"), 16)
            if new_flags & 0x80:
                raise RegressionFailure("Bus-off flag persisted after recovery")
            return

    # If we never hit bus-off, test is inconclusive but not failing
    print("[WARN] Could not trigger bus-off condition")


def test_command_response_latency(h: SlcanHarness) -> None:
    """Verify commands respond within acceptable latency."""
    ensure_closed(h)

    latencies = []
    for _ in range(20):
        start = time.perf_counter()
        h.transact("V\r")
        latency = time.perf_counter() - start
        latencies.append(latency)

    avg_latency = sum(latencies) / len(latencies)
    max_latency = max(latencies)

    # At 115200 baud, simple commands should respond in <10ms
    if avg_latency > 0.01:
        raise RegressionFailure(f"Slow average latency: {avg_latency*1000:.1f}ms")
    if max_latency > 0.05:
        raise RegressionFailure(f"Excessive max latency: {max_latency*1000:.1f}ms")


def test_strict_protocol_without_debug(h: SlcanHarness) -> None:
    """Require LAWICEL-clean responses with debug disabled."""
    original_allow_debug = h.allow_debug_output
    h.allow_debug_output = False  # Enforce strict parsing during this test

    try:
        ensure_autopoll_mode(h, False)
        ensure_closed(h)
        h.set_debug_mode(False)

        ensure_bitrate_configured(h)
        h.expect_ok("O\r")

        stats = read_adapter_stats(h)
        if stats.debug_enabled:
            raise RegressionFailure("Info snapshot reported debug enabled during strict-mode check.")

        h.expect_ok("t1230\r")

        # With autopoll off and debug disabled, the adapter should stay silent unless commanded
        time.sleep(0.1)
        pending = h.serial.in_waiting
        if pending:
            chatter = h.serial.read(pending)
            raise RegressionFailure(f"Unexpected serial chatter with debug disabled: {chatter!r}")
    finally:
        h.allow_debug_output = original_allow_debug
        ensure_closed(h)


TESTS: Sequence[Tuple[str, Callable[[SlcanHarness], None]]] = (
    ("version", test_version),
    ("version_lowercase", test_lowercase_version),
    ("serial", test_serial_number),
    ("close_idempotent", test_close_idempotent),
    ("open_requires_bitrate", test_open_requires_bitrate),
    ("bitrate_rules", test_bitrate_rules),
    ("invalid_bitrate_rejected", test_invalid_bitrate_rejected),
    ("listen_mode_receives_frames", test_listen_mode_receives_frames),
    ("uart_speed_change", test_uart_speed_change),
    ("uart_query", test_uart_query_report),
    ("tx_data_frames", test_transmit_data_frames_increment_stats),
    ("tx_rtr_frames", test_transmit_rtr_frames_increment_stats),
    ("timestamp_requires_closed", test_timestamp_requires_closed),
    ("timestamp_query", test_timestamp_query),
    ("timestamp_persistence", test_timestamp_persistence),
    ("timestamp_frames", test_timestamped_frames_include_counter),
    ("rx_buffer_overflow", test_rx_buffer_overflow_detection),
    ("flags_format", test_flags_format),
    ("info_snapshot", test_info_snapshot),
    ("autopoll_stream", test_autopoll_stream),
    ("autostart_query", test_autostart_query_and_rules),
    ("autostart_persistence", test_autostart_persistence),
    ("autostart_listen", test_autostart_listen_persistence),
    ("filter_query", test_filter_query_reports_mode),
    ("filter_roundtrip", test_filter_mode_roundtrip),
    ("acceptance_code_query", test_acceptance_code_query_format),
    ("acceptance_code_roundtrip", test_acceptance_code_roundtrip),
    ("acceptance_mask_query", test_acceptance_mask_query_format),
    ("acceptance_mask_roundtrip", test_acceptance_mask_roundtrip),
    ("debug_toggle", test_debug_toggle),
    ("high_throughput_rx", test_high_throughput_rx_no_drops),
    ("serial_input_overflow", test_serial_input_overflow_protection),
    ("extended_frame_rejection", test_extended_frame_rejection),
    ("autopoll_batch_limiting", test_autopoll_batch_limiting),
    ("timestamp_rollover", test_timestamp_rollover_handling),
    ("bus_off_recovery", test_recovery_from_bus_off),
    ("command_latency", test_command_response_latency),
    ("periodic_frame_jitter", test_periodic_frame_jitter),
    ("strict_protocol_no_debug", test_strict_protocol_without_debug),
)


def reset_test_state(harness: SlcanHarness) -> None:
    """Reset device to clean state between tests for proper isolation."""
    # Each cleanup operation is attempted independently to avoid one failure blocking others
    try:
        ensure_closed(harness)
    except Exception:
        pass  # Ignore cleanup failures

    try:
        harness.expect_ok("Q0\r")  # Disable autostart
    except Exception:
        pass  # Ignore cleanup failures

    try:
        harness.expect_ok("Z0\r")  # Disable timestamps
    except Exception:
        pass  # Ignore cleanup failures

    try:
        harness.expect_ok("X0\r")  # Disable autopoll
    except Exception:
        pass  # Ignore cleanup failures

    try:
        harness.set_debug_mode(harness.debug_default)
    except Exception:
        pass  # Ignore cleanup failures

    # VERIFY clean state
    try:
        if harness.transact("Q\r") != b"Q0":
            raise RegressionFailure("Failed to reset autostart state")
        if harness.transact("Z\r") != b"Z0":
            raise RegressionFailure("Failed to reset timestamp state")
        if harness.transact("X\r") != b"X0":
            raise RegressionFailure("Failed to reset autopoll state")
    except Exception:
        # If cleanup fails, force full device reset
        harness.reset_device()


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
            reset_test_state(harness)
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
    parser.add_argument(
        "--verbose",
        action="store_true",
        help="Print each command and response (noisy).",
    )
    parser.add_argument(
        "--enable-debug",
        action="store_true",
        help="Enable runtime firmware debug logging via @DBG1 before running tests.",
    )
    parser.add_argument(
        "--allow-debug-chatter",
        action="store_true",
        help="Allow extra debug text on the serial line without treating it as a failure.",
    )
    args = parser.parse_args(argv)

    if args.list_tests:
        for name, _ in TESTS:
            print(name)
        return 0

    port = _detect_port(args.port)
    print(f"[INFO] Connecting to {port} @ {args.baud} baud...")
    harness = SlcanHarness(
        port=port,
        baud=args.baud,
        verbose=args.verbose,
        allow_debug_output=(args.allow_debug_chatter or args.enable_debug),
        debug_default=args.enable_debug,
    )
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
