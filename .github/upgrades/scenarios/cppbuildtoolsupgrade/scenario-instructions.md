# MSVC Build Tools Upgrade

## Preferences
- **Flow Mode**: Guided

## Source Control
- **Source Branch**: main
- **Working Branch**: main (current branch)
- **Commit Strategy**: After Each Task

## User Preferences
### Technical Preferences
- **GOLD Parser**: Using v3.0 (registered from `Sources\sdk\GOLDParser3\GOLD Parser Engine.dll`)
- **FMOD**: v3.75f - may need adapter layer or migration to miniaudio
- **DirectX**: Keeping D3D9 enums for now (plan to migrate to Vulkan later, no intermediate D3D11 migration)

## Key Decisions Log
- **2024-05-18**: Updated Parser.cpp to use GOLD Parser v3.0 API (changed `LoadCompiledGrammar` and `OpenFile` signatures)
- **2024-05-18**: Fixed Parser.cpp PutTag call - stored `_ReductionPtr` in variable for proper COM interface resolution
- **2024-05-18**: Fixed Parser.cpp reduction variable scope - added braces around case statement
- **2024-05-18**: Updated ShaderParser.cpp to use `d3d9types.h` instead of `d3d8types.h` (D3D8→D3D9, will migrate to Vulkan later)
- **2024-05-18**: Fixed ShaderParser.cpp GetTokens calls - v3.0 API takes short by value, not pointer
- **2024-05-18**: Added missing D3D9 constants (D3DTEXF_*, D3DTSS_*) not present in Windows SDK
- **2024-05-18**: Fixed SymbolPtr to _SymbolPtr for proper COM smart pointer type
- **2024-05-18**: Fixed narrowing conversions - cast return -1 to static_cast<DWORD>(-1)
