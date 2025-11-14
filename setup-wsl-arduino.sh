#!/bin/bash

echo "Setting up WSL for Arduino development..."
echo

# Check if running as root for group modifications
if [[ $EUID -eq 0 ]]; then
    echo "ERROR: Don't run this script as root. It will ask for sudo when needed."
    exit 1
fi

# Add user to dialout group for serial access
echo "Adding user to dialout group for serial device access..."
sudo usermod -a -G dialout $USER
echo "✓ Added $USER to dialout group"
echo "NOTE: You may need to log out and back in for group changes to take effect"
echo "      Or run 'newgrp dialout' in your current session"
echo

# Install USB/IP tools if not already installed
echo "Checking USB/IP tools..."
if ! command -v usbip &> /dev/null; then
    echo "Installing USB/IP tools..."
    sudo apt update
    sudo apt install -y linux-tools-generic hwdata
    sudo update-alternatives --install /usr/local/bin/usbip usbip /usr/lib/linux-tools/*/usbip 20
    echo "✓ USB/IP tools installed"
else
    echo "✓ USB/IP tools already installed"
fi

# Ensure PlatformIO is in PATH
echo "Checking PlatformIO PATH..."
if ! command -v pio &> /dev/null; then
    echo "Adding PlatformIO to PATH..."
    export PATH="$HOME/.platformio/penv/bin:$PATH"
    if ! grep -q "platformio/penv/bin" ~/.bashrc; then
        echo 'export PATH="$HOME/.platformio/penv/bin:$PATH"' >> ~/.bashrc
        echo "✓ Added PlatformIO to ~/.bashrc"
    fi
    echo "✓ PlatformIO PATH configured"
else
    echo "✓ PlatformIO is in PATH"
fi

echo
echo "Setup complete! To use Arduino:"
echo "1. In Windows (as Admin): Run attach-arduino.bat"
echo "2. In WSL: Run 'pio run --target upload' to upload firmware"
echo "3. In WSL: Run 'pio device monitor' to view serial output"
echo
echo "If you get permission errors, run 'newgrp dialout' or log out/in to WSL"
