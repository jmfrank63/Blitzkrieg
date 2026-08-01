[CmdletBinding()]
param(
    [string]$OutputRoot = "",
    [string]$Zig = "zig",
    [string]$Optimize = "Debug",
    [string]$Target = "x86_64-windows-msvc",
    [switch]$Launch,
    [ValidateSet("legacy", "sdl_gpu")]
    [string]$LaunchRenderer = "",
    [string[]]$GameArguments = @(),
    [int]$LaunchTimeoutSeconds = 0
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $OutputRoot = Join-Path ([IO.Path]::GetTempPath()) ("blitzkrieg-menu-mission-" + [guid]::NewGuid().ToString("N"))
}
$OutputRoot = [IO.Path]::GetFullPath($OutputRoot)
$legacyRoot = Join-Path $OutputRoot "legacy"
$sdlRoot = Join-Path $OutputRoot "sdl_gpu"
$logsRoot = Join-Path $OutputRoot "logs"
$capturesRoot = Join-Path $OutputRoot "captures"
$cacheRoot = Join-Path $OutputRoot "build-cache"

New-Item -ItemType Directory -Force -Path $legacyRoot, $sdlRoot, $logsRoot, $capturesRoot, $cacheRoot | Out-Null

function Stop-RunningGame {
    Get-Process -Name "Game" -ErrorAction SilentlyContinue | ForEach-Object {
        Write-Host ("stopping stale Game.exe pid={0}" -f $_.Id)
        Stop-Process -Id $_.Id -Force
    }
}

function Invoke-ZigStep {
    param(
        [string]$Renderer,
        [string]$InstallRoot,
        [string]$CachePath,
        [string]$Step,
        [string]$LogPath
    )

    $installRelative = [IO.Path]::GetRelativePath($repoRoot, $InstallRoot)
    $arguments = @(
        "build", "--cache-dir", $CachePath,
        "-Dtarget=$Target", "-Doptimize=$Optimize", "-Drenderer=$Renderer",
        "-Dinstall-dir=$installRelative", "-Dcopy-data=true", $Step
    )
    Write-Host ("building renderer={0} step={1}" -f $Renderer, $Step)
    Push-Location $repoRoot
    try {
        & $Zig @arguments 2>&1 | Tee-Object -FilePath $LogPath
        if ($LASTEXITCODE -ne 0) {
            throw "zig $Step failed for $Renderer with exit code $LASTEXITCODE. See $LogPath"
        }
    }
    finally {
        Pop-Location
    }
}

function Prepare-Renderer {
    param([string]$Renderer, [string]$InstallRoot)

    $rendererCache = Join-Path $cacheRoot $Renderer
    New-Item -ItemType Directory -Force -Path $rendererCache | Out-Null
    $buildLog = Join-Path $logsRoot ("{0}-build.log" -f $Renderer)
    if ($Renderer -eq "sdl_gpu") {
        $shaderLog = Join-Path $logsRoot "sdl_gpu-shaders.log"
        Invoke-ZigStep -Renderer $Renderer -InstallRoot $InstallRoot -CachePath $rendererCache -Step "gfxgpu-shaders" -LogPath $shaderLog
    }
    Invoke-ZigStep -Renderer $Renderer -InstallRoot $InstallRoot -CachePath $rendererCache -Step "install-game" -LogPath $buildLog

    $gamePath = Join-Path $InstallRoot "Game.exe"
    if (-not (Test-Path -LiteralPath $gamePath)) { throw "Missing staged executable: $gamePath" }
    $dataPath = Join-Path $InstallRoot "Data"
    if (-not (Test-Path -LiteralPath $dataPath)) { throw "Missing staged Data directory: $dataPath" }
    return $gamePath
}

Stop-RunningGame
$legacyGame = Prepare-Renderer -Renderer "legacy" -InstallRoot $legacyRoot
$sdlGame = Prepare-Renderer -Renderer "sdl_gpu" -InstallRoot $sdlRoot

$manifest = [ordered]@{
    packet = "P08-M03"
    commit = (& git -C $repoRoot rev-parse HEAD).Trim()
    target = $Target
    optimize = $Optimize
    output_root = $OutputRoot
    renderers = [ordered]@{
        legacy = [ordered]@{ executable = $legacyGame; data = (Join-Path $legacyRoot "Data") }
        sdl_gpu = [ordered]@{ executable = $sdlGame; data = (Join-Path $sdlRoot "Data") }
    }
    scenarios = @(
        "main-menu",
        "loading-screen",
        "initial-mission",
        "terrain-close-up",
        "units-and-selection",
        "particles-explosions",
        "shadows-water",
        "pause-ui",
        "return-to-menu"
    )
    captures_root = $capturesRoot
    logs_root = $logsRoot
    human_acceptance = "awaiting human acceptance"
}
$manifest | ConvertTo-Json -Depth 12 | Set-Content -Encoding utf8 (Join-Path $OutputRoot "manifest.json")

if ($Launch) {
    if ([string]::IsNullOrWhiteSpace($LaunchRenderer)) {
        throw "-Launch requires -LaunchRenderer legacy or sdl_gpu; review one staged renderer at a time."
    }
    $argumentList = @("-windowed") + $GameArguments
    Stop-RunningGame
    $game = if ($LaunchRenderer -eq "legacy") { $legacyGame } else { $sdlGame }
    $stdout = Join-Path $logsRoot ("{0}-stdout.log" -f $LaunchRenderer)
    $stderr = Join-Path $logsRoot ("{0}-stderr.log" -f $LaunchRenderer)
    Write-Host ("launching {0}: {1}" -f $LaunchRenderer, $game)
    $process = Start-Process -FilePath $game -ArgumentList $argumentList -WorkingDirectory (Split-Path -Parent $game) -RedirectStandardOutput $stdout -RedirectStandardError $stderr -PassThru
    if ($LaunchTimeoutSeconds -gt 0) {
        if (-not $process.WaitForExit($LaunchTimeoutSeconds * 1000)) {
            Stop-Process -Id $process.Id -Force
            $process.WaitForExit()
        }
    }
}

Write-Host "P08-M03 preparation complete."
Write-Host ("staging={0}" -f $OutputRoot)
Write-Host "human_acceptance=awaiting human acceptance"
