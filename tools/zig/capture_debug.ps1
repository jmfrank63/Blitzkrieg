# Captures OutputDebugString messages from a process using a simple debugger
# Since we don't have CDB or DebugView, we use .NET to attach as a debugger
$ErrorActionPreference = 'Stop'
$installDir = $args[0]
Set-Location -LiteralPath $installDir

# Use a simple approach: run the game and capture Windows Event Log entries
# The crash should generate a Windows Error Reporting event
$game = Join-Path $installDir 'Game.exe'

# Run the game
$p = Start-Process -FilePath $game -WorkingDirectory $installDir -PassThru -Wait -NoNewWindow
$code = $p.ExitCode
Write-Host "ExitCode: $code (0x$($code.ToString('X8')))"

# Check for Windows Error Reporting entries
$wer = Get-WinEvent -FilterHashtable @{LogName='Application'; ProviderName='Windows Error Reporting'; StartTime=(Get-Date).AddMinutes(-5)} -ErrorAction SilentlyContinue | Select-Object -First 5
if ($wer) {
    foreach ($entry in $wer) {
        Write-Host "`n--- WER Event ---"
        Write-Host $entry.Message
    }
}

# Check for Application Error events
$appErrors = Get-WinEvent -FilterHashtable @{LogName='Application'; ProviderName='Application Error'; StartTime=(Get-Date).AddMinutes(-5)} -ErrorAction SilentlyContinue | Select-Object -First 5
if ($appErrors) {
    foreach ($entry in $appErrors) {
        Write-Host "`n--- Application Error Event ---"
        Write-Host $entry.Message
    }
}