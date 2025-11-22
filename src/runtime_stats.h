/*******************************************************************************
 * FILE: runtime_stats.h
 * 
 * DESCRIPTION:
 * Header file defining the runtime statistics structure and API for tracking
 * CAN bus adapter performance metrics and diagnostics during operation.
 * 
 * STRUCTURE:
 * 
 * 1. CanRuntimeStats STRUCTURE:
 *    - commandCount: Total LAWICEL commands received from host
 *    - framesRx: Total CAN frames successfully received
 *    - framesTx: Total CAN frames successfully transmitted
 *    - lastCommandMillis: Timestamp of most recent command
 *    - lastRxMillis: Timestamp of most recent RX frame
 *    - lastTxMillis: Timestamp of most recent TX frame
 *    - rxBufferDrops: Frames dropped due to buffer full condition
 *    - rxBufferOverflows: Count of buffer overflow events
 *    - currentFramesPerSecond: Real-time bus load metric (updated every 500ms)
 * 
 * 2. API FUNCTIONS:
 *    - statsReset(): Initialize all counters to zero
 *    - statsRecordCommand(): Increment command counter and timestamp
 *    - statsRecordRxFrame(): Increment RX counter and timestamp
 *    - statsRecordTxFrame(): Increment TX counter and timestamp
 *    - statsRecordRxDrop(): Increment drop counter
 *    - statsRecordRxOverflow(): Increment overflow counter
 * 
 * 3. GLOBAL INSTANCE:
 *    - g_canStats: Externally declared global statistics instance
 * 
 * ROLE IN CODEBASE:
 * This module provides observable metrics for the CAN adapter's operation,
 * accessible via the LAWICEL 'i' command. Statistics are updated by can-232.cpp
 * during message reception, transmission, and command processing. The volatile
 * qualifiers ensure safe access from interrupt contexts where CAN reception
 * occurs. These metrics are essential for debugging performance issues,
 * monitoring bus health, and detecting buffer overflow conditions.
 * 
 * USAGE:
 * - Called by can-232.cpp to track all CAN and protocol events
 * - Reported to host via custom 'i' command for runtime diagnostics
 * - Reset during initialization via statsReset()
 *******************************************************************************/

#ifndef RUNTIME_STATS_H
#define RUNTIME_STATS_H

#include <Arduino.h>

struct CanRuntimeStats {
    volatile unsigned long commandCount;
    volatile unsigned long framesRx;
    volatile unsigned long framesTx;
    volatile unsigned long lastCommandMillis;
    volatile unsigned long lastRxMillis;
    volatile unsigned long lastTxMillis;
    volatile unsigned long rxBufferDrops;
    volatile unsigned long rxBufferOverflows;
    volatile unsigned int currentFramesPerSecond;  // Current frames per second rate
};

extern CanRuntimeStats g_canStats;

void statsReset();
void statsRecordCommand();
void statsRecordRxFrame();
void statsRecordTxFrame();
void statsRecordRxDrop();
void statsRecordRxOverflow();

#endif  // RUNTIME_STATS_H
