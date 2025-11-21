# Arduino Uno R3 Specifications and Hardware Layout

## Overview

The Arduino Uno R3 is a microcontroller board based on the ATmega328P microcontroller. It is the most popular and widely used Arduino board, providing a standard platform for embedded development.

## Technical Specifications

### Microcontroller
- **Chip**: ATmega328P
- **Architecture**: 8-bit AVR RISC
- **Clock Speed**: 16 MHz (external crystal oscillator)
- **Instruction Set**: AVR instruction set (131 instructions)

### Memory
- **Flash Memory**: 32 KB (0.5 KB used by bootloader)
  - Available for user programs: ~31.5 KB
- **SRAM**: 2 KB
  - Used for variables and runtime data
  - Volatile memory (lost on power cycle)
- **EEPROM**: 1 KB
  - Non-volatile memory for persistent data storage
  - Can be read/written via EEPROM library

### Power Specifications
- **Operating Voltage**: 5V
- **Input Voltage (recommended)**: 7-12V DC
- **Input Voltage (limits)**: 6-20V DC
- **DC Current per I/O Pin**: 40 mA
- **DC Current for 3.3V Pin**: 50 mA
- **Power Consumption**: ~50 mA (idle), ~80-100 mA (active)

### Digital I/O Pins
- **Total Digital Pins**: 14
- **PWM Pins**: 6 (pins 3, 5, 6, 9, 10, 11)
  - 8-bit PWM resolution (0-255)
- **Interrupt Pins**: 2 (pins 2, 3)
  - External interrupt capability
- **SPI Pins**: 
  - MOSI: Pin 11
  - MISO: Pin 12
  - SCK: Pin 13
  - SS: Pin 10 (default, can be changed)
- **I2C Pins**:
  - SDA: Pin A4 (analog pin 4)
  - SCL: Pin A5 (analog pin 5)
  - Uses Wire library
- **Serial Pins**:
  - TX: Pin 1
  - RX: Pin 0
  - Hardware UART (also used for USB communication)

### Analog Input Pins
- **Total Analog Pins**: 6 (A0-A5)
- **Resolution**: 10-bit (0-1023)
- **Reference Voltage**: 5V (default), can be changed to 3.3V or external
- **Input Impedance**: ~100 MΩ

### Physical Dimensions
- **Length**: 68.6 mm (2.7 inches)
- **Width**: 53.4 mm (2.1 inches)
- **Weight**: ~25 grams
- **Mounting Holes**: 4 (M3 screws)

## Pin Layout and Functions

### Digital Pins (0-13)

| Pin | Function | Notes |
|-----|----------|-------|
| 0 | RX (Serial) | Hardware serial receive, also used for USB communication |
| 1 | TX (Serial) | Hardware serial transmit, also used for USB communication |
| 2 | Digital I/O | External interrupt (INT0) |
| 3 | Digital I/O, PWM | External interrupt (INT1), Timer2 PWM |
| 4 | Digital I/O | General purpose |
| 5 | Digital I/O, PWM | Timer0 PWM |
| 6 | Digital I/O, PWM | Timer0 PWM |
| 7 | Digital I/O | General purpose |
| 8 | Digital I/O | General purpose |
| 9 | Digital I/O, PWM | Timer1 PWM |
| 10 | Digital I/O, PWM, SS | Timer1 PWM, SPI Slave Select (default) |
| 11 | Digital I/O, PWM, MOSI | Timer2 PWM, SPI Master Out Slave In |
| 12 | Digital I/O, MISO | SPI Master In Slave Out |
| 13 | Digital I/O, SCK | SPI Serial Clock, built-in LED |

### Analog Pins (A0-A5)

| Pin | Function | Notes |
|-----|----------|-------|
| A0 | Analog Input | Can also be used as digital pin 14 |
| A1 | Analog Input | Can also be used as digital pin 15 |
| A2 | Analog Input | Can also be used as digital pin 16 |
| A3 | Analog Input | Can also be used as digital pin 17 |
| A4 | Analog Input, SDA | Can also be used as digital pin 18, I2C Data |
| A5 | Analog Input, SCL | Can also be used as digital pin 19, I2C Clock |

### Power Pins

| Pin | Function | Voltage | Max Current |
|-----|----------|---------|-------------|
| VIN | Input Voltage | 7-12V (recommended) | ~1A |
| 5V | Regulated 5V Output | 5V | ~800mA (from USB) |
| 3.3V | Regulated 3.3V Output | 3.3V | 50mA |
| GND | Ground | 0V | - |
| IOREF | I/O Reference Voltage | 5V | - |

### Special Pins

| Pin | Function | Notes |
|-----|----------|-------|
| AREF | Analog Reference | External reference voltage for ADC |
| RESET | Reset | Active LOW, resets microcontroller |

## Hardware Layout Diagram

```
                    ┌─────────────────────────┐
                    │      USB-B Connector    │
                    └─────────────────────────┘
                              │
        ┌─────────────────────────────────────┐
        │                                     │
        │  [RESET]  [3.3V]  [5V]  [GND]  [VIN]│
        │                                     │
        │  [A0]  [A1]  [A2]  [A3]  [A4]  [A5] │
        │                                     │
        │  [AREF]  [GND]  [D13]  [D12]  [D11]│
        │                                     │
        │  [D10]  [D9]  [D8]  [D7]  [D6]  [D5]│
        │                                     │
        │  [D4]  [D3]  [D2]  [D1]  [D0]  [GND]│
        │                                     │
        │         [ICSP Header]               │
        │                                     │
        └─────────────────────────────────────┘
                    │
        ┌─────────────────────────┐
        │   Power LED (ON)        │
        │   Pin 13 LED (L)        │
        └─────────────────────────┘
```

## Communication Interfaces

### Serial (UART)
- **Hardware**: Built-in USART
- **Pins**: TX (Pin 1), RX (Pin 0)
- **Baud Rates**: 300 to 115200 (and higher)
- **Library**: `Serial` object

### SPI (Serial Peripheral Interface)
- **Hardware**: Built-in SPI controller
- **Pins**: MOSI (11), MISO (12), SCK (13), SS (10)
- **Speed**: Up to 8 MHz (half of system clock)
- **Mode**: Master mode
- **Library**: `SPI` library

### I2C (Two-Wire Interface)
- **Hardware**: Built-in TWI controller
- **Pins**: SDA (A4), SCL (A5)
- **Speed**: Standard mode (100 kHz) or Fast mode (400 kHz)
- **Address Space**: 7-bit addressing (128 addresses)
- **Library**: `Wire` library

## Programming

### Bootloader
- **Bootloader Size**: 0.5 KB
- **Upload Method**: USB (via onboard USB-to-Serial chip)
- **Programmer**: Arduino as ISP, USBtinyISP, or similar

### Development Environment
- **IDE**: Arduino IDE, PlatformIO, Atmel Studio
- **Language**: C/C++ (Arduino framework)
- **Compiler**: avr-gcc

### USB-to-Serial Chip
- **Chip**: ATmega16U2 (or CH340 on some clones)
- **Function**: Converts USB to serial for programming and communication
- **Driver**: Usually automatic on modern operating systems

## Power Options

1. **USB Power**: 5V via USB-B connector
   - Provides ~500mA at 5V
   - Powers the board and can power external devices

2. **DC Barrel Jack**: 7-12V DC input
   - Regulated to 5V via onboard regulator
   - Can provide more current than USB

3. **VIN Pin**: Direct voltage input
   - Bypasses barrel jack
   - Requires 7-12V DC

4. **5V Pin**: Direct 5V input
   - Bypasses onboard regulator
   - Must be regulated 5V
   - Use with caution

## Physical Characteristics

### Connectors
- **Digital Pins**: 0.1" spacing headers (male)
- **Analog Pins**: 0.1" spacing headers (male)
- **Power Pins**: 0.1" spacing headers (male)
- **USB Connector**: USB-B type
- **DC Power**: 2.1mm center-positive barrel jack
- **ICSP Header**: 6-pin header for in-system programming

### LEDs
- **Power LED**: Indicates board is powered (ON)
- **Pin 13 LED**: Connected to digital pin 13 (L)
- **TX/RX LEDs**: Indicate serial communication (on some boards)

## Compatibility Notes

- **Shields**: Compatible with standard Arduino shields
- **Libraries**: Works with most Arduino libraries
- **Clones**: Many compatible clones available (may use CH340 USB chip)
- **Voltage Levels**: 5V logic (not 3.3V tolerant on most pins)

## Common Applications

- Prototyping and development
- Educational projects
- Hobby electronics
- CAN bus interfaces (with shields)
- Sensor interfaces
- Motor control
- Data logging
- IoT projects (with additional modules)

## References

- [Official Arduino Uno R3 Documentation](https://docs.arduino.cc/hardware/uno-rev3) - Complete hardware specifications and pin mapping
- [ATmega328P Datasheet](https://ww1.microchip.com/downloads/en/DeviceDoc/ATmega48A-PA-88A-PA-168A-PA-328-P-DS-DS40002061A.pdf) - Microchip Technology official microcontroller datasheet
- [Arduino Hardware Reference](https://www.arduino.cc/en/uploads/Main/arduino-uno-schematic.pdf) - Official schematic diagram and electrical specifications

