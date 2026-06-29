$ErrorActionPreference = 'Stop'

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$headers = @(
    'Sources/src/SFX/CommonStructs.h',
    'Sources/src/SFX/SFX.h',
    'Sources/src/SFX/SampleSounds.h',
    'Sources/src/SFX/SoundEngine.h',
    'Sources/src/SFX/SoundManager.h',
    'Sources/src/SFX/SoundObjectFactory.h',
    'Sources/src/SFX/Specific.h',
    'Sources/src/SFX/StreamFadeOff.h',
    'Sources/src/SFX/StreamingSound.h'
)

$forbidden = '(FMOD|FSOUND|fmod\.h)'
$failures = @()

foreach ($header in $headers) {
    $path = Join-Path $repoRoot $header
    if (-not (Test-Path $path)) {
        $failures += "${header}: missing header"
        continue
    }

    $matches = Select-String -Path $path -Pattern $forbidden -AllMatches
    foreach ($match in $matches) {
        $failures += ('{0}:{1}: {2}' -f $header, $match.LineNumber, $match.Line.Trim())
    }
}

if ($failures.Count -gt 0) {
    Write-Host 'SFX public headers still expose FMOD symbols:'
    $failures | ForEach-Object { Write-Host "  $_" }
    exit 1
}

Write-Host 'SFX public headers are free of FMOD symbols.'
