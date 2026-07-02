$ErrorActionPreference = "Stop"
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$soundEngine = Get-Content -Raw -Path (Join-Path $root "Sources/src/SFX/SoundEngine.cpp")
$openVideo = Get-Content -Raw -Path (Join-Path $root "Sources/src/Scene/OpenVideoPlayer.cpp")

if ($openVideo -notmatch 'PlayStream\(\s*szAudioStreamName\.c_str\(\),\s*bLooped,\s*0\s*\)') {
	throw "Open video sidecar audio must start immediately without requesting a fade."
}

if ($soundEngine -notmatch 'streamFadeOff\.Clear\(\);\s*SetStreamVolume\(\s*1\.0f\s*\);\s*CloseStreaming\(\);') {
	throw "Immediate PlayStream must cancel any previous fade before opening the new stream."
}

Write-Host "Open video audio fade checks passed."
