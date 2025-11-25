#!/usr/bin/env python3
"""
Field-ready runner that imports the slcan regression harness, exercises every
implemented LAWICEL/SLCAN command, and logs the results so you can transfer
the folder directly to a Windows machine.
"""

from __future__ import annotations

import argparse
import os
import platform
import signal
import sys
import time
from datetime import datetime
from pathlib import Path
from typing import Sequence

# Global flag for graceful shutdown
_shutdown_requested = False


def _signal_handler(signum, frame) -> None:
    """Handle interrupt signals for graceful shutdown."""
    global _shutdown_requested
    if not _shutdown_requested:
        _shutdown_requested = True
        print("\n[WARNING] Interrupt signal received. Shutting down gracefully...")
        print("[HINT] Press Ctrl+C again to force immediate exit")
        signal.signal(signal.SIGINT, signal.SIG_DFL)  # Allow force exit on second Ctrl+C
    else:
        print("\n[ERROR] Force exit requested")
        sys.exit(130)


def _prepare_import_path() -> None:
    root = Path(__file__).resolve().parents[1]
    if str(root) not in sys.path:
        sys.path.insert(0, str(root))


def _validate_baud_rate(baud: int) -> None:
    """Validate that the baud rate is supported by the SLCAN device."""
    # Import the UART baud table from the test harness
    try:
        from tests import slcan_smoke
        if baud not in slcan_smoke.UART_BAUD_TABLE:
            supported = ", ".join(str(b) for b in sorted(slcan_smoke.UART_BAUD_TABLE))
            raise ValueError(f"Unsupported baud rate {baud}. Supported rates: {supported}")
    except ImportError:
        # Fallback validation if import fails
        common_rates = [2400, 9600, 19200, 38400, 57600, 115200, 230400, 500000]
        if baud not in common_rates:
            supported = ", ".join(str(b) for b in sorted(common_rates))
            raise ValueError(f"Unsupported baud rate {baud}. Common rates: {supported}")


def _validate_log_path(log_path: str) -> None:
    """Validate that the log file path is writable."""
    try:
        log_dir = Path(log_path).parent
        if log_dir.exists():
            if not os.access(log_dir, os.W_OK):
                raise ValueError(f"Log directory {log_dir} is not writable")
        else:
            # Try to create the directory
            log_dir.mkdir(parents=True, exist_ok=True)
    except (OSError, PermissionError) as exc:
        raise ValueError(f"Cannot create/write to log path {log_path}: {exc}")


def _validate_port(port: str) -> None:
    """Validate that the port path exists or looks like a valid serial port."""
    if not port:
        raise ValueError("Port cannot be empty")

    # Check if it's a common serial port pattern
    import re
    if not (re.match(r"^COM\d+$", port) or  # Windows COM ports
            re.match(r"^/dev/tty\w+$", port) or  # Unix-style serial ports
            Path(port).exists()):  # Direct path exists
        raise ValueError(f"Port '{port}' does not exist and doesn't match common serial port patterns")


def _collect_environment_info() -> dict[str, str]:
    """Collect environment information for logging."""
    return {
        "timestamp": datetime.now().isoformat(),
        "python_version": f"{sys.version_info.major}.{sys.version_info.minor}.{sys.version_info.micro}",
        "platform": platform.platform(),
        "architecture": platform.architecture()[0],
        "hostname": platform.node(),
        "working_directory": str(Path.cwd()),
        "script_path": str(Path(__file__).resolve()),
    }


def _generate_log_filename(base_name: str) -> str:
    """Generate a timestamped log filename if not explicitly provided."""
    if base_name != "field-suite.log":
        return base_name  # User provided explicit name

    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    return f"field-suite_{timestamp}.log"


def _backup_existing_log(log_path: str) -> None:
    """Create a backup of existing log file if it exists."""
    log_file = Path(log_path)
    if log_file.exists():
        backup_name = log_file.with_suffix(f".backup_{int(time.time())}{log_file.suffix}")
        try:
            log_file.rename(backup_name)
            print(f"[INFO] Backed up existing log to {backup_name}")
        except OSError:
            pass  # Ignore backup failures


def _format_results(port: str, baud: int, results: Sequence[tuple[str, bool, str]], env_info: dict[str, str] | None = None, total_duration: float | None = None) -> str:
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    header = [f"Field suite run @ {timestamp}", f"Port: {port}", f"Baud: {baud}"]
    lines = [*header]

    if env_info:
        lines.append("Environment:")
        for key, value in env_info.items():
            lines.append(f"  {key}: {value}")
        lines.append("")

    lines.append("-" * 60)
    for name, ok, message in results:
        status = "PASS" if ok else "FAIL"
        line = f"[{status}] {name}"
        if message:
            line += f" - {message}"
        lines.append(line)
    lines.append("-" * 60)

    # Basic summary
    total_tests = len(results)
    failed_tests = sum(1 for _, ok, _ in results if not ok)
    passed_tests = total_tests - failed_tests
    success_rate = (passed_tests / total_tests * 100) if total_tests > 0 else 0

    lines.append(f"Summary: {passed_tests}/{total_tests} tests passed ({success_rate:.1f}%)")

    if total_duration is not None:
        lines.append(f"Total duration: {total_duration:.2f} seconds")
        if total_tests > 0:
            avg_duration = total_duration / total_tests
            lines.append(f"Average test time: {avg_duration:.2f} seconds")

    # Failure analysis
    if failed_tests > 0:
        lines.append("")
        lines.append("Failed tests:")
        for name, ok, message in results:
            if not ok:
                lines.append(f"  - {name}: {message}")

    return "\n".join(lines)


def _perform_device_health_check(harness) -> bool:
    """Perform basic device connectivity and responsiveness checks."""
    print("[INFO] Performing device health check...")

    checks = [
        ("Version query", "V\r", lambda resp: resp and len(resp) > 0),
        ("Serial number", "N\r", lambda resp: resp and len(resp) > 0),
        ("Close channel", "C\r", lambda resp: resp in (b"", b"\x07")),
    ]

    for check_name, command, validator in checks:
        try:
            print(f"  Checking {check_name}...", end="", flush=True)
            response = harness.transact(command)
            print(f" response: {response!r}", end="")
            if validator(response):
                print(" OK")
            else:
                print(f" FAILED (unexpected response)")
                return False
        except Exception as exc:
            print(f" FAILED ({exc})")
            return False

    print("[INFO] Device health check passed")
    return True


def _run_tests_with_progress(harness, fail_fast: bool) -> tuple[list[tuple[str, bool, str]], float]:
    """Run tests with progress indication and timing."""
    from tests import slcan_smoke

    results = []
    total_tests = len(slcan_smoke.TESTS)
    start_time = time.time()

    print(f"[INFO] Starting {total_tests} tests...")

    for i, (name, func) in enumerate(slcan_smoke.TESTS, 1):
        if _shutdown_requested:
            print(f"\n[WARNING] Test execution interrupted at test {i}/{total_tests}")
            break

        test_start = time.time()
        progress = f"[{i:2d}/{total_tests}]"
        print(f"{progress} Running {name}...", end="", flush=True)

        try:
            func(harness)
            duration = time.time() - test_start
            print(f" PASS ({duration:.2f}s)")
            results.append((name, True, ""))
        except slcan_smoke.RegressionFailure as exc:
            duration = time.time() - test_start
            print(f" FAIL ({duration:.2f}s) - {exc}")
            results.append((name, False, str(exc)))
            if fail_fast:
                print("[INFO] Stopping on first failure (--fail-fast)")
                break
        except Exception as exc:
            duration = time.time() - test_start
            print(f" ERROR ({duration:.2f}s) - {exc}")
            results.append((name, False, f"Unexpected error: {exc}"))
            if fail_fast:
                print("[INFO] Stopping on first failure (--fail-fast)")
                break
        finally:
            slcan_smoke.reset_test_state(harness)

    total_duration = time.time() - start_time
    passed = sum(1 for _, ok, _ in results if ok)
    failed = len(results) - passed

    print(f"[INFO] Completed {len(results)} tests in {total_duration:.2f}s "
          f"({passed} passed, {failed} failed)")

    return results, total_duration


def main(argv: Sequence[str] | None = None) -> int:
    # Install signal handler for graceful shutdown
    signal.signal(signal.SIGINT, _signal_handler)

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", help="Serial/SLCAN port (COMx on Windows).")
    parser.add_argument("--baud", type=int, default=115200, help="UART baud rate.")
    parser.add_argument(
        "--log",
        default="field-suite.log",
        help="Path to the log file (defaults to timestamped name like 'field-suite_20251122_143052.log').",
    )
    parser.add_argument(
        "--fail-fast",
        action="store_true",
        help="Stop after the first failing test (useful for quick field reports).",
    )
    parser.add_argument(
        "--overwrite-log",
        action="store_true",
        help="Recreate the log file instead of appending.",
    )
    args = parser.parse_args(argv)

    # Validate arguments
    try:
        _validate_baud_rate(args.baud)
        # Generate timestamped log filename by default
        log_path = _generate_log_filename(args.log)
        _validate_log_path(log_path)
        if args.port:
            _validate_port(args.port)
    except ValueError as exc:
        print(f"[ERROR] Argument validation failed: {exc}")
        return 1

    _prepare_import_path()

    # Collect environment information
    env_info = _collect_environment_info()

    # Import test harness
    try:
        from tests import slcan_smoke
    except ImportError as exc:  # pragma: no cover (environment-specific)
        print(f"[ERROR] Unable to import tests.slcan_smoke: {exc}")
        print("[HINT] Ensure you're running from the field-kit directory and all dependencies are installed")
        return 1

    # Detect port
    try:
        port = slcan_smoke._detect_port(args.port)
    except SystemExit as exc:  # pragma: no cover (harness detection error)
        print(f"[ERROR] Port detection failed: {exc}")
        print("[HINT] Check USB connection, device enumeration, and try specifying --port explicitly")
        return 1

    print(f"[INFO] Running regression suite against {port} @ {args.baud} baud.")

    # Initialize harness
    try:
        harness = slcan_smoke.SlcanHarness(port=port, baud=args.baud)
    except SystemExit as exc:
        print(f"[ERROR] Failed to initialize harness: {exc}")
        print("[HINT] Verify the port is not in use by another program and the device is properly connected")
        return 1
    except Exception as exc:
        print(f"[ERROR] Unexpected error initializing harness: {exc}")
        return 1

    # Perform device health check
    if not _perform_device_health_check(harness):
        print("[ERROR] Device health check failed - aborting test suite")
        print("[HINT] Check device connection, power, and basic SLCAN functionality")
        try:
            harness.close()
        except:
            pass
        return 1

    # Run tests with progress reporting
    total_duration = 0.0
    try:
        results, total_duration = _run_tests_with_progress(harness, args.fail_fast)
    except KeyboardInterrupt:
        print("\n[WARNING] Test execution interrupted by user")
        return 130  # Standard exit code for SIGINT
    except Exception as exc:
        print(f"[ERROR] Unexpected error during test execution: {exc}")
        return 1
    finally:
        try:
            harness.close()
        except Exception as exc:
            print(f"[WARNING] Error during final cleanup: {exc}")

    success = all(ok for _, ok, _ in results)
    summary_text = _format_results(port, args.baud, results, env_info, total_duration)
    print(summary_text)

    # Write log file
    try:
        log_mode = "w" if args.overwrite_log else "a"
        if args.overwrite_log:
            _backup_existing_log(log_path)
        with open(log_path, log_mode, encoding="utf-8") as fh:
            fh.write(summary_text)
            fh.write("\n\n")
        print(f"[INFO] Results written to {log_path}")
    except (OSError, PermissionError) as exc:
        print(f"[ERROR] Failed to write log file {log_path}: {exc}")
        print("[HINT] Check file permissions and disk space")
        return 1

    return 0 if success else 2


if __name__ == "__main__":
    raise SystemExit(main())
