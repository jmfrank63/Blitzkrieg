$ErrorActionPreference = 'Stop'

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$backend = Join-Path $repoRoot 'Sources\src\SFX\AudioBackendOpen.cpp'
$text = Get-Content -LiteralPath $backend -Raw

function Require-Pattern {
    param(
        [string]$Pattern,
        [string]$Message
    )

    if ($text -notmatch $Pattern) {
        throw $Message
    }
}

Require-Pattern 'float\s+g_fDistanceFactor\s*=' 'Open backend must keep a distance factor.'
Require-Pattern 'float\s+g_fRolloffFactor\s*=' 'Open backend must keep a rolloff factor.'
Require-Pattern 'float\s+fBaseVolume;' 'Open channels must remember base volume.'
Require-Pattern 'float\s+fDistanceVolume;' 'Open channels must remember distance attenuation.'
Require-Pattern 'float\s+fPan;' 'Open channels must remember pan.'
Require-Pattern 'void\s+ApplyChannelMix\s*\(' 'Open backend must apply channel volume and pan from one helper.'
Require-Pattern 'ma_sound_set_volume\s*\([^;]+fBaseVolume\s*\*\s*[^;]+fDistanceVolume' 'Open backend must combine base volume and distance attenuation.'
Require-Pattern 'ma_sound_set_pan\s*\([^;]+fPan' 'Open backend must apply stored pan.'
Require-Pattern 'SetDistanceFactor\s*\(\s*float\s+fFactor\s*\)[\s\S]*g_fDistanceFactor\s*=' 'SetDistanceFactor must update open backend state.'
Require-Pattern 'SetRolloffFactor\s*\(\s*float\s+fFactor\s*\)[\s\S]*g_fRolloffFactor\s*=' 'SetRolloffFactor must update open backend state.'
Require-Pattern 'SetChannel3DAttributes\s*\(\s*int\s+nChannel,\s*const\s+CVec3\s+&vPos\s*\)[\s\S]*fDistanceVolume\s*=' 'SetChannel3DAttributes must update distance attenuation.'
Require-Pattern 'SetChannel3DAttributes\s*\(\s*int\s+nChannel,\s*const\s+CVec3\s+&vPos\s*\)[\s\S]*fPan\s*=' 'SetChannel3DAttributes must update positional pan.'
Require-Pattern 'SetChannel3DAttributes\s*\(\s*int\s+nChannel,\s*const\s+CVec3\s+&vPos\s*\)[\s\S]*ApplyChannelMix\s*\(' 'SetChannel3DAttributes must apply the updated mix.'

Write-Host 'Open audio backend 3D mix guard passed.'
