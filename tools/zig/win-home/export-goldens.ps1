# Makes the D-11 goldens: the MFC ResourceEditor's own export of each fixture
# project under tools/zig/fixtures/resource_editor/<ext>/, written into
# <ext>/golden/. Run it on win-home (Windows, where the MFC editor.exe and its
# DLLs run); it cannot run on Linux or macOS.
#
# It drives the editor's batch mode (CEditorApp::RunBatchMode):
#   editor.exe <*.ext> <folder with projects> <destination folder> -f
# The editor exports each project to the export path the project stores,
# relative to the destination folder, and reports failures in message boxes,
# so watch the screen for one while it runs.
#
# The fixture folder is copied to a scratch folder first, so the editor never
# writes next to the tracked project (its batch mode writes backup.tmp and a
# config file beside it). The golden folder is cleared except README.md and
# .gitkeep, then filled from the scratch destination. Commit the result.
[CmdletBinding()]
param(
    [string]$EditorPath = "",
    [string[]]$Extensions = @("wpn", "mcp", "trc", "scp", "spt", "unt", "msh", "obt", "fnc", "bld", "bdg",
                              "pcp", "eff", "til", "3rd", "3rv", "mip", "chc", "cgc", "mdc"),
    [string]$ScratchRoot = "",
    [int]$TimeoutSeconds = 600
)

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path
$fixtureRoot = Join-Path $repoRoot "tools/zig/fixtures/resource_editor"
if ([string]::IsNullOrWhiteSpace($EditorPath)) {
    $EditorPath = Join-Path $repoRoot "Sources/src/bin/editor.exe"
}
$EditorPath = [IO.Path]::GetFullPath($EditorPath)
if (-not (Test-Path $EditorPath)) { throw "MFC editor.exe does not exist: $EditorPath" }
if ([string]::IsNullOrWhiteSpace($ScratchRoot)) {
    $ScratchRoot = Join-Path ([IO.Path]::GetTempPath()) ("blitzkrieg-goldens-" + [guid]::NewGuid().ToString("N"))
}
$ScratchRoot = [IO.Path]::GetFullPath($ScratchRoot)
New-Item -ItemType Directory -Force -Path $ScratchRoot | Out-Null
$commit = (& git -C $repoRoot rev-parse HEAD).Trim()
Write-Host "editor   $EditorPath"
Write-Host "commit   $commit"
Write-Host "scratch  $ScratchRoot"

$failed = @()
foreach ($ext in $Extensions) {
    $fixture = Join-Path $fixtureRoot $ext
    if (-not (Test-Path (Join-Path $fixture "project.$ext"))) { throw "Fixture project is missing: $fixture/project.$ext" }

    $source = Join-Path $ScratchRoot "$ext/source"
    $dest = Join-Path $ScratchRoot "$ext/export"
    New-Item -ItemType Directory -Force -Path $source, $dest | Out-Null
    Get-ChildItem -Path $fixture -Force | Where-Object { $_.Name -ne "golden" } |
        Copy-Item -Destination $source -Recurse -Force

    $process = Start-Process -FilePath $EditorPath -ArgumentList @("*.$ext", "`"$source`"", "`"$dest\`"", "-f") `
        -WorkingDirectory (Split-Path -Parent $EditorPath) -PassThru
    if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
        $process.Kill()
        Write-Host "FAIL $ext timed out after $TimeoutSeconds s (a message box is probably open)"
        $failed += $ext
        continue
    }
    $files = @(Get-ChildItem -Path $dest -Recurse -File)
    if ($files.Count -eq 0) {
        Write-Host "FAIL $ext exported nothing"
        $failed += $ext
        continue
    }

    $golden = Join-Path $fixture "golden"
    New-Item -ItemType Directory -Force -Path $golden | Out-Null
    Get-ChildItem -Path $golden -Force | Where-Object { $_.Name -notin @("README.md", ".gitkeep") } |
        Remove-Item -Recurse -Force
    Get-ChildItem -Path $dest -Force | Copy-Item -Destination $golden -Recurse -Force
    Write-Host ("PASS $ext " + $files.Count + " files")
}

if ($failed.Count -gt 0) {
    Write-Host ("VERDICT=FAIL " + ($failed -join " "))
    exit 1
}
Write-Host "VERDICT=PASS goldens written; commit tools/zig/fixtures/resource_editor/*/golden"
