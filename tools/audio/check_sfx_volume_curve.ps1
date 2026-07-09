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

$dataConsts = Get-Content -Raw (Join-Path $repoRoot 'Data/consts.xml')
$sourceConsts = Get-Content -Raw (Join-Path $repoRoot 'Sources/src/data/consts.xml')
foreach ($consts in @($dataConsts, $sourceConsts)) {
    if ($consts -notmatch 'StreamMasterVolume="1\.0"') {
        $failures += 'Default music stream master volume must be full scale.'
        break
    }
    if ($consts -notmatch 'VideoStreamMasterVolume="1\.0"') {
        $failures += 'Video stream master volume must have a full-scale default independent of music volume.'
        break
    }
}

$sfxHeader = Get-Content -Raw (Join-Path $repoRoot 'Sources/src/SFX/SFX.h')
$soundEngineHeader = Get-Content -Raw (Join-Path $repoRoot 'Sources/src/SFX/SoundEngine.h')
$openVideo = Get-Content -Raw (Join-Path $repoRoot 'Sources/src/Scene/OpenVideoPlayer.cpp')
if ($sfxHeader -notmatch 'PlayVideoStream' -or $soundEngineHeader -notmatch 'PlayVideoStream') {
    $failures += 'ISFX must expose a video stream playback path separate from music stream volume settings.'
}
if ($engineSource -notmatch 'PlayVideoStream\([^\)]*\)[\s\S]*Sound\.VideoStreamMasterVolume[\s\S]*SetChannelVolume') {
    $failures += 'SoundEngine::PlayVideoStream must apply Sound.VideoStreamMasterVolume to the active stream channel.'
}
if ($openVideo -notmatch 'PlayVideoStream\(') {
    $failures += 'OpenVideoPlayer must use the video stream playback path for extracted movie audio.'
}

if ($failures.Count -gt 0) {
    Write-Host 'SFX volume curve setup is incomplete:'
    $failures | ForEach-Object { Write-Host "  $_" }
    exit 1
}

Write-Host 'SFX volume uses the configured perceptual master curve.'
