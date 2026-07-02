$ErrorActionPreference = "Stop"

$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$header = Get-Content -Raw -Path (Join-Path $root "Sources/src/Scene/OpenVideoPlayer.h")
$source = Get-Content -Raw -Path (Join-Path $root "Sources/src/Scene/OpenVideoPlayer.cpp")
$router = Get-Content -Raw -Path (Join-Path $root "Sources/src/Scene/VideoPlayer.cpp")

function Assert-Contains($Text, $Pattern, $Message) {
	if ($Text -notmatch $Pattern) {
		throw $Message
	}
}

Assert-Contains $header 'SOpenVideoDecoderState\s*\*pDecoderState' "Open video player must retain decoder state between frames."
Assert-Contains $header 'nDecodedFrame' "Open video player must track the last decoded frame."
Assert-Contains $header 'DecodeNextFrame' "Open video player must expose sequential frame decoding."
Assert-Contains $header 'DestroyDecoder' "Open video player must clean up persistent decoder resources."
Assert-Contains $source 'ogg_stream_packetout' "Open video frame advance must drain queued Ogg packets."
Assert-Contains $source 'th_decode_packetin' "Open video frame advance must decode Theora packets."
Assert-Contains $source 'if\s*\(\s*nDecodedFrame\s*<\s*nTargetFrame' "Open video Update must advance decoded frames toward playback time."
Assert-Contains $source 'DestroyDecoder\(\)' "Open video Stop must release decoder state."
Assert-Contains $source 'nLumaBaseX\s*=\s*ti\.pic_x\s*&\s*~hdec' "Open video upload must align Theora luma crop to chroma boundaries."
Assert-Contains $source 'nChromaBaseX\s*=\s*ti\.pic_x\s*>>\s*hdec' "Open video upload must compute chroma crop from the original Theora crop."
if ($router -match 'fallback to Bink') {
	throw "Open video testing must not silently fall back to the removed backend when an open replacement is selected."
}

Write-Host "Open video frame-advance checks passed."
