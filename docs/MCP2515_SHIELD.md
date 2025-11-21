# Inland/Keyestudio MCP2515 CAN Bus Shield Specifications

## Overview

The Inland/Keyestudio MCP2515 CAN Bus Shield is an add-on board that enables Arduino Uno and compatible boards to communicate over CAN (Controller Area Network) bus. It is commonly used for automotive diagnostics, vehicle network analysis, and industrial CAN applications.

## Technical Specifications

### CAN Controller
- **Chip**: MCP2515 (Microchip Technology)
- **Interface**: SPI (Serial Peripheral Interface)
- **CAN Protocol**: CAN 2.0A/B compliant
- **Bit Rates**: 10 Kbps to 1 Mbps (configurable)
- **Message Buffers**: 
  - 2 transmit buffers
  - 2 receive buffers with 2 acceptance filters each
  - 6 acceptance filters total

### CAN Transceiver
- **Chip**: TJA1050 (NXP Semiconductors) or SN65HVD230 (Texas Instruments)
- **Standard**: ISO 11898-2 compliant
- **Voltage**: 5V operation
- **Speed**: Up to 1 Mbps
- **Protection**: Built-in bus fault protection

### Power Specifications
- **Operating Voltage**: 5V (from Arduino)
- **Current Consumption**: ~50-100 mA (typical)
- **CAN Bus Voltage**: 12V nominal (automotive)

### Communication Interface
- **SPI Speed**: Up to 10 MHz (typically runs at 4-8 MHz)
- **SPI Mode**: Mode 0 (CPOL=0, CPHA=0)
- **Chip Select**: Digital Pin 10 (default, can be changed)

### Physical Characteristics
- **Form Factor**: Standard Arduino shield
- **Dimensions**: Compatible with Arduino Uno footprint
- **Connectors**: 
  - CAN bus terminal block (CANH, CANL, GND)
  - Optional DB9 connector (on some models)
- **LED Indicators**: 
  - Power LED
  - CAN activity LEDs (TX/RX)

## Pin Connections to Arduino

### SPI Interface (Required)
| Shield Pin | Arduino Pin | Function |
|------------|-------------|----------|
| CS | D10 | Chip Select (SPI) |
| MOSI | D11 | Master Out Slave In |
| MISO | D12 | Master In Slave Out |
| SCK | D13 | Serial Clock |
| INT | D2 | Interrupt (optional but recommended) |

### Power Connections
| Shield Pin | Arduino Pin | Function |
|------------|-------------|----------|
| VCC | 5V | Power supply |
| GND | GND | Ground |

### Additional Connections (Model Dependent)
- Some shields have jumpers for:
  - CS pin selection (D9 or D10)
  - Interrupt pin selection (D2 or D3)
  - Termination resistor enable (120Ω)

## Hardware Layout

### Top View (Component Side)
```
┌─────────────────────────────────────┐
│                                     │
│  [CAN Terminal Block]               │
│  CANH  CANL  GND                    │
│                                     │
│  [MCP2515]    [TJA1050/SN65HVD230] │
│                                     │
│  [LEDs]  [Jumpers]  [DB9?]         │
│                                     │
│  [Arduino Headers - Male Pins]      │
│                                     │
└─────────────────────────────────────┘
```

### Pin Mapping Details

#### SPI Pins (Fixed)
- **MOSI (D11)**: Data from Arduino to MCP2515
- **MISO (D12)**: Data from MCP2515 to Arduino
- **SCK (D13)**: Clock signal
- **CS (D10)**: Chip select (active LOW)

#### Interrupt Pin (Recommended)
- **INT (D2)**: Interrupt output from MCP2515
  - Goes LOW when CAN message received or transmit complete
  - Enables efficient interrupt-driven operation
  - Can be changed via jumper on some shields

#### CAN Bus Connections
- **CANH**: CAN High signal (typically yellow wire)
- **CANL**: CAN Low signal (typically green wire)
- **GND**: Ground reference for CAN bus

## CAN Bus Wiring

### Standard CAN Bus (Differential)
```
Device 1          Device 2          Device 3
CANH ──────────── CANH ──────────── CANH
CANL ──────────── CANL ──────────── CANL
GND  ──────────── GND  ──────────── GND
```

### Termination Resistors
- **Value**: 120Ω (standard CAN bus termination)
- **Location**: At each end of the CAN bus
- **Purpose**: Prevents signal reflections
- **Shield**: May have jumper to enable/disable onboard termination

### CAN Bus Voltage Levels
- **CANH**: 2.5V (idle), 3.5V (dominant), 1.5V (recessive)
- **CANL**: 2.5V (idle), 1.5V (dominant), 3.5V (recessive)
- **Differential**: 0V (recessive), 2V (dominant)

## MCP2515 Features

### Message Filters
- **6 Acceptance Filters**: 
  - RXB0: 2 filters (RXF0, RXF1)
  - RXB1: 4 filters (RXF2, RXF3, RXF4, RXF5)
- **2 Acceptance Masks**: 
  - RXM0 (for RXB0)
  - RXM1 (for RXB1)
- **Filter Modes**: Standard (11-bit) or Extended (29-bit) IDs

### Operating Modes
- **Normal Mode**: Full CAN operation (TX and RX)
- **Listen-Only Mode**: Receive only, no ACK transmission
- **Loopback Mode**: Internal loopback for testing
- **Configuration Mode**: For setup and configuration
- **Sleep Mode**: Low power consumption

### Error Detection
- **Error Flags**: 
  - Bus Error
  - Error Warning
  - Error Passive
  - Bus Off
- **Error Counters**: Transmit and Receive Error Counters (TEC, REC)

## Library Usage

### Seeed Studio CAN_BUS_Shield Library
```cpp
#include <SPI.h>
#include "mcp_can.h"

MCP_CAN CAN(10);  // CS pin on D10

void setup() {
    Serial.begin(115200);
    
    // Initialize CAN bus at 500 kbps
    if(CAN.begin(CAN_500KBPS) == CAN_OK) {
        Serial.println("CAN init ok!");
    } else {
        Serial.println("CAN init fail!");
    }
}

void loop() {
    // Receive message
    unsigned char len = 0;
    unsigned char buf[8];
    
    if(CAN.checkReceive() == CAN_MSGAVAIL) {
        CAN.readMsgBuf(&len, buf);
        unsigned long canId = CAN.getCanId();
        
        Serial.print("ID: ");
        Serial.print(canId, HEX);
        Serial.print(" Data: ");
        for(int i = 0; i < len; i++) {
            Serial.print(buf[i], HEX);
            Serial.print(" ");
        }
        Serial.println();
    }
    
    // Transmit message
    unsigned char data[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    CAN.sendMsgBuf(0x123, 0, 8, data);
    delay(100);
}
```

### Pin Configuration
- Default CS pin: **D10**
- Default INT pin: **D2** (if interrupt enabled)
- SPI pins: **D11 (MOSI), D12 (MISO), D13 (SCK)** - fixed by Arduino hardware

## Bit Rate Configuration

Common CAN bit rates supported:
- **10 Kbps**: Long distance, low speed
- **20 Kbps**: Medium distance
- **50 Kbps**: Standard low-speed
- **100 Kbps**: Medium speed
- **125 Kbps**: Common automotive (OBD-II)
- **250 Kbps**: Medium-high speed
- **500 Kbps**: High speed (most common)
- **800 Kbps**: Very high speed
- **1 Mbps**: Maximum speed (short distance)

## Hardware Considerations

### Power Supply
- Shield draws power from Arduino 5V pin
- Ensure Arduino power supply can handle additional ~100mA
- Use external power for Arduino if powering multiple shields

### SPI Bus Sharing
- MCP2515 shares SPI bus with other devices
- Use proper CS (Chip Select) management
- Only one device should be active at a time

### Interrupt Usage
- **Recommended**: Use interrupt pin (D2) for efficient operation
- **Polling**: Can poll MCP2515 status registers if interrupt not used
- **Performance**: Interrupt-driven is faster and more efficient

### CAN Bus Termination
- **Required**: 120Ω termination resistors at bus ends
- **Shield**: May have onboard termination (check jumper)
- **Multiple Devices**: Only terminate at physical ends of bus

### ESD Protection
- CAN bus lines are exposed to external environment
- Use proper grounding
- Consider additional protection for harsh environments

## Common Issues and Solutions

### Problem: CAN init fails
- **Solution**: Check SPI connections, verify CS pin, check power supply

### Problem: No messages received
- **Solution**: Verify CAN bus wiring (CANH/CANL), check termination resistors, verify bit rate

### Problem: Messages corrupted
- **Solution**: Check bus termination, verify wiring, reduce bit rate, check for electrical noise

### Problem: Shield not detected
- **Solution**: Verify SPI connections, check CS pin configuration, test with multimeter

## Compatible Arduino Boards

- Arduino Uno R3
- Arduino Mega 2560
- Arduino Nano (with adapter)
- Arduino Leonardo (with modifications)
- Compatible clones (Elegoo, SainSmart, etc.)

## Model Variations

### Inland MCP2515 Shield
- Blue PCB (eco-friendly version available)
- TJA1050 transceiver
- Standard terminal block
- LED indicators

### Keyestudio MCP2515 Shield
- Similar specifications
- May use SN65HVD230 transceiver
- Slight variations in layout

### Other Compatible Shields
- ElecFreaks CAN Bus Shield
- Seeduino CAN Bus Shield
- Generic MCP2515 shields

## References

- MCP2515 Datasheet: Microchip Technology
- TJA1050 Datasheet: NXP Semiconductors
- SN65HVD230 Datasheet: Texas Instruments
- Seeed Studio CAN_BUS_Shield Library: https://github.com/Seeed-Studio/CAN_BUS_Shield
- CAN 2.0 Specification: ISO 11898

## Application Notes

### Automotive Diagnostics
- OBD-II protocol support
- Vehicle network monitoring
- ECU communication

### Industrial Applications
- Machine control
- Sensor networks
- Data acquisition

### Research and Development
- Protocol analysis
- Network testing
- Reverse engineering

