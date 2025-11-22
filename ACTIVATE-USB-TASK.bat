@echo off
echo ========================================
echo WSL USB Auto-Attach Task Setup
echo ========================================
echo.
echo *** DEPRECATED: This script uses a hardcoded BUSID (1-8) ***
echo *** Use setup-windows-task.bat instead for better compatibility ***
echo.
echo This legacy script will create a Windows Task Scheduler task to
echo automatically attach an Arduino at BUSID 1-8 to WSL when you log in.
echo.
echo For a more flexible setup, use: setup-windows-task.bat
echo.
echo You need to run this as Administrator!
echo.
pause

REM Check for admin
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo.
    echo Requesting administrator privileges...
    powershell -Command "Start-Process '%~f0' -Verb RunAs"
    exit /b
)

echo.
echo Creating scheduled task...
echo.

powershell -ExecutionPolicy Bypass -Command "$BusId = '1-8'; $taskName = 'WSL-Attach-USB-' + $BusId; $existingTask = Get-ScheduledTask -TaskName $taskName -ErrorAction SilentlyContinue; if ($existingTask) { Unregister-ScheduledTask -TaskName $taskName -Confirm:$false; Write-Host 'Removed existing task' }; $action = New-ScheduledTaskAction -Execute 'usbipd.exe' -Argument \"attach --busid $BusId --wsl\"; $trigger = New-ScheduledTaskTrigger -AtLogOn; $trigger.Delay = 'PT30S'; $settings = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries -StartWhenAvailable; $principal = New-ScheduledTaskPrincipal -UserId \"$env:USERDOMAIN\$env:USERNAME\" -RunLevel Highest; Register-ScheduledTask -TaskName $taskName -Action $action -Trigger $trigger -Settings $settings -Principal $principal -Description \"Automatically attach USB device $BusId (Arduino Uno) to WSL on login\" | Out-Null; Write-Host 'SUCCESS: Task created!' -ForegroundColor Green; Write-Host \"Task name: $taskName\" -ForegroundColor Cyan; Write-Host 'Your Arduino will be attached to WSL 30 seconds after you log in to Windows.' -ForegroundColor Yellow"

echo.
echo ========================================
echo Done!
echo ========================================
echo.
pause

