$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot

Write-Host "=== Building Game (Release, x64) ===" -ForegroundColor Cyan
$output = & zig build install-game -Dtarget=x86_64-windows-msvc -Doptimize=ReleaseFast 2>&1
if ($LASTEXITCODE -ne 0) {
    Write-Host "BUILD FAILED" -ForegroundColor Red
    $output | Write-Host
    exit 1
}
Write-Host "Build succeeded." -ForegroundColor Green

Write-Host "=== Running Game ===" -ForegroundColor Cyan
& "$PSScriptRoot\zig-out\Game\x64\Release\Game.exe"
