$ErrorActionPreference = "Stop"
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)

$forbiddenPaths = @(
	("Sources/sdk/" + "BINK"),
	("Sources/src/Scene/" + "BinkVideoPlayer.cpp"),
	("Sources/src/Scene/" + "BinkVideoPlayer.h")
)

foreach ($path in $forbiddenPaths) {
	if (Test-Path (Join-Path $root $path)) {
		throw "Proprietary video removal incomplete: '$path' still exists."
	}
}

$filesToScan = @(
	"Sources/src/Scene/Scene.vcxproj",
	"Sources/src/Scene/Scene.vcxproj.filters",
	"Sources/src/Scene/VideoPlayer.cpp",
	"Sources/src/Game/Game.vcxproj",
	".vscode/c_cpp_properties.json",
	".vscode/settings.json"
)

foreach ($relativePath in $filesToScan) {
	$path = Join-Path $root $relativePath
	if (!(Test-Path $path)) {
		continue
	}
	$content = Get-Content -Raw -Path $path
	$removedBackendPattern = ("BinkVideoPlayer|CBinkVideoPlayer|binkw32|sdk[\\/]+" + "BINK|<bink\.h>|binkw32\.lib")
	if ($content -match $removedBackendPattern) {
		throw "Proprietary video removal incomplete: '$relativePath' still references the removed backend or SDK."
	}
}

$bikFiles = @(Get-ChildItem -Path (Join-Path $root "Data/movies") -Recurse -Filter "*.bik" -File -ErrorAction SilentlyContinue)
if ($bikFiles.Count -ne 0) {
	throw "Proprietary video removal incomplete: Data/movies still contains $($bikFiles.Count) legacy video file(s)."
}

Write-Host "Proprietary video removal checks passed."
