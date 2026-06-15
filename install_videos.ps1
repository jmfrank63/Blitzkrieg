<#
.SYNOPSIS
Extract compressed Blitzkrieg movie archives into the runtime Movies folder.

.DESCRIPTION
This script extracts video archives from the tracked VideoAssets/ folder into
VideoAssets/Movies, then creates a local junction from Data/movies to that folder.
The game sees the videos through the existing build output Data junction.

.EXAMPLE
.\install_videos.ps1

.EXAMPLE
.\install_videos.ps1 -ArchiveRoot .\VideoAssets -Destination .\VideoAssets\Movies
#>

param(
    [string]$ArchiveRoot = "$PSScriptRoot\VideoAssets",
    [string]$Destination = "$PSScriptRoot\VideoAssets\Movies",
    [switch]$VerboseOutput
)

function Find-7z {
    $candidates = @(
        '7z.exe',
        '7za.exe',
        'C:\Program Files\7-Zip\7z.exe',
        'C:\Program Files (x86)\7-Zip\7z.exe'
    )
    foreach ($candidate in $candidates) {
        $resolved = Get-Command $candidate -ErrorAction SilentlyContinue
        if ($resolved) { return $resolved.Source }
    }
    return $null
}

function Write-Status($message) {
    Write-Host "[install_videos] $message"
}

if (-not (Test-Path $ArchiveRoot)) {
    Write-Error "Archive root not found: $ArchiveRoot"
    exit 1
}

$sevenZip = Find-7z
if (-not $sevenZip) {
    Write-Error "7-Zip not found. Install 7-Zip and ensure 7z.exe or 7za.exe is on PATH."
    exit 1
}

$singleArchive = Get-ChildItem -Path $ArchiveRoot -Filter '*.7z' -File | Where-Object { $_.Name -notlike '*.7z.*' } | Select-Object -First 1
$splitArchiveBase = Get-ChildItem -Path $ArchiveRoot -Filter '*.7z.001' -File | Select-Object -First 1

if (-not $singleArchive -and -not $splitArchiveBase) {
    Write-Error "No supported archive found in $ArchiveRoot. Expected *.7z or *.7z.001."
    exit 1
}

if (-not (Test-Path $Destination)) {
    Write-Status "Creating destination path: $Destination"
    New-Item -ItemType Directory -Path $Destination -Force | Out-Null
}

$dataMoviesLink = Join-Path $PSScriptRoot 'Data\movies'

function Ensure-DataMoviesJunction($linkPath, $targetPath) {
    if (-not (Test-Path $linkPath)) {
        Write-Status "Creating Data/movies junction: $linkPath -> $targetPath"
        New-Item -ItemType Junction -Path $linkPath -Target $targetPath -Force | Out-Null
        return
    }

    if (-not ((Get-Item $linkPath).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
        Write-Error "$linkPath exists and is not a junction. Remove or rename it, then rerun this script."
        exit 1
    }
}

$archivePath = $null
if ($singleArchive) {
    $archivePath = $singleArchive.FullName
    Write-Status "Found single archive: $archivePath"
}
elseif ($splitArchiveBase) {
    $archivePath = $splitArchiveBase.FullName
    Write-Status "Found split archive base: $archivePath"
}

Write-Status "Extracting video assets into: $Destination"
$arguments = @('x', "`"$archivePath`"", "-o`"$Destination`"", '-y')
if ($VerboseOutput) { $arguments += '-bb3' }

$processInfo = Start-Process -FilePath $sevenZip -ArgumentList $arguments -NoNewWindow -PassThru -Wait
if ($processInfo.ExitCode -ne 0) {
    Write-Error "Extraction failed with exit code $($processInfo.ExitCode)."
    exit $processInfo.ExitCode
}

Write-Status "Extraction complete."
Ensure-DataMoviesJunction -linkPath $dataMoviesLink -targetPath $Destination
Write-Host "Extracted files are written into $Destination."
Write-Host "Data/movies points at the extracted files so the game can see them through the normal runtime Data tree."
