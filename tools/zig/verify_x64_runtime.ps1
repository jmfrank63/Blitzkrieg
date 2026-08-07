[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$InstallDir
)

$ErrorActionPreference = 'Stop'
$install = (Resolve-Path -LiteralPath $InstallDir).Path
$dumpbin = (Get-Command dumpbin.exe -ErrorAction Stop).Source
$required = @('StreamIO.dll', 'StreamIOOptionsAbi.dll', 'Anim.dll', 'GFXGPU.dll', 'Image.dll', 'Input.dll', 'Net.dll', 'SFX.dll', 'UI.dll', 'Scene.dll', 'AILogic.dll', 'GameTT.dll')
$log = Join-Path $install 'x64-runtime-validation.log'

Remove-Item -LiteralPath $log -Force -ErrorAction SilentlyContinue
foreach ($name in $required) {
    $path = Join-Path $install $name
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing staged runtime module: $name" }
    $headers = & $dumpbin /headers $path 2>&1 | Out-String
    Add-Content -LiteralPath $log -Value $headers
    if ($headers -notmatch 'machine \(x64\)' -or $headers -match 'machine \(x86\)') { throw "Non-x64 staged runtime module: $name" }
}

foreach ($binary in Get-ChildItem -LiteralPath $install -File | Where-Object { $_.Extension -in @('.dll', '.exe') }) {
    $headers = & $dumpbin /headers $binary.FullName 2>&1 | Out-String
    Add-Content -LiteralPath $log -Value "`n=== $($binary.Name) ===`n$headers"
    if ($headers -notmatch 'machine \(x64\)' -or $headers -match 'machine \(x86\)') {
        throw "Non-x64 binary in staged runtime: $($binary.Name)"
    }
}

$game = Join-Path $install 'Game.exe'
if (-not (Test-Path -LiteralPath $game -PathType Leaf)) { throw 'Missing staged Game.exe' }

# Run under the x64 debugger so OutputDebugString checkpoints and any fault
# stack are captured in the validation artifact.
$stdoutFile = Join-Path $install 'game-stdout.log'
$stderrFile = Join-Path $install 'game-stderr.log'
$cdbLog = Join-Path $install 'x64-cdb.log'
Remove-Item -LiteralPath $stdoutFile, $stderrFile -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $cdbLog -Force -ErrorAction SilentlyContinue
$cdb = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\Debuggers\x64\cdb.exe'
if (-not (Test-Path -LiteralPath $cdb -PathType Leaf)) { throw "Missing x64 CDB: $cdb" }
$cdbArgs = "-o -g -G -logo x64-cdb.log -c `"sxe av; g; .lastevent; kv; q`" .\Game.exe -x64-startup-smoke"
$process = Start-Process -FilePath $cdb -ArgumentList $cdbArgs -WorkingDirectory $install -PassThru -RedirectStandardOutput $stdoutFile -RedirectStandardError $stderrFile
$null = $process.Handle
if (-not $process.WaitForExit(90000)) {
    try { $process.Kill() } catch {}
    $process.WaitForExit()
    Add-Content -LiteralPath $log -Value 'Game exceeded the 90-second startup smoke timeout.'
    throw "Game.exe did not reach the scripted smoke exit within 90 seconds; see $log"
}
$process.Refresh()
$exitCode = $process.ExitCode
if ($null -eq $exitCode) { $exitCode = -1 }
Add-Content -LiteralPath $log -Value "Game exit code: $exitCode (0x$($exitCode.ToString('X8')))"
if (Test-Path -LiteralPath $cdbLog) {
    $debuggerOutput = Get-Content -LiteralPath $cdbLog -Raw
    Add-Content -LiteralPath $log -Value "`n--- CDB ---`n$debuggerOutput"
    if ($debuggerOutput -match '(?i)c0000005|access violation') { throw "Game.exe raised an access violation; see $log" }
    if ($debuggerOutput -notmatch 'BK_STARTUP: C6 main menu smoke checkpoint passed') { throw "Game.exe did not report the C6 main-menu checkpoint; see $log" }
}
else { throw "CDB did not produce $cdbLog" }
if (Test-Path (Join-Path $install 'game-stdout.log')) {
    $stdout = Get-Content (Join-Path $install 'game-stdout.log') -Raw -ErrorAction SilentlyContinue
    if ($stdout) { Add-Content -LiteralPath $log -Value "`n--- stdout ---`n$stdout" }
}
if (Test-Path (Join-Path $install 'game-stderr.log')) {
    $stderr = Get-Content (Join-Path $install 'game-stderr.log') -Raw -ErrorAction SilentlyContinue
    if ($stderr) { Add-Content -LiteralPath $log -Value "`n--- stderr ---`n$stderr" }
}
# Check for log.txt which the game writes via the console buffer
$logTxt = Join-Path $install 'log.txt'
if (Test-Path $logTxt) {
    $gameLog = Get-Content $logTxt -Raw -ErrorAction SilentlyContinue
    if ($gameLog) { Add-Content -LiteralPath $log -Value "`n--- log.txt ---`n$gameLog" }
}
# Check for error.txt
$errorTxt = Join-Path $install 'error.txt'
if (Test-Path $errorTxt) {
    $errorLog = Get-Content $errorTxt -Raw -ErrorAction SilentlyContinue
    if ($errorLog) { Add-Content -LiteralPath $log -Value "`n--- error.txt ---`n$errorLog" }
}

# 0xC0000005 = access violation, 0xDEAD = deliberate game exit
if ($exitCode -eq -1073741819) { throw "Game.exe crashed with access violation (0xC0000005); see $log" }
if ($exitCode -eq 0xDEAD -or $exitCode -eq 57005) { throw "Game.exe exited with 0xDEAD (deliberate failure); see $log" }
if ($exitCode -ne 0) { throw "Game.exe exited with $exitCode (0x$($exitCode.ToString('X8'))); see $log" }
