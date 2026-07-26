# Launches the staged x64 game with stderr captured, so [x64-diag] traces
# (and zig panic messages) land in a log file next to the game.
# Usage: .\tools\zig\run_x64_diag.ps1   (from the repo root)
$gameDir = Join-Path $PSScriptRoot "..\..\zig-out\Game\x64\Debug"
$log = Join-Path $gameDir "bk_stderr.log"
$p = Start-Process -FilePath (Join-Path $gameDir "Game.exe") -WorkingDirectory $gameDir -RedirectStandardError $log -PassThru
Write-Host "Game started (pid $($p.Id)); stderr -> $log"
