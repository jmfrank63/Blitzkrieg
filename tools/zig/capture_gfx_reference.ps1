[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("legacy", "sdl_gpu")]
    [string]$Renderer,
    [Parameter(Mandatory = $true)]
    [string]$CaptureCommand,
    [string]$OutputRoot = "",
    [int]$Width = 1280,
    [int]$Height = 720,
    [int]$Runs = 3,
    [int]$Seed = 1337,
    [string]$Camera = "reference-origin",
    [string]$FixedTime = "0",
    [string]$DataDirectory = "Data",
    [string]$SceneFixtureVersion = "reference-scene-v1",
    [string]$Driver = "unknown"
)

$ErrorActionPreference = "Stop"
if ($Runs -lt 3) { throw "P08-M02 requires at least three runs per renderer." }
if ($Width -le 0 -or $Height -le 0) { throw "Width and Height must be positive." }

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "../..")).Path
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $OutputRoot = Join-Path ([IO.Path]::GetTempPath()) ("blitzkrieg-gfx-reference-" + [guid]::NewGuid().ToString("N"))
}
$OutputRoot = [IO.Path]::GetFullPath($OutputRoot)
New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null

$commit = (& git -C $repoRoot rev-parse HEAD).Trim()
$resolvedData = [IO.Path]::GetFullPath((Join-Path $repoRoot $DataDirectory))
if (-not (Test-Path $resolvedData)) { throw "Data directory does not exist: $resolvedData" }

$hashes = [Collections.Generic.List[string]]::new()
for ($run = 1; $run -le $Runs; $run++) {
    $rgbaPath = Join-Path $OutputRoot ("{0}-{1}.rgba8" -f $Renderer, $run)
    $metadataPath = Join-Path $OutputRoot ("{0}-{1}.metadata.json" -f $Renderer, $run)
    $expanded = $CaptureCommand
    $replacements = @{
        "{renderer}" = $Renderer
        "{output}" = $rgbaPath
        "{width}" = $Width
        "{height}" = $Height
        "{run}" = $run
        "{seed}" = $Seed
        "{camera}" = $Camera
        "{time}" = $FixedTime
        "{data}" = $resolvedData
    }
    foreach ($key in $replacements.Keys) { $expanded = $expanded.Replace($key, [string]$replacements[$key]) }
    Write-Host ("capture renderer={0} run={1}/{2}" -f $Renderer, $run, $Runs)
    Push-Location $repoRoot
    try { Invoke-Expression $expanded } finally { Pop-Location }
    if ($LASTEXITCODE -ne 0) { throw "Capture command failed for $Renderer run $run with exit code $LASTEXITCODE." }
    if (-not (Test-Path $rgbaPath)) { throw "Capture command did not write $rgbaPath" }
    $expectedBytes = [int64]$Width * $Height * 4
    $actualBytes = (Get-Item $rgbaPath).Length
    if ($actualBytes -ne $expectedBytes) { throw "Capture $rgbaPath has $actualBytes bytes; expected $expectedBytes RGBA8 bytes." }
    $hash = (Get-FileHash -Algorithm SHA256 $rgbaPath).Hash.ToLowerInvariant()
    $hashes.Add($hash)
    $metadata = [ordered]@{
        renderer = $Renderer
        width = $Width
        height = $Height
        format = "RGBA8"
        driver = $Driver
        commit = $commit
        scene_fixture_version = $SceneFixtureVersion
        fixed_time = $FixedTime
        camera = $Camera
        random_seed = $Seed
        data_directory = $resolvedData
        run = $run
        sha256 = $hash
    }
    $metadata | ConvertTo-Json | Set-Content -Encoding utf8 $metadataPath
}

if (($hashes | Select-Object -Unique).Count -ne 1) {
    throw "Renderer $Renderer is nondeterministic: the three RGBA8 capture hashes differ ($($hashes -join ', '))."
}
Write-Host ("renderer={0} sha256={1}" -f $Renderer, $hashes[0])
Write-Host ("artifacts={0}" -f $OutputRoot)
