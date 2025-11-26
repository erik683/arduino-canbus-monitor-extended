#!/usr/bin/env python3
import serial
import time
import sys

def test_arduino():
    port = '/dev/ttyACM2'
    baud = 115200

    try:
        ser = serial.Serial(port, baud, timeout=1)
        print(f"Connected to {port} at {baud} baud")

        # Wait for Arduino to reset
        time.sleep(2)

        # Test version command
        print("Sending version command (V)...")
        ser.write(b'V\r')
        response = ser.readline().decode('ascii', errors='ignore').strip()
        print(f"Response: '{response}'")

        # Test opening CAN bus
        print("Opening CAN bus (O)...")
        ser.write(b'O\r')
        response = ser.readline().decode('ascii', errors='ignore').strip()
        print(f"Response: '{response}'")

        # Wait a bit for CAN initialization
        time.sleep(1)

        # Test our new debug commands
        print("Testing extended frame rejection (enable)...")
        ser.write(b'%EXT1\r')
        response = ser.readline().decode('ascii', errors='ignore').strip()
        print(f"Response: '{response}'")

        print("Testing extended frame rejection (disable)...")
        ser.write(b'%EXT0\r')
        response = ser.readline().decode('ascii', errors='ignore').strip()
        print(f"Response: '{response}'")

        # Test runtime stats
        print("Testing runtime stats (i)...")
        ser.write(b'i\r')
        response = ser.readline().decode('ascii', errors='ignore').strip()
        print(f"Response: '{response}'")

        # Listen for any CAN traffic for a few seconds
        print("Listening for CAN traffic for 5 seconds...")
        start_time = time.time()
        while time.time() - start_time < 5:
            if ser.in_waiting:
                data = ser.read(ser.in_waiting).decode('ascii', errors='ignore')
                if data.strip():
                    print(f"CAN Data: '{data.strip()}'")
            time.sleep(0.1)

        ser.close()
        print("Test completed")

    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)

if __name__ == "__main__":
    test_arduino()
