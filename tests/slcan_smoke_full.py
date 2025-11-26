#!/usr/bin/env python3
"""
Backward-compatible alias that delegates to tests.slcan_smoke.

Use this entry point when automation still expects slcan_smoke_full.py; it now
simply forwards CLI handling to the canonical regression harness.
"""

from pathlib import Path
import sys

# Ensure repository root is importable
ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from tests import slcan_smoke  # noqa: E402


if __name__ == "__main__":
    raise SystemExit(slcan_smoke.main())
