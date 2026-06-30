$ErrorActionPreference = 'Stop'

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$openBackend = Join-Path $repoRoot 'Sources/src/SFX/AudioBackendOpen.cpp'
$project = Join-Path $repoRoot 'Sources/src/SFX/SFX.vcxproj'

$failures = @()

if (-not (Test-Path $openBackend)) {
    $failures += 'Sources/src/SFX/AudioBackendOpen.cpp is missing.'
} else {
    $matches = Select-String -Path $openBackend -Pattern '(FMOD|FSOUND|fmod|AudioFmodCompat)' -AllMatches
    foreach ($match in $matches) {
        $relative = $match.Path.Substring($repoRoot.Path.Length + 1).Replace('\', '/')
        $failures += ('{0}:{1}: {2}' -f $relative, $match.LineNumber, $match.Line.Trim())
    }
}

$projectText = Get-Content -Raw $project
if ($projectText -notmatch 'AudioBackendOpen\.cpp') {
    $failures += 'Sources/src/SFX/SFX.vcxproj does not compile AudioBackendOpen.cpp.'
}
if ($projectText -notmatch 'SFX_USE_OPEN_AUDIO_BACKEND') {
    $failures += 'Sources/src/SFX/SFX.vcxproj does not expose SFX_USE_OPEN_AUDIO_BACKEND selection.'
}

if ($failures.Count -gt 0) {
    Write-Host 'SFX open backend scaffold is incomplete:'
    $failures | ForEach-Object { Write-Host "  $_" }
    exit 1
}

Write-Host 'SFX open backend scaffold is in place.'
