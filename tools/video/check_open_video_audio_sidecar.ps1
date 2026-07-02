$ErrorActionPreference = "Stop"

$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$header = Get-Content -Raw -Path (Join-Path $root "Sources/src/Scene/OpenVideoPlayer.h")
$source = Get-Content -Raw -Path (Join-Path $root "Sources/src/Scene/OpenVideoPlayer.cpp")
$converter = Get-Content -Raw -Path (Join-Path $root "tools/video/convert_bik_to_ogv.ps1")

function Assert-Contains($Text, $Pattern, $Message) {
	if ($Text -notmatch $Pattern) {
		throw $Message
	}
}

Assert-Contains $converter 'AudioSuffix' "BIK to OGV converter must expose the sidecar audio suffix."
Assert-Contains $converter 'AudioSuffix.*\.ogg' "BIK to OGV converter must create .audio.ogg sidecars."
Assert-Contains $converter 'ffprobe' "BIK to OGV converter must probe whether a source has audio before writing a sidecar."
Assert-Contains $header 'szAudioStreamName' "Open video player must remember the sidecar audio stream name."
Assert-Contains $header 'bAudioStreamPlaying' "Open video player must track whether it started sidecar audio."
Assert-Contains $source 'FindOpenVideoAudioStreamName' "Open video player must derive a sidecar audio stream name."
Assert-Contains $source 'PlayVideoAudioStream' "Open video player must start sidecar audio with video playback."
Assert-Contains $source 'StopVideoAudioStream' "Open video player must stop sidecar audio when video stops."
Assert-Contains $source 'PauseStreaming' "Open video player must pause/resume sidecar audio with video playback."

Write-Host "Open video audio sidecar checks passed."
