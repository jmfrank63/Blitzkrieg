cd $PSScriptRoot\zig-out\game
.\Game.exe 2>&1
$exitcode = $LASTEXITCODE
Write-Host "Exit code: $exitcode"