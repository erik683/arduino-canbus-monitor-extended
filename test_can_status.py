#!/usr/bin/env python3
import serial
import time

def test_can_status():
    port = '/dev/ttyACM1'
    baud = 115200

    try:
        ser = serial.Serial(port, baud, timeout=1)
        print(f"Connected to {port} at {baud} baud")

        # Wait for Arduino to reset
        time.sleep(2)

        # Test version command (should work)
        print("1. Testing version command...")
        ser.write(b'V\r')
        time.sleep(0.1)
        response = ser.read(10).decode('ascii', errors='ignore').strip()
        print(f"   Response: '{response}'")

        # Test CAN status before opening
        print("2. Testing CAN status before opening (F)...")
        ser.write(b'F\r')
        time.sleep(0.1)
        response = ser.read(10).decode('ascii', errors='ignore').strip()
        print(f"   Response: '{response}'")

        # Test opening CAN bus
        print("3. Testing CAN bus open (O)...")
        ser.write(b'O\r')
        time.sleep(0.1)
        response = ser.read(10).decode('ascii', errors='ignore').strip()
        print(f"   Response: '{response}'")

        # Test CAN status after opening
        print("4. Testing CAN status after opening (F)...")
        ser.write(b'F\r')
        time.sleep(0.1)
        response = ser.read(10).decode('ascii', errors='ignore').strip()
        print(f"   Response: '{response}'")

        print("5. Testing extended frame rejection enable (%EXT1)...")
        ser.write(b'%EXT1\r')
        time.sleep(0.1)
        response = ser.read(30).decode('ascii', errors='ignore').strip()
        print(f"   Response: '{response}'")

        # Test runtime stats
        print("6. Testing runtime stats (i)...")
        ser.write(b'i\r')
        time.sleep(0.1)
        response = ser.read(50).decode('ascii', errors='ignore').strip()
        print(f"   Response: '{response}'")

        ser.close()
        print("Test completed")

    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    test_can_status()
