#define _CRT_SECURE_NO_WARNINGS

#if !defined(AFX_STDAFX_H__35445E9A_4A0A_4143_8364_A56C7AF079D3__INCLUDED_)
#define AFX_STDAFX_H__35445E9A_4A0A_4143_8364_A56C7AF079D3__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define VC_EXTRALEAN		// Exclude rarely-used stuff from Windows headers


#include <assert.h>

#include <afxwin.h>         // MFC core and standard components
#include <afxext.h>         // MFC extensions
#include <afxdtctl.h>		// MFC support for Internet Explorer 4 Common Controls
#ifndef _AFX_NO_AFXCMN_SUPPORT
#include <afxcmn.h>			// MFC support for Windows Common Controls
#include <comutil.h>

#endif // _AFX_NO_AFXCMN_SUPPORT

#pragma warning( disable : 4503 4018 4786 4800 4290 4146 4244 4284 )
#include <algorithm>
#include <list>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <queue>
typedef UINT FAR *LPUINT;
typedef __int64 int64;									// due to lack of 'long long' type support
typedef unsigned __int64 QWORD;					// quadra word
#define for if(false); else for					// to achive standard variable scope resolving, declared inside 'for'
#define STDCALL __stdcall								// to use with interface function calls
#ifndef interface
#define interface struct
#endif // interface
#if _MSC_VER > 1000
#define ONCE once
#else
#define ONCE message ""
#endif // _MSC_VER > 1000

#include "..\Misc\Basic.h"							// base interfaces
#include "..\Misc\ModernAssert.h"			// modern C++ asserts
#include "..\Misc\Tools.h"							// different usefull tools
#include "..\Misc\Geometry.h"						// geometry primitives and operations (vectors, matrix, quaternion, etc.)
#include "..\Misc\2DArray.h"						// 2-D array
#include "..\Misc\HashFuncs.h"					// different usefull hash functions
#include "..\Misc\StrProc.h"						// string processing functions

#include "..\StreamIO\Globals.h"				// globals - singleton, global var system, temp buffers, console, etc.
#include "..\StreamIO\StreamIO.h"				// stream I/O base interfaces
#include "..\StreamIO\DBIO.h"						// database I/O base interfaces
#include "..\StreamIO\StructureSaver.h"	// strucutre saver base interfaces
#include "..\StreamIO\SSHelper.h"				// strucutre saver helper classes
#include "..\StreamIO\DTHelper.h"				// data tree helper classes

#include "..\Main\GameTimer.h"
#include "..\Main\GameDB.h"

#include "Specific.h"

#endif // !defined(AFX_STDAFX_H__35445E9A_4A0A_4143_8364_A56C7AF079D3__INCLUDED_)
