# GVRET Protocol Specification

## Overview

GVRET (Generalized Vehicle Reverse Engineering Tool) is a binary serial protocol used by SavvyCAN and other vehicle network analysis tools for communicating with CAN bus adapters. It provides efficient binary data transfer compared to ASCII-based protocols like LAWICEL/SLCAN.

## Protocol Basics

### Communication Parameters
- **Baud Rate**: Typically 115200 baud (configurable)
- **Data Format**: Binary protocol (not ASCII)
- **Endianness**: Little-endian for multi-byte values
- **Frame Format**: Binary packets with command bytes and data payloads

### Packet Structure

GVRET uses binary packets with the following general structure:
```
[Command Byte] [Length] [Data...] [Checksum]
```

### Command Set

#### CAN Frame Transmission
- **Standard CAN Frame**: Command byte `0x01`
  - Format: `[0x01] [ID_LOW] [ID_HIGH] [DLC] [DATA...]`
  - ID: 11-bit CAN identifier (2 bytes, little-endian)
  - DLC: Data Length Code (0-8 bytes)
  - Data: Up to 8 bytes of payload

- **Extended CAN Frame**: Command byte `0x02`
  - Format: `[0x02] [ID_LOW] [ID_MID_LOW] [ID_MID_HIGH] [ID_HIGH] [DLC] [DATA...]`
  - ID: 29-bit CAN identifier (4 bytes, little-endian)
  - DLC: Data Length Code (0-8 bytes)
  - Data: Up to 8 bytes of payload

#### CAN Frame Reception
- **Received Frame**: Command byte `0x03` or `0x04`
  - Format: `[0x03/0x04] [TIMESTAMP_LOW] [TIMESTAMP_HIGH] [ID...] [DLC] [DATA...]`
  - Timestamp: 16-bit timestamp in milliseconds (2 bytes, little-endian)
  - ID: CAN identifier (2 bytes for standard, 4 bytes for extended)
  - DLC: Data Length Code
  - Data: Up to 8 bytes of payload

#### Bus Control Commands
- **Set CAN Speed**: Command byte `0x05`
  - Format: `[0x05] [SPEED_INDEX]`
  - Speed indices: 0=10K, 1=20K, 2=50K, 3=100K, 4=125K, 5=250K, 6=500K, 7=800K, 8=1M

- **Open CAN Bus**: Command byte `0x06`
  - Opens the CAN channel for normal operation

- **Close CAN Bus**: Command byte `0x07`
  - Closes the CAN channel

- **Set Listen-Only Mode**: Command byte `0x08`
  - Opens CAN channel in listen-only mode (RX only)

#### Status and Configuration
- **Get Firmware Version**: Command byte `0x09`
  - Returns firmware version string

- **Get Serial Number**: Command byte `0x0A`
  - Returns device serial number

- **Set Digital Output**: Command byte `0x0B`
  - Format: `[0x0B] [OUTPUT_NUMBER] [STATE]`

- **Set Single Wire Mode**: Command byte `0x0C`
  - Enables single-wire CAN mode

- **Set Silent Mode**: Command byte `0x0D`
  - Enables silent mode (no ACK on CAN bus)

#### Timestamp Control
- **Enable Timestamps**: Command byte `0x0E`
  - Enables timestamping on received frames

- **Disable Timestamps**: Command byte `0x0F`
  - Disables timestamping on received frames

#### Flow Control
- **Flow Control Frame**: Command byte `0x10`
  - Used for flow control in high-speed scenarios

#### Extended Commands
- **Set Extended Filters**: Command byte `0x11`
  - Configure hardware filters for CAN IDs

- **Set Extended Masks**: Command byte `0x12`
  - Configure hardware masks for CAN IDs

### Response Codes

- **ACK**: `0x00` - Command acknowledged successfully
- **NAK**: `0xFF` - Command failed or not acknowledged
- **Data Frame**: Various command bytes for received CAN frames

### Timestamp Format

Timestamps are 16-bit values representing milliseconds since the last reset or overflow. They wrap around at 65535ms (~65.5 seconds).

### Error Handling

- Invalid commands return NAK (`0xFF`)
- Bus errors are reported via status frames
- Buffer overflows may cause frame drops (no explicit notification in basic protocol)

### Advantages over ASCII Protocols

1. **Efficiency**: Binary encoding is more compact than ASCII hex
2. **Speed**: Faster parsing and transmission
3. **Precision**: Better timestamp resolution
4. **Bandwidth**: Lower serial bandwidth usage for high-speed CAN traffic

### Compatibility Notes

- GVRET is primarily used by SavvyCAN
- Some adapters support both GVRET and LAWICEL/SLCAN protocols
- Protocol selection is typically done via configuration or initialization sequence

### Example Usage Flow

1. **Initialize Connection**: Open serial port at 115200 baud
2. **Set CAN Speed**: Send `[0x05] [SPEED_INDEX]`
3. **Open CAN Bus**: Send `[0x06]`
4. **Receive Frames**: Listen for `[0x03]` or `[0x04]` packets
5. **Transmit Frames**: Send `[0x01]` or `[0x02]` packets
6. **Close Bus**: Send `[0x07]` when done

### References

- SavvyCAN documentation and source code
- GVRET protocol implementations in various CAN adapter firmwares
- Vehicle network analysis tools documentation

