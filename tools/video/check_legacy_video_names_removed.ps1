$ErrorActionPreference = "Stop"
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)

$filesToScan = @(
	"Sources/src/UI/UIVideoButton.h",
	"Sources/src/UI/UIVideoButton.cpp",
	"Sources/src/GameTT/PlayMovieInterface.h",
	"Data/UI/MainMenu.xml",
	"Sources/src/data/UI/MainMenu.xml",
	"Data/Textes/UI/Intermission/Credits/credits.txt",
	"Data/AmericanELK/game_data_base/textes/ui/intermission/credits/credits.txt",
	"Data/AmericanELK/game_data_base/textes/ui/intermission/credits/credits.elk",
	"Data/ELK/game_data_base/textes/ui/intermission/credits/credits.elk"
)

foreach ($relativePath in $filesToScan) {
	$path = Join-Path $root $relativePath
	if (!(Test-Path $path)) {
		continue
	}
	$content = Get-Content -Raw -Path $path
	$legacyNamePattern = ("Bink" + "File|sz" + "Bink" + "File|bink" + " video|Bink" + " Video|current " + "bink")
	if ($content -match $legacyNamePattern) {
		throw "Legacy proprietary video naming remains in '$relativePath'."
	}
}

Write-Host "Legacy proprietary video naming checks passed."
