$ErrorActionPreference = 'Stop'

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$openBackend = Join-Path $repoRoot 'Sources/src/SFX/AudioBackendOpen.cpp'
$source = Get-Content -Raw $openBackend
$failures = @()

foreach ($required in @('ma_decoder_init_memory', 'ma_sound_init_from_data_source', 'ma_sound_set_end_callback', 'OpenStreamEndCallback', 'pStream', 'PlayStream')) {
    if ($source -notmatch [regex]::Escape($required)) {
        $failures += "AudioBackendOpen.cpp is missing '$required'."
    }
}

foreach ($required in @('STB_VORBIS_HEADER_ONLY', 'stb_vorbis.c')) {
    if ($source -notmatch [regex]::Escape($required)) {
        $failures += "AudioBackendOpen.cpp is missing Vorbis decoder integration marker '$required'."
    }
}

foreach ($required in @('AudioBackendXiphVorbis.h', 'OpenXiphVorbisStreamMemory', 'ReadXiphVorbisStream', 'SXiphStreamDataSource', 'ma_data_source_init', 'bUseXiphDecoder', 'TraceOpenStream( "opened xiph"')) {
    if ($source -notmatch [regex]::Escape($required)) {
        $failures += "AudioBackendOpen.cpp is missing Xiph Vorbis fallback marker '$required'."
    }
}

$xiphWrapper = Join-Path $repoRoot 'Sources/src/SFX/AudioBackendXiphVorbis.c'
if (-not (Test-Path $xiphWrapper)) {
    $failures += 'AudioBackendXiphVorbis.c is missing.'
}

$sfxProject = Get-Content -Raw (Join-Path $repoRoot 'Sources/src/SFX/SFX.vcxproj')
foreach ($required in @('ogg-1.3.5', 'vorbis-1.3.7', 'AudioBackendXiphVorbis.c', 'floor0.c', 'vorbisfile.c')) {
    if ($sfxProject -notmatch [regex]::Escape($required)) {
        $failures += "SFX.vcxproj is missing Xiph project marker '$required'."
    }
}

if ($source -match 'ma_sound_init_from_file\([^\r\n]+pOpenStream->szFileName\.c_str\(\)') {
    $failures += 'PlayStream still depends directly on the stream file path instead of storage-backed stream data.'
}

foreach ($required in @('GetSingleton<IDataStorage>()', 'OpenStream( pOpenStream->szFileName.c_str()', 'pOpenStream->encodedData')) {
    if ($source -notmatch [regex]::Escape($required)) {
        $failures += "AudioBackendOpen.cpp stream loading is missing '$required'."
    }
}

foreach ($required in @('TraceOpenStream', 'Open audio stream %s', 'TraceOpenStream( "opened"', 'TraceOpenStream( "decode failed"', 'TraceOpenStream( "started"')) {
    if ($source -notmatch [regex]::Escape($required)) {
        $failures += "AudioBackendOpen.cpp stream diagnostics are missing '$required'."
    }
}

if ($failures.Count -gt 0) {
    Write-Host 'SFX open backend stream support is incomplete:'
    $failures | ForEach-Object { Write-Host "  $_" }
    exit 1
}

Write-Host 'SFX open backend stream support is in place.'
