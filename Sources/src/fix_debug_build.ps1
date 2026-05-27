# PowerShell script to fix common build issues in VS project files
# Fixes:
# 1. /ZI and /Gy- incompatibility (change EditAndContinue to ProgramDatabase)
# 2. Deprecated /Gm option (remove MinimalRebuild)

Write-Host "Searching for project files..." -ForegroundColor Cyan

$projectFiles = Get-ChildItem -Path "." -Filter "*.vcxproj" -Recurse

if ($projectFiles.Count -eq 0) {
	Write-Host "No .vcxproj files found. Make sure you're running this from the solution directory." -ForegroundColor Red
	exit
}

Write-Host "Found $($projectFiles.Count) project file(s)`n" -ForegroundColor Green

foreach ($file in $projectFiles) {
	Write-Host "Processing: $($file.Name)"

	$content = Get-Content $file.FullName -Raw
	$modified = $false

	# Fix 1: Change EditAndContinue to ProgramDatabase to avoid /ZI and /Gy- conflict
	if ($content -match "<DebugInformationFormat>EditAndContinue</DebugInformationFormat>") {
		$content = $content -replace "<DebugInformationFormat>EditAndContinue</DebugInformationFormat>", "<DebugInformationFormat>ProgramDatabase</DebugInformationFormat>"
		Write-Host "  - Changed EditAndContinue to ProgramDatabase"
		$modified = $true
	}

	# Fix 2: Remove deprecated MinimalRebuild lines
	if ($content -match "<MinimalRebuild>.*</MinimalRebuild>") {
		$content = $content -replace "\s*<MinimalRebuild>.*</MinimalRebuild>\r?\n", ""
		Write-Host "  - Removed deprecated MinimalRebuild"
		$modified = $true
	}

	if ($modified) {
		Set-Content -Path $file.FullName -Value $content -NoNewline
		Write-Host "  => File updated" -ForegroundColor Green
	} else {
		Write-Host "  => No changes needed" -ForegroundColor Gray
	}
}

Write-Host "`nAll project files processed!" -ForegroundColor Cyan
Write-Host "Now rebuild the solution in Debug configuration." -ForegroundColor Yellow
