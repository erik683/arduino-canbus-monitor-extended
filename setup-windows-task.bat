@echo off
echo Setting up Windows Task Scheduler for automatic Arduino attachment to WSL...
echo.

REM Check if running as administrator
net session >nul 2>&1
if %errorLevel% == 0 (
    echo Running as administrator - good!
) else (
    echo ERROR: Please run this batch file as Administrator!
    echo Right-click the file and select "Run as administrator"
    pause
    exit /b 1
)

echo This will create a scheduled task that automatically attaches your Arduino
echo to WSL when Windows starts up.
echo.
echo Task details:
echo - Name: Arduino WSL Attach
echo - Triggers: At system startup
echo - Action: Run attach-arduino.ps1 with admin privileges
echo.

set /p confirm="Do you want to continue? (y/n): "
if /i not "%confirm%"=="y" goto :cancel

echo.
echo Creating scheduled task...

REM Get the current directory (where this batch file is located)
set "SCRIPT_DIR=%~dp0"
set "PS_SCRIPT=%SCRIPT_DIR%attach-arduino.ps1"

REM Remove quotes from paths if they exist
set "SCRIPT_DIR=%SCRIPT_DIR:"=%"
set "PS_SCRIPT=%PS_SCRIPT:"=%"

echo Script directory: %SCRIPT_DIR%
echo PowerShell script: %PS_SCRIPT%
echo.

REM Create the scheduled task
schtasks /create /tn "Arduino WSL Attach" /tr "powershell.exe -ExecutionPolicy Bypass -File \"%PS_SCRIPT%\"" /sc onstart /rl highest /f

if %errorLevel% == 0 (
    echo.
    echo SUCCESS: Scheduled task created!
    echo The Arduino will now be automatically attached to WSL on Windows startup.
    echo.
    echo You can manage this task in Windows Task Scheduler (search for it in Start menu).
    echo Task name: "Arduino WSL Attach"
    echo.
    echo To test immediately, run attach-arduino.bat as Administrator.
) else (
    echo.
    echo ERROR: Failed to create scheduled task.
    echo You may need to create it manually in Task Scheduler.
)

echo.
pause
exit /b 0

:cancel
echo.
echo Setup cancelled.
echo.
pause
exit /b 0
