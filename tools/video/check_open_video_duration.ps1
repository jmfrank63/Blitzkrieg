$ErrorActionPreference = "Stop"

$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$header = Get-Content -Raw -Path (Join-Path $root "Sources/src/Scene/OpenVideoPlayer.h")
$source = Get-Content -Raw -Path (Join-Path $root "Sources/src/Scene/OpenVideoPlayer.cpp")

function Assert-Contains($Text, $Pattern, $Message) {
	if ($Text -notmatch $Pattern) {
		throw $Message
	}
}

function Assert-NotContains($Text, $Pattern, $Message) {
	if ($Text -match $Pattern) {
		throw $Message
	}
}

Assert-Contains $header 'nMovieLength' "Open video player must store computed movie length."
Assert-Contains $header 'nNumFrames' "Open video player must store computed frame count."
Assert-Contains $header 'dwStartTime' "Open video player must track playback start time."
Assert-Contains $source 'FindLastOggGranulePosition' "Open video player must inspect Ogg pages for stream duration."
Assert-Contains $source 'DecodeTheoraGranuleFrame' "Open video player must decode Theora granule positions into frame numbers."
Assert-Contains $source 'nMovieLength\s*=\s*1000' "Open video player must compute millisecond duration from frames and frame rate."
Assert-Contains $source 'return\s+nMovieLength' "Play must return computed duration for valid open video files."
Assert-NotContains $source 'Open video backend has timing but no renderer' "Open video must not fall back after a successful timing probe."

Write-Host "Open video duration checks passed."
