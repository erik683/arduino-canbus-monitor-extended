# Update existing scheduled task to use new usbipd syntax (no distribution parameter)
# Run this in PowerShell as Administrator

$BusId = "1-8"
$taskName = "WSL-Attach-USB-$BusId"

$task = Get-ScheduledTask -TaskName $taskName -ErrorAction SilentlyContinue
if (-not $task) {
    Write-Host "Task not found: $taskName" -ForegroundColor Yellow
    exit 1
}

Write-Host "Current task action:" -ForegroundColor Cyan
$currentAction = $task.Actions[0]
Write-Host "  Execute: $($currentAction.Execute)"
Write-Host "  Arguments: $($currentAction.Arguments)"

# Update to new syntax
$newAction = New-ScheduledTaskAction -Execute "usbipd.exe" -Argument "attach --busid $BusId --wsl"
Set-ScheduledTask -TaskName $taskName -Action $newAction

Write-Host "`nTask updated successfully!" -ForegroundColor Green
Write-Host "New action: usbipd.exe attach --busid $BusId --wsl" -ForegroundColor Cyan

