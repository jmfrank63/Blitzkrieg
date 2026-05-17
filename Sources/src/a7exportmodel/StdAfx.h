// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#if !defined(AFX_STDAFX_H__A9DB83DB_A9FD_11D0_BFD1_444553540000__INCLUDED_)
#define AFX_STDAFX_H__A9DB83DB_A9FD_11D0_BFD1_444553540000__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers

#include <windows.h>
#include <objbase.h>
#include <comdef.h>
#include <assert.h>

typedef __int64 int64;									// due to lack of 'long long' type support
#define for if(false); else for					// to achive standard resolving area of the variables, declared inside 'for'
#define STDCALL __stdcall								// to use with interface function calls
//
#define ASSERT( a ) _ASSERT( a )
//
#pragma warning( disable : 4503 4018 4786 4800 4290 4146 4244 )
#include <algorithm>
#include <list>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <crtdbg.h>
#include <unordered_map>
#include <unordered_set>
#include <queue>
// define pragma once
#if _MSC_VER > 1000
#define ONCE once
#else
#define ONCE message ""
#endif // _MSC_VER > 1000
//

#include "..\BugSlay\BugSlayer.h"
#include "..\Misc\Basic.h"							// base interfaces
#include "..\Misc\Tools.h"							// different usefull tools
#include "..\Misc\Geometry.h"						// geometry primitives and operations (vectors, matrix, quaternion, etc.)
#include "..\Misc\2DArray.h"						// 2-D array
#include "..\Misc\HashFuncs.h"					// different usefull hash functions
#include "..\Misc\StrProc.h"						// string processing

#include "..\StreamIO\Globals.h"				// globals - singleton, global var system, temp buffers, console, etc.
#include "..\StreamIO\StreamIO.h"				// stream I/O base interfaces
#include "..\StreamIO\DBIO.h"						// database I/O base interfaces
#include "..\StreamIO\StructureSaver.h"	// strucutre saver base interfaces
#include "..\StreamIO\SSHelper.h"				// strucutre saver helper classes
#include "..\StreamIO\DTHelper.h"				// data tree helper classes

#include "..\Formats\fmtMesh.h"
#include "data.h"

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_STDAFX_H__A9DB83DB_A9FD_11D0_BFD1_444553540000__INCLUDED_)
