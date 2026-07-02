$ErrorActionPreference = "Stop"

$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$header = Get-Content -Raw -Path (Join-Path $root "Sources/src/Scene/OpenVideoPlayer.h")
$source = Get-Content -Raw -Path (Join-Path $root "Sources/src/Scene/OpenVideoPlayer.cpp")
$sceneProject = Get-Content -Raw -Path (Join-Path $root "Sources/src/Scene/Scene.vcxproj")

function Assert-Contains($Text, $Pattern, $Message) {
	if ($Text -notmatch $Pattern) {
		throw $Message
	}
}

Assert-Contains $header 'DecodeFirstFrame' "Open video player must expose first-frame decode plumbing."
Assert-Contains $header 'COpenVideoImagesList' "Open video player must keep textures for decoded video frames."
Assert-Contains $source 'theora/theoradec\.h' "Open video player must use libtheora decoder headers."
Assert-Contains $source 'ogg/ogg\.h' "Open video player must use libogg demux headers."
Assert-Contains $source 'th_decode_packetin' "Open video player must decode Theora packets."
Assert-Contains $source 'th_decode_ycbcr_out' "Open video player must retrieve decoded YCbCr frame data."
Assert-Contains $source 'YCbCrToARGB' "Open video player must convert decoded YCbCr into ARGB texture pixels."
Assert-Contains $source 'CreateTexture' "Open video player must allocate textures for decoded frames."
Assert-Contains $source 'DrawRects' "Open video player must draw decoded frame textures."
Assert-Contains $sceneProject 'libtheora_static\.lib' "Scene project must link the vendored libtheora static library."
Assert-Contains $sceneProject 'ogg-1\.3\.5\\src\\framing\.c' "Scene project must compile libogg framing for Ogg demux."

Write-Host "Open video first-frame checks passed."
