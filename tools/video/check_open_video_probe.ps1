$ErrorActionPreference = "Stop"

$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$header = Get-Content -Raw -Path (Join-Path $root "Sources/src/Scene/OpenVideoPlayer.h")
$source = Get-Content -Raw -Path (Join-Path $root "Sources/src/Scene/OpenVideoPlayer.cpp")

function Assert-Contains($Text, $Pattern, $Message) {
	if ($Text -notmatch $Pattern) {
		throw $Message
	}
}

Assert-Contains $header 'bHasMovieInfo' "Open video player must store whether metadata was probed."
Assert-Contains $header 'vMovieSize' "Open video player must store probed movie dimensions."
Assert-Contains $source 'ParseTheoraIdentificationHeader' "Open video player must parse Theora identification headers."
Assert-Contains $source 'ProbeOpenVideo' "Open video player must have a probing step."
Assert-Contains $source 'OpenStream\([^;]+STREAM_ACCESS_READ' "Open video probing must read files through IDataStorage."
Assert-Contains $source 'OpenFileStream\([^;]+STREAM_ACCESS_READ' "Open video probing must also read absolute filesystem paths."
Assert-Contains $source 'theora' "Open video probing must recognize Theora headers."
Assert-Contains $source 'Open video probe' "Open video probing must trace what it finds for manual testing."
Assert-Contains $source 'GetMovieSize[\s\S]+bHasMovieInfo[\s\S]+vMovieSize' "GetMovieSize must return probed dimensions."
Assert-Contains $source 'return 0;\s*// Rendering is not implemented yet' "Play must still return 0 so the router falls back to Bink."

Write-Host "Open video probe checks passed."
