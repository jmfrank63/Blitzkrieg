$ErrorActionPreference = 'Stop'

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$source = Join-Path $repoRoot 'Sources/src/SFX/SoundEngine.cpp'
$lines = Get-Content $source

$forbidden = '(FSOUND_Stream|FSOUND_STEREOPAN|FSOUND_SetVolume|FSOUND_SetPan|FSOUND_SetPaused|FSOUND_StopSound|FSOUND_STREAM|F_CALLBACKAPI)'
$regions = @(
    @{ Start = 'void CSoundEngine::CloseStreaming()'; End = 'bool CSoundEngine::PlayNextMelody()' },
    @{ Start = 'void CSoundEngine::SetStreamVolume'; End = 'void CSoundEngine::MapSound' },
    @{ Start = 'void CSoundEngine::SetStreamMasterVolume'; End = 'void CSoundEngine::PlayStream' },
    @{ Start = 'void CSoundEngine::PlayStream'; End = 'bool CSoundEngine::IsPaused()' },
    @{ Start = 'bool CSoundEngine::PauseStreaming'; End = 'bool CSoundEngine::Pause( bool bPause )' },
    @{ Start = 'void CSoundEngine::NotifyMelodyFinished'; End = 'bool CSoundEngine::IsStreamPlaying()const' }
)

$failures = @()

foreach ($region in $regions) {
    $startIndex = -1
    $endIndex = -1
    for ($i = 0; $i -lt $lines.Count; ++$i) {
        if ($startIndex -eq -1 -and $lines[$i].Contains($region.Start)) {
            $startIndex = $i
            continue
        }
        if ($startIndex -ne -1 -and $lines[$i].Contains($region.End)) {
            $endIndex = $i
            break
        }
    }

    if ($startIndex -eq -1 -or $endIndex -eq -1) {
        $failures += "region not found: $($region.Start)"
        continue
    }

    for ($i = $startIndex; $i -lt $endIndex; ++$i) {
        if ($lines[$i] -match $forbidden) {
            $failures += ('Sources/src/SFX/SoundEngine.cpp:{0}: {1}' -f ($i + 1), $lines[$i].Trim())
        }
    }
}

if ($failures.Count -gt 0) {
    Write-Host 'SoundEngine.cpp stream regions still call FMOD directly:'
    $failures | ForEach-Object { Write-Host "  $_" }
    exit 1
}

Write-Host 'SoundEngine.cpp stream regions use the private audio backend boundary.'
