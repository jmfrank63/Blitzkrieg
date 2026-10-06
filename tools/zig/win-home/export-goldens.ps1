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
    [int]$TimeoutSeconds = 600,
    # Optional GOG mode: batch-export one GOG mod project (a folder holding current.bld) into
    # -GogOut, which is never inside the repository, and print the BK_GOG_* values to use.
    [string]$GogProject = "",
    [string]$GogOut = ""
)

$ErrorActionPreference = "Stop"
# pwsh -File passes "-Extensions a,b" as one string; split it into the list it means.
$Extensions = @($Extensions | ForEach-Object { $_ -split "," } | Where-Object { $_ -ne "" })
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "../../..")).Path
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

if (-not [string]::IsNullOrWhiteSpace($GogProject)) {
    if ([string]::IsNullOrWhiteSpace($GogOut)) { throw "-GogProject needs -GogOut (an uncommitted folder)" }
    $GogProject = [IO.Path]::GetFullPath($GogProject)
    $GogOut = [IO.Path]::GetFullPath($GogOut)
    if ($GogOut.StartsWith($repoRoot, [StringComparison]::OrdinalIgnoreCase)) { throw "-GogOut must be outside the repository: GOG files are never committed" }
    if (-not (Test-Path (Join-Path $GogProject "current.bld"))) { throw "No current.bld in $GogProject" }
    # The editor writes backup.tmp and a config file beside the project, so it works on a copy.
    $gogSource = Join-Path $ScratchRoot "gog/source"
    New-Item -ItemType Directory -Force -Path $gogSource, $GogOut | Out-Null
    Get-ChildItem -Path $GogProject -Force | Copy-Item -Destination $gogSource -Recurse -Force
    $process = Start-Process -FilePath $EditorPath -ArgumentList @("*.bld", "`"$gogSource`"", "`"$GogOut\\`"", "-f") `
        -WorkingDirectory (Split-Path -Parent $EditorPath) -PassThru
    if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
        $process.Kill()
        Write-Host "FAIL gog timed out after $TimeoutSeconds s (a message box is probably open)"
        exit 1
    }
    $gogFiles = @(Get-ChildItem -Path $GogOut -Recurse -File)
    if ($gogFiles.Count -eq 0) { Write-Host "FAIL gog exported nothing"; exit 1 }
    Write-Host ("PASS gog " + $gogFiles.Count + " files in $GogOut")
    Write-Host "Run the port's check with:"
    Write-Host "  BK_GOG_ROOT=<the GOG install holding the INTEX2 mod projects>"
    Write-Host "  BK_GOG_GOLDEN=$GogOut"
    exit 0
}

$failed = @()
foreach ($ext in $Extensions) {
    $fixture = Join-Path $fixtureRoot $ext
    if (-not (Test-Path (Join-Path $fixture "project.$ext"))) { throw "Fixture project is missing: $fixture/project.$ext" }

    $source = Join-Path $ScratchRoot "$ext/source"
    $dest = Join-Path $ScratchRoot "$ext/export"
    New-Item -ItemType Directory -Force -Path $source, $dest | Out-Null
    Get-ChildItem -Path $fixture -Force | Where-Object { $_.Name -ne "golden" } |
        Copy-Item -Destination $source -Recurse -Force
    # The fixtures carry no export path, and batch mode refuses a project without a relative one
    # (CParentFrame::ExportSingleFile returns -3 or -4). The port's comparator compares the files at
    # the top of its export folder, so the scratch copy gets <own_data><export_file_name>1.xml. A
    # gamma.cfg with MFC's defaults (all 0) keeps ReadConfigFile from reporting a missing config.
    # Only the scratch copy changes; the tracked fixture stays as it is.
    $latin1 = [Text.Encoding]::GetEncoding(28591)
    $projectFile = Join-Path $source "project.$ext"
    $text = [IO.File]::ReadAllText($projectFile, $latin1)
    if ($text -notmatch "<export_file_name>") {
        if ($text -match "<own_data>") {
            $text = ([regex]"<own_data>").Replace($text, "<own_data><export_file_name>1.xml</export_file_name>", 1)
        } else {
            $text = $text.Insert($text.LastIndexOf("</"), "<own_data><export_file_name>1.xml</export_file_name></own_data>`r`n")
        }
        [IO.File]::WriteAllText($projectFile, $text, $latin1)
    }
    [IO.File]::WriteAllText((Join-Path $source "gamma.cfg"), "<?xml version=`"1.0`"?>`r`n<base Brightness=`"0`" Contrast=`"0`" Gamma=`"0`"/>`r`n", $latin1)

    # Batch export reads the stats from the project's own cached <RPG>/<desc> block, which MFC writes
    # whenever it saves a project (CParentFrame::ExportSingleFile -> LoadRPGStats). The fixtures are
    # written by the port and lack it, so let the editor open and save the scratch copy first (-os).
    $openSave = Start-Process -FilePath $EditorPath -ArgumentList @("*.$ext", "`"$source`"", "`"$dest\\`"", "-os") `
        -WorkingDirectory (Split-Path -Parent $EditorPath) -PassThru
    if (-not $openSave.WaitForExit($TimeoutSeconds * 1000)) {
        $openSave.Kill()
        Write-Host "FAIL $ext open-and-save timed out after $TimeoutSeconds s (a message box is probably open)"
        $failed += $ext
        continue
    }
    if ($openSave.ExitCode -ne 0) { Write-Host ("WARN $ext open-and-save exit code " + $openSave.ExitCode + "; exporting the unsaved fixture") }
    Remove-Item -Force -ErrorAction SilentlyContinue (Join-Path $source "backup.tmp")
    # The destination keeps its trailing backslash doubled: in "dir\" Windows reads the backslash
    # before the closing quote as an escaped quote, so -f was swallowed and batch mode skipped the export.
    $process = Start-Process -FilePath $EditorPath -ArgumentList @("*.$ext", "`"$source`"", "`"$dest\\`"", "-f") `
        -WorkingDirectory (Split-Path -Parent $EditorPath) -PassThru
    if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
        $process.Kill()
        Write-Host "FAIL $ext timed out after $TimeoutSeconds s (a message box is probably open)"
        $failed += $ext
        continue
    }
    $files = @(Get-ChildItem -Path $dest -Recurse -File)
    if ($files.Count -eq 0) {
        Write-Host ("FAIL $ext exported nothing (editor exit code " + $process.ExitCode + ")")
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
