#!/usr/bin/env python3
import serial
import time

def test_diagnostics():
    port = '/dev/ttyACM1'
    baud = 115200

    try:
        ser = serial.Serial(port, baud, timeout=1)
        print(f"Connected to {port} at {baud} baud")

        # Wait for Arduino to reset
        time.sleep(2)

        # Clear any buffered data
        ser.reset_input_buffer()

        # Test version command
        print("1. Version (V):")
        ser.write(b'V\r')
        time.sleep(0.1)
        response = ser.read(20).decode('ascii', errors='ignore').strip()
        print(f"   '{response}'")

        # Test serial number
        print("2. Serial number (N):")
        ser.write(b'N\r')
        time.sleep(0.1)
        response = ser.read(20).decode('ascii', errors='ignore').strip()
        print(f"   '{response}'")

        # Test CAN status flags before opening
        print("3. CAN status before opening (F):")
        ser.write(b'F\r')
        time.sleep(0.1)
        response = ser.read(20).decode('ascii', errors='ignore').strip()
        print(f"   '{response}' (hex: {[hex(ord(c)) for c in response]})")

        # Try to set CAN bitrate first
        print("4. Setting CAN bitrate to 125KBPS (S4):")
        ser.write(b'S4\r')
        time.sleep(0.1)
        response = ser.read(20).decode('ascii', errors='ignore').strip()
        print(f"   '{response}'")

        # Now try to open CAN bus
        print("5. Opening CAN bus (O):")
        ser.write(b'O\r')
        time.sleep(0.1)
        response = ser.read(20).decode('ascii', errors='ignore').strip()
        print(f"   '{response}'")

        # Check CAN status after opening
        print("6. CAN status after opening (F):")
        ser.write(b'F\r')
        time.sleep(0.1)
        response = ser.read(20).decode('ascii', errors='ignore').strip()
        print(f"   '{response}' (hex: {[hex(ord(c)) for c in response]})")

        # Test runtime stats
        print("7. Runtime stats (i):")
        ser.write(b'i\r')
        time.sleep(0.1)
        response = ser.read(50).decode('ascii', errors='ignore').strip()
        print(f"   '{response}'")

        # Test our custom debug commands
        print("8. Extended frame debug (#EXT):")
        ser.write(b'#EXT\r')
        time.sleep(0.1)
        response = ser.read(30).decode('ascii', errors='ignore').strip()
        print(f"   '{response}'")

        print("9. Extended frame rejection (%EXT1):")
        ser.write(b'%EXT1\r')
        time.sleep(0.1)
        response = ser.read(30).decode('ascii', errors='ignore').strip()
        print(f"   '{response}'")

        ser.close()
        print("\nTest completed. Arduino is responding but CAN shield may not be connected.")

    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    test_diagnostics()
