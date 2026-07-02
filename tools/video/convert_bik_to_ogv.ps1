param(
	[string]$InputRoot,
	[string]$FfmpegPath = "ffmpeg",
	[string]$FfprobePath = "ffprobe",
	[int]$KeyFrameInterval = 1,
	[string]$AudioSuffix = ".audio",
	[switch]$Force,
	[switch]$NoRecurse,
	[switch]$WhatIf
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
if ([string]::IsNullOrWhiteSpace($InputRoot)) {
	$InputRoot = Join-Path $repoRoot "Data/movies"
}

if (-not (Test-Path -LiteralPath $InputRoot)) {
	throw "Input root does not exist: $InputRoot"
}

$ffmpeg = Get-Command $FfmpegPath -ErrorAction SilentlyContinue
if (-not $ffmpeg) {
	throw "ffmpeg was not found. Install ffmpeg or pass -FfmpegPath <path-to-ffmpeg.exe>."
}

$ffprobe = Get-Command $FfprobePath -ErrorAction SilentlyContinue
if (-not $ffprobe) {
	throw "ffprobe was not found. Install ffmpeg or pass -FfprobePath <path-to-ffprobe.exe>."
}

$bikFiles = Get-ChildItem -LiteralPath $InputRoot -Filter *.bik -File -Recurse:(!$NoRecurse)

if ($bikFiles.Count -eq 0) {
	Write-Host "No .bik files found under $InputRoot."
	exit 0
}

$converted = 0
$skipped = 0
$failed = 0
$audioConverted = 0
$audioSkipped = 0

function Test-HasAudioStream($Path) {
	$output = & $ffprobe.Source -v error -select_streams a:0 -show_entries stream=index -of csv=p=0 $Path
	return -not [string]::IsNullOrWhiteSpace(($output | Select-Object -First 1))
}

foreach ($bik in $bikFiles) {
	$ogvPath = [System.IO.Path]::ChangeExtension($bik.FullName, ".ogv")
	$audioPath = [System.IO.Path]::Combine($bik.DirectoryName, "$([System.IO.Path]::GetFileNameWithoutExtension($bik.Name))$AudioSuffix.ogg")
	if ((Test-Path -LiteralPath $ogvPath) -and -not $Force) {
		Write-Host "Skip existing: $ogvPath"
		++$skipped
	}
	else {
		$args = @(
			"-hide_banner",
			"-y",
			"-i", $bik.FullName,
			"-map", "0:v:0",
			"-map", "0:a?",
			"-c:v", "libtheora",
			"-q:v", "7",
			"-g", $KeyFrameInterval,
			"-c:a", "libvorbis",
			"-q:a", "4",
			$ogvPath
		)

		if ($WhatIf) {
			Write-Host "Would convert: $($bik.FullName) -> $ogvPath"
		}
		else {
			Write-Host "Converting: $($bik.FullName) -> $ogvPath"
			& $ffmpeg.Source @args
			if ($LASTEXITCODE -eq 0) {
				++$converted
			}
			else {
				++$failed
				Write-Warning "ffmpeg failed for $($bik.FullName) with exit code $LASTEXITCODE"
			}
		}
	}

	if (Test-HasAudioStream $bik.FullName) {
		if ((Test-Path -LiteralPath $audioPath) -and -not $Force) {
			Write-Host "Skip existing audio: $audioPath"
			++$audioSkipped
			continue
		}
		$audioArgs = @(
			"-hide_banner",
			"-y",
			"-i", $bik.FullName,
			"-map", "0:a:0",
			"-vn",
			"-c:a", "libvorbis",
			"-q:a", "4",
			$audioPath
		)
		if ($WhatIf) {
			Write-Host "Would extract audio: $($bik.FullName) -> $audioPath"
			continue
		}
		Write-Host "Extracting audio: $($bik.FullName) -> $audioPath"
		& $ffmpeg.Source @audioArgs
		if ($LASTEXITCODE -eq 0) {
			++$audioConverted
		}
		else {
			++$failed
			Write-Warning "ffmpeg audio extraction failed for $($bik.FullName) with exit code $LASTEXITCODE"
		}
	}
}

Write-Host "BIK to OGV conversion finished. Converted=$converted Skipped=$skipped AudioConverted=$audioConverted AudioSkipped=$audioSkipped Failed=$failed"
if ($failed -ne 0) {
	exit 1
}
