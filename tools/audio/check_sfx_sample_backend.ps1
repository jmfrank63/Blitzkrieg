$ErrorActionPreference = 'Stop'

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$sampleSource = Join-Path $repoRoot 'Sources/src/SFX/SampleSounds.cpp'

$forbidden = '(AudioFmodCompat\.h|FMOD|FSOUND)'
$matches = Select-String -Path $sampleSource -Pattern $forbidden -AllMatches

if ($matches.Count -gt 0) {
    Write-Host 'SampleSounds.cpp still calls the FMOD compatibility layer directly:'
    foreach ($match in $matches) {
        Write-Host ('  Sources/src/SFX/SampleSounds.cpp:{0}: {1}' -f $match.LineNumber, $match.Line.Trim())
    }
    exit 1
}

Write-Host 'SampleSounds.cpp uses the private audio backend boundary.'
