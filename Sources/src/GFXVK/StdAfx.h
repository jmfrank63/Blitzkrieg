// GFXVK precompiled header equivalent.
// Unlike the GFX DLL, GFXVK does NOT include Specific.h or any
// D3D headers.  It uses the IGFX interface types which are pure
// virtual and backend-agnostic.
#define _CRT_SECURE_NO_WARNINGS
#define _NOTHREADS 1
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <comutil.h>
#include <assert.h>
#include <math.h>
#pragma warning( disable : 4503 4018 4786 4800 4290 4146 4244 4284 )
#include <typeinfo>
#include <algorithm>
#include <list>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <queue>
typedef __int64 int64;
typedef unsigned __int64 QWORD;
#define for if(false); else for
#define STDCALL __stdcall
#ifndef interface
#define interface struct
#endif

#include "../../Misc/Basic.h"
#include "../../Misc/ModernAssert.h"
#include "../../Misc/Tools.h"
#include "../../Misc/Geometry.h"
#include "../../Misc/2DArray.h"
#include "../../Misc/HashFuncs.h"
#include "../../Misc/StrProc.h"
#include "../../Misc/Win32Helper.h"

#include "../../StreamIO/Globals.h"
#include "../../StreamIO/StreamIO.h"
#include "../../StreamIO/DBIO.h"
#include "../../StreamIO/StructureSaver.h"
#include "../../StreamIO/SSHelper.h"
#include "../../StreamIO/DTHelper.h"

#include "../../Main/GameTimer.h"
#include "../../Main/GameDB.h"

// GFX types — pure interface, no D3D dependency
#include "../../GFX/GFX.h"
#include "../../GFX/GFXTypes.h"
#include "../../GFX/GFXHelper.h"
#include "../../GFX/CommonStructs.h"

// Manager classes — backend-agnostic
#include "../../GFX/TextureManager.h"
#include "../../GFX/FontManager.h"
#include "../../GFX/GeometryManager.h"
#include "../../GFX/VideoCheck.h"
