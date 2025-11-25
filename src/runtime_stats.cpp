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
 *    - statsRecordCommand(): Increments command counter (atomic via interrupt disable)
 *    - statsRecordRxFrame(): Increments RX counter and updates last RX time (atomic)
 *    - statsRecordTxFrame(): Increments TX counter and updates last TX time (atomic)
 *    - statsRecordRxDrop(): Increments drop counter when buffer is full
 *    - statsRecordRxOverflow(): Increments overflow event counter
 * 
 * THREAD SAFETY CONSIDERATIONS:
 * Most stat updates occur from the main loop. The volatile qualifiers on CanRuntimeStats
 * members ensure visibility across contexts. statsReset() uses interrupt disabling for
 * atomic multi-field updates. All counter increments use interrupt disabling to ensure
 * atomicity of 32-bit operations on AVR 8-bit platforms.
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
    noInterrupts();
    g_canStats.commandCount++;
    interrupts();
    writeMillis(g_canStats.lastCommandMillis);
}

void statsRecordRxFrame() {
    noInterrupts();
    g_canStats.framesRx++;
    interrupts();
    writeMillis(g_canStats.lastRxMillis);
}

void statsRecordTxFrame() {
    noInterrupts();
    g_canStats.framesTx++;
    interrupts();
    writeMillis(g_canStats.lastTxMillis);
}

void statsRecordRxDrop() {
    noInterrupts();
    g_canStats.rxBufferDrops++;
    interrupts();
}

void statsRecordRxOverflow() {
    noInterrupts();
    g_canStats.rxBufferOverflows++;
    interrupts();
}
