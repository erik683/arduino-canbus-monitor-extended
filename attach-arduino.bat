@echo off
echo Attaching Arduino Uno to WSL...
echo Searching for Arduino Uno (VID:PID 2341:0043)
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
echo Searching for Arduino Uno (VID:PID 2341:0043)...

REM Search for Arduino Uno by VID:PID and extract BUSID
for /f "tokens=1" %%i in ('usbipd list ^| findstr "2341:0043"') do (
    set BUSID=%%i
    goto :found
)

REM If not found, show error
echo Arduino Uno not found. Available devices listed above.
echo Please make sure your Arduino is connected and recognized by Windows.
echo You can also manually attach with: usbipd attach --busid YOUR_BUSID --wsl
pause
exit /b 1

:found
echo Found Arduino at BUSID: %BUSID%
echo Attaching to WSL...

REM Attach using the detected BUSID
usbipd attach --busid %BUSID% --wsl

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
