#include "lcd_diagnostics.h"

#include <LiquidCrystal.h>
#include <stdio.h>
#include <string.h>

#include "mcp_can_dfs.h"

#ifndef LCD_RS_PIN
#define LCD_RS_PIN 8
#endif

#ifndef LCD_ENABLE_PIN
#define LCD_ENABLE_PIN 9
#endif

#ifndef LCD_D4_PIN
#define LCD_D4_PIN 4
#endif

#ifndef LCD_D5_PIN
#define LCD_D5_PIN 5
#endif

#ifndef LCD_D6_PIN
#define LCD_D6_PIN 6
#endif

#ifndef LCD_D7_PIN
#define LCD_D7_PIN 7
#endif

#ifndef LCD_COLS
#define LCD_COLS 16
#endif

#ifndef LCD_ROWS
#define LCD_ROWS 2
#endif

#ifndef LCD_FRAME_UPDATE_MS
#define LCD_FRAME_UPDATE_MS 150
#endif

namespace {

LiquidCrystal lcd(LCD_RS_PIN, LCD_ENABLE_PIN, LCD_D4_PIN, LCD_D5_PIN, LCD_D6_PIN, LCD_D7_PIN);
bool lcdReady = false;
char cachedLines[LCD_ROWS][LCD_COLS + 1];
unsigned long lastFrameUpdate = 0;

char hexDigit(uint8_t nibble) {
    return (nibble < 10) ? static_cast<char>('0' + nibble) : static_cast<char>('A' + nibble - 10);
}

void copyLiteral(const char *text, char *buffer, size_t size) {
    if (!buffer || size == 0) {
        return;
    }
    size_t idx = 0;
    while (text && text[idx] && idx < size - 1) {
        buffer[idx] = text[idx];
        ++idx;
    }
    buffer[idx] = '\0';
}

void clearCache() {
    for (uint8_t row = 0; row < LCD_ROWS; ++row) {
        memset(cachedLines[row], ' ', LCD_COLS);
        cachedLines[row][LCD_COLS] = '\0';
    }
}

void writeLine(uint8_t row, const char *text) {
    if (!lcdReady || row >= LCD_ROWS) {
        return;
    }
    char padded[LCD_COLS + 1];
    uint8_t idx = 0;
    if (text) {
        while (idx < LCD_COLS && text[idx]) {
            padded[idx] = text[idx];
            ++idx;
        }
    }
    while (idx < LCD_COLS) {
        padded[idx++] = ' ';
    }
    padded[LCD_COLS] = '\0';
    if (strncmp(padded, cachedLines[row], LCD_COLS) == 0) {
        return;
    }
    memcpy(cachedLines[row], padded, LCD_COLS + 1);
    lcd.setCursor(0, row);
    lcd.print(cachedLines[row]);
}

const char *bitrateLabel(uint8_t canSpeed) {
    switch (canSpeed) {
        case CAN_10KBPS: return "10k";
        case CAN_20KBPS: return "20k";
        case CAN_31K25BPS: return "31.25k";
        case CAN_33KBPS: return "33k";
        case CAN_40KBPS: return "40k";
        case CAN_50KBPS: return "50k";
        case CAN_80KBPS: return "80k";
        case CAN_83K3BPS: return "83.3k";
        case CAN_95KBPS: return "95k";
        case CAN_100KBPS: return "100k";
        case CAN_125KBPS: return "125k";
        case CAN_200KBPS: return "200k";
        case CAN_250KBPS: return "250k";
        case CAN_500KBPS: return "500k";
        case CAN_1000KBPS: return "1000k";
        case CAN_5KBPS: return "5k";
        default: return "?";
    }
}

const char *clockLabel(uint8_t clockSetting) {
    switch (clockSetting) {
        case MCP_16MHz: return "16MHz";
        case MCP_8MHz: return "8MHz";
        default: return "?MHz";
    }
}

void showSpeedLine(uint8_t canSpeed, uint8_t clockSetting) {
    char line[LCD_COLS + 1];
    snprintf(line, sizeof(line), "%s %s", bitrateLabel(canSpeed), clockLabel(clockSetting));
    writeLine(1, line);
}

void formatDataLine(const uint8_t *data, uint8_t len, char *buffer, size_t size) {
    if (!buffer || size == 0) {
        return;
    }
    size_t pos = 0;
    if (len == 0) {
        copyLiteral("No data", buffer, size);
        return;
    }
    const uint8_t displayed = len > 4 ? 4 : len;
    for (uint8_t i = 0; i < displayed && pos < size - 1; ++i) {
        if (i > 0 && pos < size - 1) {
            buffer[pos++] = ' ';
        }
        if (pos < size - 1) {
            buffer[pos++] = hexDigit(data[i] >> 4);
        }
        if (pos < size - 1) {
            buffer[pos++] = hexDigit(data[i] & 0x0F);
        }
    }
    if (len > displayed && pos < size - 4) {
        if (pos > 0 && buffer[pos - 1] != ' ') {
            buffer[pos++] = ' ';
        }
        buffer[pos++] = '.';
        buffer[pos++] = '.';
        buffer[pos++] = '.';
    }
    buffer[pos] = '\0';
}

}  // namespace

void LcdDiagnostics::begin() {
    lcd.begin(LCD_COLS, LCD_ROWS);
    lcdReady = true;
    clearCache();
    lcd.clear();
}

void LcdDiagnostics::showSplash() {
    writeLine(0, "CAN Monitor");
    writeLine(1, "LCD ready");
}

void LcdDiagnostics::showSerialReady(unsigned long baud) {
    writeLine(0, "Serial ready");
    char line[LCD_COLS + 1];
    snprintf(line, sizeof(line), "%lu baud", baud);
    writeLine(1, line);
}

void LcdDiagnostics::showCanAttempt(uint8_t canSpeed, uint8_t clockSetting) {
    writeLine(0, "CAN init...");
    showSpeedLine(canSpeed, clockSetting);
}

void LcdDiagnostics::showCanReady(uint8_t canSpeed, uint8_t clockSetting) {
    writeLine(0, "CAN OPEN");
    showSpeedLine(canSpeed, clockSetting);
}

void LcdDiagnostics::showCanError(uint8_t canSpeed, uint8_t clockSetting, uint8_t errorCode) {
    char line[LCD_COLS + 1];
    snprintf(line, sizeof(line), "CAN ERR %02X", errorCode);
    writeLine(0, line);
    showSpeedLine(canSpeed, clockSetting);
}

void LcdDiagnostics::showCanClosed() {
    writeLine(0, "CAN CLOSED");
    writeLine(1, "Send O to open");
}

void LcdDiagnostics::showFrame(uint32_t id, uint8_t len, const uint8_t *data, bool extended) {
    if (!lcdReady) {
        return;
    }
    const unsigned long now = millis();
    if (now - lastFrameUpdate < LCD_FRAME_UPDATE_MS) {
        return;
    }
    lastFrameUpdate = now;

    char line1[LCD_COLS + 1];
    if (extended) {
        snprintf(line1, sizeof(line1), "X%08lX L%u", static_cast<unsigned long>(id), static_cast<unsigned int>(len));
    } else {
        snprintf(line1, sizeof(line1), "S%03lX L%u", static_cast<unsigned long>(id & 0x7FF), static_cast<unsigned int>(len));
    }

    char line2[LCD_COLS + 1];
    formatDataLine(data, len, line2, sizeof(line2));

    writeLine(0, line1);
    writeLine(1, line2);
}
