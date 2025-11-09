# Quick script to create the scheduled task for Arduino at BUSID 1-8
# Run this in PowerShell as Administrator

$BusId = "1-8"
$taskName = "WSL-Attach-USB-$BusId"

# Remove existing task if it exists
$existingTask = Get-ScheduledTask -TaskName $taskName -ErrorAction SilentlyContinue
if ($existingTask) {
    Unregister-ScheduledTask -TaskName $taskName -Confirm:$false
}

# Create action (usbipd automatically selects WSL distribution and makes device available to all WSL 2 distros)
$action = New-ScheduledTaskAction -Execute "usbipd.exe" -Argument "attach --busid $BusId --wsl"

# Create trigger (at logon with 30 second delay)
$trigger = New-ScheduledTaskTrigger -AtLogOn
$trigger.Delay = "PT30S"

# Create settings
$settings = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries -StartWhenAvailable

# Create principal (run with highest privileges)
$principal = New-ScheduledTaskPrincipal -UserId "$env:USERDOMAIN\$env:USERNAME" -RunLevel Highest

# Register the task
Register-ScheduledTask -TaskName $taskName -Action $action -Trigger $trigger -Settings $settings -Principal $principal -Description "Automatically attach USB device $BusId (Arduino Uno) to WSL on login"

Write-Host "Task created: $taskName" -ForegroundColor Green
Write-Host "Your Arduino will be attached to WSL 30 seconds after you log in to Windows." -ForegroundColor Cyan

