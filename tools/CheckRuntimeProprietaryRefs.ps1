param(
    [string]$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
)

$ErrorActionPreference = "Stop"

$runtimeFiles = @(
    "Sources/src/A7.sln",
    "Sources/src/Game/Game.vcxproj",
    "Sources/src/Main/Main.vcxproj",
    "Sources/src/Net/Net.vcxproj",
    "Sources/src/GFX/GFX.vcxproj",
    "Sources/src/Formats/Formats.vcxproj",
    "Sources/src/Image/Image.vcxproj",
    "Sources/src/SFX/SFX.vcxproj",
    "Sources/src/Scene/Scene.vcxproj"
)

$blockedPaths = @(
    "Sources/src/GFX/shader.grm"
)

$blockedPatterns = @(
    "FMOD",
    "FSOUND",
    "Bink",
    "BINK",
    "A7ExportModel\.vcxproj",
    "a7exportmodel\\A7ExportModel\.vcxproj",
    "MAYA40",
    "OpenMaya",
    "GameSpy\.lib",
    "GameSpy\\GameSpy\.vcxproj",
    "GameSpyChat\.(cpp|h)",
    "GameSpyPeerChat\.(cpp|h)",
    "GSQueryReportingDriver\.(cpp|h)",
    "GSServersList\.(cpp|h)",
    "GSConsts\.h",
    "GOLDParser",
    "cpp-gpengine",
    "fmtShader",
    "shader\.grm",
    "scl\\scl\.vcxproj",
    "mfc42\.dll",
    "msvcp60\.dll",
    "msvcrt\.dll"
)

$failures = New-Object System.Collections.Generic.List[string]

foreach ($relativePath in $runtimeFiles) {
    $path = Join-Path $Root $relativePath
    if (-not (Test-Path $path)) {
        continue
    }

    $content = Get-Content -LiteralPath $path -Raw
    foreach ($pattern in $blockedPatterns) {
        if ($content -match $pattern) {
            $failures.Add("$relativePath matches $pattern")
        }
    }
}

foreach ($relativePath in $blockedPaths) {
    $path = Join-Path $Root $relativePath
    if (Test-Path $path) {
        $failures.Add("$relativePath exists")
    }
}

if ($failures.Count -gt 0) {
    $failures | Sort-Object
    throw "Runtime proprietary reference check failed with $($failures.Count) finding(s)."
}

Write-Host "Runtime proprietary reference check passed."
