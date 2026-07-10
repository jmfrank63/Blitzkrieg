$ErrorActionPreference = "Stop"

$root = Resolve-Path (Join-Path $PSScriptRoot "..\..")

function Assert-Contains($path, $pattern, $message) {
    $content = Get-Content -LiteralPath (Join-Path $root $path) -Raw
    if ($content -notmatch $pattern) {
        throw "$message ($path)"
    }
}

Assert-Contains "Data\Configs\defconf.cfg" "<KeyName>Multiplayer\.OpenSpyHost</KeyName>" "Default config must define OpenSpyHost"
Assert-Contains "Data\Configs\defconf.cfg" "<KeyName>Multiplayer\.OpenSpyMasterHost</KeyName>" "Default config must define OpenSpyMasterHost"
Assert-Contains "Data\Configs\defconf.cfg" "<KeyName>Multiplayer\.OpenSpyPeerchatHost</KeyName>" "Default config must define OpenSpyPeerchatHost"

Assert-Contains "Data\Configs\config.cfg" "<KeyName>Multiplayer\.OpenSpyHost</KeyName>" "User config must define OpenSpyHost"
Assert-Contains "Data\Configs\config.cfg" "<KeyName>Multiplayer\.OpenSpyMasterHost</KeyName>" "User config must define OpenSpyMasterHost"
Assert-Contains "Data\Configs\config.cfg" "<KeyName>Multiplayer\.OpenSpyPeerchatHost</KeyName>" "User config must define OpenSpyPeerchatHost"

Assert-Contains "Sources\src\GameSpy\GameSpyConfig.h" "Options\.Multiplayer\.OpenSpyMasterHost" "GameSpy endpoint sync must read OpenSpyMasterHost"
Assert-Contains "Sources\src\GameSpy\GameSpyConfig.h" "Options\.Multiplayer\.OpenSpyPeerchatHost" "GameSpy endpoint sync must read OpenSpyPeerchatHost"
Assert-Contains "Sources\src\GameSpy\peer\peerOperations.c" "pi_chat_server_address" "Peerchat host must be runtime-configurable"

Write-Host "OpenSpy endpoint config check passed."
