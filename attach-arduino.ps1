# PowerShell script to attach Arduino Uno to WSL
# This can be used with Windows Task Scheduler for automatic attachment

param(
    [string]$BusId = "",
    [switch]$Detach,
    [switch]$List
)

# Check if running as administrator
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Host "Please run this script as Administrator!" -ForegroundColor Red
    Write-Host "Right-click the PowerShell file and select 'Run as administrator'"
    exit 1
}

Write-Host "Arduino Uno USB Manager for WSL" -ForegroundColor Cyan
Write-Host "Hardware ID: USB\VID_2341&PID_0043\34331323735351C0F031" -ForegroundColor Gray
Write-Host ""

if ($List) {
    Write-Host "Listing USB devices..." -ForegroundColor Yellow
    usbipd list
    exit 0
}

if ($Detach) {
    Write-Host "Detaching Arduino from WSL..." -ForegroundColor Yellow
    if ($BusId) {
        usbipd detach --busid $BusId
    } else {
        # Try to find and detach Arduino
        $devices = usbipd list | Select-String "2341:0043"
        if ($devices) {
            $busId = ($devices -split '\s+')[0]
            usbipd detach --busid $busId
        } else {
            Write-Host "Could not find Arduino device to detach" -ForegroundColor Red
        }
    }
    exit 0
}

# Attach mode (default)
Write-Host "Attaching Arduino Uno to WSL..." -ForegroundColor Green

if ($BusId) {
    # Use provided BUSID
    Write-Host "Using provided BUSID: $BusId" -ForegroundColor Yellow
    usbipd attach --busid $BusId --wsl
} else {
    # Try to find Arduino by VID:PID
    Write-Host "Searching for Arduino Uno (VID:PID 2341:0043)..." -ForegroundColor Yellow
    $devices = usbipd list 2>&1 | Select-String "2341:0043"

    if ($devices) {
        $busId = ($devices -split '\s+')[0]
        Write-Host "Found Arduino at BUSID: $busId" -ForegroundColor Green
        usbipd attach --busid $busId --wsl
    } else {
        Write-Host "Arduino Uno not found. Available devices:" -ForegroundColor Yellow
        usbipd list
        Write-Host ""
        Write-Host "Please specify BUSID manually: .\attach-arduino.ps1 -BusId '1-2'" -ForegroundColor Cyan
        exit 1
    }
}

if ($LASTEXITCODE -eq 0) {
    Write-Host ""
    Write-Host "SUCCESS: Arduino should now be available in WSL as /dev/ttyACM0" -ForegroundColor Green
    Write-Host "Verify in WSL with: ls -l /dev/ttyACM*" -ForegroundColor Cyan
} else {
    Write-Host ""
    Write-Host "FAILED: Could not attach Arduino to WSL" -ForegroundColor Red
    Write-Host "Check that the device is connected and not already attached" -ForegroundColor Yellow
}
