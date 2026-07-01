param(
    [string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
)

$ErrorActionPreference = "Stop"

function Assert-FileContains {
    param(
        [string]$Path,
        [string]$Pattern,
        [string]$Message
    )

    $content = Get-Content -LiteralPath $Path -Raw
    if ($content -notmatch $Pattern) {
        throw $Message
    }
}

function Assert-PathExists {
    param(
        [string]$Path,
        [string]$Message
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        throw $Message
    }
}

$sceneDir = Join-Path $RepoRoot "Sources\src\Scene"
$sceneProject = Join-Path $sceneDir "Scene.vcxproj"
$sceneFilters = Join-Path $sceneDir "Scene.vcxproj.filters"
$sceneFactory = Join-Path $sceneDir "SceneObjectFactory.cpp"

$videoHeader = Join-Path $sceneDir "VideoPlayer.h"
$videoSource = Join-Path $sceneDir "VideoPlayer.cpp"
$openHeader = Join-Path $sceneDir "OpenVideoPlayer.h"
$openSource = Join-Path $sceneDir "OpenVideoPlayer.cpp"

Assert-PathExists $videoHeader "Missing VideoPlayer.h delegating video player."
Assert-PathExists $videoSource "Missing VideoPlayer.cpp delegating video player."
Assert-PathExists $openHeader "Missing OpenVideoPlayer.h open video backend scaffold."
Assert-PathExists $openSource "Missing OpenVideoPlayer.cpp open video backend scaffold."

Assert-FileContains $sceneFactory '#include "VideoPlayer\.h"' "Scene object factory must include VideoPlayer.h."
Assert-FileContains $sceneFactory 'REGISTER_CLASS\( this,\s*SCENE_VIDEO_PLAYER,\s*CVideoPlayer\s*\)' "SCENE_VIDEO_PLAYER must register CVideoPlayer."

foreach ($file in @("VideoPlayer.cpp", "OpenVideoPlayer.cpp")) {
    Assert-FileContains $sceneProject ([regex]::Escape("<ClCompile Include=`"$file`"")) "Scene.vcxproj must compile $file."
    Assert-FileContains $sceneFilters ([regex]::Escape("<ClCompile Include=`"$file`"")) "Scene.vcxproj.filters must list $file."
}

foreach ($file in @("VideoPlayer.h", "OpenVideoPlayer.h")) {
    Assert-FileContains $sceneProject ([regex]::Escape("<ClInclude Include=`"$file`"")) "Scene.vcxproj must include $file."
    Assert-FileContains $sceneFilters ([regex]::Escape("<ClInclude Include=`"$file`"")) "Scene.vcxproj.filters must list $file."
}

Write-Host "Video backend scaffold checks passed."
