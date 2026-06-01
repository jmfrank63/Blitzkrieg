# Copilot Instructions

## Project Overview

This is the **Blitzkrieg** game source code - a legacy Visual Studio C++ project (originally VS6, upgraded to VS2022 with v145 toolset) that compiles cleanly in Visual Studio 2022 Insiders. The codebase is being transitioned to support compilation via **VSCode Insiders** while preserving compatibility with Visual Studio.

**Critical:** This is a **legacy Windows codebase** with CRLF line endings. Protective measures are in place to prevent mass line ending changes.

## Project Structure

- **Main Solution:** `Sources/src/A7.sln` (35+ projects)
- **Main Executable:** `Sources/src/Game/Game.vcxproj` → builds `Game.exe`
- **Editor:** `Sources/src/ELK/ELK.vcxproj` → builds the level editor
- **SDKs:** `Sources/sdk/` contains third-party libraries (FMOD, BINK, STINGRAY, Maya, stlport, etc.)
- **Game Data:** `Data/` directory (linked to build output via junction)

## Build System

### MSBuild via Visual Studio 2022
- Uses MSBuild with MSVC v145 toolset (Visual Studio 2022)
- Build automation via `Directory.Build.props` and `Directory.Build.targets`
- Multiple configurations: Debug, Release, BetaRelease, FastDebug, Profiler, Template

### Key Build Features
- **Post-build automation:** DLLs automatically copied to `Game/$(Configuration)/` output directory
- **Data linking:** Build creates junction from output folder to `Data/` directory
- **Precompiled headers:** Most projects use `StdAfx.h`

### Build Scripts
- **`fix_debug_config.ps1`** - Updates projects for VS2022 compatibility (removes deprecated options, updates toolset)
- **`fix_basic_ptr.ps1`** - Fixes CPtr comparison operators in `Misc/Basic.h`
- **`Sources/src/clear.bat`** - Cleans build artifacts

## Dependencies

### External SDKs (Require Separate Licensing)
Per [README.md](../README.md), these are **not included** in the repository:
- **FMOD** (audio library) - `Sources/sdk/FMOD/`
- **BINK** (video codec) - `Sources/sdk/BINK/`
- **STINGRAY Studio 2003** (UI library) - `Sources/sdk/STINGRAY/`

All external SDKs are available in separate GitHub repositories under this organization.

### Included SDKs
- **stlport** - Custom STL implementation (must be first in include paths!)
- **S3TC** - Texture compression
- **GOLDParser** - Parser framework
- **Maya 4.0** - 3D model export tools

### System Requirements
- **Windows 10 SDK** (WINVER=0x0A00, _WIN32_WINNT=0x0A00)
- **DirectX 9** SDK
- **MSVC Build Tools** (v145 or Visual Studio 2022)

## Protective Measures (IMPORTANT!)

### Line Ending Protection
The codebase uses **CRLF line endings** (Windows standard). Protective configuration files prevent VSCode/Copilot from converting line endings:

- **`.gitattributes`** - Enforces CRLF at git level
- **`.editorconfig`** - Universal editor configuration
- **`.vscode/settings.json`** - VSCode-specific: disables auto-formatting, preserves whitespace/line endings

### Git Configuration
Before working on this codebase:
```bash
git config core.autocrlf false
```

This prevents git from auto-converting line endings.

## Coding Guidelines

### DO NOT
- ❌ Auto-format existing code (formatting is disabled by design)
- ❌ Trim trailing whitespace (preservation is intentional)
- ❌ Convert line endings from CRLF to LF
- ❌ Change file encoding (UTF-8 BOM is used)
- ❌ Reorganize includes automatically

### DO
- ✅ Preserve existing code style and formatting
- ✅ Use CRLF line endings for all new files
- ✅ Follow existing patterns in the codebase
- ✅ Test builds after changes (`Ctrl+Shift+B` in VSCode)

## Common Issues

### Build Failures

**"Cannot find include file"**
- Verify SDK paths are correct in include directories
- Check that `stlport` is **first** in the include path order (critical!)

**"Unresolved external symbols"**
- Check that external SDKs (FMOD, BINK, STINGRAY) are available
- Verify library paths are configured correctly

**"PrecompiledHeader: StdAfx.h not found"**
- Most projects use precompiled headers
- Ensure `StdAfx.h` and `StdAfx.cpp` exist in the project directory

### Line Ending Issues

**"10,000+ files changed in git"**
- This indicates line endings were converted
- Run: `git diff -w` to see if changes are only whitespace
- Reset: `git reset --hard HEAD` to discard line ending changes
- The protective files should prevent this from happening again

### WinDbgX Time Travel Debugging

**"TTD: Not found" in WinDbgX status**

WinDbgX extension requires **per-user installation** of WinDbg Preview to auto-detect TTD.

**Check if you have user installation:**
```powershell
Test-Path "$env:LOCALAPPDATA\Microsoft\WindowsApps\Microsoft.WinDbg_8wekyb3d8bbwe"
```

If this returns `False`, you need to reinstall WinDbg Preview:

**Installation Steps:**
1. Open **Microsoft Store**
2. Search for "WinDbg Preview"
3. Install (installs per-user, not system-wide)
4. This creates: `C:\Users\<Username>\AppData\Local\Microsoft\WindowsApps\Microsoft.WinDbg_8wekyb3d8bbwe\`

**VSCode Configuration (required even with user installation):**
1. Add to **user settings** (Ctrl+, → User → Edit settings.json):
   ```json
   "windbgx.cdbPath": "C:\\Users\\<YourUsername>\\AppData\\Local\\Microsoft\\WindowsApps\\cdbX64.exe"
   ```
2. Enable **"Start Automatically"** in WinDbgX extension settings

After these steps, TTD auto-detection works universally on any PC with proper installation.

## VSCode Compilation (In Progress)

The codebase is being configured for compilation via VSCode Insiders using MSBuild directly (not CMake).

### Planned VSCode Features
- Build tasks for all configurations (Debug, Release, etc.)
- IntelliSense configuration with proper include paths
- Launch configurations for debugging Game.exe and ELK.exe
- Problem matchers for MSVC compiler output

## Project Guidelines
