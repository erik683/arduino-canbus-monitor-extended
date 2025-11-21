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
