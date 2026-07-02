$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$sourceRoot = Join-Path $repoRoot "Sources\src"

$projectFiles = Get-ChildItem -Path $sourceRoot -Recurse -File -Include *.vcxproj,*.props,*.rc
$matches = @()

foreach ($file in $projectFiles) {
    $content = Get-Content -Path $file.FullName -Encoding Default
    for ($lineIndex = 0; $lineIndex -lt $content.Count; ++$lineIndex) {
        $line = $content[$lineIndex]
        if ($line -match "sdk\\\\STINGRAY|sdk/STINGRAY|STINGRAY\\\\Include|STINGRAY\\\\lib|secres\\.(h|rc)") {
            $matches += [pscustomobject]@{
                Path = $file.FullName
                LineNumber = $lineIndex + 1
                Line = $line
            }
        }
    }
}

if ($matches.Count -gt 0) {
    Write-Host "FAIL: Stingray SDK project coupling remains:" -ForegroundColor Red
    foreach ($match in $matches) {
        $relativePath = Resolve-Path -Path $match.Path -Relative
        Write-Host ("  {0}:{1}: {2}" -f $relativePath, $match.LineNumber, $match.Line.Trim())
    }
    exit 1
}

Write-Host "PASS: no Stingray SDK include, library, or resource includes remain in project files."
