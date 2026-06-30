$ErrorActionPreference = 'Stop'

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$sfxRoot = Join-Path $repoRoot 'Sources/src/SFX'
$allowed = @(
    'AudioBackend.cpp',
    'AudioFmodCompat.h',
    'SFX.vcxproj'
)

$failures = @()
$files = Get-ChildItem -Path $sfxRoot -File -Include *.cpp,*.h,*.vcxproj -Recurse

foreach ($file in $files) {
    if ($allowed -contains $file.Name) {
        continue
    }

    $matches = Select-String -Path $file.FullName -Pattern '(FMOD|FSOUND|fmod)' -AllMatches
    foreach ($match in $matches) {
        $relative = $file.FullName.Substring($repoRoot.Path.Length + 1).Replace('\', '/')
        $failures += ('{0}:{1}: {2}' -f $relative, $match.LineNumber, $match.Line.Trim())
    }
}

if ($failures.Count -gt 0) {
    Write-Host 'SFX upper layer still exposes FMOD naming or symbols:'
    $failures | ForEach-Object { Write-Host "  $_" }
    exit 1
}

Write-Host 'SFX upper layer is free of FMOD naming and symbols.'
