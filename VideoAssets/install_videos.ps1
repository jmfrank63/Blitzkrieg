<#
.SYNOPSIS
Extract Blitzkrieg movie assets from a repository archive into VideoAssets/Movies.

.DESCRIPTION
This script locates 7-Zip, extracts the configured Blitzkrieg movie archive, and writes the extracted
movie files into the VideoAssets/Movies folder so the build can create the runtime Data/movies junction.

.PARAMETER ArchivePath
The path to the archive to extract. The default is the first archive volume under the script folder.

.PARAMETER OutputPath
The directory to extract movie assets into. Defaults to the VideoAssets/Movies folder next to this script.

.EXAMPLE
.\install_videos.ps1 -ArchivePath .\BlitzkriegVideos.7z.001 -OutputPath .\Movies
#>

param(
    [string]$ArchivePath = "$PSScriptRoot\BlitzkriegVideos.7z.001",
    [string]$OutputPath = "$PSScriptRoot\Movies"
)

function Find-7z {
    $candidates = @(
        '7z.exe',
        '7za.exe',
        "$env:ProgramFiles\7-Zip\7z.exe",
        "$env:ProgramFiles\7-Zip\7za.exe",
        "$env:ProgramFiles(x86)\7-Zip\7z.exe",
        "$env:ProgramFiles(x86)\7-Zip\7za.exe"
    )

    foreach ($candidate in $candidates) {
        if ([System.IO.Path]::IsPathRooted($candidate)) {
            if (Test-Path $candidate) { return $candidate }
        }
        else {
            $resolved = Get-Command $candidate -ErrorAction SilentlyContinue
            if ($resolved) { return $resolved.Source }
        }
    }

    return $null
}

if (-not (Test-Path $ArchivePath)) {
    Write-Error "Archive not found: $ArchivePath"
    exit 1
}

$sevenZip = Find-7z
if (-not $sevenZip) {
    Write-Error "7-Zip not found. Install 7-Zip and ensure 7z.exe or 7za.exe is available on PATH."
    exit 1
}

if (-not (Test-Path $OutputPath)) {
    New-Item -ItemType Directory -Path $OutputPath | Out-Null
}

Write-Host "Extracting movie assets from $ArchivePath to $OutputPath"
& "$sevenZip" x -o"$OutputPath" "$ArchivePath" -y
if ($LASTEXITCODE -ne 0) {
    Write-Error "Movie archive extraction failed with exit code $LASTEXITCODE."
    exit $LASTEXITCODE
}

Write-Host "Movie assets extracted successfully."
