$ErrorActionPreference = "Stop"

$root = Resolve-Path (Join-Path $PSScriptRoot "..\..")

function Assert-NotContains($path, $pattern, $message) {
    $fullPath = Join-Path $root $path
    $content = Get-Content -LiteralPath $fullPath -Raw
    if ($content -match $pattern) {
        throw "$message ($path)"
    }
}

if (Test-Path (Join-Path $root "Sources\src\GameSpy")) {
    throw "The in-tree GameSpy SDK folder must not exist (Sources\src\GameSpy)"
}

Assert-NotContains "Sources\src\A7.sln" "GameSpy\.vcxproj" "The solution runtime graph must not build the GameSpy project"
Assert-NotContains "Sources\src\Main\Main.vcxproj" "GameSpy\.lib|GameSpy\.vcxproj" "Main must not link or reference GameSpy"
Assert-NotContains "Sources\src\Net\Net.vcxproj" "GameSpy\.lib|GameSpy\.vcxproj" "Net must not link or reference GameSpy"
Assert-NotContains "Sources\src\editor\editor.vcxproj" "GameSpy\.lib|GameSpy\.vcxproj" "Editor must not link or reference GameSpy"
Assert-NotContains "Sources\src\MapEditor\MapEditor.vcxproj" "GameSpy\.lib|GameSpy\.vcxproj" "MapEditor must not link or reference GameSpy"

Assert-NotContains "Sources\src\Main\GameSpyChat.h" "\.\.\\GameSpy" "Main chat headers must not include GameSpy SDK headers"
Assert-NotContains "Sources\src\Main\GameSpyPeerChat.h" "\.\.\\GameSpy" "Main peer chat headers must not include GameSpy SDK headers"
Assert-NotContains "Sources\src\Net\GSServersList.h" "\.\.\\GameSpy" "Net server-list headers must not include GameSpy SDK headers"
Assert-NotContains "Sources\src\Net\GSQueryReportingDriver.h" "\.\.\\GameSpy" "Net query-reporting headers must not include GameSpy SDK headers"

Write-Host "GameSpy runtime/build graph check passed."
