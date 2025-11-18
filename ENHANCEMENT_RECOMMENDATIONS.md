# Arduino CAN Bus Monitor - Enhancement Recommendations

**Date**: November 17, 2025  
**Current Memory Usage**: 13.2% RAM (271/2048 bytes), 35.1% Flash (11324/32256 bytes)  
**Available**: ~1777 bytes RAM, ~20932 bytes Flash

All recommendations maintain **100% LAWICEL spec compliance** and work with existing tools (SavvyCAN, CANHacker, slcan, etc.).

---

## 🚀 Performance Improvements

### 1. Circular Buffer for RX Frames (High Impact)
**Priority**: High  
**RAM Cost**: ~600 bytes  
**Complexity**: Medium

Currently, frames are processed one-by-one in autopoll mode (max 5 per loop). Add a circular buffer to queue incoming frames during high traffic.

```cpp
// In can-232.h
#define LW232_RX_BUFFER_SIZE 32  // Adjust based on available RAM

struct CanFrame {
    INT32U id;
    INT8U len;
    INT8U data[8];
    INT16U timestamp;
    INT8U extended;
};

// Add to Can232 class private members:
CanFrame rxBuffer[LW232_RX_BUFFER_SIZE];
volatile INT8U rxHead = 0;
volatile INT8U rxTail = 0;

// Helper methods
bool rxBufferFull() { return ((rxHead + 1) % LW232_RX_BUFFER_SIZE) == rxTail; }
bool rxBufferEmpty() { return rxHead == rxTail; }
void pushRxFrame(const CanFrame& frame);
bool popRxFrame(CanFrame& frame);
```

**Benefits**:
- Reduces frame loss during high bus traffic
- Improves responsiveness during burst traffic
- Decouples CAN reception from serial transmission

---

### 2. Interrupt-Driven CAN Reception
**Priority**: High  
**RAM Cost**: ~10 bytes  
**Complexity**: Medium

The MCP2515 INT pin can trigger hardware interrupts. Replace polling with ISR-based reception for lower latency.

```cpp
// In can-232.h - add to Can232 class:
volatile bool canFramePending = false;

// In arduino-canbus-monitor.ino setup():
void setup() {
    Serial.begin(LW232_DEFAULT_BAUD_RATE);
    
    // Configure MCP2515 INT pin (typically pin 2 on Arduino Uno)
    pinMode(2, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(2), canISR, FALLING);
    
    statsReset();
    Can232::init(CAN_500KBPS, MCP_16MHz);
}

// ISR function:
void canISR() {
    Can232::setFramePendingFlag();  // Set flag for main loop
}

// In Can232 class:
static void setFramePendingFlag() {
    instance()->canFramePending = true;
}

// In loopFunc(), check flag instead of polling:
void Can232::loopFunc() {
    if (canFramePending) {
        canFramePending = false;
        // Process available frames
        while (CAN_MSGAVAIL == checkReceive()) {
            receiveSingleFrame();
        }
    }
    // ... rest of loop logic
}
```

**Benefits**:
- Lower latency (microseconds vs milliseconds)
- Better real-time performance
- Reduced CPU usage in idle state
- More deterministic timing

**Notes**: 
- MCP2515 INT pin needs to be connected to Arduino pin 2 or 3 (hardware interrupt capable)
- Consider disabling interrupts during critical CAN operations

---

### 3. Hex Parsing Lookup Table
**Priority**: Medium  
**Flash Cost**: 256 bytes  
**Complexity**: Low

Replace `HexHelper::parseNibble()` arithmetic with a lookup table for 5-10x speedup.

```cpp
// In can-232.cpp
const INT8U hexLookup[256] PROGMEM = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 0x00-0x07
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 0x08-0x0F
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 0x10-0x17
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 0x18-0x1F
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 0x20-0x27
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 0x28-0x2F
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,  // '0'-'7' (0x30-0x37)
    0x08, 0x09, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // '8'-'9' (0x38-0x3F)
    0xFF, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0xFF,  // 'A'-'F' (0x40-0x47)
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 0x48-0x4F
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 0x50-0x57
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 0x58-0x5F
    0xFF, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0xFF,  // 'a'-'f' (0x60-0x67)
    // ... rest filled with 0xFF
};

INT8U HexHelper::parseNibble(INT8U hex) {
    INT8U result = pgm_read_byte(&hexLookup[hex]);
    return (result == 0xFF) ? 0 : result;  // Return 0 for invalid chars
}
```

**Benefits**:
- 5-10x faster hex parsing
- Critical for TX/RX throughput
- More consistent timing
- Worth the 256 bytes of Flash

---

## 🛡️ Reliability Improvements

### 4. Watchdog Timer Support
**Priority**: High  
**RAM Cost**: 0 bytes  
**Complexity**: Low

Protect against firmware hangs and unexpected states with AVR watchdog timer.

```cpp
// In arduino-canbus-monitor.ino
#include <avr/wdt.h>

void setup() {
    // Disable watchdog if it was left enabled from previous reset
    MCUSR = 0;
    wdt_disable();
    
    Serial.begin(LW232_DEFAULT_BAUD_RATE);
    statsReset();
    Can232::init(CAN_500KBPS, MCP_16MHz);
    
    // Enable watchdog with 1-second timeout
    wdt_enable(WDTO_1S);
}

void loop() {
    wdt_reset();  // Pet the watchdog - reset timer
    Can232::loop();
}

void serialEvent() {
    wdt_reset();  // Also reset during serial processing
    Can232::serialEvent();
}
```

**Benefits**:
- Auto-recovery from unexpected states
- Production-ready reliability
- No memory cost
- Easy to implement

**Timeout Options**: WDTO_15MS, WDTO_30MS, WDTO_60MS, WDTO_120MS, WDTO_250MS, WDTO_500MS, WDTO_1S, WDTO_2S, WDTO_4S, WDTO_8S

---

### 5. Enhanced Error Tracking
**Priority**: Medium  
**RAM Cost**: ~20 bytes  
**Complexity**: Low

Expand runtime_stats to track detailed CAN bus error conditions.

```cpp
// In runtime_stats.h
struct CanRuntimeStats {
    volatile unsigned long commandCount;
    volatile unsigned long framesRx;
    volatile unsigned long framesTx;
    volatile unsigned long lastCommandMillis;
    volatile unsigned long lastRxMillis;
    volatile unsigned long lastTxMillis;
    
    // New error tracking fields:
    volatile unsigned long rxOverflows;      // MCP2515 RX buffer overflow
    volatile unsigned long txErrors;         // Failed transmissions
    volatile unsigned long busErrorFrames;   // Error frames detected
    volatile unsigned long arbitrationLost;  // Lost arbitration count
    volatile unsigned long busOffEvents;     // Bus-off state entries
    INT8U maxBusLoad;                        // Peak bus utilization %
    INT8U currentErrorState;                 // Current MCP2515 error state
};

// Add tracking functions:
void statsRecordRxOverflow();
void statsRecordTxError();
void statsRecordBusError();
void statsRecordArbitrationLost();
void statsRecordBusOff();
```

**Integration Points**:
- Call `statsRecordRxOverflow()` when `MCP_EFLG_RX0OVR` or `MCP_EFLG_RX1OVR` detected
- Call `statsRecordTxError()` when `sendMsgBuf()` returns error
- Call `statsRecordBusError()` when error warning threshold exceeded
- Call `statsRecordArbitrationLost()` when `MCP_TXB_MLOA_M` bit set
- Call `statsRecordBusOff()` when `MCP_EFLG_TXBO` detected

**Benefits**:
- Better diagnostics for production deployment
- Early detection of bus/hardware issues
- Useful for debugging intermittent problems

---

### 6. Frame Rate Limiter
**Priority**: Medium  
**RAM Cost**: ~8 bytes  
**Complexity**: Low

Prevent accidental bus flooding from misconfigured or buggy host software.

```cpp
// In can-232.h - add to Can232 class:
#define MAX_TX_PER_SECOND 1000  // Configurable limit
unsigned long txRateLimitWindow = 0;
unsigned int txCountInWindow = 0;

// In can-232.cpp - add helper method:
bool Can232::checkTxRateLimit() {
    unsigned long now = millis();
    if (now - txRateLimitWindow >= 1000) {
        // Reset window every second
        txRateLimitWindow = now;
        txCountInWindow = 0;
    }
    if (txCountInWindow >= MAX_TX_PER_SECOND) {
        return false;  // Rate limit exceeded
    }
    txCountInWindow++;
    return true;
}

// In parseAndRunCommand() - before all TX commands (t, T, r, R):
case LW232_CMD_TX11:
    if (!checkTxRateLimit()) {
        ret = LW232_ERR;
        break;
    }
    // ... existing TX11 code ...
```

**Benefits**:
- Prevents bus flooding from bad host software
- Protects other CAN devices on the bus
- Configurable threshold
- Negligible performance impact

**Configuration Options**:
- `MAX_TX_PER_SECOND`: Adjust based on bus speed (e.g., 500 for 125kbps, 2000 for 500kbps)
- Can make this runtime-configurable with custom command
- Consider different limits for standard vs extended frames

---

## ✨ Feature Additions (LAWICEL-Compatible)

### 7. Extended Status Command (Custom 'i' Command)
**Priority**: Medium  
**RAM Cost**: 0 bytes  
**Complexity**: Low

Add a non-standard diagnostic command that won't conflict with LAWICEL protocol.

```cpp
// In parseAndRunCommand() - add new case:
case 'i':  // Custom info command (lowercase to avoid conflicts)
    // Only allow when channel is open
    if (lw232CanChannelMode == LW232_STATUS_CAN_CLOSED) {
        ret = LW232_ERR;
        break;
    }
    
    Serial.print("i");  // Command echo
    
    // Commands count (16-bit)
    HexHelper::printFullByte((g_canStats.commandCount >> 8) & 0xFF);
    HexHelper::printFullByte(g_canStats.commandCount & 0xFF);
    
    // RX frames (16-bit)
    HexHelper::printFullByte((g_canStats.framesRx >> 8) & 0xFF);
    HexHelper::printFullByte(g_canStats.framesRx & 0xFF);
    
    // TX frames (16-bit)
    HexHelper::printFullByte((g_canStats.framesTx >> 8) & 0xFF);
    HexHelper::printFullByte(g_canStats.framesTx & 0xFF);
    
    // Uptime in seconds (16-bit, wraps at ~18 hours)
    INT16U uptimeSec = millis() / 1000;
    HexHelper::printFullByte((uptimeSec >> 8) & 0xFF);
    HexHelper::printFullByte(uptimeSec & 0xFF);
    
    // Error counters (if enhanced tracking implemented)
    HexHelper::printFullByte(g_canStats.rxOverflows & 0xFF);
    HexHelper::printFullByte(g_canStats.txErrors & 0xFF);
    
    // Bus load %
    HexHelper::printFullByte(g_canStats.maxBusLoad);
    
    break;
```

**Response Format**: `iCCCCRRRRTTTTUUUUOOEELL[CR]`
- `CCCC`: Command count (hex)
- `RRRR`: RX frame count (hex)
- `TTTT`: TX frame count (hex)
- `UUUU`: Uptime seconds (hex)
- `OO`: RX overflows (hex)
- `EE`: TX errors (hex)
- `LL`: Max bus load % (hex)

**Benefits**:
- Host software can query detailed stats
- No breaking changes to LAWICEL compatibility
- Useful for monitoring and diagnostics
- Can be ignored by existing tools

---

### 8. Hardware Filtering Configuration
**Priority**: Low  
**RAM Cost**: ~20 bytes  
**Complexity**: Medium

MCP2515 has built-in hardware filters (2 masks, 6 filters) that are currently unused. Implement to offload filtering from CPU.

```cpp
// In can-232.h - add to Can232 class:
struct HardwareFilter {
    INT32U mask0;      // RXB0 mask
    INT32U filter0;    // RXB0 filter 0
    INT32U filter1;    // RXB0 filter 1
    INT32U mask1;      // RXB1 mask
    INT32U filter2;    // RXB1 filter 0
    INT32U filter3;    // RXB1 filter 1
    INT32U filter4;    // RXB1 filter 2
    INT32U filter5;    // RXB1 filter 3
    bool enabled;
};

// Add method to configure:
void configureHardwareFilters(const HardwareFilter& filters);

// In can-232.cpp:
void Can232::configureHardwareFilters(const HardwareFilter& filters) {
    if (lw232CanChannelMode != LW232_STATUS_CAN_CLOSED) {
        return;  // Can only set when closed
    }
    
    // Configure after CAN.begin() in openCanBus()
    if (filters.enabled) {
        lw232CAN.init_Mask(0, 0, filters.mask0);    // Mask for RXB0
        lw232CAN.init_Filt(0, 0, filters.filter0);  // Filter 0 for RXB0
        lw232CAN.init_Filt(1, 0, filters.filter1);  // Filter 1 for RXB0
        
        lw232CAN.init_Mask(1, 0, filters.mask1);    // Mask for RXB1
        lw232CAN.init_Filt(2, 0, filters.filter2);  // Filter 0 for RXB1
        lw232CAN.init_Filt(3, 0, filters.filter3);  // Filter 1 for RXB1
        lw232CAN.init_Filt(4, 0, filters.filter4);  // Filter 2 for RXB1
        lw232CAN.init_Filt(5, 0, filters.filter5);  // Filter 3 for RXB1
    } else {
        // Accept all frames (default)
        lw232CAN.init_Mask(0, 0, 0x00000000);
        lw232CAN.init_Mask(1, 0, 0x00000000);
    }
}
```

**Benefits**:
- Offloads filtering to hardware
- Reduces CPU load on busy buses
- More efficient than software filtering
- Can filter up to 6 specific IDs or ID ranges

**Example Usage**:
```cpp
// Filter for IDs 0x100-0x10F (mask 0x7F0, filter 0x100)
HardwareFilter filter = {
    .mask0 = 0x7F0, .filter0 = 0x100, .filter1 = 0x100,
    .mask1 = 0x7F0, .filter2 = 0x100, .filter3 = 0x100,
    .filter4 = 0x100, .filter5 = 0x100,
    .enabled = true
};
```

**Notes**: This complements the existing `myCustomAddressFilter()` software filter.

---

### 9. Bus Load Monitoring
**Priority**: Low  
**RAM Cost**: ~12 bytes  
**Complexity**: Low

Calculate approximate CAN bus utilization percentage.

```cpp
// In can-232.h - add to Can232 class:
unsigned long lastBusLoadCalc = 0;
unsigned long lastBusLoadFrameCount = 0;

// In can-232.cpp - add method:
void Can232::updateBusLoad() {
    unsigned long now = millis();
    if (now - lastBusLoadCalc < 1000) {
        return;  // Update every second
    }
    
    unsigned long framesDelta = g_canStats.framesRx - lastBusLoadFrameCount;
    
    // CAN frame timing calculation (worst case):
    // Standard frame: 1 + 11 + 1 + 6 + DLC*8 + 15 + 1 + 1 + 1 + 7 = 44 + DLC*8 bits
    // Average ~100 bits per frame at typical DLC
    // For 500kbps: max theoretical ~5000 frames/sec, practical ~3500 frames/sec
    
    INT8U canSpeedKbps = 500;  // Would need to track current speed
    switch (lw232CanSpeedSelection) {
        case CAN_10KBPS:   canSpeedKbps = 10; break;
        case CAN_20KBPS:   canSpeedKbps = 20; break;
        case CAN_50KBPS:   canSpeedKbps = 50; break;
        case CAN_100KBPS:  canSpeedKbps = 100; break;
        case CAN_125KBPS:  canSpeedKbps = 125; break;
        case CAN_250KBPS:  canSpeedKbps = 250; break;
        case CAN_500KBPS:  canSpeedKbps = 500; break;
        case CAN_1000KBPS: canSpeedKbps = 1000; break;
        case CAN_83K3BPS:  canSpeedKbps = 83; break;
    }
    
    // Approximate bus load (assumes 100 bits per frame average)
    unsigned long bitsPerSec = framesDelta * 100;
    INT8U load = (bitsPerSec * 100) / (canSpeedKbps * 1000UL);
    
    // Track peak
    if (load > g_canStats.maxBusLoad) {
        g_canStats.maxBusLoad = load;
    }
    
    lastBusLoadFrameCount = g_canStats.framesRx;
    lastBusLoadCalc = now;
}

// Call from loopFunc():
void Can232::loopFunc() {
    // ... existing code ...
    
    if (lw232CanChannelMode != LW232_STATUS_CAN_CLOSED) {
        updateBusLoad();
    }
}
```

**Benefits**:
- Monitor bus health
- Identify congestion
- Useful diagnostic metric
- Low overhead

**Notes**: This is an approximation. True bus load would require analyzing bit stuffing, inter-frame spacing, etc.

---

### 10. Optimized SPI Speed
**Priority**: Low  
**RAM Cost**: 0 bytes  
**Complexity**: Low

MCP2515 supports up to 10MHz SPI clock. Arduino default may be slower.

```cpp
// In can-232.cpp - in openCanBus() after successful begin():
INT8U Can232::openCanBus(INT8U mode) {
    INT8U ret = LW232_OK;
    INT8U initStatus = CAN_OK;
    
#ifndef _MCP_FAKE_MODE_
    initStatus = lw232CAN.begin(lw232CanSpeedSelection, lw232McpModuleClock);
    if (initStatus == CAN_OK) {
        lw232CAN.setMode(mode);
        
        // Optimize SPI speed for faster transfers
        // MCP2515 supports up to 10MHz
        // Arduino Uno @ 16MHz: DIV2=8MHz, DIV4=4MHz (default), DIV8=2MHz
        SPI.setClockDivider(SPI_CLOCK_DIV2);  // 8MHz - test stability!
    }
#endif
    
    if (initStatus != CAN_OK) {
        ret = LW232_ERR;
    }
    return ret;
}
```

**Benefits**:
- 2-4x faster SPI transfers
- Improves frame processing throughput
- Reduces latency
- Free performance boost

**Caution**: 
- Test thoroughly - some cheap MCP2515 modules may be unstable at 8MHz
- Wiring quality matters at higher speeds
- If unstable, try `SPI_CLOCK_DIV4` (4MHz) as compromise

---

## 📊 Implementation Priority

### Phase 1: Critical Reliability (Recommended First)
1. ✅ **Watchdog Timer** - Zero cost, huge reliability win
2. ✅ **Enhanced Error Tracking** - Low cost, better diagnostics
3. ✅ **Rate Limiter** - Prevents bus flooding

**Total Cost**: ~28 bytes RAM, ~200 bytes Flash

### Phase 2: Performance Boost
4. ✅ **Interrupt-Driven Reception** - Lower latency, better timing
5. ✅ **Hex Lookup Table** - Major parsing speedup
6. ✅ **Circular RX Buffer** - Prevent frame loss

**Total Cost**: ~610 bytes RAM, ~500 bytes Flash

### Phase 3: Advanced Features
7. ✅ **Extended Status Command** - Diagnostics visibility
8. ✅ **Bus Load Monitoring** - Health metrics
9. ✅ **Optimized SPI** - Free performance

**Total Cost**: ~12 bytes RAM, ~300 bytes Flash

### Phase 4: Optional Enhancements
10. ✅ **Hardware Filtering** - For specific use cases

**Total Cost**: ~20 bytes RAM, ~400 bytes Flash

---

## Memory Budget Summary

| Phase | RAM Usage | Flash Usage | Available RAM | Available Flash |
|-------|-----------|-------------|---------------|-----------------|
| Current | 271 | 11,324 | 1,777 | 20,932 |
| Phase 1 | +28 | +200 | 1,749 | 20,732 |
| Phase 2 | +610 | +500 | 1,139 | 20,232 |
| Phase 3 | +12 | +300 | 1,127 | 19,932 |
| Phase 4 | +20 | +400 | 1,107 | 19,532 |
| **Total** | **941** | **12,724** | **1,107** | **19,532** |

**All phases fit comfortably within Arduino Uno limits!**

---

## Testing Recommendations

For each enhancement:

1. **Unit Test**: Test in isolation with manual serial commands
2. **Regression Test**: Run `tests/slcan_smoke.py` to ensure LAWICEL compatibility
3. **Load Test**: Use high-traffic CAN bus to verify performance
4. **Error Injection**: Simulate bus errors, disconnections, bad commands
5. **Long-term Stability**: Run for 24+ hours monitoring for memory leaks or hangs

---

## Notes

- All enhancements maintain 100% LAWICEL spec compliance
- Custom commands use lowercase letters to avoid conflicts
- Tested concepts based on MCP2515 datasheet and Arduino best practices
- Consider your specific use case when selecting features
- Start with reliability improvements (Phase 1) before performance optimizations

---

## References

- [LAWICEL CAN232 Protocol](http://www.can232.com/docs/can232_v3.pdf)
- [MCP2515 Datasheet](https://ww1.microchip.com/downloads/en/DeviceDoc/MCP2515-Stand-Alone-CAN-Controller-with-SPI-20001801J.pdf)
- [Arduino CAN BUS Shield Library](https://github.com/Seeed-Studio/CAN_BUS_Shield)
- [AVR Watchdog Timer](https://www.nongnu.org/avr-libc/user-manual/group__avr__watchdog.html)

