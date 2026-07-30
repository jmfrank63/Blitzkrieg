[CmdletBinding()]
param(
    [ValidateSet('Build', 'Verify')]
    [string]$Mode = 'Build',
    [string]$OutputRoot = (Join-Path $PSScriptRoot '..\..\zig-out\tools\shadercross')
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$shaderCrossCommit = 'e55cf5e31ced6f3d1be5cc6d0c50e99384f9f4ba'
$shaderCrossRepo = 'https://github.com/libsdl-org/SDL_shadercross.git'
$dxcUrl = 'https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.9.2607/dxc_2026_07_29.zip'
$dxcSha256 = 'a1dfb116ba3eeae6a1582291b53a8e7bf65ad760676bd3194685c8f7367cd241'
$sdlTag = 'release-3.2.20'
$sdlCommit = '96292a5b464258a2b926e0a3d72f8b98c2a81aa6'
$sdlRepo = 'https://github.com/libsdl-org/SDL.git'

$output = if ([System.IO.Path]::IsPathRooted($OutputRoot)) {
    [System.IO.Path]::GetFullPath($OutputRoot)
} else {
    [System.IO.Path]::GetFullPath((Join-Path (Get-Location) $OutputRoot))
}
$sourceRoot = Join-Path $output 'src'
$sdlSource = Join-Path $output 'sdl-src'
$dxcArchive = Join-Path $output 'dxc.zip'
$dxcRoot = Join-Path $output 'dxc'
$sdlInstall = Join-Path $output 'sdl-install'
$spirvInstall = Join-Path $output 'spirv-install'
$buildDir = Join-Path $output 'build'
$installDir = Join-Path $output 'install'
$prefixPath = ($sdlInstall, $spirvInstall) -join ';'

function Invoke-Native([string]$File, [string[]]$Arguments) {
    & $File @Arguments
    if ($LASTEXITCODE -ne 0) { throw "Command failed ($LASTEXITCODE): $File $($Arguments -join ' ')" }
}

function Initialize-VisualStudioEnvironment {
    if (Get-Command rc.exe -ErrorAction SilentlyContinue) { return }
    $vsDevCmd = Get-ChildItem 'C:\Program Files\Microsoft Visual Studio' -Recurse -Filter 'VsDevCmd.bat' -File -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($null -eq $vsDevCmd) { throw 'VsDevCmd.bat was not found; install the Visual Studio C++ and Windows SDK workloads' }
    $lines = & cmd.exe /d /s /c "`"$($vsDevCmd.FullName)`" -arch=x64 -host_arch=x64 && set"
    if ($LASTEXITCODE -ne 0) { throw 'Visual Studio developer environment initialization failed' }
    foreach ($line in $lines) {
        if ($line -match '^([^=]+)=(.*)$') { Set-Item -Path ("Env:{0}" -f $Matches[1]) -Value $Matches[2] }
    }
    if (-not (Get-Command rc.exe -ErrorAction SilentlyContinue)) { throw 'Visual Studio environment did not expose rc.exe' }
}

function Ensure-GitSource([string]$Path, [string]$Repository, [string]$Commit) {
    if (-not (Test-Path (Join-Path $Path '.git'))) {
        New-Item -ItemType Directory -Force -Path (Split-Path $Path) | Out-Null
        Invoke-Native 'git' @('clone', '--recurse-submodules', $Repository, $Path)
    }
    Invoke-Native 'git' @('-C', $Path, 'fetch', '--depth', '1', 'origin', $Commit)
    Invoke-Native 'git' @('-C', $Path, 'checkout', '--detach', $Commit)
    Invoke-Native 'git' @('-C', $Path, 'submodule', 'update', '--init', '--recursive')
}

function Ensure-Dxc {
    if (-not (Test-Path (Join-Path $dxcRoot 'bin\x64\dxcompiler.dll'))) {
        New-Item -ItemType Directory -Force -Path $dxcRoot | Out-Null
        if (-not (Test-Path $dxcArchive)) { Invoke-WebRequest -Uri $dxcUrl -OutFile $dxcArchive }
        $actual = (Get-FileHash -LiteralPath $dxcArchive -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($actual -ne $dxcSha256) { throw "DXC SHA-256 mismatch: $actual" }
        Expand-Archive -LiteralPath $dxcArchive -DestinationPath $dxcRoot -Force
    }
}

function Invoke-CMake([string]$Source, [string]$Build, [string]$Install, [string[]]$Extra) {
    New-Item -ItemType Directory -Force -Path $Build, $Install | Out-Null
    $args = @('-S', $Source, '-B', $Build, '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release', "-DCMAKE_INSTALL_PREFIX=$Install") + $Extra
    Invoke-Native 'cmake' $args
    Invoke-Native 'cmake' @('--build', $Build, '--target', 'install', '--parallel')
}

if ($Mode -eq 'Verify') {
    $tool = Join-Path $installDir 'bin\shadercross.exe'
    if (-not (Test-Path $tool)) { throw "shadercross is not built: $tool" }
    $help = (& $tool '--help' 2>&1 | Out-String)
    if ($LASTEXITCODE -ne 0) { throw 'shadercross --help failed' }
    foreach ($option in @('-s', '-d', '-t', '-e', '-I', '-o', '-D')) {
        if ($help -notmatch [regex]::Escape($option)) { throw "shadercross help is missing option $option" }
    }
    Write-Output "shadercross help verified: $tool"
    exit 0
}

New-Item -ItemType Directory -Force -Path $output | Out-Null
Initialize-VisualStudioEnvironment
Ensure-GitSource $sourceRoot $shaderCrossRepo $shaderCrossCommit
Ensure-GitSource $sdlSource $sdlRepo $sdlCommit
Ensure-Dxc

Invoke-CMake $sdlSource (Join-Path $output 'sdl-build') $sdlInstall @('-DSDL_TESTS=OFF', '-DSDL_EXAMPLES=OFF', '-DSDL_TEST_LIBRARY=OFF')
Invoke-CMake (Join-Path $sourceRoot 'external\SPIRV-Cross') (Join-Path $output 'spirv-build') $spirvInstall @('-DSPIRV_CROSS_CLI=OFF', '-DSPIRV_CROSS_SHARED=ON', '-DSPIRV_CROSS_STATIC=OFF', '-DSPIRV_CROSS_ENABLE_TESTS=OFF')

$env:GFXGPU_SHADERCROSS_BUILD_DIR = $buildDir
$env:GFXGPU_SHADERCROSS_INSTALL_DIR = $installDir
$env:GFXGPU_SHADERCROSS_PREFIX_PATH = $prefixPath
$env:GFXGPU_SHADERCROSS_SDL3_DIR = Join-Path $sdlInstall 'lib\cmake\SDL3'
$env:GFXGPU_SHADERCROSS_DXC_DIR = $dxcRoot
Invoke-Native 'cmake' @(
    '-S', $sourceRoot,
    '-B', $buildDir,
    '-G', 'Ninja',
    '-DCMAKE_BUILD_TYPE=Release',
    "-DCMAKE_INSTALL_PREFIX=$installDir",
    "-DCMAKE_PREFIX_PATH=$prefixPath",
    "-DSDL3_DIR=$env:GFXGPU_SHADERCROSS_SDL3_DIR",
    "-DDirectXShaderCompiler_ROOT=$env:GFXGPU_SHADERCROSS_DXC_DIR",
    '-DSDLSHADERCROSS_DXC=ON',
    '-DSDLSHADERCROSS_SHARED=ON',
    '-DSDLSHADERCROSS_STATIC=OFF',
    '-DSDLSHADERCROSS_SPIRVCROSS_SHARED=ON',
    '-DSDLSHADERCROSS_VENDORED=OFF',
    '-DSDLSHADERCROSS_CLI=ON',
    '-DSDLSHADERCROSS_INSTALL=ON',
    '-DSDLSHADERCROSS_TESTS=OFF'
)
Invoke-Native 'cmake' @('--build', $buildDir, '--target', 'install', '--parallel')

$toolBin = Join-Path $installDir 'bin'
Copy-Item -LiteralPath (Join-Path $sdlInstall 'bin\SDL3.dll') -Destination $toolBin -Force
Copy-Item -LiteralPath (Join-Path $spirvInstall 'bin\spirv-cross-c-shared.dll') -Destination $toolBin -Force
Copy-Item -LiteralPath (Join-Path $dxcRoot 'bin\x64\dxcompiler.dll') -Destination $toolBin -Force
Copy-Item -LiteralPath (Join-Path $dxcRoot 'bin\x64\dxil.dll') -Destination $toolBin -Force
& $PSCommandPath -Mode Verify -OutputRoot $OutputRoot
