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
- **2024-05-18**: Fixed all narrowing conversions in ShaderParser.cpp - cast RULE_* and D3D* enum constants and -1 literals to DWORD in SPair arrays and local DWORD arrays
- **2024-05-18**: Fixed SceneDraw.cpp template - added `typename` keyword for dependent type `TContainer::value_type` in Resize2Fit function
- **2024-05-18**: Fixed FMOD 3.75 API calls - replaced non-existent `FSOUND_Sample_SetLoopMode` with correct `FSOUND_Sample_SetMode` function
- **2024-05-18**: Fixed multiple FMOD 3.75 API issues in SoundEngine.cpp - removed non-existent GEOMETRY caps, fixed listener functions (removed Listener_ prefix), changed Stream_OpenFile to Stream_Open with 4 params, fixed SetSynchCallback typo to SetSyncCallback, fixed callback signature (userdata is void* not int)
, added F_CALLBACKAPI calling convention to callback function
- **2024-05-18**: Fixed Cursor.cpp narrowing conversions - cast float to LONG in RECT initialization
- **2024-05-18**: Fixed SceneDraw.cpp CRawBuffer - changed from new TYPE[] to ::operator new for raw memory allocation (no default construction required), added destructor with ::operator delete
- **2024-05-18**: Fixed RiverBuilder.cpp - changed push_back() to emplace_back() for deque
- **2024-05-18**: Fixed TerrainInternal.cpp - changed unordered_map::resize() to reserve()
- **2024-05-18**: Fixed ObjVisObj.h - moved CObjVisObj typedef to protected section for derived class access
- **2024-05-18**: Fixed GFXHelper.h vertex structs - added explicit default constructors to SGFXTLVertex and SGFXLineVertex to support std::vector usage with types containing unions with CVec members
