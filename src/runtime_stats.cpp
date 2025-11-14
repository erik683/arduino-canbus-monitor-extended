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
