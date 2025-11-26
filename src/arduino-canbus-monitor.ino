/*
 * Arduino CAN BUS Monitor
 * Implementation of LAWICEL CAN232 v1.3 ASCII protocol
 * For CAN232/CANUSB devices: http://www.can232.com/docs/can232_v3.pdf
 *
 * Requires Arduino with Seeduino/ElecFreaks CAN BUS Shield (MCP2515)
 *
 * Copyright (C) 2015 Anton Viktorov <latonita@yandex.ru>
 *                                    https://github.com/latonita/arduino-canbus-monitor
 *
 * This library is free software. You may use/redistribute it under The MIT License terms.
 */

/*******************************************************************************
 * FILE: arduino-canbus-monitor.ino
 * 
 * DESCRIPTION:
 * This is the main Arduino sketch file that serves as the entry point for the
 * CAN bus monitor application. It provides a hardware interface between Arduino,
 * the MCP2515 CAN controller shield, and a host computer running SavvyCAN or
 * other LAWICEL-compatible software.
 * 
 * STRUCTURE AND FUNCTION:
 * - setup(): Initializes serial communication, CAN interrupt handling, CAN bus
 *   parameters (baud rate and clock), and runtime statistics
 * - loop(): Main execution loop delegating to Can232::loop() for protocol handling
 * - serialEvent(): Automatically called when serial data arrives, delegating to
 *   Can232::serialEvent() for command parsing
 * - handleCanInterrupt(): ISR callback for CAN message reception
 * - myCustomAddressFilter(): Optional user-defined filter to reduce message traffic
 *   by filtering based on CAN ID (currently configured for address 0x3d0)
 * 
 * ROLE IN CODEBASE:
 * This file acts as the minimal glue code between Arduino's setup/loop model and
 * the Can232 class which implements the full LAWICEL protocol. It configures
 * hardware-specific settings (pins, interrupts, CAN speed, clock frequency) and
 * delegates all protocol and communication logic to the modular Can232 implementation.
 *******************************************************************************/

#include <SPI.h>
#include <avr/wdt.h>
#include "mcp_can.h"
#include "can-232.h"

static void handleCanInterrupt() {
    Can232::notifyCanInterrupt();
}

void setup() {
    wdt_disable(); // Always disable first
    Serial.begin(LW232_DEFAULT_BAUD_RATE); // default COM baud rate is 230400.
#if defined(LW232_CAN_INT_PIN) && defined(digitalPinToInterrupt)
    pinMode(LW232_CAN_INT_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(LW232_CAN_INT_PIN), handleCanInterrupt, FALLING);
#endif

        // Can232::init  (RATE, CLOCK)
        // Rates: CAN_10KBPS, CAN_20KBPS, CAN_50KBPS, CAN_100KBPS, CAN_125KBPS, CAN_250KBPS, CAN_500KBPS, CAN_500KBPS, CAN_1000KBPS, CAN_83K3BPS
        //        Default is CAN_83K3BPS ;)))))))))
        // Clock: MCP_16MHz or MCP_8MHz. 
        //        Default is MCP_16MHz. Please note, not all CAN speeds supported. check big switch in mcp_can.cpp
        // defaults can be changed in mcp_can.h

//        Can232::init();             // rate and clock = LW232_DEFAULT_CAN_RATE and LW232_DEFAULT_CLOCK_FREQ
//        Can232::init(CAN_125KBPS);  // rate = 125, clock = LW232_DEFAULT_CLOCK_FREQ
    Can232::init(CAN_125KBPS, MCP_16MHz); // set default rate you need here and clock frequency of CAN shield. Typically it is 16MHz, but on some MCP2515 + TJA1050 it is 8Mhz

    // Optional custom packet filter to reduce message traffic to host software.
    // Uncomment the next line and modify myCustomAddressFilter() to filter specific CAN IDs.
    // Can232::setFilter(myCustomAddressFilter);

    wdt_enable(WDTO_1S); // 1 Second timeout
}

// Example filter function - returns LW232_FILTER_PROCESS to allow message through,
// or LW232_FILTER_SKIP to filter it out. Modify cases below for your needs.
INT8U myCustomAddressFilter(INT32U addr) {
    INT8U ret = LW232_FILTER_SKIP; //LW232_FILTER_PROCESS or LW232_FILTER_SKIP
    switch(addr) {
    //    case 0x01b: //VIN
    //    case 0x1C8:  //lights
    //    case 0x2C0: // pedals
        case 0x3d0: // sound vol, treb..
      ret = LW232_FILTER_PROCESS;
    //    case 0x000: // ?
    //    case 0x003: //shifter
    //    case 0x015: // dor open close affects this as well
    //    case 0x02c: // ???
    //        ret = 0;
    //        break;
    //    case 0x002:
    //    case 0x1a7: //fuel cons or some other
    //      ret = 1;
    //      break;
    //     default: 
    //       ret = 0;
    }

  return ret;
}

void loop() {
    wdt_reset(); // Pet the dog
    Can232::loop();
}

