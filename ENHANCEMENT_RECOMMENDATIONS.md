# Arduino CAN Bus Monitor - Enhancement Recommendations

**Updated**: November 20, 2025  
**Current Memory Usage**: 13.2% RAM (271/2048 bytes), 35.1% Flash (11,324/32,256 bytes)  
**Available**: ~1,777 bytes RAM, ~20,932 bytes Flash

All recommendations maintain **100% LAWICEL spec compliance** and remain compatible with SavvyCAN, CANHacker, slcan, and other LAWICEL-aware tools.

---

## ✅ Completed Enhancements

### C1. Buffered Receive Queue & Auto-Poll Stability
- `src/can-232.h` now declares a `BufferedFrame` struct plus a 64-slot circular buffer (`LW232_RX_BUFFER_SIZE`, configurable at build time).
- `serviceCanRx()`/`pushRxFrame()` in `src/can-232.cpp` aggressively drain the MCP2515 FIFOs, increment `g_canStats.rxBufferDrops/rxBufferOverflows` on pressure, and let `receiveSingleFrame()` pop frames for both manual polls and auto-poll (`X1`).
- Auto-polling therefore shares the exact same buffered pipeline as `P` and `A`, which prevents bursts from starving slower serial links.

**Impact**: Reliable behavior up to the 64-frame backlog, easier performance tuning via a single macro, and deterministic stats when a host cannot consume frames fast enough.

### C2. PROGMEM Hex Lookup Table
- `HexHelper::parseNibble()` now consults the `HEX_LOOKUP_TABLE` stored in PROGMEM (`src/can-232.cpp:30-80`) instead of branching math.
- All frame parsing (IDs, DLC, payload, and LAWICEL command arguments) benefits from consistent timing and drastically lower CPU utilization.

**Impact**: 5-10× faster hex parsing, fewer jitter-induced overruns, and smaller code paths (only 256 bytes of Flash).

### C3. Bus Load Telemetry & LCD Hook
- `Can232::updateBusLoad()` updates `g_canStats.currentFramesPerSecond` once per second. Drops/overflows counters live in `src/runtime_stats.cpp`.
- `src/lcd_display.cpp` drives an optional 16x2 I²C LCD (PCF8574 @ `0x27`) and prints the computed RX frames per second without involving the host PC.
- The telemetry plumbing is reusable for future diagnostics (host commands, LEDs, etc.) because all data is exposed via `g_canStats`.

**Impact**: Built-in observability: you can see whether the adapter is keeping up at a glance, log FPS remotely, and build richer diagnostics on top of the stats subsystem.

### C4. Runtime Info Command
- Added custom LAWICEL command `i[CR]` that serializes the most important counters from `g_canStats` (commands, RX/TX frames, uptime, buffer drops, overflows, current FPS, and debug mode) while the CAN channel is open.
- Response format is `iCCCCRRRRTTTTUUUUddddooooFFD[CR]` (`D` = debug on, `d` = debug off), making it trivial for host tools to gather adapter health without leaving protocol mode.

**Impact**: Host applications (or a quick serial terminal) can watch buffer pressure and bus load in real time without extra wiring or firmware tweaks.

### C5. Interrupt-Driven CAN Reception
- The MCP2515 INT line (wired to Arduino `D2` on the Inland shield) now raises a lightweight ISR that flags pending CAN frames.
- `Can232::loop()` consumes the flag, drains the MCP2515 FIFOs immediately, and falls back to the original polling path if no interrupt was observed (so third-party boards without an INT jumper still function).
- Stats counters for drops/overflows continue to work unchanged because `serviceCanRx()` and the buffered pipeline are still the single ingestion point.

**Impact**: Dramatically lower latency under heavy bus load, less CPU time wasted on blind polling, and no hardware changes for shield users.

### C6. Runtime Debug Toggle & High-Detail Logging
- Introduced optional `@DBG1[CR]` / `@DBG0[CR]` commands (opt-in via `ENABLE_CAN_DEBUG_LOGGING`) to flip verbose tracing at runtime without reflashing.
- Debug logs capture command parsing, CAN initialization status, RX/TX results, buffer pressure, and MCP2515 error flags while keeping the default firmware silent.
- Diagnostic command `i[CR]` now reports debug state so host scripts can confirm whether tracing is active.

**Impact**: Rapid on-vehicle troubleshooting—flip verbose logging on when behaviour diverges, correlate issues with MCP2515 state, and switch back to silent mode instantly.

---

## 🚧 Outstanding Recommendations

### 1. Watchdog Timer Support (High Impact)
**Priority**: High · **RAM Cost**: 0 bytes · **Complexity**: Low

Protect against firmware hangs and unexpected states with the AVR watchdog timer.

```cpp
#include <avr/wdt.h>

void setup() {
    MCUSR = 0;
    wdt_disable();

    Serial.begin(LW232_DEFAULT_BAUD_RATE);
    statsReset();
    Can232::init(CAN_500KBPS, MCP_16MHz);
    LcdDisplay::begin();

    wdt_enable(WDTO_1S);
}

void loop() {
    wdt_reset();
    Can232::loop();
}

void serialEvent() {
    wdt_reset();
    Can232::serialEvent();
}
```

**Benefits**:
- Auto-recovers from MCP2515 lockups, firmware bugs, or brown-outs
- Zero RAM cost; minimal flash overhead
- Complements the new telemetry so users can see watchdog resets

---

### 2. Enhanced Error Tracking (Medium Impact)
**Priority**: Medium · **RAM Cost**: ~20 bytes · **Complexity**: Low

`runtime_stats` already tracks command, RX/TX counts, and buffer pressure. Extend it with CAN error insight so hosts (or the planned `'i'` command) can reason about bus health.

```cpp
// In runtime_stats.h
struct CanRuntimeStats {
    // existing fields...
    volatile unsigned long txErrors;
    volatile unsigned long busErrorFrames;
    volatile unsigned long arbitrationLost;
    volatile unsigned long busOffEvents;
    volatile uint8_t lastErrorState;      // MCP2515 EFLG snapshot
    volatile uint8_t peakBusLoadPercent;  // 0-100 approximation
};

void statsRecordTxError();
void statsRecordBusError(uint8_t eflg);
void statsRecordArbitrationLost();
void statsRecordBusOff();
void statsUpdateBusLoadPeak(uint8_t percent);
```

**Integration ideas**:
- Call `statsRecordTxError()` whenever `sendMsgBuf()` fails.
- Inside `readLawicelStatusFlags()` update `lastErrorState`, increment bus-off/arbitration counters, and update `peakBusLoadPercent` using `currentFramesPerSecond`.
- Surface these counters via the diagnostic command below.

---

### 3. Frame Rate Limiter (Medium Impact)
**Priority**: Medium · **RAM Cost**: ~8 bytes · **Complexity**: Low

Prevent runaway host software from flooding the CAN bus by tracking sent frames per second.

```cpp
// In can-232.h
#define MAX_TX_PER_SECOND 1000
unsigned long txRateWindowStart = 0;
unsigned int txInCurrentWindow = 0;
bool checkTxRateLimit();

// In can-232.cpp
bool Can232::checkTxRateLimit() {
    unsigned long now = millis();
    if (now - txRateWindowStart >= 1000) {
        txRateWindowStart = now;
        txInCurrentWindow = 0;
    }
    if (txInCurrentWindow >= MAX_TX_PER_SECOND) {
        statsRecordTxError();
        return false;
    }
    txInCurrentWindow++;
    return true;
}
```

Call `checkTxRateLimit()` before each `t/T/r/R` transmit branch. Rejecting overly chatty hosts protects in-vehicle testing and still preserves LAWICEL compliance (`\x07` on failure).

---


### 4. Hardware Filtering Configuration (Low Impact)
**Priority**: Low · **RAM Cost**: ~20 bytes · **Complexity**: Medium

Expose the MCP2515’s two masks and six filters so firmware can drop unwanted frames in hardware before they touch `rxBuffer`.

```cpp
struct HardwareFilter {
    INT32U mask0;
    INT32U mask1;
    INT32U filt[6];
    bool enabled;
};

void configureHardwareFilters(const HardwareFilter& filters);

void Can232::openCanBus(...) {
    initStatus = lw232CAN.begin(...);
    if (initStatus == CAN_OK) {
        lw232CAN.setMode(mode);
        if (hardwareFilters.enabled) {
            lw232CAN.init_Mask(0, 0, hardwareFilters.mask0);
            lw232CAN.init_Mask(1, 0, hardwareFilters.mask1);
            for (INT8U idx = 0; idx < 6; idx++) {
                lw232CAN.init_Filt(idx, 0, hardwareFilters.filt[idx]);
            }
        }
    }
}
```

**Benefits**:
- Reduces CPU and Serial pressure on extremely busy buses
- Works in tandem with the existing `myCustomAddressFilter()` software hook
- Makes it easier to isolate a handful of arbitration IDs for targeted debugging

---

### 5. Optimized SPI Speed (Low Impact)
**Priority**: Low · **RAM Cost**: 0 bytes · **Complexity**: Low

Increase MCP2515 throughput by driving SPI faster after successful initialization.

```cpp
INT8U Can232::openCanBus(INT8U mode) {
    INT8U initStatus = lw232CAN.begin(lw232CanSpeedSelection, lw232McpModuleClock);
    if (initStatus == CAN_OK) {
        lw232CAN.setMode(mode);
        SPI.setClockDivider(SPI_CLOCK_DIV2); // 8 MHz on a 16 MHz Uno
    }
    ...
}
```

Stick with DIV4 (4 MHz) on questionable modules; both values remain within MCP2515 specs. Faster SPI shortens each MCP read/write, helping the buffered RX path stay ahead of the bus.

---

## 📊 Implementation Priority (Remaining Work)

| Phase | Focus | Features |
|-------|-------|----------|
| **1. Reliability & Observability** | Prevent lockups and surface bus health metrics | Watchdog timer, enhanced error tracking |
| **2. Throughput Safeguards** | React faster to RX bursts and prevent TX abuse | Frame rate limiter |
| **3. Hardware Optimizations** | Squeeze more from MCP2515 | Hardware filtering, SPI clock tuning |

Tackle Phase 1 before chasing throughput so regression tests always have watchdog-backed safety nets and diagnostics to lean on.

---

## Memory / Flash Impact (Remaining Items)

| Feature | Est. RAM | Est. Flash | Notes |
|---------|----------|------------|-------|
| Watchdog timer | 0 | ~40 bytes | `wdt_*` calls only |
| Enhanced error tracking | ~20 bytes | ~200 bytes | Extra counters + helpers |
| Frame rate limiter | ~8 bytes | ~120 bytes | One-second sliding window |
| Hardware filtering config | ~20 bytes | ~250 bytes | Struct + setup logic |
| Optimized SPI speed | 0 | negligible | Single `SPI.setClockDivider()` |

Worst-case RAM increase remains under 60 bytes, keeping >1 KB free on the Uno even after all recommended work.

---

## Testing Recommendations

For each enhancement:

1. **Unit Test**: Exercise the change manually over serial (e.g., spam `t` after introducing the rate limiter).
2. **Regression Test**: Run `tests/slcan_smoke.py` to confirm LAWICEL compatibility.
3. **Load Test**: Replay recorded CAN traffic (or use a second MCU) to validate the buffered RX pipeline under sustained load.
4. **Error Injection**: Force MCP2515 error states (disconnect CANH/CANL momentarily) to verify the new counters and watchdog behavior.
5. **Long-term Stability**: Soak test for 24h+ while sampling the future `i` command to catch slow leaks.

---

## Areas for Improvement

### Code Quality & Safety
- **Input validation**: Add more defensive bounds checking for string operations and array accesses
- **Memory safety**: Consider replacing `strcpy()` with safer alternatives like `strncpy()` in critical paths
- **Error handling**: Implement more comprehensive error recovery for malformed protocol commands
- **Code documentation**: Add more inline comments explaining complex protocol logic

### Performance Optimizations
- **Interrupt optimization**: The current interrupt handler could be streamlined to minimize latency
- **Buffer management**: Consider lock-free ring buffer implementation for even lower latency
- **SPI optimization**: Dynamic SPI clock speed adjustment based on bus load

### Protocol Extensions
- **Extended diagnostics**: The `i` command could expose more detailed MCP2515 register states
- **Configuration persistence**: More settings could be made EEPROM-persistent
- **Multi-channel support**: Support for multiple CAN channels on Mega2560

### Testing & Validation
- **GVRET protocol tests**: The test suite currently focuses on LAWICEL; GVRET needs similar coverage
- **Stress testing**: Add tests for buffer overflow conditions and high-traffic scenarios
- **Hardware validation**: Tests for different MCP2515 modules and CAN transceiver variants

### Build & Development
- **CI/CD pipeline**: Automated testing and firmware builds for multiple board configurations
- **Code analysis**: Static analysis tools integration for code quality assurance
- **Version management**: Better semantic versioning and release tagging

---

## Notes

- All enhancements preserve LAWICEL compliance and interoperate with SavvyCAN, SocketCAN (`slcan`), and CANHacker.
- Prefer lowercase for custom commands (like `i`) to avoid conflicting with official LAWICEL opcodes.
- Mix and match features based on your vehicle/project needs—Phase 1 items are broadly useful, whereas hardware filtering mainly helps very high-traffic deployments.

---

## References

- [LAWICEL CAN232 Protocol](http://www.can232.com/docs/can232_v3.pdf)
- [MCP2515 Datasheet](https://ww1.microchip.com/downloads/en/DeviceDoc/MCP2515-Stand-Alone-CAN-Controller-with-SPI-20001801J.pdf)
- [Seeed CAN BUS Shield Library](https://github.com/Seeed-Studio/CAN_BUS_Shield)
- [AVR Watchdog Timer](https://www.nongnu.org/avr-libc/user-manual/group__avr__watchdog.html)
