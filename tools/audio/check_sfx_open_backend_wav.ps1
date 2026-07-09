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

if ($source -notmatch 'DecodeSampleWithMiniAudio') {
    $failures += 'LoadSampleFromMemory must fall back to miniaudio decoding for compressed WAV samples.'
}

if ($source -notmatch 'ma_decoder_init_memory\(\s*pData,\s*static_cast<size_t>\(\s*nSize\s*\)') {
    $failures += 'Compressed sample fallback must decode from the in-memory sample buffer.'
}

if ($source -notmatch 'ma_format_s16') {
    $failures += 'Compressed sample fallback must produce a stable PCM sample format.'
}

$buttonSelect = Join-Path $repoRoot 'Data/Sounds/Buttons/select.wav'
$buttonOk = Join-Path $repoRoot 'Data/Sounds/Buttons/ok.wav'
foreach ($buttonSound in @($buttonSelect, $buttonOk)) {
    $bytes = [System.IO.File]::ReadAllBytes($buttonSound)
    if ($bytes.Length -lt 24 -or
        [System.Text.Encoding]::ASCII.GetString($bytes, 0, 4) -ne 'RIFF' -or
        [System.Text.Encoding]::ASCII.GetString($bytes, 8, 4) -ne 'WAVE') {
        $failures += "$buttonSound is not a WAV file."
        continue
    }

    $fmtOffset = -1
    for ($i = 12; $i -le $bytes.Length - 8; ) {
        $chunkName = [System.Text.Encoding]::ASCII.GetString($bytes, $i, 4)
        $chunkSize = [System.BitConverter]::ToUInt32($bytes, $i + 4)
        if ($chunkName -eq 'fmt ') {
            $fmtOffset = $i + 8
            break
        }
        $i += 8 + [int]$chunkSize + ([int]$chunkSize -band 1)
    }

    if ($fmtOffset -lt 0) {
        $failures += "$buttonSound is missing its fmt chunk."
        continue
    }

    $audioFormat = [System.BitConverter]::ToUInt16($bytes, $fmtOffset)
    if ($audioFormat -eq 1) {
        $failures += "$buttonSound is plain PCM; this guard expects the menu click assets to exercise compressed WAV fallback."
    }
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
