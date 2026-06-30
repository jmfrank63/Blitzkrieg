$ErrorActionPreference = 'Stop'

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$backendWrapper = Join-Path $repoRoot 'Sources/src/SFX/AudioBackend.cpp'
$backendImpl = Join-Path $repoRoot 'Sources/src/SFX/AudioBackendFmod.cpp'
$backendImplHeader = Join-Path $repoRoot 'Sources/src/SFX/AudioBackendImpl.h'

$failures = @()

if (-not (Test-Path $backendImpl)) {
    $failures += 'Sources/src/SFX/AudioBackendFmod.cpp is missing.'
}

if (-not (Test-Path $backendImplHeader)) {
    $failures += 'Sources/src/SFX/AudioBackendImpl.h is missing.'
}

$matches = Select-String -Path $backendWrapper -Pattern '(FMOD|FSOUND|fmod|AudioFmodCompat)' -AllMatches
foreach ($match in $matches) {
    $relative = $match.Path.Substring($repoRoot.Path.Length + 1).Replace('\', '/')
    $failures += ('{0}:{1}: {2}' -f $relative, $match.LineNumber, $match.Line.Trim())
}

if ($failures.Count -gt 0) {
    Write-Host 'SFX backend scaffold is incomplete:'
    $failures | ForEach-Object { Write-Host "  $_" }
    exit 1
}

Write-Host 'SFX backend scaffold is in place.'
