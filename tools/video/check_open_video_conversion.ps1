$ErrorActionPreference = "Stop"

$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$converter = Get-Content -Raw -Path (Join-Path $root "tools/video/convert_bik_to_ogv.ps1")

function Assert-Contains($Text, $Pattern, $Message) {
	if ($Text -notmatch $Pattern) {
		throw $Message
	}
}

Assert-Contains $converter 'KeyFrameInterval' "BIK to OGV converter must expose a keyframe interval."
Assert-Contains $converter '"-g",\s*\$KeyFrameInterval' "BIK to OGV converter must pass the keyframe interval to ffmpeg."
Assert-Contains $converter 'libtheora' "BIK to OGV converter must continue to encode Theora video."

Write-Host "Open video conversion checks passed."
