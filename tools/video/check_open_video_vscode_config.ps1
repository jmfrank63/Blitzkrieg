$ErrorActionPreference = "Stop"

$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$tasksPath = Join-Path $root ".vscode/tasks.json"
$launchPath = Join-Path $root ".vscode/launch.json"

$tasks = Get-Content -Raw -Path $tasksPath
$launch = Get-Content -Raw -Path $launchPath

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

Assert-Contains $tasks '"label"\s*:\s*"Build Scene \(Debug, Open Video\)"' "Missing Build Scene (Debug, Open Video) task."
Assert-Contains $tasks '"\$\{workspaceFolder\}/Sources/src/Scene/Scene\.vcxproj"' "Open Video task must build Scene.vcxproj."
Assert-Contains $tasks '"label"\s*:\s*"Build Game \(Debug, Open Video\)"' "Missing Build Game (Debug, Open Video) task."
Assert-Contains $tasks '"Build Scene \(Debug, Open Video\)"' "Open Video game task must depend on the Scene open video task."
Assert-NotContains $tasks 'Open Audio' "VS Code tasks still reference Open Audio."
Assert-NotContains $tasks 'AudioBackend=Open' "VS Code tasks still pass AudioBackend=Open."

Assert-Contains $launch '"name"\s*:\s*"WinDbg: Debug Game\.exe \(Open Video\)"' "Missing WinDbg Open Video launch configuration."
Assert-Contains $launch '"name"\s*:\s*"Debug Game\.exe \(Open Video\)"' "Missing cppvsdbg Open Video launch configuration."
Assert-Contains $launch '"preLaunchTask"\s*:\s*"Build Game \(Debug, Open Video\)"' "Open Video launch configs must use the Open Video build task."
Assert-NotContains $launch 'Open Audio' "VS Code launch configs still reference Open Audio."

Write-Host "Open Video VS Code configuration checks passed."
