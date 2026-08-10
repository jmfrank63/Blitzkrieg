param(
    [string]$InstallDir = "zig-out/game/windows/x86_64/debug",
    [string]$CommandFile = "tools/debug/cdb-exit3.txt",
    [string]$OutputFile = "tools/debug/cdb-vscode-capture.txt"
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "../..")
Set-Location $repoRoot

$cdbPath = Join-Path ${env:ProgramFiles(x86)} "Windows Kits\10\Debuggers\x86\cdb.exe"
if (-not (Test-Path $cdbPath)) {
    throw "Missing CDB debugger: $cdbPath"
}

$cmdPath = Join-Path $repoRoot $CommandFile
if (-not (Test-Path $cmdPath)) {
    throw "Missing CDB command file: $cmdPath"
}

$installPath = Join-Path $repoRoot $InstallDir
$exePath = Join-Path $installPath "Game.exe"
if (-not (Test-Path $exePath)) {
    throw "Missing staged executable: $exePath"
}

$outPath = Join-Path $repoRoot $OutputFile
Remove-Item -LiteralPath $outPath -Force -ErrorAction SilentlyContinue

Write-Host "CDB: $cdbPath"
Write-Host "CMD: $cmdPath"
Write-Host "EXE: $exePath"
Write-Host "LOG: $outPath"

& $cdbPath -lines -logo $outPath -G -g -cf $cmdPath $exePath
$cdbExit = $LASTEXITCODE
Write-Host "CDB_EXIT=$cdbExit"

if (-not (Test-Path $outPath)) {
    throw "CDB did not produce output log: $outPath"
}

$patterns = @(
    "BREAKPOINT=",
    "EXITCODE_HEX=",
    "Assertion failed",
    "invariant fail",
    "LinkObject.cpp",
    "Expression:",
    "Access violation",
    "c0000005"
)

Write-Host "\n--- CDB summary ---"
$hits = Select-String -Path $outPath -Pattern $patterns -SimpleMatch -ErrorAction SilentlyContinue
if ($hits) {
    $hits | ForEach-Object { Write-Host $_.Line }
} else {
    Get-Content -Path $outPath -Tail 80
}

exit $cdbExit
