$ErrorActionPreference = 'Stop'

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$soundEngine = Join-Path $repoRoot 'Sources/src/SFX/SoundEngine.cpp'
$gameMain = Join-Path $repoRoot 'Sources/src/Game/main.cpp'
$engineSource = Get-Content -Raw $soundEngine
$mainSource = Get-Content -Raw $gameMain
$failures = @()

if ($engineSource -notmatch 'MasterVolumeToByte\([^\)]*\)[\s\S]*fDecibels\s*=\s*\(\s*fClampedVolume\s*-\s*1\.0f\s*\)\s*\*\s*60\.0f') {
    $failures += 'SoundEngine master volume conversion must use the 60 dB perceptual curve.'
}

if ($mainSource -match 'SetSFXMasterVolume\(\s*1\.0f\s*\)') {
    $failures += 'Game startup must not overwrite SFX master volume with a hardcoded full-volume value.'
}

if ($mainSource -notmatch 'SetSFXMasterVolume\(\s*GetGlobalVar\(\s*"Sound\.SFXMasterVolume"\s*,\s*1\.0f\s*\)\s*\)') {
    $failures += 'Game startup must apply Sound.SFXMasterVolume to SFX, matching stream master volume.'
}

if ($failures.Count -gt 0) {
    Write-Host 'SFX volume curve setup is incomplete:'
    $failures | ForEach-Object { Write-Host "  $_" }
    exit 1
}

Write-Host 'SFX volume uses the configured perceptual master curve.'
