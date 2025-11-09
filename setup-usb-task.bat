@echo off
REM Batch file wrapper to run the PowerShell script as Administrator
REM This will prompt for elevation if not already running as admin

echo Setting up Windows Task Scheduler for USB device auto-attachment to WSL...
echo.

REM Check if running as admin, if not, restart with elevation
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo Requesting administrator privileges...
    powershell -Command "Start-Process '%~f0' -Verb RunAs"
    exit /b
)

REM Run the PowerShell script
powershell -ExecutionPolicy Bypass -File "%~dp0setup-usb-task.ps1" %*

pause

