param(
    [string]$InstallDir = "zig-out/game/windows/x86_64/debug",
    [int]$StartupTimeoutSeconds = 3
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "../..")
$gamePath = Join-Path (Join-Path $repoRoot $InstallDir) "Game.exe"

if (-not (Test-Path -LiteralPath $gamePath)) {
    throw "Missing game executable: $gamePath"
}

$game = Start-Process -FilePath $gamePath -WorkingDirectory (Split-Path -Parent $gamePath) -PassThru
try {
    if ($game.WaitForExit($StartupTimeoutSeconds * 1000)) {
        throw "Game.exe exited during startup with code $($game.ExitCode)."
    }
}
finally {
    if (-not $game.HasExited) {
        Stop-Process -Id $game.Id -Force
        $game.WaitForExit()
    }
}
