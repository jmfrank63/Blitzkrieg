$ErrorActionPreference = 'Stop'

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$openBackend = Join-Path $repoRoot 'Sources/src/SFX/AudioBackendOpen.cpp'
$source = Get-Content -Raw $openBackend
$failures = @()

foreach ($required in @('ParseWaveSample', 'RIFF', 'WAVE', 'fmt ', 'data', 'nSampleRate', 'nPcmBytes')) {
    if ($source -notmatch [regex]::Escape($required)) {
        $failures += "AudioBackendOpen.cpp is missing '$required'."
    }
}

if ($source -notmatch 'if\s*\(\s*!ParseWaveSample\(\s*pData,\s*nSize,\s*pSample\s*\)\s*\)[\s\S]*delete\s+pSample;[\s\S]*return\s+0;') {
    $failures += 'LoadSampleFromMemory must return 0 when WAV parsing fails.'
}

if ($source -notmatch 'pSample->nPcmBytes\s*>\s*0') {
    $failures += 'ParseWaveSample must reject WAV files with an empty data chunk.'
}

if ($source -notmatch 'pSample->pcmData\.empty\(\)') {
    $failures += 'ParseWaveSample must guard against indexing an empty PCM buffer.'
}

if ($source -match 'GetSampleRate\([^\)]*\)\s*\{[^}]*return 44100;') {
    $failures += 'GetSampleRate still returns a hardcoded 44100 value.'
}

if ($source -match 'GetSampleLength\([^\)]*\)\s*\{[^}]*->nSize') {
    $failures += 'GetSampleLength still returns the source buffer size instead of decoded PCM length.'
}

if ($failures.Count -gt 0) {
    Write-Host 'SFX open backend WAV ingestion is incomplete:'
    $failures | ForEach-Object { Write-Host "  $_" }
    exit 1
}

Write-Host 'SFX open backend parses WAV sample metadata.'
