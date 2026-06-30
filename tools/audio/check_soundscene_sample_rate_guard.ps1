$ErrorActionPreference = 'Stop'

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$soundScene = Join-Path $repoRoot 'Sources\src\Scene\SoundScene.cpp'
$text = Get-Content -LiteralPath $soundScene -Raw
$failures = @()

if ($text -notmatch 'pszSoundPath\[0\]\s*==\s*''0''\s*&&\s*pszSoundPath\[1\]\s*==\s*0') {
    $failures += 'SoundScene must treat sound path "0" as no sound.'
}

if ($text -notmatch 'const\s+unsigned\s+int\s+nSampleRate\s*=') {
    $failures += 'CSound constructor must cache sample rate before calculating timeToPlay.'
}

if ($text -notmatch 'timeToPlay\s*=\s*\(pSample\s*==\s*0\s*\|\|\s*nSampleRate\s*==\s*0\)\s*\?\s*0') {
    $failures += 'CSound constructor must avoid dividing by zero sample rate.'
}

if ($text -notmatch 'GetSamplesPassed\(\)[\s\S]*nSampleRate\s*==\s*0[\s\S]*return\s+0;') {
    $failures += 'GetSamplesPassed must avoid dividing by zero sample rate.'
}

if ($text -match '/\s*[^;\r\n]*GetSampleRate\s*\(') {
    $failures += 'SoundScene must not divide directly by GetSampleRate().'
}

if ($text -notmatch 'UnSubstitute\(\)[\s\S]*timeToPlay\s*=\s*\(pSample\s*==\s*0\s*\|\|\s*nSampleRate\s*==\s*0\)\s*\?\s*0') {
    $failures += 'UnSubstitute must avoid dividing by zero sample rate.'
}

if ($text -notmatch 'Substitute\([^\)]*\)[\s\S]*pSound\s*==\s*0\s*\?\s*0\s*:\s*pSound->GetSampleRate\(\)[\s\S]*timeToPlay\s*=\s*\(pSound\s*==\s*0\s*\|\|\s*nSampleRate\s*==\s*0\)\s*\?\s*0') {
    $failures += 'Substitute must avoid null sound and zero sample-rate calculations.'
}

if ($failures.Count -gt 0) {
    Write-Host 'SoundScene sample-rate guard is incomplete:'
    $failures | ForEach-Object { Write-Host "  $_" }
    exit 1
}

Write-Host 'SoundScene guards empty and failed sound samples.'
