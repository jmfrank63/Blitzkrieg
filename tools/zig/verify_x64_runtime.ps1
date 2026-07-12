[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$InstallDir
)

$ErrorActionPreference = 'Stop'
$install = (Resolve-Path -LiteralPath $InstallDir).Path
$dumpbin = (Get-Command dumpbin.exe -ErrorAction Stop).Source
$required = @('StreamIO.dll', 'Anim.dll', 'GFX.dll', 'Image.dll', 'Input.dll', 'Net.dll', 'SFX.dll', 'UI.dll', 'Scene.dll', 'AILogic.dll')
$log = Join-Path $install 'x64-runtime-validation.log'

Remove-Item -LiteralPath $log -Force -ErrorAction SilentlyContinue
foreach ($name in $required) {
    $path = Join-Path $install $name
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing staged runtime module: $name" }
    $headers = & $dumpbin /headers $path 2>&1 | Out-String
    Add-Content -LiteralPath $log -Value $headers
    if ($headers -notmatch 'machine \(x64\)' -or $headers -match 'machine \(x86\)') { throw "Non-x64 staged runtime module: $name" }
}

$game = Join-Path $install 'Game.exe'
if (-not (Test-Path -LiteralPath $game -PathType Leaf)) { throw 'Missing staged Game.exe' }
$process = Start-Process -FilePath $game -WorkingDirectory $install -PassThru
if (-not $process.WaitForExit(90000)) {
    $process.Kill()
    $process.WaitForExit()
    Add-Content -LiteralPath $log -Value 'Game exceeded the 90-second startup smoke timeout.'
    throw "Game.exe did not reach the scripted smoke exit within 90 seconds; see $log"
}
Add-Content -LiteralPath $log -Value "Game exit code: $($process.ExitCode)"
if ($process.ExitCode -ne 0) { throw "Game.exe exited with $($process.ExitCode); see $log" }
