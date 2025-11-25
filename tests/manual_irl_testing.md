# Manual IRL Testing Procedures for Custom Serial Commands

This document provides comprehensive manual testing procedures for in-depth real-world validation of custom serial commands in the Arduino CAN Bus Monitor firmware. These procedures go beyond the automated smoke test suite and are designed for thorough field testing and debugging.

## Overview

The automated `slcan_smoke.py` test suite provides basic validation of LAWICEL/SLCAN command functionality. However, real-world usage often reveals edge cases, timing issues, and environmental dependencies that require manual testing. This document covers:

- `poll_single_frame` (P command) - Single frame polling from RX buffer
- `poll_all_frames` (A command) - Bulk frame polling from RX buffer
- `listen_mode_receives_frames` (L command) - Listen-only mode reception
- `timestamp_frames` (Z command) - Timestamp functionality with frame reception
- `autopoll_stream` (X command) - Automatic frame streaming
- `filter_query` (W command) - Filter mode querying
- `filter_roundtrip` (W command) - Filter mode configuration and persistence

## Prerequisites

### Hardware Requirements

- Arduino UNO with MCP2515 CAN shield
- USB connection to host computer
- CAN bus connection with active traffic (125 kbps recommended)
- CAN analyzer/sniffer (SavvyCAN, PCAN, or similar) for verification
- At least one other CAN node generating traffic continuously

### Software Requirements

- Serial terminal program (minicom, screen, PuTTY, or similar)
- Python 3.9+ with pyserial for optional helper script
- CAN bus monitoring software for traffic verification

### Environment Setup

1. **Connect Hardware**:
   - Plug Arduino into USB port
   - Connect CAN shield to live 125 kbps bus with active traffic
   - Verify CAN bus has continuous frame transmission (recommended: 1-10 frames/second)
   - Ensure proper CAN bus termination (120Ω resistors at both ends)

2. **Identify Serial Port**:
   - Linux: `/dev/ttyACM0` or `/dev/ttyUSB0`
   - Windows: `COM3`, `COM4`, etc.
   - macOS: `/dev/tty.usbmodemXXXX`

3. **Configure Serial Terminal**:
   - Baud rate: 500000 (default) or as configured
   - Data bits: 8
   - Stop bits: 1
   - Parity: None
   - Flow control: None
   - Local echo: Off (important - commands should not echo back)

### Bus Traffic Requirements

For meaningful testing, the CAN bus must have:
- **Continuous traffic**: At least 1 frame every few seconds
- **Known frame IDs**: Document expected frame IDs and patterns
- **Acknowledged transmissions**: Partner node must ACK transmitted frames
- **Stable bitrate**: 125 kbps (S4 command) recommended
- **No excessive errors**: Monitor for BUS OFF or ERROR states

## Serial Terminal Setup

### Using minicom (Linux/macOS)

```bash
# Install minicom if needed
sudo apt-get install minicom  # Ubuntu/Debian
brew install minicom          # macOS

# Connect to device
minicom -D /dev/ttyACM0 -b 500000

# Configure (Ctrl+A then Z for menu):
# - No hardware flow control
# - No software flow control
# - No line wrapping
# - Local echo off
```

### Using screen (Linux/macOS)

```bash
# Connect to device
screen /dev/ttyACM0 500000

# Exit with Ctrl+A then K, then Y
```

### Using PuTTY (Windows)

1. Open PuTTY
2. Select "Serial" connection type
3. Enter COM port (e.g., COM3)
4. Set speed to 500000
5. Under "Terminal" settings:
   - Uncheck "Local echo"
   - Uncheck "Local line editing"
6. Connect

### Basic Device Verification

Before starting tests, verify basic device functionality:

```bash
# Version check
V[ENTER]
# Expected: V1013 or similar

# Serial number
N[ENTER]
# Expected: NA123 or similar

# Close channel (ensure clean state)
C[ENTER]
# Expected: [ENTER] (empty response) or [BEL] (if already closed)

# Set bitrate
S4[ENTER]
# Expected: [ENTER] (empty response)

# Open channel
O[ENTER]
# Expected: [ENTER] (empty response)
```

## Testing Methodology

### Test Execution Guidelines

1. **Start Clean**: Always close channel (`C[ENTER]`) and reset device between test scenarios
2. **Document Everything**: Record exact command sequences and responses
3. **Verify with Sniffer**: Use CAN analyzer to confirm bus behavior
4. **Test Edge Cases**: Empty buffers, full buffers, error conditions
5. **Timing Matters**: Note response times and delays between operations
6. **State Awareness**: Track channel state, autopoll mode, timestamp mode, etc.

### Response Format Conventions

- `[ENTER]`: Empty response (just CR/LF)
- `[BEL]`: Error bell (ASCII 7, may display as ^G or beep)
- `text`: Literal response text
- `(hex)`: Hexadecimal values
- `...`: Variable content

### Error Handling

Common error responses:
- `[BEL]`: Command failed or invalid
- Timeout: No response within 2-5 seconds
- Garbage: Unexpected characters or corruption

## Troubleshooting Common Issues

### No CAN Traffic Available

**Symptoms**: Polling commands (`P`, `A`) return `[BEL]` or timeout
**Solutions**:
1. Verify CAN bus connection and termination
2. Check for other CAN nodes transmitting
3. Use CAN sniffer to confirm traffic presence
4. Test with different bus segments if available

### Serial Communication Issues

**Symptoms**: Commands ignored, garbled responses, connection drops
**Solutions**:
1. Verify baud rate (500000 default)
2. Check USB cable and port
3. Try different serial terminal programs
4. Reset Arduino (unplug/replug USB)

### Channel State Confusion

**Symptoms**: Commands fail unexpectedly, inconsistent behavior
**Solutions**:
1. Always start tests with `C[ENTER]` (close channel)
2. Set bitrate explicitly: `S4[ENTER]`
3. Open channel: `O[ENTER]`
4. Verify state with `F[ENTER]` (status flags)

### Buffer Overflow Conditions

**Symptoms**: Missing frames, unexpected behavior during high traffic
**Solutions**:
1. Monitor with `i[ENTER]` (diagnostic snapshot)
2. Check overflow counter in stats
3. Reduce frame rate or increase polling frequency
4. Clear buffer with repeated `A[ENTER]` commands

## Test Scenarios

Each command section below includes:
- **Basic Functionality**: Core behavior verification
- **Edge Cases**: Boundary conditions and error scenarios
- **Stress Testing**: High load and timing sensitivity
- **Integration Testing**: Interaction with other commands
- **Expected Responses**: Exact response formats to verify

---

## poll_single_frame (P Command)

Polls a single CAN frame from the RX buffer. Only works when channel is open and autopoll is disabled.

### Basic Functionality

#### Test 1.1: Single Frame Polling with Active Traffic
```
# Setup
C[ENTER]    # Close channel
S4[ENTER]   # Set 125kbps bitrate
O[ENTER]    # Open channel
X0[ENTER]   # Disable autopoll

# Test polling
P[ENTER]
```
**Expected**: Single CAN frame in LAWICEL format, e.g., `t1234AABB` or `T1ABCDEF04DEADBEEF`
**Verification**: Use CAN sniffer to confirm frame appears on bus

#### Test 1.2: Frame Type Variations
Test polling with different frame types:
```
# Standard data frame
P[ENTER]    # Expected: tiiiiddd... (11-bit ID)

# Extended data frame
P[ENTER]    # Expected: Tiiiiiiiiddd... (29-bit ID)

# Standard RTR frame
P[ENTER]    # Expected: riiiil (11-bit RTR)

# Extended RTR frame
P[ENTER]    # Expected: Riiiiiiiil (29-bit RTR)
```

#### Test 1.3: DLC Value Testing
Test frames with different data lengths:
```
# Poll frames with DLC 0-8
P[ENTER]    # Check DLC field in response
```
**Expected**: Correct DLC value in frame format (e.g., `t12300` for DLC=0, `t1232AABB` for DLC=2)

### Edge Cases

#### Test 1.4: Empty Buffer Polling
```
# Ensure no frames in buffer
C[ENTER]    # Close channel
S4[ENTER]   # Set bitrate
O[ENTER]    # Open channel
X0[ENTER]   # Disable autopoll

# Wait 10+ seconds for any stale frames to be processed
# Then poll immediately
P[ENTER]
```
**Expected**: `[BEL]` (error bell, no frames available)

#### Test 1.5: Channel Closed State
```
C[ENTER]    # Ensure channel closed
P[ENTER]
```
**Expected**: `[BEL]` (command only works with open channel)

#### Test 1.6: Autopoll Enabled State
```
C[ENTER]
S4[ENTER]
O[ENTER]
X1[ENTER]   # Enable autopoll
P[ENTER]    # Should fail
```
**Expected**: `[BEL]` (polling disabled when autopoll active)

### Stress Testing

#### Test 1.7: Rapid Polling Sequence
```
# Setup
C[ENTER]
S4[ENTER]
O[ENTER]
X0[ENTER]

# Poll repeatedly (10-20 times quickly)
P[ENTER]
P[ENTER]
P[ENTER]
# ... continue
```
**Expected**: Each poll returns a frame or `[BEL]`, no crashes or corruption

#### Test 1.8: High Traffic Buffer Management
With continuous high-rate traffic (>10 frames/sec):
```
# Setup
C[ENTER]
S4[ENTER]
O[ENTER]
X0[ENTER]

# Poll during peak traffic
P[ENTER]
P[ENTER]
P[ENTER]
```
**Expected**: Frames returned without excessive delay, no buffer corruption

#### Test 1.9: Buffer Overflow Recovery
```
# Setup with high traffic
C[ENTER]
S4[ENTER]
O[ENTER]
X0[ENTER]

# Allow buffer to fill (don't poll for 30+ seconds)
# Then start polling
P[ENTER]
P[ENTER]
P[ENTER]
```
**Expected**: Frames returned, check `i[ENTER]` for overflow counter increment

### Integration Testing

#### Test 1.10: Polling with Timestamps Disabled
```
C[ENTER]
Z0[ENTER]   # Disable timestamps
S4[ENTER]
O[ENTER]
X0[ENTER]
P[ENTER]
```
**Expected**: Standard frame format without timestamp bytes

#### Test 1.11: Polling with Timestamps Enabled
```
C[ENTER]
Z1[ENTER]   # Enable timestamps
S4[ENTER]
O[ENTER]
X0[ENTER]
P[ENTER]
```
**Expected**: Frame format with 4-digit hex timestamp appended

#### Test 1.12: Mode Switching During Polling
```
C[ENTER]
S4[ENTER]
O[ENTER]
X0[ENTER]

# Start polling sequence
P[ENTER]

# Switch to autopoll mid-operation
X1[ENTER]

# Try polling again
P[ENTER]
```
**Expected**: First poll succeeds, second poll fails with `[BEL]`

#### Test 1.13: Filter Interaction
```
C[ENTER]
W0[ENTER]   # Dual filter mode
M00000000[ENTER]  # Set acceptance code
mFFFFFFFF[ENTER]  # Set acceptance mask
S4[ENTER]
O[ENTER]
X0[ENTER]
P[ENTER]
```
**Expected**: Only frames matching filter criteria are returned

### Timing and Performance

#### Test 1.14: Polling Response Time
```
# Setup
C[ENTER]
S4[ENTER]
O[ENTER]
X0[ENTER]

# Time the response to P command
P[ENTER]
```
**Expected**: Response within 100ms when frames available

#### Test 1.15: Polling Rate Limits
```
# Setup
C[ENTER]
S4[ENTER]
O[ENTER]
X0[ENTER]

# Poll as fast as possible for 10 seconds
# Count successful polls vs total attempts
```
**Expected**: No more than hardware buffer limits (64 frames), no crashes

---

## poll_all_frames (A Command)

Drains all pending CAN frames from the RX buffer. Returns frames followed by an "A" terminator. Only works when channel is open and autopoll is disabled.

### Basic Functionality

#### Test 2.1: Basic Buffer Draining
```
# Setup
C[ENTER]    # Close channel
S4[ENTER]   # Set 125kbps bitrate
O[ENTER]    # Open channel
X0[ENTER]   # Disable autopoll

# Allow buffer to accumulate frames (wait 5-10 seconds)
# Then drain all at once
A[ENTER]
```
**Expected**: Multiple frames followed by `A[ENTER]`, e.g.:
```
t1234AABB[ENTER]
t5678CCDD[ENTER]
T1ABCDEF04DEADBEEF[ENTER]
A[ENTER]
```

#### Test 2.2: Empty Buffer Draining
```
# Setup
C[ENTER]
S4[ENTER]
O[ENTER]
X0[ENTER]

# Wait for any stale frames, then drain immediately
A[ENTER]
```
**Expected**: Only `A[ENTER]` (no frames available)

#### Test 2.3: Mixed Frame Types in Buffer
```
# Setup with traffic containing various frame types
C[ENTER]
S4[ENTER]
O[ENTER]
X0[ENTER]

# Allow mixed traffic to accumulate
A[ENTER]
```
**Expected**: Standard and extended frames, data and RTR frames in FIFO order, terminated by `A`

### Edge Cases

#### Test 2.4: Channel Closed State
```
C[ENTER]    # Ensure channel closed
A[ENTER]
```
**Expected**: `[BEL]` (command requires open channel)

#### Test 2.5: Autopoll Enabled State
```
C[ENTER]
S4[ENTER]
O[ENTER]
X1[ENTER]   # Enable autopoll
A[ENTER]
```
**Expected**: `[BEL]` (bulk polling disabled when autopoll active)

#### Test 2.6: Single Frame in Buffer
```
# Setup
C[ENTER]
S4[ENTER]
O[ENTER]
X0[ENTER]

# Wait for exactly one frame, then drain
A[ENTER]
```
**Expected**: One frame followed by `A[ENTER]`

### Stress Testing

#### Test 2.7: Full Buffer Draining (64 frames)
```
# Setup with high traffic
C[ENTER]
S4[ENTER]
O[ENTER]
X0[ENTER]

# Allow buffer to fill completely (may take time)
# Monitor with i[ENTER] for frame count
A[ENTER]
```
**Expected**: Up to 64 frames followed by `A`, check that buffer is now empty

#### Test 2.8: High Frequency Draining
```
# Setup with continuous traffic
C[ENTER]
S4[ENTER]
O[ENTER]
X0[ENTER]

# Drain repeatedly
A[ENTER]
A[ENTER]
A[ENTER]
```
**Expected**: Each drain returns frames or just `A`, no corruption or crashes

#### Test 2.9: Buffer Overflow During Drain
```
# Setup with very high traffic
C[ENTER]
S4[ENTER]
O[ENTER]
X0[ENTER]

# Start draining while buffer is filling rapidly
A[ENTER]
```
**Expected**: Partial drain completes, check overflow counter in `i[ENTER]`

### Integration Testing

#### Test 2.10: With Timestamps Disabled
```
C[ENTER]
Z0[ENTER]   # Disable timestamps
S4[ENTER]
O[ENTER]
X0[ENTER]

A[ENTER]
```
**Expected**: Standard frame formats without timestamps

#### Test 2.11: With Timestamps Enabled
```
C[ENTER]
Z1[ENTER]   # Enable timestamps
S4[ENTER]
O[ENTER]
X0[ENTER]

A[ENTER]
```
**Expected**: Each frame includes 4-digit hex timestamp

#### Test 2.12: Filter Interaction
```
C[ENTER]
W0[ENTER]   # Dual filter mode
M00000000[ENTER]  # Restrictive acceptance code
m00000000[ENTER]  # Restrictive acceptance mask
S4[ENTER]
O[ENTER]
X0[ENTER]

A[ENTER]
```
**Expected**: Only frames matching filter criteria, or just `A` if none match

#### Test 2.13: Mode Switching During Drain
```
C[ENTER]
S4[ENTER]
O[ENTER]
X0[ENTER]

# Start drain operation
A[ENTER]

# Immediately switch to autopoll
X1[ENTER]

# Try drain again
A[ENTER]
```
**Expected**: First drain succeeds, second fails with `[BEL]`

### FIFO Ordering Verification

#### Test 2.14: Frame Order Preservation
```
# Setup with known traffic pattern
C[ENTER]
S4[ENTER]
O[ENTER]
X0[ENTER]

# Use CAN sniffer to record frame sequence
# Then drain buffer
A[ENTER]

# Compare order with sniffer capture
```
**Expected**: Frames returned in same order they arrived

#### Test 2.15: Timestamp Ordering
```
C[ENTER]
Z1[ENTER]   # Enable timestamps
S4[ENTER]
O[ENTER]
X0[ENTER]

A[ENTER]
```
**Expected**: Timestamps should be monotonically increasing (within timing resolution)

### Performance Testing

#### Test 2.16: Drain Response Time
```
# Setup with full buffer
C[ENTER]
S4[ENTER]
O[ENTER]
X0[ENTER]

# Time complete drain operation
A[ENTER]
```
**Expected**: Complete within 1-2 seconds for full 64-frame buffer

#### Test 2.17: Memory/Resource Limits
```
# Continuous high-traffic testing
C[ENTER]
S4[ENTER]
O[ENTER]
X0[ENTER]

# Drain repeatedly for extended period
# Monitor for memory leaks or corruption
```
**Expected**: Consistent behavior over time, no gradual performance degradation

---

## listen_mode_receives_frames (L Command)

Opens CAN channel in listen-only mode. Receives frames but cannot transmit. Useful for passive monitoring and bus analysis.

### Basic Functionality

#### Test 3.1: Listen Mode Reception
```
# Setup
C[ENTER]    # Close channel
S4[ENTER]   # Set 125kbps bitrate
L[ENTER]    # Open in listen-only mode

# Poll for frames
P[ENTER]
A[ENTER]
```
**Expected**: Frames received successfully, channel opens in listen mode

#### Test 3.2: Listen vs Normal Mode Comparison
```
# Test normal mode first
C[ENTER]
S4[ENTER]
O[ENTER]    # Normal mode
P[ENTER]    # Should work

# Switch to listen mode
C[ENTER]
L[ENTER]    # Listen mode
P[ENTER]    # Should also work
```
**Expected**: Both modes can receive frames

### Transmit Blocking Verification

#### Test 3.3: Transmit Commands in Listen Mode
```
C[ENTER]
S4[ENTER]
L[ENTER]    # Open in listen mode

# Try various transmit commands
t1230[ENTER]        # Standard data frame
T123456780[ENTER]   # Extended data frame
r1230[ENTER]        # Standard RTR
R123456780[ENTER]   # Extended RTR
```
**Expected**: All transmit commands return `[BEL]` (blocked in listen mode)

#### Test 3.4: Transmit Statistics in Listen Mode
```
C[ENTER]
S4[ENTER]
L[ENTER]

# Check initial stats
i[ENTER]

# Try transmitting
t1230[ENTER]

# Check stats again
i[ENTER]
```
**Expected**: TX counters should not increment, frames_tx should remain 0

### Mode Switching

#### Test 3.5: Listen to Normal Mode Switch
```
C[ENTER]
S4[ENTER]
L[ENTER]    # Start in listen mode

# Try transmit (should fail)
t1230[ENTER]

# Switch to normal mode
C[ENTER]    # Close
O[ENTER]    # Open normal

# Try transmit (should work)
t1230[ENTER]
```
**Expected**: Transmit blocked in listen mode, works in normal mode

#### Test 3.6: Normal to Listen Mode Switch
```
C[ENTER]
S4[ENTER]
O[ENTER]    # Start in normal mode

# Transmit should work
t1230[ENTER]

# Switch to listen mode
C[ENTER]    # Close
L[ENTER]    # Open listen

# Transmit should now fail
t1230[ENTER]
```
**Expected**: Mode switching works correctly in both directions

#### Test 3.7: Listen Mode After Reset
```
C[ENTER]
S4[ENTER]
L[ENTER]    # Open listen mode

# Check it works
P[ENTER]

# Reset device (close/reopen serial if needed)
# Or power cycle Arduino

# Check mode persists
V[ENTER]    # Basic connectivity
```
**Expected**: Listen mode does not persist after reset (channel closes)

### Integration Testing

#### Test 3.8: Listen Mode with Autopoll
```
C[ENTER]
S4[ENTER]
X1[ENTER]   # Enable autopoll
L[ENTER]    # Open listen mode

# Monitor for automatic frame output
# Wait 10-30 seconds
```
**Expected**: Frames stream automatically in listen mode

#### Test 3.9: Listen Mode with Timestamps
```
C[ENTER]
Z1[ENTER]   # Enable timestamps
S4[ENTER]
L[ENTER]    # Open listen mode
X0[ENTER]   # Disable autopoll

P[ENTER]
```
**Expected**: Polled frames include timestamps

#### Test 3.10: Listen Mode with Filters
```
C[ENTER]
W0[ENTER]   # Dual filter mode
M00000000[ENTER]  # Set acceptance code
mFFFFFFFF[ENTER]  # Set acceptance mask
S4[ENTER]
L[ENTER]    # Open listen mode

P[ENTER]
```
**Expected**: Only frames matching filters are received

### Edge Cases

#### Test 3.11: Listen Mode Without Bitrate
```
C[ENTER]
# Don't set bitrate
L[ENTER]
```
**Expected**: `[BEL]` (bitrate must be set first)

#### Test 3.12: Listen Mode When Already Open
```
C[ENTER]
S4[ENTER]
O[ENTER]    # Open normal first
L[ENTER]    # Try to switch to listen
```
**Expected**: May fail or succeed depending on implementation

#### Test 3.13: Listen Mode Status Flags
```
C[ENTER]
S4[ENTER]
L[ENTER]    # Open listen mode

F[ENTER]    # Read status flags
```
**Expected**: Status flags indicate listen-only mode (bit pattern shows receive-only state)

### Stress Testing

#### Test 3.14: Listen Mode High Traffic
```
C[ENTER]
S4[ENTER]
L[ENTER]

# With very high bus traffic
P[ENTER]
P[ENTER]
A[ENTER]
```
**Expected**: Listen mode handles high traffic without issues

#### Test 3.15: Listen Mode Long Duration
```
C[ENTER]
S4[ENTER]
L[ENTER]

# Leave in listen mode for extended period (hours)
# Periodically check reception
P[ENTER]
```
**Expected**: Stable operation over time, no mode degradation

---

## timestamp_frames (Z Command with Frame Reception)

Controls timestamp mode for received frames. When enabled (Z1), frames include 4-digit hex timestamp suffix indicating milliseconds since device start.

### Basic Functionality

#### Test 4.1: Timestamp Mode Query
```
# Query current timestamp mode
Z[ENTER]
```
**Expected**: `Z0` or `Z1` indicating current mode

#### Test 4.2: Enable Timestamp Mode
```
Z1[ENTER]
Z[ENTER]
```
**Expected**: `Z1` confirming mode enabled

#### Test 4.3: Disable Timestamp Mode
```
Z0[ENTER]
Z[ENTER]
```
**Expected**: `Z0` confirming mode disabled

#### Test 4.4: Timestamped Frame Format
```
C[ENTER]
Z1[ENTER]   # Enable timestamps
S4[ENTER]
O[ENTER]
X0[ENTER]   # Disable autopoll

P[ENTER]
```
**Expected**: Frame with 4-digit hex timestamp, e.g., `t1234AABB1234`

### Timestamp Format Verification

#### Test 4.5: Timestamp Position in Frame
```
C[ENTER]
Z1[ENTER]
S4[ENTER]
O[ENTER]
X0[ENTER]

P[ENTER]
```
**Expected**: Timestamp appears at end of frame data, before final CR

#### Test 4.6: Timestamp Length
```
# Test with different frame types
C[ENTER]
Z1[ENTER]
S4[ENTER]
O[ENTER]
X0[ENTER]

P[ENTER]    # Standard frame
P[ENTER]    # Extended frame
P[ENTER]    # RTR frame
```
**Expected**: All frames have exactly 4 hex digits appended

#### Test 4.7: Timestamp Hex Format
```
C[ENTER]
Z1[ENTER]
S4[ENTER]
O[ENTER]
X0[ENTER]

P[ENTER]
```
**Expected**: Timestamp consists of valid hex characters (0-9, A-F)

### Timing Accuracy

#### Test 4.8: Timestamp Monotonicity
```
C[ENTER]
Z1[ENTER]
S4[ENTER]
O[ENTER]
X0[ENTER]

# Poll multiple frames in sequence
P[ENTER]
P[ENTER]
P[ENTER]
```
**Expected**: Timestamps increase or stay same (within resolution limits)

#### Test 4.9: Timestamp Resolution
```
C[ENTER]
Z1[ENTER]
S4[ENTER]
O[ENTER]
X0[ENTER]

# Poll frames with known timing
# Use external timer to measure intervals
P[ENTER]
# Wait 1 second
P[ENTER]
```
**Expected**: Timestamp difference corresponds to elapsed time (within ~10ms resolution)

### Persistence Testing

#### Test 4.10: Timestamp Mode Persistence
```
Z1[ENTER]
Z[ENTER]    # Confirm Z1

# Reset device (close/reopen serial port)
# Device should auto-reset Arduino

Z[ENTER]    # Check persistence
```
**Expected**: `Z1` persists after reset (EEPROM storage)

#### Test 4.11: Timestamp Counter Reset
```
C[ENTER]
Z1[ENTER]
S4[ENTER]
O[ENTER]

P[ENTER]    # Get first timestamp
# Reset device
# Re-enable timestamp mode
Z[ENTER]    # Should still be Z1

P[ENTER]    # Get timestamp after reset
```
**Expected**: Timestamp counter resets to 0 after device reset

### Edge Cases

#### Test 4.12: Timestamp Mode with Channel Closed
```
C[ENTER]    # Ensure closed
Z1[ENTER]   # Should work
Z[ENTER]    # Should work
```
**Expected**: Timestamp commands work regardless of channel state

#### Test 4.13: Invalid Timestamp Mode
```
Z2[ENTER]   # Invalid mode
Z9[ENTER]   # Invalid mode
```
**Expected**: `[BEL]` for invalid modes

#### Test 4.14: Timestamp Mode During Channel Open
```
C[ENTER]
S4[ENTER]
O[ENTER]    # Channel open

Z1[ENTER]   # Should work while open
Z[ENTER]    # Should work while open
```
**Expected**: Timestamp commands work with channel open

### Integration Testing

#### Test 4.15: Timestamps with Autopoll
```
C[ENTER]
Z1[ENTER]
S4[ENTER]
X1[ENTER]   # Enable autopoll
O[ENTER]

# Monitor automatic output
```
**Expected**: Autopoll frames include timestamps

#### Test 4.16: Timestamps with Bulk Poll
```
C[ENTER]
Z1[ENTER]
S4[ENTER]
O[ENTER]
X0[ENTER]

A[ENTER]
```
**Expected**: All frames in bulk poll include timestamps

#### Test 4.17: Timestamp Counter Wraparound
```
C[ENTER]
Z1[ENTER]
S4[ENTER]
O[ENTER]
X0[ENTER]

# Leave device running for extended period
# Monitor timestamp values over time
# Should eventually wrap from FFFF back to 0000
```
**Expected**: 16-bit counter wraps correctly (FFFF → 0000)

### Stress Testing

#### Test 4.18: High Frequency Timestamping
```
C[ENTER]
Z1[ENTER]
S4[ENTER]
O[ENTER]
X0[ENTER]

# With high traffic rate
P[ENTER]
P[ENTER]
P[ENTER]
```
**Expected**: Timestamps remain accurate under load

#### Test 4.19: Timestamp Format Stress
```
# Test with various frame formats and DLC values
C[ENTER]
Z1[ENTER]
S4[ENTER]
O[ENTER]
X0[ENTER]

# Poll frames with DLC 0, 1, 8
P[ENTER]
```
**Expected**: Timestamp format consistent across all frame types

---

## autopoll_stream (X Command)

Controls automatic frame streaming mode. When enabled (X1), received frames are sent immediately to serial without requiring polling commands.

### Basic Functionality

#### Test 5.1: Autopoll Mode Query
```
# Query current autopoll mode
X[ENTER]
```
**Expected**: `X0` or `X1` indicating current mode

#### Test 5.2: Enable Autopoll Mode
```
X1[ENTER]
X[ENTER]
```
**Expected**: `X1` confirming autopoll enabled

#### Test 5.3: Disable Autopoll Mode
```
X0[ENTER]
X[ENTER]
```
**Expected**: `X0` confirming autopoll disabled

#### Test 5.4: Autopoll Frame Streaming
```
C[ENTER]
S4[ENTER]
X1[ENTER]   # Enable autopoll
O[ENTER]    # Open channel

# Monitor serial output for 10-30 seconds
```
**Expected**: Frames appear automatically as they are received

### Buffer Behavior

#### Test 5.5: Circular Buffer Operation
```
C[ENTER]
S4[ENTER]
X1[ENTER]
O[ENTER]

# With continuous traffic, monitor output
# Should see continuous frame stream
```
**Expected**: 64-frame circular buffer prevents memory exhaustion

#### Test 5.6: Buffer Overflow Handling
```
C[ENTER]
S4[ENTER]
X1[ENTER]
O[ENTER]

# With very high traffic rate
# Monitor for any overflow indicators
i[ENTER]    # Check overflow counter
```
**Expected**: Overflow counter increments, no crashes or corruption

### Mode Switching

#### Test 5.7: Autopoll Enable/Disable Cycle
```
C[ENTER]
S4[ENTER]
X0[ENTER]   # Start disabled
O[ENTER]

# Should see no automatic frames

X1[ENTER]   # Enable
# Should start seeing automatic frames

X0[ENTER]   # Disable
# Should stop automatic frames
```
**Expected**: Clean mode switching without issues

#### Test 5.8: Autopoll with Channel Closed
```
C[ENTER]    # Ensure closed
X1[ENTER]   # Try to enable
```
**Expected**: May succeed or fail (implementation dependent)

#### Test 5.9: Polling Commands with Autopoll Enabled
```
C[ENTER]
S4[ENTER]
X1[ENTER]   # Enable autopoll
O[ENTER]

P[ENTER]    # Try single poll
A[ENTER]    # Try bulk poll
```
**Expected**: Both commands return `[BEL]` (disabled when autopoll active)

### Integration Testing

#### Test 5.10: Autopoll with Timestamps
```
C[ENTER]
Z1[ENTER]   # Enable timestamps
S4[ENTER]
X1[ENTER]   # Enable autopoll
O[ENTER]

# Monitor automatic output
```
**Expected**: All frames include timestamps

#### Test 5.11: Autopoll with Listen Mode
```
C[ENTER]
S4[ENTER]
X1[ENTER]
L[ENTER]    # Open listen-only

# Monitor output
```
**Expected**: Frames received in listen mode stream automatically

#### Test 5.12: Autopoll with Filters
```
C[ENTER]
W0[ENTER]   # Dual filter mode
M00000000[ENTER]  # Set acceptance code
mFFFFFFFF[ENTER]  # Set acceptance mask
S4[ENTER]
X1[ENTER]
O[ENTER]

# Monitor filtered output
```
**Expected**: Only frames matching filter criteria appear in stream

### Stress Testing

#### Test 5.13: High Traffic Autopoll
```
C[ENTER]
S4[ENTER]
X1[ENTER]
O[ENTER]

# With maximum bus traffic
# Monitor serial output rate
```
**Expected**: Handles high traffic without serial buffer overflow

#### Test 5.14: Autopoll Long Duration
```
C[ENTER]
S4[ENTER]
X1[ENTER]
O[ENTER]

# Leave autopoll running for extended period
# Monitor stability
```
**Expected**: Continuous operation without degradation

#### Test 5.15: Autopoll Buffer Boundary
```
C[ENTER]
S4[ENTER]
X1[ENTER]
O[ENTER]

# Allow buffer to fill to 64 frames
# Monitor output continuity
i[ENTER]    # Check buffer status
```
**Expected**: Smooth circular buffer operation at boundaries

### Persistence Testing

#### Test 5.16: Autopoll Mode Persistence
```
X1[ENTER]
X[ENTER]    # Confirm X1

# Reset device
X[ENTER]    # Check after reset
```
**Expected**: Autopoll mode may or may not persist (check implementation)

### Edge Cases

#### Test 5.17: Autopoll Invalid Modes
```
X2[ENTER]   # Invalid mode
X9[ENTER]   # Invalid mode
```
**Expected**: `[BEL]` for invalid modes

#### Test 5.18: Autopoll Mode Switching Stress
```
# Rapid enable/disable cycling
X1[ENTER]
X0[ENTER]
X1[ENTER]
X0[ENTER]
```
**Expected**: No corruption or crashes from rapid switching

#### Test 5.19: Autopoll During Frame Reception
```
C[ENTER]
S4[ENTER]
O[ENTER]
X0[ENTER]   # Start with autopoll disabled

# While receiving frames, enable autopoll
X1[ENTER]

# Should start streaming buffered frames
```
**Expected**: Buffered frames stream out when autopoll enabled

---

## filter_query (W Command)

Queries the current hardware filter mode. LAWICEL devices support dual filter mode (W0) and single filter mode (W1).

### Basic Functionality

#### Test 6.1: Filter Mode Query
```
# Query current filter mode
W[ENTER]
```
**Expected**: `W0` (dual filter) or `W1` (single filter)

#### Test 6.2: Default Filter Mode
```
# After device reset/power-on
W[ENTER]
```
**Expected**: `W0` (dual filter mode is typically default)

#### Test 6.3: Query Response Format
```
W[ENTER]
```
**Expected**: Exactly 2 characters: `W` followed by `0` or `1`

### State Dependencies

#### Test 6.4: Query with Channel Closed
```
C[ENTER]    # Ensure closed
W[ENTER]
```
**Expected**: Works regardless of channel state

#### Test 6.5: Query with Channel Open
```
C[ENTER]
S4[ENTER]
O[ENTER]    # Channel open
W[ENTER]
```
**Expected**: Works with channel open

#### Test 6.6: Query During Active Reception
```
C[ENTER]
S4[ENTER]
O[ENTER]
X1[ENTER]   # Autopoll active

W[ENTER]
```
**Expected**: Query works during active reception

### Integration Testing

#### Test 6.7: Query After Mode Changes
```
W[ENTER]    # Check initial mode

W1[ENTER]   # Change to single filter
W[ENTER]    # Query should show W1

W0[ENTER]   # Change to dual filter
W[ENTER]    # Query should show W0
```
**Expected**: Query reflects current mode after changes

#### Test 6.8: Query with Acceptance Settings
```
W[ENTER]    # Check mode

M12345678[ENTER]  # Set acceptance code
m9ABCDEF0[ENTER]  # Set acceptance mask

W[ENTER]    # Query again
```
**Expected**: Filter mode unchanged by acceptance register settings

### Persistence Testing

#### Test 6.9: Filter Mode Persistence
```
W1[ENTER]   # Set single filter mode
W[ENTER]    # Confirm W1

# Reset device
W[ENTER]    # Check after reset
```
**Expected**: Filter mode may persist (EEPROM) or reset to default

#### Test 6.10: Persistence Across Power Cycles
```
W1[ENTER]
W[ENTER]    # Confirm W1

# Power cycle Arduino
W[ENTER]    # Check after power cycle
```
**Expected**: Filter mode persistence depends on EEPROM implementation

### Edge Cases

#### Test 6.11: Query During Filter Configuration
```
# Start setting acceptance code
M1234567   # Incomplete command (no CR yet)

W[ENTER]    # Query during incomplete command
```
**Expected**: Query works independently of incomplete commands

#### Test 6.12: Rapid Query Sequence
```
W[ENTER]
W[ENTER]
W[ENTER]
W[ENTER]
```
**Expected**: Consistent responses, no corruption

### Documentation Testing

#### Test 6.13: Verify Mode Behavior
```
W[ENTER]    # Check current mode

# Set restrictive filter
M00000000[ENTER]
m00000000[ENTER]

# Generate test frames that should be filtered
# Use CAN sniffer to verify filtering behavior matches mode
```
**Expected**: W0 vs W1 modes show different filtering behavior

---

## filter_roundtrip (W Command)

Sets and verifies hardware filter mode changes. Tests configuration persistence and actual filtering behavior with real CAN frames.

### Basic Functionality

#### Test 7.1: Mode Change to Single Filter
```
W[ENTER]    # Check current mode
W1[ENTER]   # Set single filter mode
W[ENTER]    # Verify change
```
**Expected**: `W1` returned after setting

#### Test 7.2: Mode Change to Dual Filter
```
W[ENTER]    # Check current mode
W0[ENTER]   # Set dual filter mode
W[ENTER]    # Verify change
```
**Expected**: `W0` returned after setting

#### Test 7.3: Roundtrip Mode Changes
```
W0[ENTER]   # Set dual
W[ENTER]    # Verify W0
W1[ENTER]   # Set single
W[ENTER]    # Verify W1
W0[ENTER]   # Back to dual
W[ENTER]    # Verify W0
```
**Expected**: Clean mode switching in both directions

### State Dependencies

#### Test 7.4: Mode Change with Channel Closed
```
C[ENTER]    # Ensure closed
W1[ENTER]   # Change mode
W[ENTER]    # Verify
```
**Expected**: Mode changes work with channel closed

#### Test 7.5: Mode Change with Channel Open
```
C[ENTER]
S4[ENTER]
O[ENTER]    # Channel open
W1[ENTER]   # Change mode
```
**Expected**: May work or fail (implementation dependent)

#### Test 7.6: Mode Change During Active Reception
```
C[ENTER]
S4[ENTER]
O[ENTER]
X1[ENTER]   # Autopoll active

W1[ENTER]   # Change during reception
```
**Expected**: Mode change behavior during active reception

### Persistence Testing

#### Test 7.7: Filter Mode Persistence
```
W1[ENTER]   # Set single filter
W[ENTER]    # Confirm W1

# Reset device
W[ENTER]    # Check persistence
```
**Expected**: Mode may persist or reset to default

#### Test 7.8: Persistence with Acceptance Registers
```
W1[ENTER]
M12345678[ENTER]  # Set acceptance code
m9ABCDEF0[ENTER]  # Set acceptance mask

# Reset device
W[ENTER]          # Check filter mode
M[ENTER]          # Check acceptance code
m[ENTER]          # Check acceptance mask
```
**Expected**: All filter settings persistence behavior

### Integration Testing

#### Test 7.9: Filter Mode with Acceptance Code/Mask
```
W0[ENTER]   # Dual filter mode
M00000000[ENTER]  # Set code
mFFFFFFFF[ENTER]  # Set mask (accept all)

W1[ENTER]   # Change to single filter
M11111111[ENTER]  # Change code
mAAAAAAAA[ENTER]  # Change mask

W[ENTER]    # Verify mode
M[ENTER]    # Verify code
m[ENTER]    # Verify mask
```
**Expected**: Independent operation of mode and registers

#### Test 7.10: Filter Behavior Verification
```
# Setup with known CAN traffic IDs
W0[ENTER]   # Dual filter

# Set filter to accept only specific ID range
M000007FF[ENTER]  # Acceptance code
m000007FF[ENTER]  # Acceptance mask

C[ENTER]
S4[ENTER]
O[ENTER]
X0[ENTER]

P[ENTER]    # Poll frames
```
**Expected**: Only frames matching filter criteria received

### Edge Cases

#### Test 7.11: Invalid Filter Modes
```
W2[ENTER]   # Invalid mode
W9[ENTER]   # Invalid mode
WA[ENTER]   # Non-numeric
```
**Expected**: `[BEL]` for all invalid modes

#### Test 7.12: Mode Change During Incomplete Commands
```
# Start acceptance code setting
M1234567   # No CR yet

W1[ENTER]  # Change mode during incomplete command
```
**Expected**: Mode change works independently

#### Test 7.13: Rapid Mode Switching
```
W0[ENTER]
W1[ENTER]
W0[ENTER]
W1[ENTER]
W[ENTER]   # Final check
```
**Expected**: No corruption from rapid switching

### Stress Testing

#### Test 7.14: Filter Mode Changes Under Load
```
C[ENTER]
S4[ENTER]
O[ENTER]
X1[ENTER]   # Autopoll active with traffic

W1[ENTER]   # Change mode under load
W0[ENTER]   # Change back
```
**Expected**: Mode changes work under traffic load

#### Test 7.15: Filter Persistence Stress
```
# Multiple mode changes and resets
W1[ENTER]
# Reset
W0[ENTER]
# Reset
W1[ENTER]
# Reset

W[ENTER]   # Final persistence check
```
**Expected**: Consistent persistence behavior

### Functional Verification

#### Test 7.16: Dual vs Single Filter Behavior
```
# Test with CAN bus having multiple ID ranges

# Dual filter mode (W0)
W0[ENTER]
M00000000[ENTER]  # Code
mFFFF0000[ENTER]  # Mask (filter on high 16 bits)

P[ENTER]   # Check received frames

# Single filter mode (W1)
W1[ENTER]
M00000000[ENTER]  # Code
mFFFF0000[ENTER]  # Mask

P[ENTER]   # Compare received frames
```
**Expected**: Different filtering behavior between W0 and W1 modes

#### Test 7.17: Filter Boundary Testing
```
# Test edge cases in filter ranges
W0[ENTER]

# Test with mask that creates boundary conditions
M7FF7FF7[ENTER]   # Code with boundary values
m7FF7FF7[ENTER]   # Mask with boundary values

P[ENTER]   # Verify filtering at boundaries
```
**Expected**: Correct filtering at ID range boundaries
