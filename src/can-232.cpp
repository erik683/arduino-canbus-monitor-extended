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

#include <SPI.h>
#include <EEPROM.h>
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
#define dbg0(x)   debug.print(x)
#define dbg1(x)   debug.println(x)
#define dbg2(x,y) debug.print(x); debug.println(y)
#define dbgH(x)   debug.print(x,HEX)
#define DEBUG_RX_PIN 8
#define DEBUG_TX_PIN 9
#else
#define dbg_begin(x)
#define dbg0(x) 
#define dbg1(x) 
#define dbg2(x,y)
#define dbgH(x)
#endif

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
    // Default to autopoll ON for compatibility with most CAN monitoring applications
    lw232AutoPoll = LW232_AUTOPOLL_ON;

    maybeAutoStart();
}

void Can232::setFilterFunc(INT8U (*userFunc)(INT32U)) {
    instance()->userAddressFilterFunc = userFunc;
}

void Can232::loopFunc() {
    // Service CAN RX interrupts first
    if (mcpInterruptPending) {
        mcpInterruptPending = false;
        serviceCanRx();
    }

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
    if (lw232CanChannelMode != LW232_STATUS_CAN_CLOSED && lw232AutoPoll == LW232_AUTOPOLL_ON) {
        // Initialize batch tracking if not already started
        if (autopollBatchStartTime == 0) {
            autopollBatchStartTime = millis();
            autopollBatchBytes = 0;
        }

        bool batchLimitReached = false;

        // Drain frames from hardware into buffer and send in bounded batches
        int totalProcessed = 0;
        int processed = 0;
        while (processed < 10 && CAN_MSGAVAIL == checkReceive() && !batchLimitReached) {
            BufferedFrame frame;
            RxReadStatus status = readCanFrame(frame);
            if (status == RX_READ_READY) {
                // Check if adding this frame would exceed batch limits
                unsigned int frameSize = estimateFrameSize(frame);
                unsigned long elapsedMs = millis() - autopollBatchStartTime;

                if (autopollBatchBytes + frameSize > AUTOPOLL_MAX_BATCH_BYTES ||
                    elapsedMs >= AUTOPOLL_MAX_BATCH_TIME_MS) {
                    // Put frame back in buffer for next batch (don't lose it)
                    pushRxFrame(frame);
                    batchLimitReached = true;
                    break;
                }

                emitFrameToSerial(frame);
                Serial.write(LW232_CR);
                autopollBatchBytes += frameSize;
                processed++;
                totalProcessed++;
            }
        }

        // Send buffered frames in same batch
        BufferedFrame frame;
        while (popRxFrame(frame) && !batchLimitReached) {
            // Check if adding this frame would exceed batch limits
            unsigned int frameSize = estimateFrameSize(frame);
            unsigned long elapsedMs = millis() - autopollBatchStartTime;

            if (autopollBatchBytes + frameSize > AUTOPOLL_MAX_BATCH_BYTES ||
                elapsedMs >= AUTOPOLL_MAX_BATCH_TIME_MS) {
                // Put frame back in buffer for next batch
                pushRxFrame(frame);
                batchLimitReached = true;
                break;
            }

            emitFrameToSerial(frame);
            Serial.write(LW232_CR);
            autopollBatchBytes += frameSize;
            totalProcessed++;
        }

        // Always flush after processing batch, even if empty
        Serial.flush();

        // Reset batch tracking if batch is complete (limit reached or no work done)
        if (batchLimitReached || totalProcessed == 0) {
            autopollBatchStartTime = 0;
            autopollBatchBytes = 0;
        }
    }
}
void Can232::serialEventFunc() {
    while (Serial.available()) {
        char inChar = (char)Serial.read();
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
#if LW232_ENABLE_STATS
    statsRecordCommand();
#endif
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
        // Choose behavior: will it fail or not when not implemented command comes in. Some can monitors might be affected by this selection.
        Serial.write(LW232_RET_ASCII_ERROR);
        //Serial.write(LW232_RET_ASCII_OK);
        break;
    default:
        Serial.write(LW232_RET_ASCII_ERROR);
    }
    Serial.flush();  // Ensure command response is sent immediately
    applyPendingSerialBaudChange();
    return 0;
}

INT8U Can232::parseAndRunCommand() {
    INT8U ret = LW232_OK;
    INT8U idx = 0;
    lw232LastErr = LW232_OK;

    // Check minimum message length
    if (strlen((char*)lw232Message) < 1) {
        return LW232_ERR;
    }

    switch (lw232Message[0]) {
        case LW232_CMD_SETUP:
        // Sn[CR] Setup with standard CAN bit-rates where n is 0-9. Bare S[CR] reports the current selection.
        {
            const INT8U modeChar = lw232Message[1];
            const bool hasArgument = (modeChar != LW232_CR && modeChar != 0);
            if (!hasArgument) {
                // Bare S[CR] - report current CAN speed index
                Serial.print(LW232_CMD_SETUP);
                HexHelper::printNibble(lw232CanSpeedIndex);
                break;
            }
            // Sn[CR] - set CAN speed
            if (strlen((char*)lw232Message) != 3 || lw232Message[2] != LW232_CR) {
                ret = LW232_ERR;
            }
            else if (lw232CanChannelMode == LW232_STATUS_CAN_CLOSED) {
                // Validate that the bitrate index is a digit '0'-'9'
                if (lw232Message[1] < '0' || lw232Message[1] > '9') {
                    ret = LW232_ERR;
                    break;
                }
                idx = HexHelper::parseNibbleWithLimit(lw232Message[1], LW232_CAN_BAUD_NUM);
                if (idx == 0xFF || idx >= LW232_CAN_BAUD_NUM) {
                    ret = LW232_ERR;
                } else {
                    lw232CanSpeedIndex = idx;
                    lw232CanSpeedSelection = lw232CanBaudRates[idx];
                    lw232BitrateConfigured = true;
                    persistCanSpeedSelection();
                }
            }
            else {
                ret = LW232_ERR;
            }
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
#ifndef _MCP_FAKE_MODE_
            // Put MCP2515 into CONFIG mode to stop receiving frames and disable interrupts
            lw232CAN.setMode(MODE_CONFIG);
#endif
            // Clear any buffered frames to prevent stale data issues
            clearRxBuffer();
            // Reset interrupt flag to prevent any pending interrupt handling
            mcpInterruptPending = false;
            // Reset batch tracking state to prevent stale timing issues on reopen
            autopollBatchStartTime = 0;
            autopollBatchBytes = 0;
        }
        // Close command should always succeed (be idempotent)
        // else {
        //     ret = LW232_ERR;
        // }
        break;
    case LW232_CMD_TX11:
        // tiiildd...[CR] Transmit a standard (11bit) CAN frame.
        if (lw232CanChannelMode == LW232_STATUS_CAN_OPEN_NORMAL) {
            if (!parseCanStdId()) {
                ret = LW232_ERR;
                break;
            }
            lw232PacketLen = HexHelper::parseNibbleWithLimit(lw232Message[LW232_OFFSET_STD_PKT_LEN], LW232_FRAME_MAX_LENGTH + 1);
            if (lw232PacketLen == 0xFF) {
                ret = LW232_ERR;
                break;
            }
            for (; idx < lw232PacketLen; idx++) {
                lw232Buffer[idx] = HexHelper::parseFullByte(lw232Message[LW232_OFFSET_STD_PKT_DATA + idx * 2], lw232Message[LW232_OFFSET_STD_PKT_DATA + idx * 2 + 1]);
            }
            INT8U mcpErr = sendMsgBuf(lw232CanId, 0, 0, lw232PacketLen, lw232Buffer);
            if (lw232AutoPoll) {
                ret = (mcpErr == CAN_OK) ? LW232_OK_SMALL : LW232_ERR;
            } else {
                ret = LW232_OK_SMALL;  // Always return OK for valid TX commands
            } 
        }
        else {
            ret = LW232_ERR;
        }
        break;
    case LW232_CMD_TX29:
        // Tiiiiiiiildd...[CR] Transmit an extended (29bit) CAN frame
        if (lw232CanChannelMode == LW232_STATUS_CAN_OPEN_NORMAL) {
            if (!parseCanExtId()) {
                ret = LW232_ERR;
                break;
            }
            lw232PacketLen = HexHelper::parseNibbleWithLimit(lw232Message[LW232_OFFSET_EXT_PKT_LEN], LW232_FRAME_MAX_LENGTH + 1);
            if (lw232PacketLen == 0xFF) {
                ret = LW232_ERR;
                break;
            }
            for (; idx < lw232PacketLen; idx++) {
                lw232Buffer[idx] = HexHelper::parseFullByte(lw232Message[LW232_OFFSET_EXT_PKT_DATA + idx * 2], lw232Message[LW232_OFFSET_EXT_PKT_DATA + idx * 2 + 1]);
            }
            INT8U mcpErr = sendMsgBuf(lw232CanId, 1, 0, lw232PacketLen, lw232Buffer);
            if (lw232AutoPoll) {
                ret = (mcpErr == CAN_OK) ? LW232_OK_BIG : LW232_ERR;
            } else {
                ret = LW232_OK_BIG;  // Always return OK for valid TX commands
            }
        }
        else {
            ret = LW232_ERR;
        }
        break;
    case LW232_CMD_RTR11:
        // riiil[CR] Transmit an standard RTR (11bit) CAN frame.
        if (lw232CanChannelMode == LW232_STATUS_CAN_OPEN_NORMAL) {
            if (!parseCanStdId()) {
                ret = LW232_ERR;
                break;
            }
            lw232PacketLen = HexHelper::parseNibbleWithLimit(lw232Message[LW232_OFFSET_STD_PKT_LEN], LW232_FRAME_MAX_LENGTH + 1);
            if (lw232PacketLen == 0xFF) {
                ret = LW232_ERR;
                break;
            }
            INT8U mcpErr = sendMsgBuf(lw232CanId, 0, 1, lw232PacketLen, lw232Buffer);
            if (lw232AutoPoll) {
                ret = (mcpErr == CAN_OK) ? LW232_OK_SMALL : LW232_ERR;
            } else {
                ret = LW232_OK_SMALL;  // Always return OK for valid RTR commands
            }
        }
        else {
            ret = LW232_ERR;
        }
        break;
    case LW232_CMD_RTR29:
        // Riiiiiiiil[CR] Transmit an extended RTR (29bit) CAN frame.
        if (lw232CanChannelMode == LW232_STATUS_CAN_OPEN_NORMAL) {
            if (!parseCanExtId()) {
                ret = LW232_ERR;
                break;
            }
            lw232PacketLen = HexHelper::parseNibbleWithLimit(lw232Message[LW232_OFFSET_EXT_PKT_LEN], LW232_FRAME_MAX_LENGTH + 1);
            if (lw232PacketLen == 0xFF) {
                ret = LW232_ERR;
                break;
            }
            INT8U mcpErr = sendMsgBuf(lw232CanId, 1, 1, lw232PacketLen, lw232Buffer);
            if (lw232AutoPoll) {
                ret = (mcpErr == CAN_OK) ? LW232_OK_BIG : LW232_ERR;
            } else {
                ret = LW232_OK_BIG;  // Always return OK for valid RTR commands
            }
        } else {
            ret = LW232_ERR;
        }
        break;
    case LW232_CMD_POLL_ONE:
        // P[CR] Poll incomming FIFO for CAN frames (single poll)
        if (lw232CanChannelMode != LW232_STATUS_CAN_CLOSED && lw232AutoPoll == LW232_AUTOPOLL_OFF) {
            // First drain any pending frames from hardware into buffer
            serviceCanRx();

            // Then check if we have buffered frames
            BufferedFrame frame;
            if (popRxFrame(frame)) {
                emitFrameToSerial(frame);
                ret = LW232_OK;
            } else {
                ret = LW232_ERR;  // No frames available
            }
        } else {
            ret = LW232_ERR;
        }
        break;
    case LW232_CMD_POLL_MANY:
        // A[CR] Polls incomming FIFO for CAN frames (all pending frames)
        if (lw232CanChannelMode != LW232_STATUS_CAN_CLOSED && lw232AutoPoll == LW232_AUTOPOLL_OFF) {
            // First drain any pending frames from hardware into buffer
            serviceCanRx();

            // Then consume all buffered frames
            BufferedFrame frame;
            bool foundFrames = false;
            while (popRxFrame(frame)) {
                emitFrameToSerial(frame);
                Serial.write(LW232_CR);
                foundFrames = true;
            }
            if (foundFrames) {
                Serial.print(LW232_ALL);
            } else {
                ret = LW232_ERR;  // No frames available
            }
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
        const INT8U modeChar = lw232Message[1];
        const bool hasArgument = (modeChar != LW232_CR && modeChar != 0);
        if (!hasArgument) {
            Serial.print(LW232_CMD_AUTOPOLL);
            Serial.print(lw232AutoPoll == LW232_AUTOPOLL_ON ? LW232_ON_ONE : LW232_OFF);
            break;
        }
        if (lw232CanChannelMode != LW232_STATUS_CAN_CLOSED) {
            ret = LW232_ERR;
            break;
        }
        lw232AutoPoll = (lw232Message[1] == LW232_ON_ONE) ? LW232_AUTOPOLL_ON : LW232_AUTOPOLL_OFF;
        //todo: save to eeprom
        break;
    }
    case LW232_CMD_FILTER: {
        // Wn[CR] Filter mode setting. By default CAN232 works in dual filter mode (0) and is backwards compatible with previous CAN232 versions. Bare W[CR] reports the current mode.
        const INT8U modeChar = lw232Message[1];
        const bool hasArgument = (modeChar != LW232_CR && modeChar != 0);
        if (!hasArgument) {
            Serial.print(LW232_CMD_FILTER);
            Serial.print((char)('0' + lw232FilterMode));  // Report current filter mode
            break;
        }
        // Set filter mode
        if (modeChar == '0' || modeChar == '1') {
            lw232FilterMode = modeChar - '0';
            // TODO: Apply hardware filter changes to MCP2515 if needed
        } else {
            ret = LW232_ERR;
        }
        break;
    }
    case LW232_CMD_ACC_CODE: {
        // Mxxxxxxxx[CR] Sets Acceptance Code Register (ACn Register of SJA1000). // we use MCP2515, not supported. Bare M[CR] reports the current value.
        const INT8U modeChar = lw232Message[1];
        const bool hasArgument = (modeChar != LW232_CR && modeChar != 0);
        if (!hasArgument) {
            Serial.print(LW232_CMD_ACC_CODE);
            // Print current acceptance code as 8 hex digits
            for (int i = 0; i < 4; i++) {
                HexHelper::printFullByte(lw232AcceptanceCode[i]);
            }
            break;
        }
        // Set acceptance code: parse 8 hex digits (4 bytes)
        if (strlen((char*)lw232Message) != 10 || lw232Message[9] != LW232_CR) {  // M + 8 digits + CR
            ret = LW232_ERR;
            break;
        }
        // Parse into temporary array first to avoid partial updates
        INT8U tempCode[4];
        bool parseOk = true;
        for (int i = 0; i < 4 && parseOk; i++) {
            tempCode[i] = HexHelper::parseFullByte(lw232Message[1 + i*2], lw232Message[2 + i*2], &parseOk);
        }
        if (!parseOk) {
            ret = LW232_ERR;
        } else {
            // Only update global array if all parsing succeeded
            for (int i = 0; i < 4; i++) {
                lw232AcceptanceCode[i] = tempCode[i];
            }
        }
        // TODO: Apply to MCP2515 hardware filters if supported
        break;
    }
    case LW232_CMD_ACC_MASK: {
        // mxxxxxxxx[CR] Sets Acceptance Mask Register (AMn Register of SJA1000). Bare m[CR] reports the current value.
        const INT8U modeChar = lw232Message[1];
        const bool hasArgument = (modeChar != LW232_CR && modeChar != 0);
        if (!hasArgument) {
            Serial.print(LW232_CMD_ACC_MASK);
            // Print current acceptance mask as 8 hex digits
            for (int i = 0; i < 4; i++) {
                HexHelper::printFullByte(lw232AcceptanceMask[i]);
            }
            break;
        }
        // Set acceptance mask: parse 8 hex digits (4 bytes)
        if (strlen((char*)lw232Message) != 10 || lw232Message[9] != LW232_CR) {  // m + 8 digits + CR
            ret = LW232_ERR;
            break;
        }
        // Parse into temporary array first to avoid partial updates
        INT8U tempMask[4];
        bool parseOk = true;
        for (int i = 0; i < 4 && parseOk; i++) {
            tempMask[i] = HexHelper::parseFullByte(lw232Message[1 + i*2], lw232Message[2 + i*2], &parseOk);
        }
        if (!parseOk) {
            ret = LW232_ERR;
        } else {
            // Only update global array if all parsing succeeded
            for (int i = 0; i < 4; i++) {
                lw232AcceptanceMask[i] = tempMask[i];
            }
        }
        // TODO: Apply to MCP2515 hardware filters if supported
        break;
    }
    case LW232_CMD_UART: {
        // Un[CR] Setup UART with a new baud rate where n is 0-6. Bare U[CR] reports the current selection.
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
    case LW232_CMD_DEBUG: {
        // @DBGn[CR] Runtime debug toggle (0=off, 1=on)
        if (strlen((char*)lw232Message) >= 5 && lw232Message[1] == 'D' && lw232Message[2] == 'B' && lw232Message[3] == 'G') {
            if (lw232Message[4] == '1') {
                lw232DebugMode = true;
                Serial.print("DEBUG ON\r");
            } else if (lw232Message[4] == '0') {
                lw232DebugMode = false;
                Serial.print("DEBUG OFF\r");
            } else {
                ret = LW232_ERR;
            }
        } else {
            ret = LW232_ERR;
        }
        break;
    }
    case LW232_CMD_DEBUG_EXT: {
        // #EXT[CR] Show raw registers for next extended frame
        if (strlen((char*)lw232Message) >= 4 && lw232Message[1] == 'E' && lw232Message[2] == 'X' && lw232Message[3] == 'T') {
            lw232DebugExtFrames = true;
            Serial.print("EXTENDED FRAME DEBUG ON\r");
        } else {
            ret = LW232_ERR;
        }
        break;
    }
    case LW232_CMD_REJECT_EXT: {
        // %EXTn[CR] Reject extended frames (0=accept, 1=reject)
        if (strlen((char*)lw232Message) >= 5 && lw232Message[1] == 'E' && lw232Message[2] == 'X' && lw232Message[3] == 'T') {
            if (lw232Message[4] == '1') {
                lw232RejectExtendedFrames = true;
                Serial.print("REJECT EXTENDED FRAMES ON\r");
            } else if (lw232Message[4] == '0') {
                lw232RejectExtendedFrames = false;
                Serial.print("REJECT EXTENDED FRAMES OFF\r");
            } else {
                ret = LW232_ERR;
            }
        } else {
            ret = LW232_ERR;
        }
        break;
    }
#if LW232_ENABLE_STATS
    case LW232_CMD_INFO: {
        // i[CR] Diagnostic snapshot: iCCCCRRRRTTTTUUUUddddooooFFD
        if (lw232CanChannelMode == LW232_STATUS_CAN_CLOSED) {
            ret = LW232_ERR;
            break;
        }
        Serial.print(LW232_CMD_INFO);
        // Print command count (4 hex digits, lower 16 bits)
        HexHelper::printFullByte(LOW_BYTE(LOW_WORD(g_canStats.commandCount)));
        HexHelper::printFullByte(HIGH_BYTE(LOW_WORD(g_canStats.commandCount)));
        // Print RX count (4 hex digits, lower 16 bits)
        HexHelper::printFullByte(LOW_BYTE(LOW_WORD(g_canStats.framesRx)));
        HexHelper::printFullByte(HIGH_BYTE(LOW_WORD(g_canStats.framesRx)));
        // Print TX count (4 hex digits, lower 16 bits)
        HexHelper::printFullByte(LOW_BYTE(LOW_WORD(g_canStats.framesTx)));
        HexHelper::printFullByte(HIGH_BYTE(LOW_WORD(g_canStats.framesTx)));
        // Print uptime in seconds (4 hex digits, lower 16 bits)
        unsigned long uptime_seconds = millis() / 1000UL;
        HexHelper::printFullByte(LOW_BYTE(LOW_WORD(uptime_seconds)));
        HexHelper::printFullByte(HIGH_BYTE(LOW_WORD(uptime_seconds)));
        // Print drops (4 hex digits, lower 16 bits)
        HexHelper::printFullByte(LOW_BYTE(LOW_WORD(g_canStats.rxBufferDrops)));
        HexHelper::printFullByte(HIGH_BYTE(LOW_WORD(g_canStats.rxBufferDrops)));
        // Print overflows (4 hex digits, lower 16 bits)
        HexHelper::printFullByte(LOW_BYTE(LOW_WORD(g_canStats.rxBufferOverflows)));
        HexHelper::printFullByte(HIGH_BYTE(LOW_WORD(g_canStats.rxBufferOverflows)));
        // Print FPS (2 hex digits)
        HexHelper::printFullByte(LOW_BYTE(g_canStats.currentFramesPerSecond));
        // Print debug flag
        Serial.print(lw232DebugMode ? 'D' : '0');
        break;
    }
#endif  // LW232_ENABLE_STATS
    case LW232_CMD_YANK: {
        // Yn[CR] Yank/reset command (SavvyCAN compatibility)
        // For now, just accept any parameter and return OK
        // This appears to be a SavvyCAN-specific command
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



unsigned int Can232::estimateFrameSize(const BufferedFrame& frame) {
    const bool extendedFrame = (frame.flags & LW232_FRAME_FLAG_EXTENDED) != 0;
    unsigned int size = 0;

    if (extendedFrame) {
        size += 1; // 'T'
        size += 8; // 8 hex digits for extended ID
    } else {
        size += 1; // 't'
        size += 3; // 3 hex digits for standard ID
    }

    size += 1; // length nibble
    size += frame.len * 2; // 2 hex digits per data byte

    if (lw232TimeStamp == LW232_TIMESTAMP_ON_NORMAL) {
        size += 4; // 4 hex digits for timestamp
    }

    size += 1; // CR character

    return size;
}

void Can232::emitFrameToSerial(const BufferedFrame& frame) {
    const bool extendedFrame = (frame.flags & LW232_FRAME_FLAG_EXTENDED) != 0;
    if (extendedFrame) {
        Serial.print(LW232_TR29);
        HexHelper::printFullByte(HIGH_BYTE(HIGH_WORD(frame.id)));
        HexHelper::printFullByte(LOW_BYTE(HIGH_WORD(frame.id)));
        HexHelper::printFullByte(HIGH_BYTE(LOW_WORD(frame.id)));
        HexHelper::printFullByte(LOW_BYTE(LOW_WORD(frame.id)));
    }
    else {
        Serial.print(LW232_TR11);
        HexHelper::printNibble(HIGH_BYTE(LOW_WORD(frame.id)));
        HexHelper::printFullByte(LOW_BYTE(LOW_WORD(frame.id)));
    }
    //write data len
    HexHelper::printNibble(frame.len);
    //write data
    for (INT8U idx = 0; idx < frame.len; idx++) {
        HexHelper::printFullByte(frame.data[idx]);
    }
    //write timestamp if needed
    if (lw232TimeStamp == LW232_TIMESTAMP_ON_NORMAL) {
        // Convert microseconds to milliseconds and apply LAWICEL 60-second rollover
        INT16U timestampMs = (frame.timestamp / 1000UL) % 60000;
        HexHelper::printFullByte(HIGH_BYTE(timestampMs));
        HexHelper::printFullByte(LOW_BYTE(timestampMs));
    }
}

INT8U Can232::receiveSingleFrame() {
    INT8U ret = LW232_OK;
    INT8U idx = 0;
    if (CAN_OK == readMsgBufID(&lw232CanId, &lw232PacketLen, lw232Buffer)) {
        // Capture timestamp immediately after reading frame from MCP2515
        INT32U capturedMicros = micros();

        if (lw232CanId > 0x1FFFFFFF) {
            ret = LW232_ERR; // address if totally wrong
        }
        else if (checkPassFilter(lw232CanId)) {// do we want to skip some addresses?
            const INT8U extendedFrame = isExtendedFrame();
            if (extendedFrame) {
                Serial.print(LW232_TR29);
                HexHelper::printFullByte(HIGH_BYTE(HIGH_WORD(lw232CanId)));
                HexHelper::printFullByte(LOW_BYTE(HIGH_WORD(lw232CanId)));
                HexHelper::printFullByte(HIGH_BYTE(LOW_WORD(lw232CanId)));
                HexHelper::printFullByte(LOW_BYTE(LOW_WORD(lw232CanId)));
            }
            else {
                Serial.print(LW232_TR11);
                HexHelper::printNibble(HIGH_BYTE(LOW_WORD(lw232CanId)));
                HexHelper::printFullByte(LOW_BYTE(LOW_WORD(lw232CanId)));
            }
            //write data len
            HexHelper::printNibble(lw232PacketLen);
            //write data
            for (idx = 0; idx < lw232PacketLen; idx++) {
                HexHelper::printFullByte(lw232Buffer[idx]);
            }
            //write timestamp if needed
            if (lw232TimeStamp == LW232_TIMESTAMP_ON_NORMAL) {
                // Convert microseconds to milliseconds and apply LAWICEL 60-second rollover
                INT16U timestampMs = (capturedMicros / 1000UL) % 60000;
                HexHelper::printFullByte(HIGH_BYTE(timestampMs));
                HexHelper::printFullByte(LOW_BYTE(timestampMs));
            }
#if LW232_ENABLE_STATS
            statsRecordRxFrame();
#endif
        }
    }
    else {
        ret = LW232_ERR;
    }
    return ret;
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

INT8U Can232::openCanBus(INT8U mode) {
    INT8U ret = LW232_OK;
    INT8U initStatus = CAN_OK;

#ifndef _MCP_FAKE_MODE_
    // Add timeout protection for CAN initialization (5 second timeout)
    unsigned long startTime = millis();
    const unsigned long timeoutMs = 5000;

    // Try CAN initialization with timeout
    while (millis() - startTime < timeoutMs) {
        initStatus = lw232CAN.begin(lw232CanSpeedSelection, lw232McpModuleClock);
        if (initStatus == CAN_OK) {
            break; // Success, exit the timeout loop
        }
        delay(100); // Brief delay before retry
    }

    if (initStatus == CAN_OK) {
        // Set the requested mode after successful initialization
        lw232CAN.setMode(mode);
    } else {
        Serial.println("CAN initialization timed out");
    }
#endif

    if (initStatus != CAN_OK) {
        ret = LW232_ERR;
    }
    return ret;
}


INT8U Can232::sendMsgBuf(INT32U id, INT8U ext, INT8U rtr, INT8U len, INT8U *buf) {
#if LW232_ENABLE_STATS
    statsRecordTxFrame();  // Count transmission attempts, not just successes
#endif
#ifndef _MCP_FAKE_MODE_
    return lw232CAN.sendMsgBuf(id, ext, rtr, len, buf);
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
        dbg2("Loaded autostart from EEPROM: mode=", storedMode);
    } else {
        // Validation failed - use defaults and log issue
        lw232AutoStart = LW232_AUTOSTART_OFF;
        lw232CanSpeedIndex = findCanBaudIndex(LW232_DEFAULT_CAN_RATE);

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
        // Don't print unsolicited messages - violates LAWICEL spec
        // Serial.println("Auto-start disabled");
        return;
    }
    if (lw232CanSpeedIndex >= LW232_CAN_BAUD_NUM) {
        lw232AutoStart = LW232_AUTOSTART_OFF;
        persistAutoStartPreference();
        // Don't print unsolicited messages - violates LAWICEL spec
        // Serial.println("Auto-start disabled due to invalid speed index");
        return;
    }
    lw232CanSpeedSelection = lw232CanBaudRates[lw232CanSpeedIndex];
    lw232BitrateConfigured = true;
    const INT8U requestedMode = (lw232AutoStart == LW232_AUTOSTART_ON_NORMAL) ? MODE_NORMAL : MODE_LISTENONLY;
    // Auto-start should be silent per LAWICEL spec - no unsolicited messages
    // Serial.print("Attempting CAN auto-start (");
    // Serial.print(requestedMode == MODE_NORMAL ? "normal" : "listen");
    // Serial.print(" mode)... ");
    if (openCanBus(requestedMode) == LW232_OK) {
        lw232CanChannelMode = (requestedMode == MODE_NORMAL) ? LW232_STATUS_CAN_OPEN_NORMAL : LW232_STATUS_CAN_OPEN_LISTEN;
        // Serial.println("SUCCESS");
    } else {
        lw232CanChannelMode = LW232_STATUS_CAN_CLOSED;
        // Serial.println("FAILED - CAN hardware not responding");
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

bool Can232::parseCanStdId() {
    INT8U nibble1 = HexHelper::parseNibble(lw232Message[1]);
    INT8U nibble2 = HexHelper::parseNibble(lw232Message[2]);
    INT8U nibble3 = HexHelper::parseNibble(lw232Message[3]);

    if (nibble1 == 0xFF || nibble2 == 0xFF || nibble3 == 0xFF) {
        return false; // Invalid hex character in ID
    }

    lw232CanId = (((INT32U)nibble1) << 8)
        + (((INT32U)nibble2) << 4)
        + ((INT32U)nibble3);
    lw232CanId &= 0x7FF;
    return true;
}

bool Can232::parseCanExtId() {
    INT8U nibble1 = HexHelper::parseNibble(lw232Message[1]);
    INT8U nibble2 = HexHelper::parseNibble(lw232Message[2]);
    INT8U nibble3 = HexHelper::parseNibble(lw232Message[3]);
    INT8U nibble4 = HexHelper::parseNibble(lw232Message[4]);
    INT8U nibble5 = HexHelper::parseNibble(lw232Message[5]);
    INT8U nibble6 = HexHelper::parseNibble(lw232Message[6]);
    INT8U nibble7 = HexHelper::parseNibble(lw232Message[7]);
    INT8U nibble8 = HexHelper::parseNibble(lw232Message[8]);

    if (nibble1 == 0xFF || nibble2 == 0xFF || nibble3 == 0xFF || nibble4 == 0xFF ||
        nibble5 == 0xFF || nibble6 == 0xFF || nibble7 == 0xFF || nibble8 == 0xFF) {
        return false; // Invalid hex character in ID
    }

    lw232CanId = (((INT32U)nibble1) << 28)
        + (((INT32U)nibble2) << 24)
        + (((INT32U)nibble3) << 20)
        + (((INT32U)nibble4) << 16)
        + (((INT32U)nibble5) << 12)
        + (((INT32U)nibble6) << 8)
        + (((INT32U)nibble7) << 4)
        + ((INT32U)nibble8);
    lw232CanId &= 0x1FFFFFFF;
    return true;
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
    if (hex >= '0' && hex <= '9') {
        return hex - '0';
    } else if (hex >= 'a' && hex <= 'f') {
        return hex - 'a' + 10;
    } else if (hex >= 'A' && hex <= 'F') {
        return hex - 'A' + 10;
    } else {
        return 0xFF; // error indicator
    }
}

INT8U HexHelper::parseFullByte(INT8U H, INT8U L, bool *ok) {
    bool hValid = (H >= '0' && H <= '9') || (H >= 'a' && H <= 'f') || (H >= 'A' && H <= 'F');
    bool lValid = (L >= '0' && L <= '9') || (L >= 'a' && L <= 'f') || (L >= 'A' && L <= 'F');
    if (ok) *ok = hValid && lValid;
    return (parseNibble(H) << 4) + parseNibble(L);
}

bool Can232::rxBufferEmpty() const {
    noInterrupts();  // Disable interrupts for atomic read
    bool empty = (rxCount == 0);
    interrupts();  // Re-enable interrupts
    return empty;
}

bool Can232::pushRxFrame(const BufferedFrame& frame) {
    noInterrupts();  // Disable interrupts for atomic operation
    if (rxCount >= LW232_RX_BUFFER_SIZE) {
        interrupts();  // Re-enable interrupts before returning
#if LW232_ENABLE_STATS
        statsRecordRxOverflow();
#endif
        return false; // Buffer full
    }

    rxBuffer[rxHead] = frame;
    rxHead = (rxHead + 1) % LW232_RX_BUFFER_SIZE;
    rxCount++;
    interrupts();  // Re-enable interrupts
    return true;
}

bool Can232::popRxFrame(BufferedFrame& frame) {
    noInterrupts();  // Disable interrupts for atomic operation
    if (rxBufferEmpty()) {
        interrupts();  // Re-enable interrupts before returning
        return false; // Buffer empty
    }

    frame = rxBuffer[rxTail];
    rxTail = (rxTail + 1) % LW232_RX_BUFFER_SIZE;
    rxCount--;
    interrupts();  // Re-enable interrupts
    return true;
}

void Can232::clearRxBuffer() {
    noInterrupts();  // Disable interrupts for atomic operation
    rxHead = 0;
    rxTail = 0;
    rxCount = 0;
    interrupts();  // Re-enable interrupts
}

Can232::RxReadStatus Can232::readCanFrame(BufferedFrame& frame) {
    INT32U id;
    INT8U len;
    INT8U buf[8];

    if (readMsgBufID(&id, &len, buf) != CAN_OK) {
        return RX_READ_NONE;
    }

    // Capture timestamp immediately after reading frame from MCP2515
    INT32U capturedMicros = micros();

    if (checkPassFilter(id)) {
        // Frame passes filter, buffer it
        frame.id = id;
        frame.timestamp = capturedMicros;
        frame.len = len;
        frame.flags = isExtendedFrame() ? LW232_FRAME_FLAG_EXTENDED : 0;

        // Debug extended frames if requested (before rejection check)
        if ((frame.flags & LW232_FRAME_FLAG_EXTENDED) && lw232DebugExtFrames) {
            Serial.print("[EXT_DEBUG] Extended frame detected - ID: 0x");
            Serial.print(id, HEX);
            Serial.print(" Data: ");
            for (int i = 0; i < len; i++) {
                HexHelper::printFullByte(buf[i]);
            }
            Serial.print(" Length: ");
            Serial.print(len);
            Serial.print("\r\n");
            lw232DebugExtFrames = false; // Only show once
        }

        // Reject extended frames if configured to do so
        if ((frame.flags & LW232_FRAME_FLAG_EXTENDED) && lw232RejectExtendedFrames) {
            return RX_READ_SKIPPED; // Skip extended frames
        }

        if (lw232CAN.isRemoteRequest()) {
            frame.flags |= LW232_FRAME_FLAG_REMOTE;
        }
        memcpy(frame.data, buf, len);

#if LW232_ENABLE_STATS
        statsRecordRxFrame();
#endif
        return RX_READ_READY;
    } else {
        // Frame filtered out
        return RX_READ_SKIPPED;
    }
}

void Can232::serviceCanRx() {
    // Drain MCP2515 RX buffers into software buffer
    // Process up to 5 frames per call to avoid blocking too long
    int processed = 0;
    while (processed < 5 && CAN_MSGAVAIL == checkReceive()) {
        BufferedFrame frame;
        RxReadStatus status = readCanFrame(frame);
        if (status == RX_READ_READY) {
            pushRxFrame(frame);
        }
        processed++;
    }
}

INT8U HexHelper::parseNibbleWithLimit(INT8U hex, INT8U limit) {
    INT8U ret = parseNibble(hex);
    if (ret == 0xFF) {
        return 0xFF; // propagate error
    } else if (ret < limit) {
        return ret;
    } else {
        return 0xFF; // out of range
    }
}


