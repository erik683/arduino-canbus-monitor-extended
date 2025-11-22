/*****************************************************************************************
 * This is implementation of CAN BUS ASCII protocol based on LAWICEL v1.3 serial protocol
 
 *  of CAN232/CANUSB device (http://www.can232.com/docs/can232_v3.pdf)
 *
* Made for Arduino with Seeduino/ElecFreaks CAN BUS Shield based on MCP2515
*
* Copyright (C) 2015 Anton Viktorov <latonita@yandex.ru>
*                                    https://github.com/latonita/can-ascii
*
* This library is free software. You may use/redistribute it under The MIT License terms. 
*
*****************************************************************************************/

/*******************************************************************************
 * FILE: can-232.cpp
 * 
 * DESCRIPTION:
 * Implementation file for the Can232 class, providing full LAWICEL CAN232 v1.3
 * ASCII protocol and SavvyCAN GVRET binary protocol support. This is the core
 * of the CAN bus monitor, handling all protocol commands, message buffering,
 * filtering, and persistent configuration.
 * 
 * MAJOR FUNCTIONAL AREAS:
 * 
 * 1. INITIALIZATION & CONFIGURATION:
 *    - EEPROM persistence and migration (timestamp, autostart, CAN speed)
 *    - Hardware filter configuration for MCP2515 (masks/filters)
 *    - Serial baud rate switching (with safe scheduling mechanism)
 *    - Auto-start mode for power-on CAN channel opening
 * 
 * 2. COMMAND PROCESSING (parseAndRunCommand):
 *    - S: CAN bitrate selection (0-9: 10k to 1000k, plus 83.3k)
 *    - O/L: Open channel (normal/listen-only mode)
 *    - C: Close channel
 *    - t/T: Transmit standard/extended CAN frames
 *    - r/R: Transmit standard/extended RTR frames
 *    - P/A: Poll single/all pending frames
 *    - F: Read MCP2515 status flags
 *    - X: Auto-poll mode toggle
 *    - W/M/m: Hardware filter configuration
 *    - U: UART baud rate change
 *    - V/N: Version and serial number
 *    - Z: Timestamp enable/disable
 *    - Q: Autostart configuration
 *    - i: Runtime diagnostics (custom extension)
 *    - @: Debug mode and GVRET protocol switch
 * 
 * 3. DUAL PROTOCOL SUPPORT:
 *    - LAWICEL: ASCII text protocol (loopLawicel, parseAndRunCommand)
 *    - GVRET: Binary protocol (loopGvret, processGvretByte, state machine)
 *    - Automatic protocol detection via handshake bytes
 *    - Protocol-specific frame emission (emitFrameToSerial vs emitFrameToGvret)
 * 
 * 4. MESSAGE BUFFERING & FILTERING:
 *    - Circular RX buffer with configurable size
 *    - Interrupt-driven CAN frame reception (serviceCanRx)
 *    - User-defined software filters (checkPassFilter)
 *    - Hardware filters (MCP2515 masks/filters)
 *    - Overflow detection and statistics tracking
 * 
 * 5. RUNTIME MONITORING:
 *    - Bus load calculation (frames per second)
 *    - TX/RX counters, buffer drops, command count
 *    - MCP2515 error flag reading and reporting
 * 
 * STRUCTURE:
 * The Can232 class uses the singleton pattern with static interface methods
 * delegating to a private instance. State machines handle serial input parsing
 * for both LAWICEL (stringComplete flag) and GVRET (GvretRxState enum). The
 * main loop() function services the CAN RX buffer and emits frames according
 * to the selected protocol and auto-poll settings.
 * 
 * ROLE IN CODEBASE:
 * This file is the application logic layer, coordinating between:
 * - arduino-canbus-monitor.ino (setup/loop/ISR glue code)
 * - mcp_can.cpp (low-level MCP2515 SPI driver)
 * - runtime_stats.cpp (statistics tracking)
 * - Serial port (host computer communication)
 * - EEPROM (persistent configuration storage)
 *******************************************************************************/

#include <SPI.h>
#include <EEPROM.h>
#include <avr/pgmspace.h>
#include <string.h>
#include "mcp_can.h"
#include "can-232.h"
#include "runtime_stats.h"

#ifndef ENABLE_CAN_DEBUG_LOGGING
#define ENABLE_CAN_DEBUG_LOGGING 0
#endif

#if ENABLE_CAN_DEBUG_LOGGING
#define LOGGING_ENABLED
#endif

#ifdef LOGGING_ENABLED
#define dbg_begin(x) debug.begin(x)
#define dbg0(x)   if (instance() && instance()->lw232DebugMode) debug.print(x)
#define dbg1(x)   if (instance() && instance()->lw232DebugMode) debug.println(x)
#define dbg2(x,y) if (instance() && instance()->lw232DebugMode) { debug.print(x); debug.println(y); }
#define dbgH(x)   if (instance() && instance()->lw232DebugMode) debug.print(x,HEX)
#define DEBUG_RX_PIN 8
#define DEBUG_TX_PIN 9
#else
#define dbg_begin(x)
#define dbg0(x)
#define dbg1(x)
#define dbg2(x,y)
#define dbgH(x)
#endif

#define HEX_INVALID 0xFF

static const INT8U HEX_LOOKUP_TABLE[256] PROGMEM = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 0x00-0x0F
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 0x10-0x1F
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 0x20-0x2F
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 0x30-0x3F
    0xFF, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 0x40-0x4F
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 0x50-0x5F
    0xFF, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 0x60-0x6F
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 0x70-0x7F
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 0x80-0x8F
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 0x90-0x9F
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 0xA0-0xAF
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 0xB0-0xBF
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 0xC0-0xCF
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 0xD0-0xDF
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 0xE0-0xEF
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF  // 0xF0-0xFF
};

static inline INT8U readHexLookup(INT8U c) {
    return pgm_read_byte(&HEX_LOOKUP_TABLE[c]);
}

static inline void emitHex16(unsigned long value) {
    HexHelper::printFullByte((value >> 8) & 0xFF);
    HexHelper::printFullByte(value & 0xFF);
}

static bool parseHexBytes(const INT8U* input, size_t byteCount, INT8U* output) {
    for (size_t idx = 0; idx < byteCount; idx++) {
        bool ok = false;
        output[idx] = HexHelper::parseFullByte(input[idx * 2], input[idx * 2 + 1], &ok);
        if (!ok) {
            return false;
        }
    }
    return true;
}

static INT32U decodeSjaStdId(INT8U high, INT8U low) {
    // ACR/AMR layout: high = ID10..ID3, low bits 7..5 = ID2..ID0
    return ((((INT32U)high) << 3) | (((INT32U)low) >> 5)) & 0x7FF;
}

static INT32U decodeSjaExtId(const INT8U* bytes) {
    // Map 4 ACR/AMR bytes into a 29-bit identifier/mask
    INT32U id = ((INT32U)bytes[0] << 21) |
                ((INT32U)bytes[1] << 13) |
                ((INT32U)bytes[2] << 5)  |
                ((INT32U)bytes[3] >> 3);
    return id & 0x1FFFFFFF;
}

static INT32U baudIndexToBps(INT8U idx);

#ifdef LOGGING_ENABLED
    // software serial #2: TX = digital pin 8, RX = digital pin 9
    // on the Mega, use other pins instead, since 8 and 9 don't work on the Mega
  
      SoftwareSerial debug(DEBUG_RX_PIN, DEBUG_TX_PIN);
  //#define debug Serial
#endif

// Singleton instance - intentionally uses new() without delete() for embedded systems
// where memory is managed throughout the program lifecycle
Can232* Can232::_instance = 0;

Can232* Can232::instance() {
    if (_instance == 0)
        _instance = new Can232();
    return _instance;
}

void Can232::init(INT8U defaultCanSpeed, const INT8U clock) {
    dbg_begin(LW232_DEFAULT_BAUD_RATE); // logging through software serial 
    dbg1("CAN ASCII. Welcome to debug");

    instance()->lw232CanSpeedSelection = defaultCanSpeed;
    instance()->lw232CanSpeedIndex = findCanBaudIndex(defaultCanSpeed);
    instance()->lw232McpModuleClock = clock;
    instance()->lw232BitrateConfigured = false;
    instance()->initFunc();
}

void Can232::setFilter(INT8U (*userFunc)(INT32U)) {
    instance()->setFilterFunc(userFunc);
}

void Can232::loop() {
    instance()->loopFunc();
}

void Can232::serialEvent() {
    instance()->serialEventFunc();
}

void Can232::initFunc() {
    if (!inputString.reserve(LW232_INPUT_STRING_BUFFER_SIZE)) {
        dbg0("inputString.reserve failed in initFunc. less optimal String work is expected");
    }

    protocolMode = LW232_DEFAULT_PROTOCOL_MODE;
    gvretBufferLen = 0;
    gvretTimestampsEnabled = true;
    gvretSilentMode = false;
    gvretHandshakeCount = 0;
    gvretRxState = GvretRxState::WAIT_START;
    gvretRxStep = 0;
    gvretCurrentCmd = 0;
    gvretHostFrameId = 0;
    gvretHostBus = 0;
    gvretHostLen = 0;
    gvretCan0Baud = baudIndexToBps(lw232CanSpeedIndex);

    // Initialize EEPROM to defaults if not properly set - MUST be first
    initializeEepromIfNeeded();

    // Now safe to load preferences
    loadTimestampPreference();
    loadAutoStartPreference();

    // Initialize runtime state
    lw232SerialBaudIndex = LW232_DEFAULT_UART_BAUD_INDEX;
    lw232PendingSerialBaudIndex = 0xFF;
    // Channel stays closed until host selects a bitrate (LAWICEL default).
    lw232BitrateConfigured = false;
    lw232FilterMode = 0x00;
    memset(lw232AcceptanceCode, 0, sizeof(lw232AcceptanceCode));
    memset(lw232AcceptanceMask, 0, sizeof(lw232AcceptanceMask));
    lw232HwFilterDirty = true;

    clearRxBuffer();
    maybeAutoStart();
}

void Can232::setFilterFunc(INT8U (*userFunc)(INT32U)) {
    instance()->userAddressFilterFunc = userFunc;
}

void Can232::loopFunc() {
    if (protocolMode == LW232_PROTOCOL_GVRET) {
        loopGvret();
        return;
    }
    loopLawicel();
}

void Can232::loopLawicel() {
    if (stringComplete) {
        size_t len = inputString.length();
        if (len > 0 && len < LW232_FRAME_MAX_SIZE) {
            strcpy((char*)lw232Message, inputString.c_str());
            exec();
        }
        // clear the string:
        inputString = "";
        stringComplete = false;
    }
    bool servicedByInterrupt = false;
    if (consumeInterruptFlag()) {
        serviceCanRx();
        servicedByInterrupt = true;
    }
    if (!servicedByInterrupt) {
        serviceCanRx();
    }
    if (lw232CanChannelMode != LW232_STATUS_CAN_CLOSED && lw232AutoPoll == LW232_AUTOPOLL_ON) {
        int recv = 0;
        while (recv++ < 5) {
            if (CAN_OK == receiveSingleFrame()) {
                dbg0('+');
                Serial.write(LW232_CR);
            }
            else {
                break;
            }
        }
        Serial.flush();
    }

    // Update bus load metrics when CAN is active
    if (lw232CanChannelMode != LW232_STATUS_CAN_CLOSED) {
        updateBusLoad();
    }
}
void Can232::loopGvret() {
    processGvretSerial();

    bool servicedByInterrupt = false;
    if (consumeInterruptFlag()) {
        serviceCanRx();
        servicedByInterrupt = true;
    }
    if (!servicedByInterrupt) {
        serviceCanRx();
    }
    emitGvretRxFrames();

    if (lw232CanChannelMode != LW232_STATUS_CAN_CLOSED) {
        updateBusLoad();
    }
}
void Can232::processGvretSerial() {
    while (Serial.available()) {
        const INT8U b = (INT8U)Serial.read();
        processGvretByte(b);
    }
}

bool Can232::handleGvretHostFrame() {
    if (lw232CanChannelMode != LW232_STATUS_CAN_OPEN_NORMAL) {
        return false;
    }
    if (gvretHostBus != 0) {
        return false;
    }
    const bool isExtended = (gvretHostFrameId & (1UL << 31)) != 0;
    const INT32U id = isExtended ? (gvretHostFrameId & 0x1FFFFFFF) : (gvretHostFrameId & 0x7FF);
    const INT8U status = sendMsgBuf(id, isExtended ? 1 : 0, 0, gvretHostLen, gvretHostData);
    return status == CAN_OK;
}

void Can232::processGvretByte(INT8U b) {
    switch (gvretRxState) {
    case GvretRxState::WAIT_START:
        if (b == GVRET_HANDSHAKE_BYTE) {
            gvretHandshakeCount++;
            if (gvretHandshakeCount >= 2) {
                switchProtocol(LW232_PROTOCOL_GVRET);
            }
            return;
        }
        gvretHandshakeCount = 0;
        if (b == GVRET_START_BYTE) {
            gvretRxState = GvretRxState::GET_COMMAND;
        }
        break;
    case GvretRxState::GET_COMMAND:
        gvretCurrentCmd = b;
        gvretRxStep = 0;
        gvretBufferLen = 0;
        switch (gvretCurrentCmd) {
        case 0x00: // SavvyCAN transmit frame command
            gvretRxState = GvretRxState::BUILD_CAN_FRAME;
            gvretHostFrameId = 0;
            gvretHostLen = 0;
            gvretHostBus = 0;
            break;
        case 0x05: // setup CAN bus (two 32-bit words + terminator)
            gvretRxState = GvretRxState::GET_SETUP_BYTES;
            break;
        case 0x06: // request canbus params
            emitGvretBusParams();
            gvretRxState = GvretRxState::WAIT_START;
            break;
        case 0x07: // device info
            emitGvretDeviceInfo();
            gvretRxState = GvretRxState::WAIT_START;
            break;
        case 0x09: // validation ping
            emitGvretValidation();
            gvretRxState = GvretRxState::WAIT_START;
            break;
        case 0x0C: // number of buses
            emitGvretNumBuses();
            gvretRxState = GvretRxState::WAIT_START;
            break;
        case 0x0D: // extended buses info
            emitGvretExtBuses();
            gvretRxState = GvretRxState::WAIT_START;
            break;
        case 0x01: // time sync request
            emitGvretTimeSync();
            gvretRxState = GvretRxState::WAIT_START;
            break;
        default:
            gvretRxState = GvretRxState::IGNORE_COMMAND;
            break;
        }
        break;
    case GvretRxState::BUILD_CAN_FRAME:
        // Host-originated CAN frame (command 0)
        if (gvretRxStep < 4) {
            gvretHostFrameId |= ((INT32U)b << (gvretRxStep * 8));
        } else if (gvretRxStep == 4) {
            gvretHostBus = b & 0x0F;
        } else if (gvretRxStep == 5) {
            gvretHostLen = b & 0x0F;
            if (gvretHostLen > LW232_FRAME_MAX_LENGTH) {
                gvretRxState = GvretRxState::WAIT_START;
                gvretHostFrameId = 0;
                gvretHostLen = 0;
                break;
            }
        } else {
            INT8U dataIdx = gvretRxStep - 6;
            if (dataIdx < GVRET_MAX_PACKET_SIZE) {
                gvretHostData[dataIdx] = b;
            }
            if (dataIdx + 1 >= gvretHostLen) {
                // Optional terminator byte may follow; accept one extra
                handleGvretHostFrame();
                gvretHostFrameId = 0;
                gvretHostLen = 0;
                gvretRxState = GvretRxState::WAIT_START;
            }
        }
        gvretRxStep++;
        break;
    case GvretRxState::GET_SETUP_BYTES:
        // Expect two 32-bit baud descriptors plus a padding byte
        if (gvretRxStep < GVRET_MAX_PACKET_SIZE) {
            gvretBuffer[gvretRxStep] = b;
        }
        gvretRxStep++;
        if (gvretRxStep >= 9) {
            INT32U can0Descriptor = ((INT32U)gvretBuffer[0]) |
                                    ((INT32U)gvretBuffer[1] << 8) |
                                    ((INT32U)gvretBuffer[2] << 16) |
                                    ((INT32U)gvretBuffer[3] << 24);
            const bool requestListenOnly = (can0Descriptor & 0x20000000UL) != 0;
            const bool requestEnabled = (can0Descriptor & 0x40000000UL) != 0;
            const INT32U requestedBps = can0Descriptor & 0x1FFFFFFFUL;

            const INT8U newIdx = findCanBaudIndexByBps(requestedBps);
            if (newIdx != 0xFF) {
                lw232CanSpeedIndex = newIdx;
                lw232CanSpeedSelection = lw232CanBaudRates[newIdx];
                lw232BitrateConfigured = true;
                persistCanSpeedSelection();
                gvretCan0Baud = requestedBps;
            }
            gvretCan0ListenOnly = requestListenOnly;
            gvretCan0Enabled = requestEnabled;
            if (requestEnabled && lw232BitrateConfigured) {
                INT8U mode = requestListenOnly ? MODE_LISTENONLY : MODE_NORMAL;
                if (openCanBus(mode) == LW232_OK) {
                    lw232CanChannelMode = requestListenOnly ? LW232_STATUS_CAN_OPEN_LISTEN : LW232_STATUS_CAN_OPEN_NORMAL;
                }
            } else {
                lw232CanChannelMode = LW232_STATUS_CAN_CLOSED;
                clearRxBuffer();
            }
            gvretRxState = GvretRxState::WAIT_START;
        }
        break;
    case GvretRxState::IGNORE_COMMAND:
        gvretRxState = GvretRxState::WAIT_START;
        break;
    }
}

void Can232::resetGvretParser() {
    gvretBufferLen = 0;
}

void Can232::switchProtocol(INT8U mode) {
    if (mode != LW232_PROTOCOL_LAWICEL && mode != LW232_PROTOCOL_GVRET) {
        return;
    }
    protocolMode = mode;
    resetGvretParser();
    stringComplete = false;
    inputString = "";
    if (protocolMode == LW232_PROTOCOL_GVRET) {
        gvretTimestampsEnabled = true;
        gvretHandshakeCount = 0;
        gvretRxState = GvretRxState::WAIT_START;
        gvretRxStep = 0;
        gvretHostFrameId = 0;
        gvretHostLen = 0;
    }
}

void Can232::serialEventFunc() {
    if (protocolMode == LW232_PROTOCOL_GVRET) {
        return;
    }
    while (Serial.available()) {
        char inChar = (char)Serial.read();
        if (inChar == '\n') {
            // Hosts often send CRLF; ignore the line-feed so commands don't get prefixed with '\n'
            continue;
        }
        if ((INT8U)inChar == GVRET_HANDSHAKE_BYTE) {
            gvretHandshakeCount++;
            if (gvretHandshakeCount >= 2) {
                switchProtocol(LW232_PROTOCOL_GVRET);
                return;
            }
        } else {
            gvretHandshakeCount = 0;
        }
        if (inputString.length() < LW232_INPUT_STRING_BUFFER_SIZE - 1) {
            inputString += inChar;
        }
        if (inChar == LW232_CR) {
            stringComplete = true;
            break;
        }
    }
}

void Can232::notifyCanInterrupt() {
    instance()->mcpInterruptPending = true;
}

INT8U Can232::exec() {
    dbg2("Command received:", inputString);
    statsRecordCommand();
    lw232LastErr = parseAndRunCommand();
    switch (lw232LastErr) {
    case LW232_OK:
        Serial.write(LW232_RET_ASCII_OK);
        break;
    case LW232_OK_SMALL:
        Serial.write(LW232_RET_ASCII_OK_SMALL);
        Serial.write(LW232_RET_ASCII_OK);
        break;
    case LW232_OK_BIG:
        Serial.write(LW232_RET_ASCII_OK_BIG);
        Serial.write(LW232_RET_ASCII_OK);
        break;
    case LW232_ERR_NOT_IMPLEMENTED:
        dbg2("Command not implemented:", lw232Message[0]);
        // Choose behavior: will it fail or not when not implemented command comes in. Some can monitors might be affected by this selection.
        Serial.write(LW232_RET_ASCII_ERROR);
        //Serial.write(LW232_RET_ASCII_OK);
        break;
    case LW232_ERR_UNKNOWN_CMD:
        dbg2("Unknown command:", lw232Message[0]);
        Serial.write(LW232_RET_ASCII_ERROR);
        break;
    default:
        dbg2("Command error:", lw232LastErr);
        Serial.write(LW232_RET_ASCII_ERROR);
    }
    applyPendingSerialBaudChange();
    return 0;
}

INT8U Can232::parseAndRunCommand() {
    INT8U ret = LW232_OK;
    INT8U idx = 0;
    lw232LastErr = LW232_OK;

    const size_t msgLen = strlen((char*)lw232Message);
    if (msgLen < 1) {
        return LW232_ERR;
    }
    if (lw232Message[msgLen - 1] != LW232_CR) {
        return LW232_ERR;
    }
    const size_t payloadLen = msgLen - 1;

    switch (lw232Message[0]) {
        case LW232_CMD_SETUP:
        // Sn[CR] Setup with standard CAN bit-rates where n is 0-9.
        if (payloadLen < 2) {
            ret = LW232_ERR;
        }
        else if (lw232CanChannelMode == LW232_STATUS_CAN_CLOSED) {
            idx = HexHelper::parseNibbleWithLimit(lw232Message[1], LW232_CAN_BAUD_NUM);
            if (idx == HEX_INVALID) {
                ret = LW232_ERR;
            } else {
                lw232CanSpeedIndex = idx;
                lw232CanSpeedSelection = lw232CanBaudRates[idx];
                gvretCan0Baud = baudIndexToBps(idx);
                lw232BitrateConfigured = true;
                persistCanSpeedSelection();
            }
        }
        else {
            ret = LW232_ERR;
        }
        break;
        case LW232_CMD_SETUP_BTR:
        // sxxyy[CR] Setup with BTR0/BTR1 CAN bit-rates where xx and yy is a hex value.
        ret = LW232_ERR; break;
        case LW232_CMD_OPEN:
        // O[CR] Open the CAN channel in normal mode (sending & receiving).
        if (!lw232BitrateConfigured) {
            ret = LW232_ERR;
        }
        else if (lw232CanChannelMode == LW232_STATUS_CAN_CLOSED) {
            ret = openCanBus(MODE_NORMAL);
            if (ret == LW232_OK) {
              lw232CanChannelMode = LW232_STATUS_CAN_OPEN_NORMAL;
              gvretCan0Enabled = true;
              gvretCan0ListenOnly = false;
            }
        }
        else {
            ret = LW232_OK;
        }
        break;
        case LW232_CMD_LISTEN:
        // L[CR] Open the CAN channel in listen only mode (receiving).
        if (!lw232BitrateConfigured) {
            ret = LW232_ERR;
        }
        else if (lw232CanChannelMode == LW232_STATUS_CAN_CLOSED) {
            ret = openCanBus(MODE_LISTENONLY);
            if (ret == LW232_OK) {
              lw232CanChannelMode = LW232_STATUS_CAN_OPEN_LISTEN;
              gvretCan0Enabled = true;
              gvretCan0ListenOnly = true;
            }
        }
        else {
            ret = LW232_ERR;
        }
        break;
        case LW232_CMD_CLOSE:
        // C[CR] Close the CAN channel.
        if (lw232CanChannelMode != LW232_STATUS_CAN_CLOSED) {
            lw232CanChannelMode = LW232_STATUS_CAN_CLOSED;
            clearRxBuffer();
            gvretCan0Enabled = false;
        }
        else {
            ret = LW232_ERR;
        }
        break;
    case LW232_CMD_TX11:
        // tiiildd...[CR] Transmit a standard (11bit) CAN frame.
        if (lw232CanChannelMode == LW232_STATUS_CAN_OPEN_NORMAL) {
            if (payloadLen <= LW232_OFFSET_STD_PKT_LEN) {
                ret = LW232_ERR;
                break;
            }
            if (!parseCanStdId()) {
                ret = LW232_ERR;
                break;
            }
            lw232PacketLen = HexHelper::parseNibbleWithLimit(
                lw232Message[LW232_OFFSET_STD_PKT_LEN],
                LW232_FRAME_MAX_LENGTH + 1
            );
            if (lw232PacketLen == HEX_INVALID) {
                ret = LW232_ERR;
                break;
            }
            const size_t requiredPayload = LW232_OFFSET_STD_PKT_DATA + (size_t)lw232PacketLen * 2;
            if (payloadLen != requiredPayload) {
                ret = LW232_ERR;
                break;
            }
            bool dataOk = true;
            for (INT8U dataIdx = 0; dataIdx < lw232PacketLen; dataIdx++) {
                bool parsed = false;
                lw232Buffer[dataIdx] = HexHelper::parseFullByte(
                    lw232Message[LW232_OFFSET_STD_PKT_DATA + dataIdx * 2],
                    lw232Message[LW232_OFFSET_STD_PKT_DATA + dataIdx * 2 + 1],
                    &parsed
                );
                if (!parsed) {
                    dataOk = false;
                    break;
                }
            }
            if (!dataOk) {
                ret = LW232_ERR;
                break;
            }
            INT8U mcpErr = sendMsgBuf(lw232CanId, 0, 0, lw232PacketLen, lw232Buffer);
            if (mcpErr != CAN_OK) {
                ret = LW232_ERR;
            } else if (lw232AutoPoll) {
                ret = LW232_OK_SMALL;
            }
        }
        else {
            ret = LW232_ERR;
        }
        break;
    case LW232_CMD_TX29:
        // Tiiiiiiiildd...[CR] Transmit an extended (29bit) CAN frame
        if (lw232CanChannelMode == LW232_STATUS_CAN_OPEN_NORMAL) {
            if (payloadLen <= LW232_OFFSET_EXT_PKT_LEN) {
                ret = LW232_ERR;
                break;
            }
            if (!parseCanExtId()) {
                ret = LW232_ERR;
                break;
            }
            lw232PacketLen = HexHelper::parseNibbleWithLimit(
                lw232Message[LW232_OFFSET_EXT_PKT_LEN],
                LW232_FRAME_MAX_LENGTH + 1
            );
            if (lw232PacketLen == HEX_INVALID) {
                ret = LW232_ERR;
                break;
            }
            const size_t requiredPayload = LW232_OFFSET_EXT_PKT_DATA + (size_t)lw232PacketLen * 2;
            if (payloadLen != requiredPayload) {
                ret = LW232_ERR;
                break;
            }
            bool dataOk = true;
            for (INT8U dataIdx = 0; dataIdx < lw232PacketLen; dataIdx++) {
                bool parsed = false;
                lw232Buffer[dataIdx] = HexHelper::parseFullByte(
                    lw232Message[LW232_OFFSET_EXT_PKT_DATA + dataIdx * 2],
                    lw232Message[LW232_OFFSET_EXT_PKT_DATA + dataIdx * 2 + 1],
                    &parsed
                );
                if (!parsed) {
                    dataOk = false;
                    break;
                }
            }
            if (!dataOk) {
                ret = LW232_ERR;
                break;
            }
            if (CAN_OK != sendMsgBuf(lw232CanId, 1, 0, lw232PacketLen, lw232Buffer)) {
                ret = LW232_ERR;
            } else if (lw232AutoPoll) {
                ret = LW232_OK_BIG;
            } else {
              ret = LW232_OK;
            }
        }
        else {
            ret = LW232_ERR;
        }
        break;
    case LW232_CMD_RTR11:
        // riiil[CR] Transmit an standard RTR (11bit) CAN frame.
        if (lw232CanChannelMode == LW232_STATUS_CAN_OPEN_NORMAL) {
            if (payloadLen <= LW232_OFFSET_STD_PKT_LEN) {
                ret = LW232_ERR;
                break;
            }
            if (!parseCanStdId()) {
                ret = LW232_ERR;
                break;
            }
            lw232PacketLen = HexHelper::parseNibbleWithLimit(
                lw232Message[LW232_OFFSET_STD_PKT_LEN],
                LW232_FRAME_MAX_LENGTH + 1
            );
            if (lw232PacketLen == HEX_INVALID || payloadLen != LW232_OFFSET_STD_PKT_DATA) {
                ret = LW232_ERR;
                break;
            }
            if (CAN_OK != sendMsgBuf(lw232CanId, 0, 1, lw232PacketLen, lw232Buffer)) {
                ret = LW232_ERR;
            }
            else if (lw232AutoPoll) {
                ret = LW232_OK_SMALL;
            }
        }
        else {
            ret = LW232_ERR;
        }
        break;
    case LW232_CMD_RTR29:
        // Riiiiiiiil[CR] Transmit an extended RTR (29bit) CAN frame.
        if (lw232CanChannelMode == LW232_STATUS_CAN_OPEN_NORMAL) {
            if (payloadLen <= LW232_OFFSET_EXT_PKT_LEN) {
                ret = LW232_ERR;
                break;
            }
            if (!parseCanExtId()) {
                ret = LW232_ERR;
                break;
            }
            lw232PacketLen = HexHelper::parseNibbleWithLimit(
                lw232Message[LW232_OFFSET_EXT_PKT_LEN],
                LW232_FRAME_MAX_LENGTH + 1
            );
            if (lw232PacketLen == HEX_INVALID || payloadLen != LW232_OFFSET_EXT_PKT_DATA) {
                ret = LW232_ERR;
                break;
            }
            if (CAN_OK != sendMsgBuf(lw232CanId, 1, 1, lw232PacketLen, lw232Buffer)) {
                ret = LW232_ERR;
            }
            else if (lw232AutoPoll) {
                ret = LW232_OK_SMALL; // not a typo. strangely can232_v3.pdf tells to return "z[CR]", not "Z[CR]" as in 29bit. todo: check if it is error in pdf???
            }
        } else {
            ret = LW232_ERR;
        }
        break;
    case LW232_CMD_POLL_ONE:
        // P[CR] Poll incomming FIFO for CAN frames (single poll)
        if (lw232CanChannelMode != LW232_STATUS_CAN_CLOSED && lw232AutoPoll == LW232_AUTOPOLL_OFF) {
            serviceCanRx();
            if (!rxBufferEmpty()) {
                ret = receiveSingleFrame();
            } else {
                ret = LW232_ERR;
            }
        } else {
            ret = LW232_ERR;
        }
        break;
    case LW232_CMD_POLL_MANY:
        // A[CR] Polls incomming FIFO for CAN frames (all pending frames)
        if (lw232CanChannelMode != LW232_STATUS_CAN_CLOSED && lw232AutoPoll == LW232_AUTOPOLL_OFF) {
            serviceCanRx();
            while (!rxBufferEmpty()) {
                INT8U frameRet = receiveSingleFrame();
                if (frameRet != LW232_OK) {
                    ret = frameRet;
                    break;
                }
                Serial.write(LW232_CR);
            }
            if (ret == LW232_OK)
                Serial.print(LW232_ALL);
        } else {
            ret = LW232_ERR;
        }
        break;
    case LW232_CMD_FLAGS:
        // F[CR] Read Status Flags.
        if (lw232CanChannelMode == LW232_STATUS_CAN_CLOSED) {
            ret = LW232_ERR;
        } else {
            Serial.print(LW232_FLAG);
            HexHelper::printFullByte(readLawicelStatusFlags());
        }
        break;
    case LW232_CMD_AUTOPOLL: {
        // Xn[CR] Sets Auto Poll/Send ON/OFF for received frames. Bare X[CR] reports the current mode.
        const INT8U autopollChar = lw232Message[1];
        const bool hasArgument = (autopollChar != LW232_CR && autopollChar != 0);
        if (!hasArgument) {
            Serial.print(LW232_CMD_AUTOPOLL);
            Serial.print(lw232AutoPoll == LW232_AUTOPOLL_ON ? LW232_ON_ONE : LW232_OFF);
            break;
        }
        if (lw232CanChannelMode == LW232_STATUS_CAN_CLOSED) {
            lw232AutoPoll = (autopollChar == LW232_ON_ONE) ? LW232_AUTOPOLL_ON : LW232_AUTOPOLL_OFF;
            // TODO: save to EEPROM
        } else {
            ret = LW232_ERR;
        }
        break;
    }
    case LW232_CMD_FILTER:
        // Wn[CR] Filter mode setting. By default CAN232 works in dual filter mode (0) and is backwards compatible with previous CAN232 versions.
        // Supports query when called without an argument (returns W0/W1).
        {
            const INT8U modeChar = lw232Message[1];
            const bool hasArgument = (modeChar != LW232_CR && modeChar != 0);
            if (!hasArgument) {
                Serial.print(LW232_CMD_FILTER);
                Serial.print(lw232FilterMode ? LW232_ON_ONE : LW232_OFF);
                break;
            }
            if (payloadLen != 2 || (modeChar != LW232_OFF && modeChar != LW232_ON_ONE)) {
                ret = LW232_ERR;
                break;
            }
            if (lw232CanChannelMode != LW232_STATUS_CAN_CLOSED) {
                ret = LW232_ERR;
                break;
            }
            lw232FilterMode = (modeChar == LW232_ON_ONE) ? 0x01 : 0x00;
            markHardwareFiltersDirty();
        }
        break;
    case LW232_CMD_ACC_CODE: {
        // Mxxxxxxxx[CR] Sets Acceptance Code Register (ACn Register of SJA1000).
        const bool hasArgument = (payloadLen > 1);
        if (!hasArgument) {
            Serial.print(LW232_CMD_ACC_CODE);
            for (INT8U idx = 0; idx < 4; idx++) {
                HexHelper::printFullByte(lw232AcceptanceCode[idx]);
            }
            break;
        }
        if (payloadLen != 9 || lw232CanChannelMode != LW232_STATUS_CAN_CLOSED) {
            ret = LW232_ERR;
            break;
        }
        if (!parseHexBytes(lw232Message + 1, 4, lw232AcceptanceCode)) {
            ret = LW232_ERR;
            break;
        }
        markHardwareFiltersDirty();
        break;
    }
    case LW232_CMD_ACC_MASK: {
        // mxxxxxxxx[CR] Sets Acceptance Mask Register (AMn Register of SJA1000).
        const bool hasArgument = (payloadLen > 1);
        if (!hasArgument) {
            Serial.print(LW232_CMD_ACC_MASK);
            for (INT8U idx = 0; idx < 4; idx++) {
                HexHelper::printFullByte(lw232AcceptanceMask[idx]);
            }
            break;
        }
        if (payloadLen != 9 || lw232CanChannelMode != LW232_STATUS_CAN_CLOSED) {
            ret = LW232_ERR;
            break;
        }
        if (!parseHexBytes(lw232Message + 1, 4, lw232AcceptanceMask)) {
            ret = LW232_ERR;
            break;
        }
        markHardwareFiltersDirty();
        break;
    }
    case LW232_CMD_UART: {
        // Un[CR] Setup UART with a new baud rate where n is 0-7. Bare U[CR] reports the current selection.
        const INT8U modeChar = lw232Message[1];
        if (modeChar == LW232_CR || modeChar == 0) {
            Serial.print(LW232_CMD_UART);
            HexHelper::printNibble(lw232SerialBaudIndex);
            break;
        }
        if (modeChar < '0' || modeChar > ('0' + LW232_UART_BAUD_NUM - 1)) {
            ret = LW232_ERR;
            break;
        }
        idx = modeChar - '0';
        if (idx >= LW232_UART_BAUD_NUM) {
            ret = LW232_ERR;
            break;
        }
        if (idx != lw232SerialBaudIndex) {
            scheduleSerialBaudChange(idx);
        }
        break;
    }
    case LW232_CMD_VERSION1:
    case LW232_CMD_VERSION2:
        // V[CR] Get Version number of both CAN232 hardware and software
        Serial.print(LW232_LAWICEL_VERSION_STR);
        break;
    case LW232_CMD_SERIAL:
        // N[CR] Get Serial number of the CAN232.
        Serial.print(LW232_LAWICEL_SERIAL_NUM);
        break;
    case LW232_CMD_TIMESTAMP: {
        // Zn[CR] Sets Time Stamp ON/OFF for received frames only. Bare Z[CR] reports the current mode.
        const INT8U modeChar = lw232Message[1];
        const bool hasArgument = (modeChar != LW232_CR && modeChar != 0);
        if (!hasArgument) {
            Serial.print(LW232_CMD_TIMESTAMP);
            Serial.print(lw232TimeStamp == LW232_TIMESTAMP_ON_NORMAL ? LW232_ON_ONE : LW232_OFF);
            break;
        }
        if (lw232CanChannelMode != LW232_STATUS_CAN_CLOSED) {
            ret = LW232_ERR;
            break;
        }
        INT8U newMode;
        if (modeChar == LW232_ON_ONE) {
            newMode = LW232_TIMESTAMP_ON_NORMAL;
        }
        else if (modeChar == LW232_OFF) {
            newMode = LW232_TIMESTAMP_OFF;
        }
        else {
            ret = LW232_ERR;
            break;
        }
        if (newMode != lw232TimeStamp) {
            lw232TimeStamp = newMode;
            persistTimestampPreference();
        }
        break;
    }
    case LW232_CMD_AUTOSTART: {
        // Qn[CR] Auto Startup feature (from power on). Bare Q[CR] reports the current mode.
        const INT8U modeChar = lw232Message[1];
        const bool hasArgument = (modeChar != LW232_CR && modeChar != 0);
        if (!hasArgument) {
            Serial.print(LW232_CMD_AUTOSTART);
            INT8U statusChar = LW232_OFF;
            if (lw232AutoStart == LW232_AUTOSTART_ON_NORMAL) {
                statusChar = LW232_ON_ONE;
            } else if (lw232AutoStart == LW232_AUTOSTART_ON_LISTEN) {
                statusChar = LW232_ON_TWO;
            }
            Serial.print((char)statusChar);  // Cast to char explicitly
            break;
        }
        if (lw232CanChannelMode != LW232_STATUS_CAN_CLOSED) {
            ret = LW232_ERR;
            break;
        }
        INT8U newMode;
        if (modeChar == LW232_ON_ONE) {
            newMode = LW232_AUTOSTART_ON_NORMAL;
        }
        else if (modeChar == LW232_ON_TWO) {
            newMode = LW232_AUTOSTART_ON_LISTEN;
        }
        else if (modeChar == LW232_OFF) {
            newMode = LW232_AUTOSTART_OFF;
        }
        else {
            ret = LW232_ERR;
            break;
        }
        if (newMode != lw232AutoStart) {
            lw232AutoStart = newMode;
            persistAutoStartPreference();
        }
        break;
    }
    case LW232_CMD_INFO: {
        if (lw232CanChannelMode == LW232_STATUS_CAN_CLOSED) {
            ret = LW232_ERR;
            break;
        }
        Serial.print(LW232_CMD_INFO);
        emitHex16(g_canStats.commandCount);
        emitHex16(g_canStats.framesRx);
        emitHex16(g_canStats.framesTx);
        emitHex16(millis() / 1000UL);
        emitHex16(g_canStats.rxBufferDrops);
        emitHex16(g_canStats.rxBufferOverflows);
        HexHelper::printFullByte((INT8U)(g_canStats.currentFramesPerSecond & 0xFF));
        // Include debug mode status in diagnostic output
        Serial.print(lw232DebugMode ? "D" : "d");
        break;
    }
    case LW232_CMD_DEBUG: {
        if (payloadLen >= 6 && strncmp((char*)lw232Message, "@GVRET", 6) == 0) {
            switchProtocol(LW232_PROTOCOL_GVRET);
            Serial.print("GVRET");
            break;
        }
        // @DBGn[CR] Runtime debug toggle (0=off, 1=on)
        if (payloadLen >= 4 && lw232Message[1] == 'D' && lw232Message[2] == 'B' && lw232Message[3] == 'G') {
            if (lw232Message[4] == '0') {
                lw232DebugMode = false;
                Serial.print("DEBUG OFF\r");
            } else if (lw232Message[4] == '1') {
                lw232DebugMode = true;
                Serial.print("DEBUG ON\r");
            } else {
                ret = LW232_ERR;
            }
        } else {
            ret = LW232_ERR;
        }
        break;
    }
    default:
        ret = LW232_ERR_UNKNOWN_CMD;
    }

    return ret;
}

INT8U Can232::checkReceive() {
#ifndef _MCP_FAKE_MODE_
    return lw232CAN.checkReceive();
#else
    return CAN_MSGAVAIL;
#endif
}

INT8U Can232::readMsgBufID(INT32U *ID, INT8U *len, INT8U buf[]) {
#ifndef _MCP_FAKE_MODE_
    return lw232CAN.readMsgBufID(ID, len, buf);
#else
    *ID = random(0x100, 0x110);
    *len = 4;
    buf[0] = random(0x01, 0x10);
    buf[1] = random(0xa1, 0xf0);
    buf[2] = 0x00;
    buf[3] = 0x00;
    return CAN_OK;
#endif
}



INT8U Can232::receiveSingleFrame() {
    BufferedFrame frame;
    serviceCanRx();
    if (!popRxFrame(frame)) {
        return LW232_ERR;
    }
    emitFrameToSerial(frame);
    statsRecordRxFrame();
    return LW232_OK;
}

void Can232::emitFrameToSerial(const BufferedFrame& frame) {
    char buffer[34];
    INT8U pos = 0;
    const bool isExtended = (frame.flags & LW232_FRAME_FLAG_EXTENDED) != 0;

    buffer[pos++] = isExtended ? LW232_TR29 : LW232_TR11;

    if (isExtended) {
        HexHelper::byteToHex((INT8U)((frame.id >> 24) & 0xFF), &buffer[pos]); pos += 2;
        HexHelper::byteToHex((INT8U)((frame.id >> 16) & 0xFF), &buffer[pos]); pos += 2;
        HexHelper::byteToHex((INT8U)((frame.id >> 8) & 0xFF), &buffer[pos]); pos += 2;
        HexHelper::byteToHex((INT8U)(frame.id & 0xFF), &buffer[pos]); pos += 2;
    } else {
        const INT16U canId = (INT16U)(frame.id & 0x7FF);
        buffer[pos++] = HexHelper::toHexChar((canId >> 8) & 0x0F);
        HexHelper::byteToHex((INT8U)(canId & 0xFF), &buffer[pos]); pos += 2;
    }

    buffer[pos++] = HexHelper::toHexChar(frame.len & 0x0F);

    for (INT8U idx = 0; idx < frame.len; idx++) {
        HexHelper::byteToHex(frame.data[idx], &buffer[pos]);
        pos += 2;
    }

    if (lw232TimeStamp == LW232_TIMESTAMP_ON_NORMAL) {
        HexHelper::byteToHex(HIGH_BYTE(frame.timestamp), &buffer[pos]); pos += 2;
        HexHelper::byteToHex(LOW_BYTE(frame.timestamp), &buffer[pos]); pos += 2;
    }

    buffer[pos++] = LW232_CR;
    Serial.write(buffer, pos);
}

void Can232::emitFrameToGvret(const BufferedFrame& frame) {
    const bool isExtended = (frame.flags & LW232_FRAME_FLAG_EXTENDED) != 0;
    const INT32U idField = isExtended ? (frame.id | (1UL << 31)) : frame.id;
    const INT32U timestampMicros = gvretTimestampsEnabled ? (INT32U)frame.timestamp * 1000UL : 0;
    const INT8U busEncoded = 0; // single-bus hardware

    Serial.write(GVRET_START_BYTE);
    Serial.write((INT8U)0x00); // CAN frame indicator

    Serial.write((INT8U)(timestampMicros & 0xFF));
    Serial.write((INT8U)((timestampMicros >> 8) & 0xFF));
    Serial.write((INT8U)((timestampMicros >> 16) & 0xFF));
    Serial.write((INT8U)((timestampMicros >> 24) & 0xFF));

    Serial.write((INT8U)(idField & 0xFF));
    Serial.write((INT8U)((idField >> 8) & 0xFF));
    Serial.write((INT8U)((idField >> 16) & 0xFF));
    Serial.write((INT8U)((idField >> 24) & 0xFF));

    INT8U lenBus = (INT8U)((busEncoded << 4) & 0xF0);
    lenBus |= (frame.len & 0x0F);
    Serial.write(lenBus);
    for (INT8U idx = 0; idx < frame.len; idx++) {
        Serial.write(frame.data[idx]);
    }
}

void Can232::emitGvretRxFrames() {
    if (lw232CanChannelMode == LW232_STATUS_CAN_CLOSED) {
        return;
    }

    INT8U emitted = 0;
    BufferedFrame frame;
    while (emitted < GVRET_MAX_FRAMES_PER_LOOP && popRxFrame(frame)) {
        emitFrameToGvret(frame);
        statsRecordRxFrame();
        emitted++;
    }
}

void Can232::emitGvretBusParams() {
    Serial.write(GVRET_START_BYTE);
    Serial.write((INT8U)0x06);

    INT8U flags0 = 0;
    if (gvretCan0Enabled) {
        flags0 |= 0x01;
    }
    if (gvretCan0ListenOnly) {
        flags0 |= 0x10;
    }
    Serial.write(flags0);
    Serial.write((INT8U)(gvretCan0Baud & 0xFF));
    Serial.write((INT8U)((gvretCan0Baud >> 8) & 0xFF));
    Serial.write((INT8U)((gvretCan0Baud >> 16) & 0xFF));
    Serial.write((INT8U)((gvretCan0Baud >> 24) & 0xFF));

    // CAN1 not present
    Serial.write((INT8U)0x00);
    Serial.write((INT8U)0x00);
    Serial.write((INT8U)0x00);
    Serial.write((INT8U)0x00);
    Serial.write((INT8U)0x00);
}

void Can232::emitGvretDeviceInfo() {
    Serial.write(GVRET_START_BYTE);
    Serial.write((INT8U)0x07);
    Serial.write((INT8U)(GVRET_DEVICE_BUILD_NUM & 0xFF));
    Serial.write((INT8U)((GVRET_DEVICE_BUILD_NUM >> 8) & 0xFF));
    Serial.write((INT8U)0x00); // eeprom version
    Serial.write((INT8U)0x01); // file type marker
    Serial.write((INT8U)0x00); // autolog
    Serial.write((INT8U)0x00); // single wire not supported
}

void Can232::emitGvretNumBuses() {
    Serial.write(GVRET_START_BYTE);
    Serial.write((INT8U)0x0C);
    Serial.write((INT8U)0x01); // only CAN0
}

void Can232::emitGvretExtBuses() {
    Serial.write(GVRET_START_BYTE);
    Serial.write((INT8U)0x0D);
    // SWCAN, LIN1, LIN2 placeholders (15 bytes total)
    for (INT8U idx = 0; idx < 15; idx++) {
        Serial.write((INT8U)0x00);
    }
}

void Can232::emitGvretValidation() {
    Serial.write(GVRET_START_BYTE);
    Serial.write((INT8U)0x09);
}

void Can232::emitGvretTimeSync() {
    Serial.write(GVRET_START_BYTE);
    Serial.write((INT8U)0x01);
    const INT32U nowUs = micros();
    Serial.write((INT8U)(nowUs & 0xFF));
    Serial.write((INT8U)((nowUs >> 8) & 0xFF));
    Serial.write((INT8U)((nowUs >> 16) & 0xFF));
    Serial.write((INT8U)((nowUs >> 24) & 0xFF));
}

void Can232::serviceCanRx() {
    if (lw232CanChannelMode == LW232_STATUS_CAN_CLOSED) {
        return;
    }
    const INT16U maxDrainAttempts = LW232_RX_BUFFER_SIZE;
    INT16U processed = 0;
    while (processed < maxDrainAttempts && CAN_MSGAVAIL == checkReceive()) {
        processed++;
        BufferedFrame frame;
        const RxReadStatus status = readCanFrame(frame);
        if (status == RX_READ_NONE) {
            break;
        }
        if (status == RX_READ_SKIPPED) {
            continue;
        }
        if (!pushRxFrame(frame)) {
            statsRecordRxDrop();
            statsRecordRxOverflow();
            if (lw232DebugMode) {
                dbg1("RX buffer overflow");
            }
        } else if (lw232DebugMode) {
            dbg2("RX frame processed, ID:", frame.id);
        }
    }
}

bool Can232::consumeInterruptFlag() {
    bool pending = false;
    noInterrupts();
    if (mcpInterruptPending) {
        pending = true;
        mcpInterruptPending = false;
    }
    interrupts();
    return pending;
}

Can232::RxReadStatus Can232::readCanFrame(BufferedFrame& frame) {
    INT32U canId = 0;
    INT8U len = 0;
    if (CAN_OK != readMsgBufID(&canId, &len, frame.data)) {
        return RX_READ_NONE;
    }
    if (canId > 0x1FFFFFFF) {
        return RX_READ_SKIPPED;
    }

    frame.flags = 0;
    const INT8U extended = isExtendedFrame();
    if (extended) {
        frame.flags |= LW232_FRAME_FLAG_EXTENDED;
    }
#ifndef _MCP_FAKE_MODE_
    if (lw232CAN.isRemoteRequest()) {
        frame.flags |= LW232_FRAME_FLAG_REMOTE;
    }
#endif

    if (checkPassFilter(canId) != LW232_FILTER_PROCESS) {
        return RX_READ_SKIPPED;
    }

    frame.id = extended ? (canId & 0x1FFFFFFF) : (canId & 0x7FF);
    frame.len = (len > LW232_FRAME_MAX_LENGTH) ? LW232_FRAME_MAX_LENGTH : len;
    frame.timestamp = static_cast<INT16U>(millis() % 60000);
    return RX_READ_READY;
}

bool Can232::rxBufferEmpty() const {
    return rxCount == 0;
}

void Can232::clearRxBuffer() {
    noInterrupts();
    rxHead = 0;
    rxTail = 0;
    rxCount = 0;
    interrupts();
}

bool Can232::pushRxFrame(const BufferedFrame& frame) {
    bool pushed = true;
    noInterrupts();
    if (rxCount >= LW232_RX_BUFFER_SIZE) {
        pushed = false;
    } else {
        rxBuffer[rxHead] = frame;
        rxHead++;
        if (rxHead >= LW232_RX_BUFFER_SIZE) {
            rxHead = 0;
        }
        rxCount++;
    }
    interrupts();
    return pushed;
}

bool Can232::popRxFrame(BufferedFrame& frame) {
    bool popped = false;
    noInterrupts();
    if (rxCount > 0) {
        frame = rxBuffer[rxTail];
        rxTail++;
        if (rxTail >= LW232_RX_BUFFER_SIZE) {
            rxTail = 0;
        }
        rxCount--;
        popped = true;
    }
    interrupts();
    return popped;
}


INT8U Can232::isExtendedFrame() {
#ifndef _MCP_FAKE_MODE_
    return lw232CAN.isExtendedFrame();
#else
    return lw232CanId > 0x7FF ? 1 : 0; //simple hack for fake mode
#endif
}


INT8U Can232::checkPassFilter(INT32U addr) {
	if (userAddressFilterFunc == 0) 
		return LW232_FILTER_PROCESS;
	
	return (*userAddressFilterFunc)(addr);
}

void Can232::markHardwareFiltersDirty() {
    lw232HwFilterDirty = true;
}

bool Can232::applyHardwareFilters(INT8U targetMode) {
#ifndef _MCP_FAKE_MODE_
    const bool singleMode = (lw232FilterMode != 0);

    // Decode standard filters and masks
    const INT32U stdFilter0 = decodeSjaStdId(lw232AcceptanceCode[0], lw232AcceptanceCode[1]);
    const INT32U stdFilter1 = decodeSjaStdId(lw232AcceptanceCode[2], lw232AcceptanceCode[3]);
    const INT32U stdMask0 = decodeSjaStdId(lw232AcceptanceMask[0], lw232AcceptanceMask[1]);
    const INT32U stdMask1 = decodeSjaStdId(lw232AcceptanceMask[2], lw232AcceptanceMask[3]);

    // Decode extended filter and mask
    const INT32U extFilter = decodeSjaExtId(lw232AcceptanceCode);
    const INT32U extMask = decodeSjaExtId(lw232AcceptanceMask);

    INT8U status = MCP2515_OK;

    if (singleMode) {
        // Single mode: focus on extended ID filtering with duplicated filters for coverage
        status |= lw232CAN.init_Mask(0, 1, extMask);
        status |= lw232CAN.init_Mask(1, 1, extMask);
        for (INT8U f = 0; f < 6; f++) {
            status |= lw232CAN.init_Filt(f, 1, extFilter);
        }
    } else {
        // Dual mode: two standard masks with filters distributed correctly
        status |= lw232CAN.init_Mask(0, 0, stdMask0);
        status |= lw232CAN.init_Mask(1, 0, stdMask1);
        // Filters 0-1 controlled by RXM0 (stdMask0)
        status |= lw232CAN.init_Filt(0, 0, stdFilter0);
        status |= lw232CAN.init_Filt(1, 0, stdFilter0);
        // Filters 2-5 controlled by RXM1 (stdMask1)
        status |= lw232CAN.init_Filt(2, 0, stdFilter1);
        status |= lw232CAN.init_Filt(3, 0, stdFilter1);
        status |= lw232CAN.init_Filt(4, 0, stdFilter1);
        status |= lw232CAN.init_Filt(5, 0, stdFilter1);
    }

    lw232CAN.setMode(targetMode);
    lw232HwFilterDirty = (status != MCP2515_OK);
    if (status != MCP2515_OK) {
        dbg1("MCP2515 filter programming failed with status " + String(status));
    }
    return status == MCP2515_OK;
#else
    (void)targetMode;
    lw232HwFilterDirty = false;
    return true;
#endif
}

INT8U Can232::openCanBus(INT8U mode) {
    INT8U ret = LW232_OK;
    INT8U initStatus = CAN_OK;
    dbg2("CAN open request: mode=", mode);
    dbg2("CAN speed selection=", lw232CanSpeedSelection);
#ifndef _MCP_FAKE_MODE_
    initStatus = lw232CAN.begin(lw232CanSpeedSelection, lw232McpModuleClock);
    if (initStatus == CAN_OK) {
        markHardwareFiltersDirty(); // Controller reset during begin() clears filters
        if (!applyHardwareFilters(mode)) {
            initStatus = CAN_FAILINIT;
            dbg1("CAN filter init failed");
        } else {
            dbg1("CAN initialized successfully");
        }
    } else {
        dbg2("CAN init failed with status=", initStatus);
    }
#else
    lw232HwFilterDirty = false;
#endif
    if (initStatus != CAN_OK) {
        ret = LW232_ERR;
    } else {
        clearRxBuffer();
    }
    return ret;
}


INT8U Can232::sendMsgBuf(INT32U id, INT8U ext, INT8U rtr, INT8U len, INT8U *buf) {
#ifndef _MCP_FAKE_MODE_
    INT8U status = lw232CAN.sendMsgBuf(id, ext, rtr, len, buf);
    if (status == CAN_OK) {
        statsRecordTxFrame();
        if (lw232DebugMode) {
            dbg2("TX frame sent, ID:", id);
        }
    } else if (lw232DebugMode) {
        dbg2("TX failed, status:", status);
    }
    return status;
#else
    Serial.print("<sending:");
    Serial.print(id, HEX);
    Serial.print(',');
    if (ext) Serial.print('+');
    else Serial.print('-');
    if (rtr) Serial.print('+');
    else Serial.print('-');
    Serial.print(',');
    Serial.print(len, DEC);
    Serial.print(',');
    int i;
    for (i = 0; i < len; i++) printFullByte(buf[i]);
    statsRecordTxFrame();
    return CAN_OK;
#endif
}


void Can232::scheduleSerialBaudChange(INT8U idx) {
    lw232PendingSerialBaudIndex = idx;
}

void Can232::applyPendingSerialBaudChange() {
    if (lw232PendingSerialBaudIndex == 0xFF) {
        return;
    }
    Serial.flush();
    // Use a longer delay to ensure stability
    delay(50);
    // Don't call Serial.end() as it may not be reliable
    Serial.begin(lw232SerialBaudRates[lw232PendingSerialBaudIndex]);
    // Additional delay after begin
    delay(20);
    lw232SerialBaudIndex = lw232PendingSerialBaudIndex;
    lw232PendingSerialBaudIndex = 0xFF;
}

void Can232::initializeEepromIfNeeded() {
    // Check for new format first (magic marker present)
    const INT8U magic = EEPROM.read(LW232_EEPROM_ADDR_MAGIC);
    if (magic == LW232_EEPROM_MAGIC_VALUE) {
        // EEPROM is in new format - validate autostart block
        const INT8U version = EEPROM.read(LW232_EEPROM_ADDR_AUTOSTART);
        const INT8U storedMode = EEPROM.read(LW232_EEPROM_ADDR_AUTOSTART + 1);
        const INT8U storedIndex = EEPROM.read(LW232_EEPROM_ADDR_AUTOSTART + 2);
        const INT8U checksum = EEPROM.read(LW232_EEPROM_ADDR_AUTOSTART + 3);
        const INT8U expectedChecksum = computeAutoStartChecksum(version, storedMode, storedIndex);

        // Check if autostart block is corrupted
        if (version != LW232_AUTOSTART_BLOCK_VERSION ||
            checksum != expectedChecksum ||
            storedIndex >= LW232_CAN_BAUD_NUM ||
            storedMode > LW232_AUTOSTART_ON_LISTEN) {

            // Reinitialize autostart block
            EEPROM.write(LW232_EEPROM_ADDR_AUTOSTART, LW232_AUTOSTART_BLOCK_VERSION);
            EEPROM.write(LW232_EEPROM_ADDR_AUTOSTART + 1, LW232_AUTOSTART_OFF);
            EEPROM.write(LW232_EEPROM_ADDR_AUTOSTART + 2, findCanBaudIndex(LW232_DEFAULT_CAN_RATE));
            EEPROM.write(LW232_EEPROM_ADDR_AUTOSTART + 3, computeAutoStartChecksum(
                LW232_AUTOSTART_BLOCK_VERSION,
                LW232_AUTOSTART_OFF,
                findCanBaudIndex(LW232_DEFAULT_CAN_RATE)
            ));

            dbg1("EEPROM autostart block corrupted, reinitialized");
        }
    } else {
        // Check if this is old format (timestamp stored at address 0x00)
        const INT8U oldTimestamp = EEPROM.read(LW232_EEPROM_ADDR_MAGIC); // Was 0x00 in old format
        if (oldTimestamp == LW232_TIMESTAMP_OFF || oldTimestamp == LW232_TIMESTAMP_ON_NORMAL) {
            // Old format detected - migrate to new format
            dbg1("Migrating EEPROM from old format");

            // Move timestamp from 0x00 to 0x01
            EEPROM.write(LW232_EEPROM_ADDR_TIMESTAMP, oldTimestamp);

            // Initialize autostart block (wasn't implemented in old version)
            EEPROM.write(LW232_EEPROM_ADDR_AUTOSTART, LW232_AUTOSTART_BLOCK_VERSION);
            EEPROM.write(LW232_EEPROM_ADDR_AUTOSTART + 1, LW232_AUTOSTART_OFF);
            EEPROM.write(LW232_EEPROM_ADDR_AUTOSTART + 2, findCanBaudIndex(LW232_DEFAULT_CAN_RATE));
            EEPROM.write(LW232_EEPROM_ADDR_AUTOSTART + 3, computeAutoStartChecksum(
                LW232_AUTOSTART_BLOCK_VERSION,
                LW232_AUTOSTART_OFF,
                findCanBaudIndex(LW232_DEFAULT_CAN_RATE)
            ));

            // Write magic marker to indicate new format
            EEPROM.write(LW232_EEPROM_ADDR_MAGIC, LW232_EEPROM_MAGIC_VALUE);

            dbg1("EEPROM migration completed");
        } else {
            // Neither new format nor old format - initialize to defaults
            dbg1("EEPROM uninitialized - setting defaults");

            // Write magic marker
            EEPROM.write(LW232_EEPROM_ADDR_MAGIC, LW232_EEPROM_MAGIC_VALUE);

            // Initialize timestamp setting
            EEPROM.write(LW232_EEPROM_ADDR_TIMESTAMP, LW232_TIMESTAMP_OFF);

            // Initialize autostart settings with proper structure
            EEPROM.write(LW232_EEPROM_ADDR_AUTOSTART, LW232_AUTOSTART_BLOCK_VERSION);
            EEPROM.write(LW232_EEPROM_ADDR_AUTOSTART + 1, LW232_AUTOSTART_OFF);  // mode
            EEPROM.write(LW232_EEPROM_ADDR_AUTOSTART + 2, findCanBaudIndex(LW232_DEFAULT_CAN_RATE));  // speed index
            EEPROM.write(LW232_EEPROM_ADDR_AUTOSTART + 3, computeAutoStartChecksum(
                LW232_AUTOSTART_BLOCK_VERSION,
                LW232_AUTOSTART_OFF,
                findCanBaudIndex(LW232_DEFAULT_CAN_RATE)
            ));

            // Initialize current runtime values to match EEPROM
            lw232TimeStamp = LW232_TIMESTAMP_OFF;
            lw232AutoStart = LW232_AUTOSTART_OFF;
            lw232CanSpeedIndex = findCanBaudIndex(LW232_DEFAULT_CAN_RATE);
        }
    }
}

void Can232::loadTimestampPreference() {
    INT8U stored = EEPROM.read(LW232_EEPROM_ADDR_TIMESTAMP);
    if (stored == LW232_TIMESTAMP_ON_NORMAL || stored == LW232_TIMESTAMP_OFF) {
        lw232TimeStamp = stored;
    } else {
        lw232TimeStamp = LW232_TIMESTAMP_OFF;
        EEPROM.update(LW232_EEPROM_ADDR_TIMESTAMP, LW232_TIMESTAMP_OFF);
    }
}

void Can232::persistTimestampPreference() {
    EEPROM.update(LW232_EEPROM_ADDR_TIMESTAMP, lw232TimeStamp);
}

INT8U Can232::findCanBaudIndex(INT8U canSpeed) {
    for (INT8U idx = 0; idx < LW232_CAN_BAUD_NUM; idx++) {
        if (lw232CanBaudRates[idx] == canSpeed) {
            return idx;
        }
    }
    return 0;
}

INT8U Can232::findCanBaudIndexByBps(INT32U bps) {
    switch (bps) {
    case 10000: return 0;
    case 20000: return 1;
    case 50000: return 2;
    case 100000: return 3;
    case 125000: return 4;
    case 250000: return 5;
    case 500000: return 6;
    case 800000: return 6; // closest supported rate on this hardware
    case 1000000: return 8;
    case 83333: return 9;
    default:
        return 0xFF;
    }
}

static INT32U baudIndexToBps(INT8U idx) {
    switch (idx) {
    case 0: return 10000;
    case 1: return 20000;
    case 2: return 50000;
    case 3: return 100000;
    case 4: return 125000;
    case 5: return 250000;
    case 6: return 500000;
    case 7: return 500000; // hardware constraint
    case 8: return 1000000;
    case 9: return 83333;
    default:
        return 0;
    }
}

INT8U Can232::computeAutoStartChecksum(INT8U version, INT8U mode, INT8U idx) {
    return version ^ mode ^ idx ^ LW232_AUTOSTART_CHECKSUM_SEED;
}

void Can232::loadAutoStartPreference() {
    // Load autostart preferences from EEPROM with robust error handling
    const INT8U version = EEPROM.read(LW232_EEPROM_ADDR_AUTOSTART);
    const INT8U storedMode = EEPROM.read(LW232_EEPROM_ADDR_AUTOSTART + 1);
    const INT8U storedIndex = EEPROM.read(LW232_EEPROM_ADDR_AUTOSTART + 2);
    const INT8U checksum = EEPROM.read(LW232_EEPROM_ADDR_AUTOSTART + 3);

    // Compute expected checksum
    const INT8U expectedChecksum = computeAutoStartChecksum(version, storedMode, storedIndex);

    // Validate all parameters
    const bool versionValid = (version == LW232_AUTOSTART_BLOCK_VERSION);
    const bool checksumValid = (checksum == expectedChecksum);
    const bool modeValid = (storedMode <= LW232_AUTOSTART_ON_LISTEN);
    const bool indexValid = (storedIndex < LW232_CAN_BAUD_NUM);

    // Load settings if all validation passes
    if (versionValid && checksumValid && modeValid && indexValid) {
        lw232AutoStart = storedMode;
        lw232CanSpeedIndex = storedIndex;
        gvretCan0Baud = baudIndexToBps(lw232CanSpeedIndex);
        dbg2("Loaded autostart from EEPROM: mode=", storedMode);
    } else {
        // Validation failed - use defaults and log issue
        lw232AutoStart = LW232_AUTOSTART_OFF;
        lw232CanSpeedIndex = findCanBaudIndex(LW232_DEFAULT_CAN_RATE);
        gvretCan0Baud = baudIndexToBps(lw232CanSpeedIndex);

        // Log validation failures for debugging
        if (!versionValid) {
            dbg2("EEPROM autostart version invalid: ", version);
        } else if (!checksumValid) {
            dbg2("EEPROM autostart checksum invalid: ", checksum);
            dbg2("Expected: ", expectedChecksum);
        } else if (!modeValid) {
            dbg2("EEPROM autostart mode invalid: ", storedMode);
        } else if (!indexValid) {
            dbg2("EEPROM autostart index invalid: ", storedIndex);
        }
    }
}

void Can232::persistAutoStartPreference() {
    const INT8U version = LW232_AUTOSTART_BLOCK_VERSION;
    EEPROM.update(LW232_EEPROM_ADDR_AUTOSTART, version);
    EEPROM.update(LW232_EEPROM_ADDR_AUTOSTART + 1, lw232AutoStart);
    EEPROM.update(LW232_EEPROM_ADDR_AUTOSTART + 2, lw232CanSpeedIndex);
    EEPROM.update(LW232_EEPROM_ADDR_AUTOSTART + 3, computeAutoStartChecksum(version, lw232AutoStart, lw232CanSpeedIndex));
}

void Can232::persistCanSpeedSelection() {
    persistAutoStartPreference();
}

void Can232::maybeAutoStart() {
    if (lw232AutoStart == LW232_AUTOSTART_OFF) {
        return;
    }
    if (lw232CanSpeedIndex >= LW232_CAN_BAUD_NUM) {
        lw232AutoStart = LW232_AUTOSTART_OFF;
        persistAutoStartPreference();
        return;
    }
    lw232CanSpeedSelection = lw232CanBaudRates[lw232CanSpeedIndex];
    lw232BitrateConfigured = true;
    const INT8U requestedMode = (lw232AutoStart == LW232_AUTOSTART_ON_NORMAL) ? MODE_NORMAL : MODE_LISTENONLY;
    if (openCanBus(requestedMode) == LW232_OK) {
        lw232CanChannelMode = (requestedMode == MODE_NORMAL) ? LW232_STATUS_CAN_OPEN_NORMAL : LW232_STATUS_CAN_OPEN_LISTEN;
        gvretCan0Enabled = true;
        gvretCan0ListenOnly = (requestedMode != MODE_NORMAL);
    } else {
        lw232CanChannelMode = LW232_STATUS_CAN_CLOSED;
        clearRxBuffer();
        gvretCan0Enabled = false;
    }
}


INT8U Can232::readLawicelStatusFlags() {
    // Mirrors LAWICEL CANUSB manual bit order: RXQ full, TXQ full, EI, DOI, -, EPI, ALI, BEI.
    INT8U status = 0;
    const INT8U interruptFlags = lw232CAN.getInterruptFlags();
    const bool rx0Pending = (interruptFlags & MCP_RX0IF) != 0;
    const bool rx1Pending = (interruptFlags & MCP_RX1IF) != 0;
    if (rx0Pending && rx1Pending) {
        status |= 0x01;
    }

    INT8U txCtrl[3] = {0, 0, 0};
    lw232CAN.getTxCtrlRegisters(&txCtrl[0], &txCtrl[1], &txCtrl[2]);
    const bool tx0Busy = (txCtrl[0] & MCP_TXB_TXREQ_M) != 0;
    const bool tx1Busy = (txCtrl[1] & MCP_TXB_TXREQ_M) != 0;
    const bool tx2Busy = (txCtrl[2] & MCP_TXB_TXREQ_M) != 0;
    if (tx0Busy && tx1Busy && tx2Busy) {
        status |= 0x02;
    }

    INT8U eflg = 0;
    lw232CAN.checkError(&eflg);
    if (lw232DebugMode && eflg != 0) {
        dbg2("MCP2515 EFLG:", eflg);
    }
    if (eflg & MCP_EFLG_EWARN) {
        status |= 0x04;
    }
    if (eflg & (MCP_EFLG_RX0OVR | MCP_EFLG_RX1OVR)) {
        status |= 0x08;
    }
    if (eflg & (MCP_EFLG_TXEP | MCP_EFLG_RXEP)) {
        status |= 0x20;
    }
    if (((txCtrl[0] | txCtrl[1] | txCtrl[2]) & MCP_TXB_MLOA_M) != 0) {
        status |= 0x40;
    }
    if (eflg & MCP_EFLG_TXBO) {
        status |= 0x80;
    }
    return status;
}

void Can232::updateBusLoad() {
    unsigned long now = millis();
    if (now - lastBusLoadCalc < 500) {
        return;  // Update every 500ms
    }

    unsigned long framesDelta = g_canStats.framesRx - lastBusLoadFrameCount;

    // Calculate frames per second
    g_canStats.currentFramesPerSecond = (unsigned int)framesDelta;

    lastBusLoadFrameCount = g_canStats.framesRx;
    lastBusLoadCalc = now;
}

bool Can232::parseCanStdId() {
    INT32U id = 0;
    for (INT8U idx = 0; idx < 3; idx++) {
        INT8U nibble = HexHelper::parseNibble(lw232Message[1 + idx]);
        if (nibble == HEX_INVALID) {
            return false;
        }
        id = (id << 4) | nibble;
    }
    lw232CanId = id & 0x7FF;
    return true;
}

bool Can232::parseCanExtId() {
    INT32U id = 0;
    for (INT8U idx = 0; idx < 8; idx++) {
        INT8U nibble = HexHelper::parseNibble(lw232Message[1 + idx]);
        if (nibble == HEX_INVALID) {
            return false;
        }
        id = (id << 4) | nibble;
    }
    lw232CanId = id & 0x1FFFFFFF;
    return true;
}

char HexHelper::toHexChar(INT8U nibble) {
    nibble &= 0x0F;
    return (nibble < 10) ? static_cast<char>('0' + nibble)
                         : static_cast<char>('A' + (nibble - 10));
}

void HexHelper::byteToHex(INT8U value, char *out) {
    out[0] = toHexChar(value >> 4);
    out[1] = toHexChar(value & 0x0F);
}

void HexHelper::printFullByte(INT8U b) {
    if (b < 0x10) {
        Serial.print('0');
       // dbg0('0');
    }
    Serial.print(b, HEX);
    //dbgH(b);
}

void HexHelper::printNibble(INT8U b) {
    Serial.print(b & 0x0F, HEX);
    //dbgH(b & 0x0F);
}


INT8U HexHelper::parseNibble(INT8U hex) {
    INT8U value = readHexLookup(hex);
    return (value <= 0x0F) ? value : HEX_INVALID;
}

INT8U HexHelper::parseFullByte(INT8U H, INT8U L, bool *ok) {
    INT8U hi = parseNibble(H);
    INT8U lo = parseNibble(L);
    const bool success = (hi != HEX_INVALID && lo != HEX_INVALID);
    if (ok) {
        *ok = success;
    }
    return success ? (INT8U)((hi << 4) + lo) : 0;
}

INT8U HexHelper::parseNibbleWithLimit(INT8U hex, INT8U limit) {
    INT8U ret = parseNibble(hex);
    if (ret == HEX_INVALID || ret >= limit) {
        return HEX_INVALID;
    }
    return ret;
}
