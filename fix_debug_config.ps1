# Fix Debug configuration issues across all projects
# This script:
# 1. Removes deprecated /Gm (MinimalRebuild)
# 2. Changes /ZI to /Zi (EditAndContinue to ProgramDatabase)
# 3. Fixes SoundVerifycation Debug output path
# 4. Ensures all projects use v145 toolset

$ErrorActionPreference = "Stop"
$projects = Get-ChildItem -Path "." -Filter "*.vcxproj" -Recurse
$fixedCount = 0
$changedFiles = @()

Write-Host "Found $($projects.Count) project files to process`n"

foreach ($proj in $projects) {
    try {
        $content = Get-Content $proj.FullName -Raw -Encoding UTF8
        $originalContent = $content
        $modified = $false

        # Fix 1: Remove deprecated MinimalRebuild (option 'Gm')
        if ($content -match '<MinimalRebuild>true</MinimalRebuild>') {
            $content = $content -replace '<MinimalRebuild>true</MinimalRebuild>', '<MinimalRebuild>false</MinimalRebuild>'
            $modified = $true
        }

        # Fix 2: Change EditAndContinue (/ZI) to ProgramDatabase (/Zi)
        # This fixes the '/ZI' and '/Gy-' incompatibility
        if ($content -match '<DebugInformationFormat>EditAndContinue</DebugInformationFormat>') {
            $content = $content -replace '<DebugInformationFormat>EditAndContinue</DebugInformationFormat>', '<DebugInformationFormat>ProgramDatabase</DebugInformationFormat>'
            $modified = $true
        }

        # Fix 3: Fix v143 toolset references (change to v145)
        if ($content -match '<PlatformToolset>v143</PlatformToolset>') {
            $content = $content -replace '<PlatformToolset>v143</PlatformToolset>', '<PlatformToolset>v145</PlatformToolset>'
            $modified = $true
        }

        # Fix 4: Special fix for SoundVerifycation Debug output path
        if ($proj.Name -eq "SoundVerifycation.vcxproj") {
            # Fix the Debug configuration's OutputFile that's currently pointing to Release
            $debugLinkPattern = "(<ItemDefinitionGroup Condition=`"'\`$\(Configuration\)\|\`$\(Platform\)'=='Debug\|Win32'`">.*?<Link>.*?<OutputFile>)\.\\Release\\(SoundVerifycation\.exe</OutputFile>)"
            if ($content -match $debugLinkPattern) {
                $content = $content -replace $debugLinkPattern, '$1.\Debug\$2'
                $modified = $true
            }
        }

        if ($modified) {
            # Backup original
            $backupPath = "$($proj.FullName).bak"
            $originalContent | Set-Content $backupPath -NoNewline -Encoding UTF8

            # Write fixed content
            $content | Set-Content $proj.FullName -NoNewline -Encoding UTF8
            $fixedCount++
            $changedFiles += $proj.FullName
            Write-Host "✓ Fixed: $($proj.Name)" -ForegroundColor Green
        }
    }
    catch {
        Write-Host "✗ Error processing $($proj.Name): $_" -ForegroundColor Red
    }
}

Write-Host "`n========================================="
Write-Host "Summary:" -ForegroundColor Cyan
Write-Host "  Total projects processed: $($projects.Count)"
Write-Host "  Projects fixed: $fixedCount" -ForegroundColor Green
Write-Host "========================================="

if ($changedFiles.Count -gt 0) {
    Write-Host "`nChanged files:" -ForegroundColor Yellow
    $changedFiles | ForEach-Object { Write-Host "  $_" }
    Write-Host "`nBackup files created with .bak extension" -ForegroundColor Yellow
    Write-Host "You can remove backups with: Get-ChildItem -Recurse -Filter '*.vcxproj.bak' | Remove-Item" -ForegroundColor Gray
}

Write-Host "`nDone! You can now try building in Debug mode." -ForegroundColor Cyan
