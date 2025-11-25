# IRL Command Testing Checklist

**Date:** _______________ **Tester:** _______________ **Location:** _______________

**Hardware:** Arduino UNO + MCP2515 Shield
**Serial Port:** _______________ **Baud Rate:** _______________
**CAN Bus Speed:** _______________ **Bus Load:** _______________ frames/sec

**Bus Traffic Description:**
- Frame IDs observed: ________________________
- Data patterns: ________________________
- Other nodes: ________________________

---

## Test Environment Verification

### Basic Connectivity
- [ ] Device enumerates as serial port
- [ ] Serial terminal connects successfully
- [ ] Basic commands respond (`V[ENTER]` returns version)
- [ ] CAN bus has active traffic (verified with sniffer)

### Device State
- [ ] Channel closed by default (`C[ENTER]` returns empty or BEL)
- [ ] Bitrate unset (`O[ENTER]` fails with BEL)
- [ ] Autopoll disabled (`X[ENTER]` returns X0)
- [ ] Timestamps disabled (`Z[ENTER]` returns Z0)

---

## poll_single_frame (P Command) Tests

### Basic Functionality
- [ ] **P.1** Single frame polling with traffic
  - Command: `C[ENTER] S4[ENTER] O[ENTER] X0[ENTER] P[ENTER]`
  - Expected: Single CAN frame in LAWICEL format
  - Actual: ________________________
  - Notes: ________________________

- [ ] **P.2** Frame type variations
  - Test standard/extended frames, data/RTR frames
  - Expected: Correct format for each type
  - Actual: ________________________
  - Notes: ________________________

- [ ] **P.3** DLC value testing (0-8)
  - Expected: Correct DLC in frame format
  - Actual: ________________________
  - Notes: ________________________

### Edge Cases
- [ ] **P.4** Empty buffer polling
  - Command: Wait 10s after open, then `P[ENTER]`
  - Expected: `[BEL]`
  - Actual: ________________________
  - Notes: ________________________

- [ ] **P.5** Channel closed state
  - Command: `C[ENTER] P[ENTER]`
  - Expected: `[BEL]`
  - Actual: ________________________
  - Notes: ________________________

- [ ] **P.6** Autopoll enabled state
  - Command: `X1[ENTER] O[ENTER] P[ENTER]`
  - Expected: `[BEL]`
  - Actual: ________________________
  - Notes: ________________________

### Stress Testing
- [ ] **P.7** Rapid polling (10-20 times)
  - Expected: Frames or `[BEL]`, no corruption
  - Actual: ________________________
  - Notes: ________________________

- [ ] **P.8** High traffic polling
  - Expected: Responsive under load
  - Actual: ________________________
  - Notes: ________________________

### Integration
- [ ] **P.9** With timestamps disabled
  - Command: `Z0[ENTER]` then poll
  - Expected: Standard format
  - Actual: ________________________
  - Notes: ________________________

- [ ] **P.10** With timestamps enabled
  - Command: `Z1[ENTER]` then poll
  - Expected: Frame + 4-digit hex timestamp
  - Actual: ________________________
  - Notes: ________________________

---

## poll_all_frames (A Command) Tests

### Basic Functionality
- [ ] **A.1** Buffer draining with traffic
  - Command: `C[ENTER] S4[ENTER] O[ENTER] X0[ENTER]` wait 5s `A[ENTER]`
  - Expected: Multiple frames + `A[ENTER]`
  - Actual: ________________________
  - Notes: ________________________

- [ ] **A.2** Empty buffer draining
  - Command: Fresh buffer `A[ENTER]`
  - Expected: Only `A[ENTER]`
  - Actual: ________________________
  - Notes: ________________________

- [ ] **A.3** Mixed frame types
  - Expected: All frame types in FIFO order
  - Actual: ________________________
  - Notes: ________________________

### Edge Cases
- [ ] **A.4** Channel closed state
  - Command: `C[ENTER] A[ENTER]`
  - Expected: `[BEL]`
  - Actual: ________________________
  - Notes: ________________________

- [ ] **A.5** Autopoll enabled state
  - Command: `X1[ENTER] O[ENTER] A[ENTER]`
  - Expected: `[BEL]`
  - Actual: ________________________
  - Notes: ________________________

### Stress Testing
- [ ] **A.6** Full buffer (64 frames)
  - Expected: All 64 frames + `A[ENTER]`
  - Actual: ________________________
  - Notes: ________________________

- [ ] **A.7** High frequency draining
  - Expected: Consistent behavior
  - Actual: ________________________
  - Notes: ________________________

### Integration
- [ ] **A.8** With timestamps enabled
  - Command: `Z1[ENTER]` then drain
  - Expected: All frames timestamped
  - Actual: ________________________
  - Notes: ________________________

- [ ] **A.9** FIFO ordering verification
  - Expected: Frames in arrival order
  - Actual: ________________________
  - Notes: ________________________

---

## listen_mode_receives_frames (L Command) Tests

### Basic Functionality
- [ ] **L.1** Listen mode reception
  - Command: `C[ENTER] S4[ENTER] L[ENTER] P[ENTER]`
  - Expected: Frames received
  - Actual: ________________________
  - Notes: ________________________

- [ ] **L.2** Listen vs normal mode
  - Expected: Both modes receive frames
  - Actual: ________________________
  - Notes: ________________________

### Transmit Blocking
- [ ] **L.3** Transmit in listen mode
  - Command: `L[ENTER]` then `t1230[ENTER]`
  - Expected: `[BEL]`
  - Actual: ________________________
  - Notes: ________________________

- [ ] **L.4** TX stats in listen mode
  - Command: Check `i[ENTER]` TX counter
  - Expected: TX counter unchanged
  - Actual: ________________________
  - Notes: ________________________

### Mode Switching
- [ ] **L.5** Listen to normal mode
  - Expected: Transmit works after switch
  - Actual: ________________________
  - Notes: ________________________

- [ ] **L.6** Normal to listen mode
  - Expected: Transmit blocked after switch
  - Actual: ________________________
  - Notes: ________________________

### Integration
- [ ] **L.7** Listen with autopoll
  - Command: `X1[ENTER] L[ENTER]`
  - Expected: Automatic streaming
  - Actual: ________________________
  - Notes: ________________________

- [ ] **L.8** Listen with timestamps
  - Expected: Timestamped frames
  - Actual: ________________________
  - Notes: ________________________

---

## timestamp_frames (Z Command) Tests

### Basic Functionality
- [ ] **Z.1** Mode query
  - Command: `Z[ENTER]`
  - Expected: `Z0` or `Z1`
  - Actual: ________________________
  - Notes: ________________________

- [ ] **Z.2** Enable timestamps
  - Command: `Z1[ENTER] Z[ENTER]`
  - Expected: `Z1`
  - Actual: ________________________
  - Notes: ________________________

- [ ] **Z.3** Timestamped frame format
  - Command: `Z1[ENTER]` then poll frame
  - Expected: Frame + 4-digit hex
  - Actual: ________________________
  - Notes: ________________________

### Timing Accuracy
- [ ] **Z.4** Monotonic timestamps
  - Expected: Increasing values
  - Actual: ________________________
  - Notes: ________________________

- [ ] **Z.5** Timestamp resolution
  - Expected: ~10ms accuracy
  - Actual: ________________________
  - Notes: ________________________

### Persistence
- [ ] **Z.6** Mode persistence after reset
  - Expected: `Z1` persists or resets
  - Actual: ________________________
  - Notes: ________________________

- [ ] **Z.7** Counter reset on reboot
  - Expected: Counter resets to 0
  - Actual: ________________________
  - Notes: ________________________

### Edge Cases
- [ ] **Z.8** Invalid modes
  - Command: `Z2[ENTER]`
  - Expected: `[BEL]`
  - Actual: ________________________
  - Notes: ________________________

### Integration
- [ ] **Z.9** With autopoll
  - Expected: Stream includes timestamps
  - Actual: ________________________
  - Notes: ________________________

- [ ] **Z.10** With bulk poll
  - Expected: All frames timestamped
  - Actual: ________________________
  - Notes: ________________________

---

## autopoll_stream (X Command) Tests

### Basic Functionality
- [ ] **X.1** Mode query
  - Command: `X[ENTER]`
  - Expected: `X0` or `X1`
  - Actual: ________________________
  - Notes: ________________________

- [ ] **X.2** Enable autopoll
  - Command: `X1[ENTER] X[ENTER]`
  - Expected: `X1`
  - Actual: ________________________
  - Notes: ________________________

- [ ] **X.3** Automatic streaming
  - Command: `X1[ENTER] O[ENTER]`
  - Expected: Frames stream automatically
  - Actual: ________________________
  - Notes: ________________________

### Buffer Behavior
- [ ] **X.4** Circular buffer operation
  - Expected: Continuous streaming
  - Actual: ________________________
  - Notes: ________________________

- [ ] **X.5** Buffer overflow handling
  - Expected: Overflow counter increments
  - Actual: ________________________
  - Notes: ________________________

### Mode Switching
- [ ] **X.6** Enable/disable cycle
  - Expected: Clean switching
  - Actual: ________________________
  - Notes: ________________________

- [ ] **X.7** Polling with autopoll enabled
  - Command: `P[ENTER]` or `A[ENTER]`
  - Expected: `[BEL]`
  - Actual: ________________________
  - Notes: ________________________

### Integration
- [ ] **X.8** With timestamps
  - Expected: Timestamped streaming
  - Actual: ________________________
  - Notes: ________________________

- [ ] **X.9** With listen mode
  - Expected: Listen-only streaming
  - Actual: ________________________
  - Notes: ________________________

### Stress Testing
- [ ] **X.10** High traffic autopoll
  - Expected: Handles load
  - Actual: ________________________
  - Notes: ________________________

---

## filter_query (W Command) Tests

### Basic Functionality
- [ ] **W.1** Mode query
  - Command: `W[ENTER]`
  - Expected: `W0` or `W1`
  - Actual: ________________________
  - Notes: ________________________

- [ ] **W.2** Default mode
  - Expected: `W0` (dual filter)
  - Actual: ________________________
  - Notes: ________________________

### State Dependencies
- [ ] **W.3** Query with channel closed
  - Expected: Works
  - Actual: ________________________
  - Notes: ________________________

- [ ] **W.4** Query with channel open
  - Expected: Works
  - Actual: ________________________
  - Notes: ________________________

### Integration
- [ ] **W.5** After mode changes
  - Expected: Reflects current mode
  - Actual: ________________________
  - Notes: ________________________

### Persistence
- [ ] **W.6** Mode persistence
  - Expected: Persists or resets
  - Actual: ________________________
  - Notes: ________________________

---

## filter_roundtrip (W Command) Tests

### Basic Functionality
- [ ] **WR.1** Change to single filter
  - Command: `W1[ENTER] W[ENTER]`
  - Expected: `W1`
  - Actual: ________________________
  - Notes: ________________________

- [ ] **WR.2** Change to dual filter
  - Command: `W0[ENTER] W[ENTER]`
  - Expected: `W0`
  - Actual: ________________________
  - Notes: ________________________

- [ ] **WR.3** Roundtrip changes
  - Expected: Clean switching
  - Actual: ________________________
  - Notes: ________________________

### State Dependencies
- [ ] **WR.4** Change with channel closed
  - Expected: Works
  - Actual: ________________________
  - Notes: ________________________

- [ ] **WR.5** Change with channel open
  - Expected: Works or fails
  - Actual: ________________________
  - Notes: ________________________

### Persistence
- [ ] **WR.6** Persistence after reset
  - Expected: Consistent behavior
  - Actual: ________________________
  - Notes: ________________________

### Integration
- [ ] **WR.7** With acceptance registers
  - Expected: Independent operation
  - Actual: ________________________
  - Notes: ________________________

- [ ] **WR.8** Filter behavior verification
  - Expected: Actual filtering works
  - Actual: ________________________
  - Notes: ________________________

### Edge Cases
- [ ] **WR.9** Invalid modes
  - Command: `W2[ENTER]`
  - Expected: `[BEL]`
  - Actual: ________________________
  - Notes: ________________________

---

## Summary

**Total Tests:** ________ **Passed:** ________ **Failed:** ________ **Skipped:** ________

### Failed Tests
1. ________________________
2. ________________________
3. ________________________

### Anomalies Observed
1. ________________________
2. ________________________
3. ________________________

### Environmental Notes
- Bus stability: ________________________
- Traffic patterns: ________________________
- Interference observed: ________________________
- Hardware issues: ________________________

### Recommendations
- ________________________
- ________________________
- ________________________
