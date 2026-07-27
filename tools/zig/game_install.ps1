param(
    [string]$InstallDir = "zig-out/game",
    [switch]$CopyData,
    [switch]$IncludeEditors,
    [switch]$Run,
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$GameArgs
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "../..")
$binDir = Join-Path $repoRoot "zig-out/bin"
$resolvedInstallDir = Join-Path $repoRoot $InstallDir
$dataDir = Join-Path $repoRoot "Data"
$installDataDir = Join-Path $resolvedInstallDir "Data"
$dataConfigsDir = Join-Path $dataDir "Configs"
$editorsDir = Join-Path $resolvedInstallDir "Editors"

function Copy-OptionalEditor {
    param(
        [string]$SourcePath,
        [string]$DestinationPath
    )

    if (Test-Path $SourcePath) {
        $destinationParent = Split-Path -Parent $DestinationPath
        if (-not (Test-Path $destinationParent)) {
            New-Item -ItemType Directory -Path $destinationParent | Out-Null
        }
        Copy-Item -Path $SourcePath -Destination $DestinationPath -Force
        return $true
    }

    return $false
}

if (-not (Test-Path $binDir)) {
    throw "Missing built binaries directory: $binDir"
}

if (-not (Test-Path $resolvedInstallDir)) {
    New-Item -ItemType Directory -Path $resolvedInstallDir | Out-Null
}

Copy-Item -Path (Join-Path $binDir "*") -Destination $resolvedInstallDir -Recurse -Force

if (Test-Path $installDataDir) {
    Remove-Item -Path $installDataDir -Recurse -Force
}

if ($CopyData) {
    if (-not (Test-Path $dataDir)) {
        throw "Missing Data directory: $dataDir"
    }
    Copy-Item -Path $dataDir -Destination $installDataDir -Recurse -Force
} else {
    if (-not (Test-Path $dataDir)) {
        throw "Missing Data directory: $dataDir"
    }
    New-Item -ItemType Junction -Path $installDataDir -Target $dataDir | Out-Null
}

$configSource = Join-Path $dataConfigsDir "config.cfg"
$defconfSource = Join-Path $dataConfigsDir "defconf.cfg"
if (-not (Test-Path $defconfSource)) {
    throw "Missing config source file: $defconfSource"
}

if (Test-Path $configSource) {
    Copy-Item -Path $configSource -Destination (Join-Path $resolvedInstallDir "config.cfg") -Force
} else {
    Copy-Item -Path $defconfSource -Destination (Join-Path $resolvedInstallDir "config.cfg") -Force
}
Copy-Item -Path $defconfSource -Destination (Join-Path $resolvedInstallDir "defconf.cfg") -Force

if (Test-Path $editorsDir) {
    Remove-Item -Path $editorsDir -Recurse -Force
}

if ($IncludeEditors) {
    $copiedEditorsCount = 0

    if (Copy-OptionalEditor -SourcePath (Join-Path $repoRoot "Sources/src/bin/editor.exe") -DestinationPath (Join-Path $editorsDir "editor.exe")) {
        $copiedEditorsCount++
    }
    if (Copy-OptionalEditor -SourcePath (Join-Path $repoRoot "Sources/src/bin/MapEditor.exe") -DestinationPath (Join-Path $editorsDir "MapEditor.exe")) {
        $copiedEditorsCount++
    }
    if (Copy-OptionalEditor -SourcePath (Join-Path $repoRoot "Sources/src/bin/ExcelExporter.exe") -DestinationPath (Join-Path $editorsDir "ExcelExporter.exe")) {
        $copiedEditorsCount++
    }
    if (Copy-OptionalEditor -SourcePath (Join-Path $repoRoot "Sources/elk/ELK.exe") -DestinationPath (Join-Path $editorsDir "ELK.exe")) {
        $copiedEditorsCount++
    }

    if ($copiedEditorsCount -eq 0) {
        throw "No editor executables were found to include."
    }
}

if ($Run) {
    Push-Location $resolvedInstallDir
    try {
        if ($GameArgs) {
            & .\Game.exe @GameArgs
        } else {
            & .\Game.exe
        }
        $exitCode = $LASTEXITCODE
    }
    finally {
        Pop-Location
    }

    if ($null -ne $exitCode) {
        exit $exitCode
    }
}
