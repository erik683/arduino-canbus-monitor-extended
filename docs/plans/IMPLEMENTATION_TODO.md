# Implementation TODO

- [ ] **Reliability & Observability**
  - [ ] Add AVR watchdog init/reset calls in `src/arduino-canbus-monitor.ino`; log last reset reason into telemetry.
  - [ ] Extend `runtime_stats.{h,cpp}` with CAN health metrics (tx errors, bus-off, arbitration lost, EFLG snapshot, FPS peak).
  - [ ] Expose new stats via the `i` command (serializer in `src/can-232.cpp`) and show FPS/health on LCD when present.

- [ ] **Protocol & Safety Completeness**
  - [ ] Implement LAWICEL `W/M/m/s` handling in `src/can-232.cpp` (enforce CAN-closed precondition, validate args).
  - [ ] Wire filter/mask/custom bitrate plumbing into `src/mcp_can.cpp` so changes hit the MCP2515.
  - [ ] Add TX rate limiter ahead of `t/T/r/R` branches with LAWICEL error response on throttle.
  - [ ] Make INT vs polling selectable at runtime for shields without INT wiring.

- [ ] **Throughput Tuning**
  - [ ] Add SPI clock divider presets (safe/fast) applied after successful CAN init; document default per board quality.
  - [ ] Provide buffer-size presets for Uno vs Mega and note RAM headroom in `README.md`.
  - [ ] Add optional hardware filter presets (accept list/range) applied on open to cut RX noise.

- [ ] **Testing & CI**
  - [ ] Replace Travis with GitHub Actions: build PlatformIO envs, clang-format/tidy, optional hardware run of `tests/slcan_smoke.py` behind `HARDWARE_TESTS=1`.
  - [ ] Extend harness with malformed-command negative cases and configurable timeouts; add fast simulated/loopback mode for PRs.

- [ ] **Developer Experience**
  - [ ] Tidy `platformio.ini` profiles (Uno/Mega), move ports to overrides, and document `pio run -t compiledb`.
  - [ ] Add pre-commit hooks for C++/Python format/lint and mention them in `README.md` or `CODEMAP.md`.
  - [ ] Consolidate filtering docs highlights into `README.md` while keeping `docs/plans` as roadmap summaries.

- [ ] **Releases & Automation**
  - [ ] Publish tagged firmware binaries for Uno/Mega with checksums and `wokwi.toml` references; maintain CHANGELOG.
  - [ ] Bundle `scripts/` into a one-command Windows/WSL setup flow and capture `tests/field-kit` logs as release artifacts for regression tracking.
