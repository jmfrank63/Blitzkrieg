$ErrorActionPreference = 'Stop'

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$source = Join-Path $repoRoot 'Sources/src/SFX/SoundEngine.cpp'
$lines = Get-Content $source

$forbidden = '(AudioFmodCompat\.h|FMOD_VERSION|FSOUND_GetVersion|FSOUND_SetOutput|FSOUND_GetNumDrivers|FSOUND_GetDriverName|FSOUND_GetDriverCaps|FSOUND_CAPS|FSOUND_GetOutputHandle|FSOUND_SetDriver|FSOUND_OUTPUT|FSOUND_SetHWND|FSOUND_Init|FSOUND_GetMixer|FSOUND_MIXER|FSOUND_Close|FSOUND_3D_SetDistanceFactor|FSOUND_3D_SetRolloffFactor)'
$regions = @(
    @{ Start = '#include "AudioFmodCompat.h"'; End = '#include "SampleSounds.h"' },
    @{ Start = 'bool CSoundEngine::SearchDevices()'; End = 'bool CSoundEngine::IsInitialized()' },
    @{ Start = 'IRefCount* CSoundEngine::QI'; End = 'bool CSoundEngine::Init' },
    @{ Start = 'bool CSoundEngine::Init'; End = 'void CSoundEngine::Done()' },
    @{ Start = 'void CSoundEngine::Done()'; End = 'void CSoundEngine::SetDistanceFactor' },
    @{ Start = 'void CSoundEngine::SetDistanceFactor'; End = 'void CSoundEngine::SetRolloffFactor' },
    @{ Start = 'void CSoundEngine::SetRolloffFactor'; End = 'void CSoundEngine::Update' }
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

    if ($startIndex -eq -1) {
        continue
    }
    if ($endIndex -eq -1) {
        $endIndex = [Math]::Min($startIndex + 1, $lines.Count)
    }

    for ($i = $startIndex; $i -lt $endIndex; ++$i) {
        if ($lines[$i] -match $forbidden) {
            $failures += ('Sources/src/SFX/SoundEngine.cpp:{0}: {1}' -f ($i + 1), $lines[$i].Trim())
        }
    }
}

if ($failures.Count -gt 0) {
    Write-Host 'SoundEngine.cpp device/control regions still call FMOD directly:'
    $failures | ForEach-Object { Write-Host "  $_" }
    exit 1
}

Write-Host 'SoundEngine.cpp device/control regions use the private audio backend boundary.'
