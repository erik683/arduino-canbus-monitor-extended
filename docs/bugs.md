Short version: all the red stuff is clustered in two places:

The “policy/behavior” layer in can-232.cpp (autopoll, autostart, extended-frame toggle, timestamps, batching).

The runtime_stats and RX ring-buffer plumbing that sits under autopoll.

The MCP2515 driver itself is basically fine; all the low-level things that would break early (tx, listen mode, filters, etc.) are passing.

Let me walk through each failing test and point at the most likely code chunks.

1. [FAIL] autopoll_stream

This is almost certainly in the autopoll path in Can232::loop() and how you toggle the autopoll state:

Relevant code:

Can232::loop() autopoll block:

The whole section guarded by
if (lw232AutoPoll == LW232_AUTOPOLL_ON_AFTER_INIT && lw232CanChannelMode == LW232_STATUS_CAN_OPEN) { ... }

Uses serviceCanRx(), emitFrameToSerial(), Serial.write(LW232_CR).

Command handler for X:

case LW232_CMD_AUTOPOLL: in Can232::parseAndRunCommand(...)

Logic that sets lw232AutoPoll to LW232_AUTOPOLL_ON_WAIT vs LW232_AUTOPOLL_ON_AFTER_INIT.

If autopoll_stream is failing while manual A works, that means:

RX + ring buffer are working (because A uses them),

but that lw232AutoPoll state machine and/or the conditions in the autopoll branch in loop() aren’t letting that code actually run, or the frames never get flushed out to serial.

That’s all in can-232.cpp, not the MCP driver.

2. [FAIL] autostart_persistence / [FAIL] autostart_listen

These two are basically screaming “look at EEPROM + autostart init.”

Relevant functions in can-232.cpp:

EEPROM management:

void Can232::initializeEepromIfNeeded()

void Can232::loadAutoStartPreference()

void Can232::persistAutoStartPreference()

INT8U Can232::computeAutoStartChecksum(...)

INT8U Can232::findCanBaudIndex(INT32U bitrate)

Startup / auto-open:

void Can232::initFunc() (calls initializeEepromIfNeeded, loadTimestampPreference, loadAutoStartPreference, then maybeAutoStart() indirectly)

void Can232::maybeAutoStart() – where lw232AutoStart + lw232CanSpeedIndex actually trigger openCanBus(...).

Command handler:

case LW232_CMD_AUTOSTART: handling Q0/Q1/Q2 and calling persistAutoStartPreference().

The facts:

autostart_query passes ⇒ the Q command itself is fine while the device is running.

The failures only show up after reset ⇒ bug is in how you save/load that state or call maybeAutoStart() at boot, not in the live Q handler.

So I’d look closely at:

Are you definitely calling maybeAutoStart() on every boot in initFunc()?

Does loadAutoStartPreference() always set a valid lw232CanSpeedIndex and lw232AutoStart when the EEPROM is already initialized?

In maybeAutoStart(), is the LW232_AUTOSTART_ON_LISTEN branch actually calling openCanBus(LW232_MODE_LISTEN) and leaving the channel open, so that a subsequent C command returns OK rather than BEL?

That’s the hot zone for these two tests.

3. [FAIL] high_throughput_rx and [FAIL] autopoll_batch_limiting

These both sit right on top of the autopoll + RX ring buffer pipeline:

Key places:

RX ring buffer:

static BufferedFrame lw232RxBuffer[LW232_RX_BUFFER_SIZE];

static volatile INT8U lw232RxHead, lw232RxTail;

bool pushRxFrame(const BufferedFrame& frame)

bool peekRxFrame(BufferedFrame& frame)

bool popRxFrame(BufferedFrame& frame)

MCP drain and buffering:

void Can232::serviceCanRx() – drains MCP2515 into the ring buffer.

RxReadStatus Can232::readCanFrame(BufferedFrame& frame) – reads from MCP, sets frame.timestamp, flags, etc.

Autopoll batching:

The large block in Can232::loop() using:

LW232_MAX_HW_DRAIN_PER_CALL

AUTOPOLL_MAX_BATCH_BYTES

AUTOPOLL_MAX_BATCH_TIME_MS

autopollBatchStartTime / autopollBatchBytes

Serial.flush() at the end of each batch.

Why they fail:

high_throughput_rx_no_drops:

It enables autopoll and runs 10s of traffic, then checks:

That frames are not corrupted (DLC vs payload) ⇒ that part is probably fine.

That (stats_after.drops - stats_before.drops)/len(frames) <= 0.05.

This is governed by:

statsRecordRxOverflow() (called in pushRxFrame when buffer full)

statsRecordRxDrop() (currently never called from anywhere in can-232.cpp)

Plus how aggressively serviceCanRx() and the autopoll loop are draining the hardware.

So if drops are high, it’s either:

Buffer too small / drained too slowly (LW232_RX_BUFFER_SIZE, LW232_MAX_HW_DRAIN_PER_CALL), or

You’re spending too much time inside Serial.flush() / printing, so RX gets starved.

autopoll_batch_limiting:

This specifically tests that you don’t just dump an unbounded firehose: it expects batches to be limited, with noticeable gaps between bursts.

That’s exactly what this block is supposed to do:

if (autopollBatchBytes + frameSize > AUTOPOLL_MAX_BATCH_BYTES ||
    elapsedMs >= AUTOPOLL_MAX_BATCH_TIME_MS) {
    pushRxFrame(frame);        // put it back
    batchLimitReached = true;  // stop here, continue next loop
    break;
}


If the test says this fails, likely issues are:

AUTOPOLL_MAX_BATCH_BYTES / AUTOPOLL_MAX_BATCH_TIME_MS values don’t match the assumptions in the smoke test (too big / too permissive).

autopollBatchStartTime/autopollBatchBytes aren’t being reset the way the test expects (so what you think is “bounded” still looks like one continuous stream to them).

So: debug anything around serviceCanRx() + pushRxFrame/popRxFrame + autopoll batching and Serial.flush().

4. [FAIL] extended_frame_rejection

This is scoped very tightly:

Command handler: %EXT0 / %EXT1

case LW232_CMD_REJECT_EXT: in Can232::parseAndRunCommand(...)
toggles lw232RejectExtendedFrames and prints the status string.

RX path:

RxReadStatus Can232::readCanFrame(BufferedFrame& frame):

Uses isExtendedFrame() to set LW232_FRAME_FLAG_EXTENDED.

Applies:

if ((frame.flags & LW232_FRAME_FLAG_EXTENDED) && lw232RejectExtendedFrames) {
    return RX_READ_SKIPPED;
}


What can go wrong:

isExtendedFrame() isn’t returning what you think for received frames (standard vs extended).

The %EXT1 parser might be mis-matching the string (the multi-char check on "EXT" in lw232Message is a bit brittle).

The test also cares that the response text from %EXT1\r contains the correct substring. If your string format differs from what it expects, it’ll mark this as a failure even if the logic works.

So I’d focus on:

%EXT branch in parseAndRunCommand.

isExtendedFrame() / flagging in readCanFrame.

5. [FAIL] timestamp_rollover

This points right at the timestamp math:

Key function in can-232.cpp:

INT16U Can232::buildTimestampMs(INT32U capturedMicros) {
    if (capturedMicros < lastTimestampMicros) {
        microsRolloverOffsetUs += (1ULL << 32);
    }
    lastTimestampMicros = capturedMicros;

    const uint64_t adjustedMicros = microsRolloverOffsetUs + (uint64_t)capturedMicros;
    const uint32_t timestampMs = (uint32_t)((adjustedMicros / 1000ULL) % 60000ULL);

    static INT16U lastEmittedTimestampMs = 0;
    INT16U monotonicTimestampMs = (INT16U)timestampMs;

    if (monotonicTimestampMs < lastEmittedTimestampMs) {
        if (lastEmittedTimestampMs > 30000 && monotonicTimestampMs < 30000) {
            // treat as rollover, accept wrap
        } else {
            // force monotonic
            monotonicTimestampMs = lastEmittedTimestampMs + 1;
            if (monotonicTimestampMs >= 60000)
                monotonicTimestampMs = 0;
        }
    }

    lastEmittedTimestampMs = monotonicTimestampMs;
    return monotonicTimestampMs;
}


Plus wherever frame.timestamp is captured (in readCanFrame) and consumed:

emitFrameToSerial() prints buildTimestampMs(frame.timestamp) when timestamps are enabled.

If that test is failing, it means either:

capturedMicros per frame isn’t actually monotonic (e.g. reuse of an uninitialized value or a different timebase), or

The “rollover detection” heuristic (the 30000/30000 cut-off) doesn’t match what the test expects, so you’re artificially “correcting” timestamps in a way that yields big backward jumps in their analysis.

All of that lives in buildTimestampMs + how/when you call it.

6. [FAIL] command_latency

This is about how quickly you respond to commands like V\r while the system is doing its thing.

The pain points are:

The top of Can232::loop():

You always call serviceCanRx() before checking for a complete serial line.

If bus is busy and serviceCanRx() + autopoll + Serial I/O take too long, the smoke test sees latency.

Autopoll batching:

You always call Serial.flush() after a batch, even when totalProcessed == 0. That’s a blocking call and can definitely increase command latency under heavy traffic or with a slow host.

So I’d look at:

Re-ordering loop() so command processing doesn’t get starved by RX/autopoll.

Minimizing or eliminating Serial.flush() in the hot path.

This is all in Can232::loop() and the autopoll block in can-232.cpp.

7. [FAIL] periodic_frame_jitter

The test is:

Autopoll OFF initially, then ON.

Z1 to enable timestamps.

Host sends a stream of t... frames with a fixed period (e.g. 50ms).

It measures device timestamps on RX and checks max jitter.

This sits on top of:

The timestamp path again (buildTimestampMs and frame.timestamp acquisition in readCanFrame).

The autopoll emission loop and Serial.flush() – any batching or big serial flushes will cluster frames and make the effective timestamp vs real time look jittery.

So: same two hot zones as above:

Timestamp math: buildTimestampMs.

Autopoll / batching / flush behavior: autopoll block in Can232::loop().

TL;DR mapping

If you want a punch list of “where to look in the codebase” for each red test:

autopoll_stream, autopoll_batch_limiting, high_throughput_rx, command_latency, periodic_frame_jitter
→ can-232.cpp:

Can232::loop() autopoll section

serviceCanRx(), readCanFrame()

RX ring buffer (pushRxFrame/peekRxFrame/popRxFrame)

emitFrameToSerial() and the use of Serial.flush()

Batch limit macros in can-232.h (AUTOPOLL_MAX_BATCH_BYTES, AUTOPOLL_MAX_BATCH_TIME_MS, LW232_MAX_HW_DRAIN_PER_CALL)

autostart_persistence, autostart_listen
→ can-232.cpp:

initializeEepromIfNeeded()

loadAutoStartPreference() / persistAutoStartPreference()

maybeAutoStart()

case LW232_CMD_AUTOSTART in parseAndRunCommand

extended_frame_rejection
→ can-232.cpp:

%EXT command handler (LW232_CMD_REJECT_EXT)

lw232RejectExtendedFrames and the extended check in readCanFrame()

isExtendedFrame() implementation

timestamp_rollover
→ can-232.cpp:

buildTimestampMs()

Where frame.timestamp gets set in readCanFrame()

How often buildTimestampMs is called relative to micros() wrap
