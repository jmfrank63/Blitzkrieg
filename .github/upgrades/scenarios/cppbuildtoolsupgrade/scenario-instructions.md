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
- **2024-05-18**: Removed obsolete STLport configuration from PlanePathTest/stdafx.h - removed stl_user_config.h include and _STLP_USE_MFC define (using standard Visual Studio STL)
- **2024-05-18**: Fixed AICellsTiles.h - added missing AIInternalConsts.h include for SConsts definition
- **2024-05-18**: Fixed AILogic/UpdatableObject.h - replaced obsolete `std::construct( pRangeArea );` with placement new `new (pRangeArea) SShootAreas();`
- **2024-05-18**: Fixed AIGeometry.h SRect - added explicit `SRect() = default;` constructor (union with CVec2 deleted implicit default constructor)
- **2024-05-18**: Fixed PlanePathTest CManuver.cpp - removed nonexistent `pObj->vB2Pos = vPos;` line (vB2Pos member doesn't exist)
- **2024-05-18**: Fixed PlanePathTest AI_PLANE_MANUVER constants - defined AI_PLANE_MANUVER_GENERIC (1000) and AI_PLANE_MANUVER_GORKA (1001) in CManuverBuilder.h and CManuverContainer.h
- **2024-05-18**: Fixed PlanePathTest CManuverContainer.cpp - added AIInternalConsts.h include for SConsts::PLANE_MIN_HEIGHT and Win32Random.h include for NWin32Random::Random()
- **2024-05-18**: Fixed ComplexPathFraction.cpp - removed duplicate BASIC_REGISTER_CLASS(CPathFractionArcLine3D) registration
- **2024-05-18**: Fixed ComplexPathFraction.cpp tangent-point code - converted CVec3 x1t to CVec2 for FindTangentPoints call and vTangent1 calculation
- **2024-05-18**: Fixed PlanePathTest.cpp - removed obsolete Enable3dControls() and Enable3dControlsStatic() calls (no longer needed in modern MFC)
