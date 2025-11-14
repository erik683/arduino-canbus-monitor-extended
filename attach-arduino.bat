@echo off
echo Attaching Arduino Uno to WSL...
echo Hardware ID: USB\VID_2341^&PID_0043\34331323735351C0F031
echo.

REM Check if running as administrator
net session >nul 2>&1
if %errorLevel% == 0 (
    echo Running as administrator - good!
) else (
    echo Please run this batch file as Administrator!
    echo Right-click the file and select "Run as administrator"
    pause
    exit /b 1
)

echo Listing USB devices...
usbipd list

echo.
echo Attaching Arduino Uno (BUSID will be auto-detected based on VID:PID 2341:0043)...
echo If this fails, you may need to find the correct BUSID from the list above
echo and modify this script to use: usbipd attach --busid YOUR_BUSID --wsl

REM Try to attach the Arduino Uno based on VID:PID
usbipd attach --hardware-id "USB\VID_2341&PID_0043\34331323735351C0F031" --wsl

if %errorLevel% == 0 (
    echo.
    echo SUCCESS: Arduino should now be available in WSL as /dev/ttyACM0
    echo You can verify in WSL with: ls -l /dev/ttyACM*
) else (
    echo.
    echo FAILED: Could not attach device. Try running 'usbipd list' to see available devices
    echo and manually attach with: usbipd attach --busid BUSID --wsl
)

echo.
pause
