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
 * FILE: can-232.h
 * 
 * DESCRIPTION:
 * Header file defining the Can232 class and associated constants for implementing
 * the LAWICEL CAN232/CANUSB ASCII protocol. This file also includes support for
 * LAWICEL protocol, making the adapter compatible with SavvyCAN and other tools
 * text-based and binary CAN monitoring applications.
 * 
 * KEY COMPONENTS:
 * 
 * 1. PROTOCOL DEFINITIONS:
 *    - LAWICEL command codes (S, O, L, C, t, T, r, R, P, A, F, X, W, M, m, U, V, N, Z, Q)
 *    - Command parameters, return codes, frame formats, and buffer sizes
 * 
 * 2. Can232 CLASS:
 *    - Singleton pattern managing all protocol state and CAN communication
 *    - Public static interface: init(), setFilter(), loop(), serialEvent()
 *    - BufferedFrame structure for queuing received CAN messages
 *    - Circular RX buffer (configurable size via LW232_RX_BUFFER_SIZE)
 *    - State machine for LAWICEL protocol parsing
 * 
 * 3. CONFIGURATION CONSTANTS:
 *    - Baud rate tables for serial UART (0-7) and CAN bus (0-9)
 *    - Default settings (500000 serial, 500 kbps CAN, 16 MHz MCP2515 clock)
 *    - Pin assignments (CS pin 10, INT pin 2)
 *    - EEPROM addresses for persistent settings (timestamp, autostart)
 * 
 * 4. HexHelper CLASS:
 *    - Utility functions for hex/ASCII conversion in LAWICEL protocol
 *    - Optimized nibble parsing with lookup table
 * 
 * ROLE IN CODEBASE:
 * This header defines the interface contract for LAWICEL protocol handling.
 * It bridges the Arduino sketch (arduino-canbus-monitor.ino), MCP2515 CAN driver
 * (mcp_can.h/cpp), and runtime statistics (runtime_stats.h/cpp). All command
 * parsing, frame buffering, filtering, EEPROM persistence, and dual-protocol
 * support are coordinated through the Can232 class declared here.
 *******************************************************************************/


#ifndef _CAN_232_H_
#define _CAN_232_H_

//#if defined(ARDUINO) && ARDUINO >= 100
//    #include "arduino.h"
//#else
//    #include "WProgram.h"
//#endif

#include "mcp_can.h"
#include "mcp_can_dfs.h"
#include "SoftwareSerial.h"

#ifndef LW232_RX_BUFFER_SIZE
// Default to a 64-frame circular buffer on small MCUs (Uno = 2 KB SRAM).
// Override in platformio.ini (e.g. -DLW232_RX_BUFFER_SIZE=128) on boards
// with more RAM such as the Mega2560.
#define LW232_RX_BUFFER_SIZE 64
#endif

#ifndef LW232_DEFAULT_UART_BAUD_INDEX
#define LW232_DEFAULT_UART_BAUD_INDEX 0x01
#endif

#define LW232_LAWICEL_VERSION_STR     "V1013"
#define LW232_LAWICEL_SERIAL_NUM      "NA666"
#define LW232_CAN_BUS_SHIELD_CS_PIN   10
#define LW232_CAN_INT_PIN             2

#define LW232_FRAME_FLAG_EXTENDED     0x01
#define LW232_FRAME_FLAG_REMOTE       0x02

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Commands not supported/not implemented:
//     s - Custom bit-rate via BTR0/BTR1 registers
//     W - Hardware filter mode (dual/single)
//     M - Acceptance code register
//     m - Acceptance mask register
//      
//   Commands fully implemented with enhancements:
//     S - Supports standard bit-rates 0-9, including S9 (83.3 kbps, not in original LAWICEL spec)
//     U - UART baud rate change (0-6) + query mode (bare 'U' returns current setting)
//     Z - Timestamp control (Z0/Z1) + query mode (bare 'Z' returns current mode), persists to EEPROM
//     Q - Auto-start control (Q0/Q1/Q2) + query mode (bare 'Q' returns current mode), persists to EEPROM
//     F - Returns MCP2515 error flags in LAWICEL-compatible bitmap format
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//                                   
//                          CODE   SUPPORTED   SYNTAX               DESCRIPTION     
//
#define LW232_CMD_SETUP     'S' //   YES+      Sn[CR]               Setup with standard CAN bit-rates where n is 0-9. Bare S[CR] reports current selection.
                                //                                  S0 10Kbit          S4 125Kbit         S8 1Mbit
                                //                                  S1 20Kbit          S5 250Kbit         S9 83.3Kbit
                                //                                  S2 50Kbit          S6 500Kbit
                                //                                  S3 100Kbit         S7 800Kbit
#define LW232_CMD_SETUP_BTR 's' //    -        sxxyy[CR]            Setup with BTR0/BTR1 CAN bit-rates where xx and yy is a hex value.
#define LW232_CMD_OPEN      'O' //   YES       O[CR]                Open the CAN channel in normal mode (sending & receiving).
#define LW232_CMD_LISTEN    'L' //   YES       L[CR]                Open the CAN channel in listen only mode (receiving).
#define LW232_CMD_CLOSE     'C' //   YES       C[CR]                Close the CAN channel.
#define LW232_CMD_TX11      't' //   YES       tiiildd...[CR]       Transmit a standard (11bit) CAN frame.
#define LW232_CMD_TX29      'T' //   YES       Tiiiiiiiildd...[CR]  Transmit an extended (29bit) CAN frame
#define LW232_CMD_RTR11     'r' //   YES       riiil[CR]            Transmit an standard RTR (11bit) CAN frame.
#define LW232_CMD_RTR29     'R' //   YES       Riiiiiiiil[CR]       Transmit an extended RTR (29bit) CAN frame.
#define LW232_CMD_POLL_ONE  'P' //   YES       P[CR]                Poll incomming FIFO for CAN frames (single poll)
#define LW232_CMD_POLL_MANY 'A' //   YES       A[CR]                Polls incomming FIFO for CAN frames (all pending frames)
#define LW232_CMD_FLAGS     'F' //   YES+      F[CR]                Read Status Flags.
#define LW232_CMD_AUTOPOLL  'X' //   YES       Xn[CR]               Sets Auto Poll/Send ON/OFF for received frames.
#define LW232_CMD_FILTER    'W' //    -        Wn[CR]               Filter mode setting. By default CAN232 works in dual filter mode (0) and is backwards compatible with previous CAN232 versions.
#define LW232_CMD_ACC_CODE  'M' //    -        Mxxxxxxxx[CR]        Sets Acceptance Code Register (ACn Register of SJA1000). // we use MCP2515, not supported
#define LW232_CMD_ACC_MASK  'm' //    -        mxxxxxxxx[CR]        Sets Acceptance Mask Register (AMn Register of SJA1000). // we use MCP2515, not supported
#define LW232_CMD_UART      'U' //   YES       Un[CR]               Setup UART with a new baud rate where n is 0-7.
#define LW232_CMD_VERSION1  'V' //   YES       v[CR]                Get Version number of both CAN232 hardware and software
#define LW232_CMD_VERSION2  'v' //   YES       V[CR]                Get Version number of both CAN232 hardware and software
#define LW232_CMD_SERIAL    'N' //   YES       N[CR]                Get Serial number of the CAN232.
#define LW232_CMD_TIMESTAMP 'Z' //   YES       Zn[CR]               Sets Time Stamp ON/OFF for received frames only.
#define LW232_CMD_AUTOSTART 'Q' //   YES  todo     Qn[CR]               Auto Startup feature (from power on).
#define LW232_CMD_YANK      'Y' //   custom   Yn[CR]               Yank/reset command (SavvyCAN compatibility)
#define LW232_CMD_INFO      'i' //   custom   i[CR]               Report runtime statistics (adapter diagnostics)
#define LW232_CMD_DEBUG     '@' //   custom   @DBGn[CR]            Runtime debug toggle (0=off, 1=on)
#define LW232_CMD_DEBUG_EXT '#' //   custom   #EXT[CR]             Show raw registers for next extended frame
#define LW232_CMD_REJECT_EXT '%' //   custom   %EXTn[CR]            Reject extended frames (0=accept, 1=reject)

#define LOW_BYTE(x)     ((unsigned char)((x)&0xFF))
#define HIGH_BYTE(x)    ((unsigned char)(((x)>>8)&0xFF))
#define LOW_WORD(x)     ((unsigned short)((x)&0xFFFF))
#define HIGH_WORD(x)    ((unsigned short)(((x)>>16)&0xFFFF))

#ifndef INT32U
#define INT32U unsigned long
#endif

#ifndef INT16U
#define INT16U word
#endif

#ifndef INT8U
#define INT8U byte
#endif

#define LW232_OK                      0x00
#define LW232_OK_SMALL                0x01
#define LW232_OK_BIG                  0x02
#define LW232_ERR                     0x03
#define LW232_ERR_NOT_IMPLEMENTED     0x04
#define LW232_ERR_UNKNOWN_CMD         0x05

#define LW232_FILTER_SKIP			  0x00
#define LW232_FILTER_PROCESS	      0x01

//#define LW232_IS_OK(x) ((x)==LW232_OK ||(x)==LW232_OK_NEW ? TRUE : FALSE)

#define LW232_CR    '\r'
#define LW232_ALL   'A'
#define LW232_FLAG  'F'
#define LW232_TR11  't'
#define LW232_TR29  'T'

#define LW232_RET_ASCII_OK             0x0D
#define LW232_RET_ASCII_ERROR          0x07
#define LW232_RET_ASCII_OK_SMALL       'z'
#define LW232_RET_ASCII_OK_BIG         'Z'

#define LW232_STATUS_CAN_CLOSED        0x00
#define LW232_STATUS_CAN_OPEN_NORMAL   0x01
#define LW232_STATUS_CAN_OPEN_LISTEN   0x02

#define LW232_FRAME_MAX_LENGTH         0x08
#define LW232_FRAME_MAX_SIZE           (sizeof("Tiiiiiiiildddddddddddddddd\r")+1)

#define LW232_INPUT_STRING_BUFFER_SIZE 64

#define LW232_PROTOCOL_LAWICEL         0x00

#define LW232_OFF                      '0'
#define LW232_ON_ONE                   '1'
#define LW232_ON_TWO                   '2'

#define LW232_AUTOPOLL_OFF             0x00
#define LW232_AUTOPOLL_ON              0x01

#define LW232_AUTOSTART_OFF            0x00
#define LW232_AUTOSTART_ON_NORMAL      0x01
#define LW232_AUTOSTART_ON_LISTEN      0x02

#define LW232_EEPROM_ADDR_MAGIC        0x00
#define LW232_EEPROM_ADDR_TIMESTAMP    0x01
#define LW232_EEPROM_ADDR_AUTOSTART    0x10
#define LW232_AUTOSTART_BLOCK_VERSION  0x01
#define LW232_AUTOSTART_CHECKSUM_SEED  0x5A
#define LW232_EEPROM_MAGIC_VALUE       0xA5

#define LW232_TIMESTAMP_OFF            0x00
#define LW232_TIMESTAMP_ON_NORMAL      0x01
#define LW232_OFFSET_STD_PKT_LEN       0x04
#define LW232_OFFSET_STD_PKT_DATA      0x05
#define LW232_OFFSET_EXT_PKT_LEN       0x09
#define LW232_OFFSET_EXT_PKT_DATA      0x0A


#define LW232_DEFAULT_BAUD_RATE        115200
#define LW232_DEFAULT_CAN_RATE         CAN_500KBPS
#define LW232_DEFAULT_CLOCK_FREQ       MCP_16MHz

#define LW232_CAN_BAUD_NUM             0x0a
#define LW232_UART_BAUD_NUM            0x08


const INT32U lw232SerialBaudRates[] //PROGMEM
= { 230400, 115200, 57600, 38400, 19200, 9600, 2400, 500000 };

const INT8U lw232CanBaudRates[] //PROGMEM
= { CAN_10KBPS, CAN_20KBPS, CAN_50KBPS, CAN_100KBPS, CAN_125KBPS, CAN_250KBPS, CAN_500KBPS, CAN_800KBPS, CAN_1000KBPS, CAN_83K3BPS };

class Can232
{
public:
    static void init(INT8U defaultCanSpeed = LW232_DEFAULT_CAN_RATE, const INT8U clock = LW232_DEFAULT_CLOCK_FREQ);
    static void setFilter(INT8U (*userFunc)(INT32U));
    static void loop();
    static void serialEvent();
    static void notifyCanInterrupt();

private:
    static Can232* _instance;
    static Can232* instance();

    struct BufferedFrame {
        INT32U id;
        INT32U timestamp;
        INT8U len;
        INT8U flags;
        INT8U data[8];
    };

#if defined(__AVR__)
    // Keep the RX queue from consuming more than half of the available SRAM on AVR
    // boards (prevents silent heap allocation failures on 2 KB parts like the Uno).
    static_assert(LW232_RX_BUFFER_SIZE * sizeof(BufferedFrame) <= (RAMEND - RAMSTART + 1) / 2,
                  "LW232_RX_BUFFER_SIZE is too large for this AVR's SRAM budget");
#endif

    enum RxReadStatus : INT8U {
        RX_READ_NONE = 0,
        RX_READ_SKIPPED = 1,
        RX_READ_READY = 2
    };

    void initFunc();
    void setFilterFunc(INT8U (*userFunc)(INT32U));
    void loopFunc();
    void serialEventFunc();
    void loopLawicel();

    INT8U (*userAddressFilterFunc)(INT32U addr) = 0;

    MCP_CAN lw232CAN = MCP_CAN(LW232_CAN_BUS_SHIELD_CS_PIN);
    INT8U lw232SerialBaudIndex = LW232_DEFAULT_UART_BAUD_INDEX; // Default to 500000 (index 7)
    INT8U lw232PendingSerialBaudIndex = 0xFF;   // 0xFF => no scheduled change
    bool lw232BitrateConfigured = false;
    INT8U readLawicelStatusFlags();

    INT8U lw232CanSpeedIndex = 0x00;
    INT8U lw232CanSpeedSelection = CAN_83K3BPS;
    INT8U lw232McpModuleClock = MCP_16MHz;
    INT8U lw232CanChannelMode = LW232_STATUS_CAN_CLOSED;
    INT8U lw232LastErr = LW232_OK;

    // Hardware acceptance filtering (LAWICEL W/M/m)
    INT8U lw232FilterMode = 0x00;                // 0 = dual (default), 1 = single
    INT8U lw232AcceptanceCode[4] = {0, 0, 0, 0}; // Raw SJA1000-style bytes
    INT8U lw232AcceptanceMask[4] = {0xFF, 0xFF, 0xFF, 0xFF}; // Raw SJA1000-style bytes, default to all bits

    INT8U lw232AutoStart = LW232_AUTOSTART_OFF;
    INT8U lw232AutoPoll  = LW232_AUTOPOLL_OFF;
    INT8U lw232TimeStamp = LW232_TIMESTAMP_OFF;
    bool lw232DebugMode = false;
    bool lw232DebugExtFrames = false;
    bool lw232RejectExtendedFrames = false;

    INT32U lw232CanId = 0;

    INT8U lw232Buffer[8];
    INT8U lw232PacketLen = 0;

    INT8U lw232Message[LW232_FRAME_MAX_SIZE];

    BufferedFrame rxBuffer[LW232_RX_BUFFER_SIZE];
    volatile INT16U rxHead = 0;
    volatile INT16U rxTail = 0;
    volatile INT16U rxCount = 0;

    // Bus load monitoring
    unsigned long lastBusLoadCalc = 0;
    unsigned long lastBusLoadFrameCount = 0;

    // Output pacing for autopoll to prevent jitter and long bursts
    static const unsigned int AUTOPOLL_MAX_BATCH_BYTES = 256;  // Stop after ~256 bytes
    static const unsigned long AUTOPOLL_MAX_BATCH_TIME_MS = 2; // Or ~2ms elapsed
    unsigned int autopollBatchBytes = 0;
    unsigned long autopollBatchStartTime = 0;

    String inputString = "";         // a string to hold incoming data
    boolean stringComplete = false;  // whether the string is complete
    volatile bool mcpInterruptPending = false;

    INT8U parseAndRunCommand();
    INT8U exec();

    void scheduleSerialBaudChange(INT8U idx);
    void applyPendingSerialBaudChange();
    void initializeEepromIfNeeded();
    void loadTimestampPreference();
    void persistTimestampPreference();
    void loadAutoStartPreference();
    void persistAutoStartPreference();
    void maybeAutoStart();
    void persistCanSpeedSelection();
    static INT8U findCanBaudIndex(INT8U canSpeed);
    static INT8U findCanBaudIndexByBps(INT32U bps);
    static INT8U computeAutoStartChecksum(INT8U version, INT8U mode, INT8U idx);

    INT8U checkReceive();
    INT8U readMsgBufID(INT32U *ID, INT8U *len, INT8U buf[]);
    INT8U receiveSingleFrame();
    void emitFrameToSerial(const BufferedFrame& frame);
    unsigned int estimateFrameSize(const BufferedFrame& frame);
    void serviceCanRx();
    RxReadStatus readCanFrame(BufferedFrame& frame);
    bool consumeInterruptFlag();
    bool rxBufferEmpty() const;
    void clearRxBuffer();
    bool pushRxFrame(const BufferedFrame& frame);
    bool popRxFrame(BufferedFrame& frame);

    INT8U isExtendedFrame();
    INT8U checkPassFilter(INT32U addr);
    INT8U openCanBus(INT8U mode = MODE_NORMAL);
    
    INT8U sendMsgBuf(INT32U id, INT8U ext, INT8U rtr, INT8U len, INT8U *buf);

    bool  parseCanStdId();
    bool  parseCanExtId();

    void updateBusLoad();
};

class HexHelper {
public:
    static void printFullByte(INT8U b);
    static void printNibble(INT8U b);

    static INT8U parseNibble(INT8U hex);
    static INT8U parseFullByte(INT8U H, INT8U L, bool *ok = nullptr);
    static INT8U parseNibbleWithLimit(INT8U hex, INT8U limit);

    static char toHexChar(INT8U nibble);
    static void byteToHex(INT8U value, char *out);
};



#endif
