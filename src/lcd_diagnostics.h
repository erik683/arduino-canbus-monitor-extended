#ifndef LCD_DIAGNOSTICS_H
#define LCD_DIAGNOSTICS_H

#include <Arduino.h>

class LcdDiagnostics {
public:
    static void begin();
    static void showSplash();
    static void showSerialReady(unsigned long baud);
    static void showCanAttempt(uint8_t canSpeed, uint8_t clockSetting);
    static void showCanReady(uint8_t canSpeed, uint8_t clockSetting);
    static void showCanError(uint8_t canSpeed, uint8_t clockSetting, uint8_t errorCode);
    static void showCanClosed();
    static void showFrame(uint32_t id, uint8_t len, const uint8_t *data, bool extended);
};

#endif
