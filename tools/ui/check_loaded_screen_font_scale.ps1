$ErrorActionPreference = 'Stop'

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$uiBasicHeader = Join-Path $repoRoot 'Sources/src/UI/UIBasic.h'
$uiBasicSource = Join-Path $repoRoot 'Sources/src/UI/UIBasic.cpp'
$uiScreenSource = Join-Path $repoRoot 'Sources/src/UI/UIScreen.cpp'

$header = Get-Content -Raw $uiBasicHeader
$basic = Get-Content -Raw $uiBasicSource
$screen = Get-Content -Raw $uiScreenSource
$failures = @()

if ($header -notmatch 'ApplyTextLayoutScale') {
    $failures += 'CSimpleWindow must expose a helper to reapply text scale after loading a saved screen.'
}

if ($basic -notmatch 'saver\.Add\(\s*35,\s*&vAppliedLayoutScale\s*\)') {
    $failures += 'CSimpleWindow must serialize vAppliedLayoutScale for future saves.'
}

if ($basic -notmatch 'void\s+CSimpleWindow::ApplyTextLayoutScale') {
    $failures += 'CSimpleWindow must implement text-only layout scale reapplication.'
}

if ($basic -notmatch 'void\s+CMultipleWindow::ApplyTextLayoutScale[\s\S]*pWindow->ApplyTextLayoutScale') {
    $failures += 'CMultipleWindow must propagate text scale reapplication to children.'
}

if ($screen -notmatch 'ApplyTextLayoutScale\(\s*vNewScale\s*\)') {
    $failures += 'CUIScreen::Reposition must reapply text scale for restored legacy screens.'
}

if ($failures.Count -gt 0) {
    Write-Host 'Loaded UI screen font scaling is incomplete:'
    $failures | ForEach-Object { Write-Host "  $_" }
    exit 1
}

Write-Host 'Loaded UI screens restore scaled font text.'
