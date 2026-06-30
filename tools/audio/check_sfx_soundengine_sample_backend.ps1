$ErrorActionPreference = 'Stop'

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$source = Join-Path $repoRoot 'Sources/src/SFX/SoundEngine.cpp'
$lines = Get-Content $source

$forbidden = '(FSOUND_PlaySoundEx|FSOUND_3D_SetAttributes|FSOUND_SetVolume|FSOUND_SetPan|FSOUND_GetError|FSOUND_GetChannelsPlaying|FSOUND_SetPaused|FSOUND_StopSound|FSOUND_IsPlaying|FSOUND_SetCurrentPosition|FSOUND_GetCurrentPosition)'
$regions = @(
    @{ Start = 'class CPlayVisitor'; End = 'static CPlayVisitor thePlayVisitor;' },
    @{ Start = 'void CSoundEngine::Update( interface ICamera *pCamera )'; End = 'void CSoundEngine::CloseStreaming()' },
    @{ Start = 'bool CSoundEngine::Pause( bool bPause )'; End = 'void CSoundEngine::ClearChannels()' },
    @{ Start = 'void CSoundEngine::ClearChannels()'; End = 'int CSoundEngine::PlaySample' },
    @{ Start = 'int CSoundEngine::PlaySample'; End = 'void CSoundEngine::UpdateSample' },
    @{ Start = 'void CSoundEngine::UpdateSample'; End = 'void CSoundEngine::StopSample' },
    @{ Start = 'void CSoundEngine::StopChannel'; End = 'unsigned int CSoundEngine::GetCurrentPosition' },
    @{ Start = 'unsigned int CSoundEngine::GetCurrentPosition'; End = 'void CSoundEngine::SetCurrentPosition' },
    @{ Start = 'void CSoundEngine::SetCurrentPosition'; End = 'void CSoundEngine::ReEnableSounds' },
    @{ Start = 'void CSoundEngine::ReEnableSounds'; End = 'void CSoundEngine::NotifyMelodyFinished' }
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
    Write-Host 'SoundEngine.cpp sample/channel regions still call FMOD directly:'
    $failures | ForEach-Object { Write-Host "  $_" }
    exit 1
}

Write-Host 'SoundEngine.cpp sample/channel regions use the private audio backend boundary.'
