param(
    [string]$InstallDir = "zig-out/game",
    [ValidateSet("all", "legacy", "sdl_gpu")]
    [string]$Renderer = "all",
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

Get-ChildItem -LiteralPath $binDir | Where-Object {
    if ($Renderer -eq "legacy") { $_.Name -ne "GFXGPU.dll" }
    elseif ($Renderer -eq "sdl_gpu") { $_.Name -ne "GFX.dll" }
    else { $true }
} | Copy-Item -Destination $resolvedInstallDir -Recurse -Force

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

if ($Renderer -eq "sdl_gpu") {
    $shaderSource = Join-Path $repoRoot "zig-out/shaders"
    if (-not (Test-Path $shaderSource)) {
        throw "Missing generated SDL GPU shaders: $shaderSource. Run 'zig build gfxgpu-shaders' first."
    }
    $shaderDestination = Join-Path $resolvedInstallDir "zig-out/shaders"
    New-Item -ItemType Directory -Force -Path $shaderDestination | Out-Null
    Copy-Item -Path (Join-Path $shaderSource "*") -Destination $shaderDestination -Recurse -Force

    $winPixVersion = "1.0.240308001"
    $winPixUrl = "https://www.nuget.org/api/v2/package/WinPixEventRuntime/$winPixVersion"
    $winPixHash = "726ACC93D6968E2146261A1E415521747D50AD69894C2B42B5D0D4C29FD66EC4"
    $winPixTemp = Join-Path ([IO.Path]::GetTempPath()) ("blitzkrieg-winpix-" + [guid]::NewGuid().ToString("N"))
    try {
        New-Item -ItemType Directory -Force -Path $winPixTemp | Out-Null
        # Expand-Archive validates the filename extension even though NuGet
        # packages are ZIP containers.
        $winPixArchive = Join-Path $winPixTemp "WinPixEventRuntime.zip"
        Invoke-WebRequest -Uri $winPixUrl -OutFile $winPixArchive
        if ((Get-FileHash -Algorithm SHA256 $winPixArchive).Hash -ne $winPixHash) {
            throw "WinPixEventRuntime package hash mismatch."
        }
        Expand-Archive -LiteralPath $winPixArchive -DestinationPath (Join-Path $winPixTemp "package") -Force
        Copy-Item -LiteralPath (Join-Path $winPixTemp "package/bin/x64/WinPixEventRuntime.dll") -Destination $resolvedInstallDir -Force
    }
    finally {
        if (Test-Path $winPixTemp) { Remove-Item -LiteralPath $winPixTemp -Recurse -Force }
    }
}

$configSource = Join-Path $dataConfigsDir "config.cfg"
$defconfSource = Join-Path $dataConfigsDir "defconf.cfg"
if (-not (Test-Path $configSource)) {
    throw "Missing config source file: $configSource"
}
if (-not (Test-Path $defconfSource)) {
    throw "Missing config source file: $defconfSource"
}

Copy-Item -Path $configSource -Destination (Join-Path $resolvedInstallDir "config.cfg") -Force
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
