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
};

extern CanRuntimeStats g_canStats;

void statsReset();
void statsRecordCommand();
void statsRecordRxFrame();
void statsRecordTxFrame();

#endif  // RUNTIME_STATS_H
