$ErrorActionPreference = "Stop"

$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$source = Get-Content -Raw -Path (Join-Path $root "Sources/src/GameTT/PlayMovieInterface.cpp")

function Assert-Contains($Text, $Pattern, $Message) {
	if ($Text -notmatch $Pattern) {
		throw $Message
	}
}

Assert-Contains $source 'Open video playmovie' "PlayMovieInterface must trace intro/intermission movie playback."
Assert-Contains $source 'szMovieName\.c_str\(\)' "PlayMovieInterface trace must include the first movie candidate."
Assert-Contains $source 'nMovieLength' "PlayMovieInterface trace must include movie play result length."
Assert-Contains $source 'Open video command' "PlayMovieInterface must trace video command setup."
Assert-Contains $source 'Open video sequence' "PlayMovieInterface must trace sequence loading."

Write-Host "PlayMovie diagnostics checks passed."
