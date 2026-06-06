<#
.SYNOPSIS
Create a compressed video archive for Blitzkrieg movie assets.

.DESCRIPTION
This helper packages the contents of a source movie folder into a 7z archive
that can be stored in the repository as a single compressed asset set.

.EXAMPLE
.\VideoAssets\create_video_archive.ps1 -SourcePath .\Data\Movies -OutputPath .\VideoAssets\BlitzkriegVideos.7z
#>

param(
    [Parameter(Mandatory=$true)]
    [string]$SourcePath,

    [string]$OutputPath = "$PSScriptRoot\BlitzkriegVideos.7z",

    [int]$VolumeSizeMB = 250,

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

if (-not (Test-Path $SourcePath)) {
    Write-Error "Source path not found: $SourcePath"
    exit 1
}

$sevenZip = Find-7z
if (-not $sevenZip) {
    Write-Error "7-Zip not found. Install 7-Zip and ensure 7z.exe or 7za.exe is on PATH."
    exit 1
}

$volumeParam = "-v${VolumeSizeMB}m"
$arguments = @('a', "-t7z", "-m0=lzma2", "-mx=9", "-mmt=on", $volumeParam, "`"$OutputPath`"", "`"$SourcePath\*`"")
if ($VerboseOutput) { $arguments += '-bb3' }

Write-Host "Creating archive from $SourcePath to $OutputPath"
$processInfo = Start-Process -FilePath $sevenZip -ArgumentList $arguments -NoNewWindow -PassThru -Wait
if ($processInfo.ExitCode -ne 0) {
    Write-Error "Archive creation failed with exit code $($processInfo.ExitCode)."
    exit $processInfo.ExitCode
}

Write-Host "Archive created successfully. Output files are stored in $PSScriptRoot."