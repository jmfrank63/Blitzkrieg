$ErrorActionPreference = 'Stop'

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$openBackend = Join-Path $repoRoot 'Sources/src/SFX/AudioBackendOpen.cpp'
$source = Get-Content -Raw $openBackend
$failures = @()

if ($source -notmatch 'FreeSample\([^\)]*\)[\s\S]*g_channels\[i\]\.pSample\s*==\s*pOpenSample[\s\S]*ResetChannel\(\s*i\s*\)') {
    $failures += 'FreeSample must stop channels that are still reading from the sample PCM buffer.'
}

if ($source -notmatch 'CloseDevice\(\)[\s\S]*ma_engine_stop\(\s*&g_engine\s*\)[\s\S]*for\s*\(\s*int\s+i\s*=\s*0;\s*i\s*<\s*cMaxOpenChannels') {
    $failures += 'CloseDevice must stop the miniaudio engine before uninitializing channel data sources.'
}

if ($source -notmatch 'ResetChannel\([^\)]*\)[\s\S]*ma_sound_stop\(\s*&g_channels\[nChannel\]\.sound\s*\)[\s\S]*ma_sound_uninit') {
    $failures += 'ResetChannel must stop a sound before uninitializing it.'
}

if ($failures.Count -gt 0) {
    Write-Host 'SFX open backend lifetime guards are incomplete:'
    $failures | ForEach-Object { Write-Host "  $_" }
    exit 1
}

Write-Host 'SFX open backend stops sounds before freeing sample buffers.'
