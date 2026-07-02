$ErrorActionPreference = "Stop"

$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$videoPlayer = Get-Content -Raw -Path (Join-Path $root "Sources/src/Scene/VideoPlayer.cpp")

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

Assert-Contains $videoPlayer 'ResolveOpenVideoName' "CVideoPlayer must resolve open video replacement names."
Assert-Contains $videoPlayer '\.ogv' "Open video resolution must check .ogv replacements."
Assert-Contains $videoPlayer '\.ogg' "Open video resolution must check .ogg replacements."
Assert-Contains $videoPlayer 'OpenStream\([^;]+STREAM_ACCESS_READ' "Open video resolution must check Data storage for candidate files."
Assert-Contains $videoPlayer 'OpenFileStream\([^;]+STREAM_ACCESS_READ' "Open video resolution must also support absolute filesystem candidates."
Assert-Contains $videoPlayer 'szResolvedFileName' "CVideoPlayer::Play must track the resolved video file name."
Assert-Contains $videoPlayer 'szOriginalFileName' "CVideoPlayer::Play must preserve the original requested Bink file."
Assert-Contains $videoPlayer 'Open video resolver' "CVideoPlayer must trace video resolution decisions for manual testing."
Assert-NotContains $videoPlayer 'fallback to Bink' "Open video replacements must not silently fall back to Bink."

Write-Host "Open video resolution checks passed."
