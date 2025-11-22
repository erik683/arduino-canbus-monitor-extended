/*******************************************************************************
 * FILE: runtime_stats.cpp
 * 
 * DESCRIPTION:
 * Implementation of runtime statistics tracking functions for monitoring CAN bus
 * adapter performance and health. Provides thread-safe counter updates suitable
 * for use from both main loop and interrupt contexts.
 * 
 * IMPLEMENTATION DETAILS:
 * 
 * 1. GLOBAL STATE:
 *    - g_canStats: Single global CanRuntimeStats instance initialized to zero
 * 
 * 2. HELPER FUNCTION:
 *    - writeMillis(): Inline helper to capture current timestamp via millis()
 *      Used to record when events occur for temporal analysis
 * 
 * 3. STATISTICS FUNCTIONS:
 *    - statsReset(): Atomically clears all statistics counters using
 *      noInterrupts()/interrupts() to prevent race conditions during reset
 *    - statsRecordCommand(): Increments command counter (not atomic - called
 *      from main loop context only)
 *    - statsRecordRxFrame(): Increments RX counter and updates last RX time
 *    - statsRecordTxFrame(): Increments TX counter and updates last TX time
 *    - statsRecordRxDrop(): Increments drop counter when buffer is full
 *    - statsRecordRxOverflow(): Increments overflow event counter
 * 
 * THREAD SAFETY CONSIDERATIONS:
 * Most stat updates occur from the main loop, but RX stats may be updated from
 * interrupt context. The volatile qualifiers on CanRuntimeStats members ensure
 * visibility across contexts. statsReset() uses interrupt disabling for atomic
 * multi-field updates. Individual counter increments are atomic on AVR 8-bit
 * platforms for single-byte and word-sized variables.
 * 
 * ROLE IN CODEBASE:
 * Provides the implementation backing the diagnostics interface defined in
 * runtime_stats.h. Called throughout can-232.cpp to track all significant
 * events. The statistics are exposed to the host computer via the LAWICEL 'i'
 * command, enabling real-time monitoring of adapter health and performance.
 *******************************************************************************/

#include "runtime_stats.h"

static inline void writeMillis(volatile unsigned long &target) {
    target = millis();
}

CanRuntimeStats g_canStats = {};

void statsReset() {
    noInterrupts();
    g_canStats.commandCount = 0;
    g_canStats.framesRx = 0;
    g_canStats.framesTx = 0;
    g_canStats.lastCommandMillis = 0;
    g_canStats.lastRxMillis = 0;
    g_canStats.lastTxMillis = 0;
    g_canStats.rxBufferDrops = 0;
    g_canStats.rxBufferOverflows = 0;
    g_canStats.currentFramesPerSecond = 0;
    interrupts();
}

void statsRecordCommand() {
    g_canStats.commandCount++;
    writeMillis(g_canStats.lastCommandMillis);
}

void statsRecordRxFrame() {
    g_canStats.framesRx++;
    writeMillis(g_canStats.lastRxMillis);
}

void statsRecordTxFrame() {
    g_canStats.framesTx++;
    writeMillis(g_canStats.lastTxMillis);
}

void statsRecordRxDrop() {
    g_canStats.rxBufferDrops++;
}

void statsRecordRxOverflow() {
    g_canStats.rxBufferOverflows++;
}
