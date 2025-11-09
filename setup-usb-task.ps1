# PowerShell script to set up Windows Task Scheduler for auto-attaching USB device to WSL
# Run this script as Administrator on Windows

param(
    [string]$BusId = ""
)

# Check if running as Administrator
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Host "ERROR: This script must be run as Administrator!" -ForegroundColor Red
    Write-Host "Right-click PowerShell and select 'Run as Administrator'" -ForegroundColor Yellow
    exit 1
}

# Check if usbipd is installed
try {
    $null = Get-Command usbipd -ErrorAction Stop
} catch {
    Write-Host "ERROR: usbipd-win is not installed!" -ForegroundColor Red
    Write-Host "Install it with: winget install --interactive --exact dorssel.usbipd-win" -ForegroundColor Yellow
    exit 1
}

# If BusId not provided, list devices and prompt
if ([string]::IsNullOrEmpty($BusId)) {
    Write-Host "`nAvailable USB devices:" -ForegroundColor Cyan
    Write-Host "======================" -ForegroundColor Cyan
    usbipd list
    
    Write-Host "`nPlease enter the BUSID of your Arduino/USB Serial device (e.g., 1-2):" -ForegroundColor Yellow
    $BusId = Read-Host "BUSID"
    
    if ([string]::IsNullOrEmpty($BusId)) {
        Write-Host "ERROR: BUSID cannot be empty!" -ForegroundColor Red
        exit 1
    }
}

# Create the task action command
# Note: usbipd automatically selects WSL distribution and makes device available to all WSL 2 distros
$actionCommand = "usbipd.exe"
$actionArguments = "attach --busid $BusId --wsl"

# Task name
$taskName = "WSL-Attach-USB-$BusId"

# Remove existing task if it exists
$existingTask = Get-ScheduledTask -TaskName $taskName -ErrorAction SilentlyContinue
if ($existingTask) {
    Write-Host "`nRemoving existing task: $taskName" -ForegroundColor Yellow
    Unregister-ScheduledTask -TaskName $taskName -Confirm:$false
}

# Create the scheduled task action
$action = New-ScheduledTaskAction -Execute $actionCommand -Argument $actionArguments

# Create trigger for WSL startup (when WSL service starts)
# We'll use a trigger that runs when the user logs on, with a delay
$trigger = New-ScheduledTaskTrigger -AtLogOn
$trigger.Delay = "PT30S"  # 30 second delay to ensure WSL is ready

# Create settings
$settings = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries -StartWhenAvailable -RunOnlyIfNetworkAvailable:$false

# Create principal (run with highest privileges)
$principal = New-ScheduledTaskPrincipal -UserId "$env:USERDOMAIN\$env:USERNAME" -RunLevel Highest

# Register the task
try {
    Register-ScheduledTask -TaskName $taskName -Action $action -Trigger $trigger -Settings $settings -Principal $principal -Description "Automatically attach USB device $BusId to WSL on login" | Out-Null
    Write-Host "`n✓ Task created successfully: $taskName" -ForegroundColor Green
    Write-Host "  The USB device will be attached to WSL 30 seconds after you log in to Windows." -ForegroundColor Cyan
    Write-Host "`nTo test immediately, run:" -ForegroundColor Yellow
    Write-Host "  usbipd attach --busid $BusId --wsl" -ForegroundColor White
    Write-Host "`nTo view the task:" -ForegroundColor Yellow
    Write-Host "  Get-ScheduledTask -TaskName '$taskName' | Format-List" -ForegroundColor White
    Write-Host "`nTo remove the task:" -ForegroundColor Yellow
    Write-Host "  Unregister-ScheduledTask -TaskName '$taskName' -Confirm:`$false" -ForegroundColor White
} catch {
    Write-Host "`nERROR: Failed to create task: $_" -ForegroundColor Red
    exit 1
}

