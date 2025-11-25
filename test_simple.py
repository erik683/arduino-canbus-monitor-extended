#!/usr/bin/env python3
import serial
import time

def test_simple():
    port = '/dev/ttyACM2'
    baud = 115200

    try:
        ser = serial.Serial(port, baud, timeout=1)
        print(f"Connected to {port} at {baud} baud")

        # Wait for Arduino to reset
        time.sleep(2)
        ser.reset_input_buffer()

        # Test just the command characters
        test_commands = [
            ("Version", b'V\r'),
            ("Hash command", b'#EXT\r'),
            ("Percent command", b'%EXT0\r'),
        ]

        for name, cmd in test_commands:
            print(f"\nTesting {name}: {cmd}")
            ser.write(cmd)
            time.sleep(0.2)
            raw_response = ser.read(50)
            print(f"Raw response: {raw_response!r}")
            response = raw_response.decode('ascii', errors='ignore').strip()
            print(f"Decoded response: '{response}'")
            if not response:
                print("  (No response - command may not be recognized)")

        ser.close()

    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    test_simple()
