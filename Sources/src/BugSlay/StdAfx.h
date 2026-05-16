// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#if !defined(AFX_STDAFX_H__A9DB83DB_A9FD_11D0_BFD1_444553540000__INCLUDED_)
#define AFX_STDAFX_H__A9DB83DB_A9FD_11D0_BFD1_444553540000__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

// STLport configuration defines
#define _NOTHREADS 1
#define _STLP_DEBUG_TERMINATE 1
#define _STLP_DEBUG_MESSAGE 1
//
#ifndef __AFX__

#include <Winsock2.h>
#include <comutil.h>
#include <assert.h>

#include <afxwin.h>											// MFC core and standard components
#include <afxext.h>											// MFC extensions
#include <afxdtctl.h>										// MFC support for Internet Explorer 4 Common Controls
#ifndef _AFX_NO_AFXCMN_SUPPORT
#include <afxcmn.h>											// MFC support for Windows Common Controls
#endif // _AFX_NO_AFXCMN_SUPPORT
#include <comutil.h>
// undef some Windows API defines, like GetObject and CreateObject
#ifdef GetObject
#undef GetObject
#endif // GetObject
#ifdef CreateObject
#undef CreateObject
#endif // CreateObject
#endif // __AFX__

//#include <stdlib.h>
#include <math.h>
// STL headers
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
//
typedef __int64 int64;									// due to lack of 'long long' type support
typedef unsigned __int64 QWORD;					// quadra word
#define for if(false); else for					// to achive standard variable scope resolving, declared inside 'for'
#define STDCALL __stdcall								// to use with interface function calls
// define 'interface' keyword
#ifndef interface
#define interface struct
#endif // interface
// define pragma once
#if _MSC_VER > 1000
#define ONCE once
#else
#define ONCE message ""
#endif // _MSC_VER > 1000
// define ASSERT for easy access and common usage. in the non-_DEBUG mode turn it off
#ifdef _DEBUG
#ifndef ASSERT
#define ASSERT( x ) assert( x )
#endif // ASSERT
#else
#ifndef ASSERT
#define ASSERT( x )
#endif // ASSERT
#endif // _DEBUG
//
#include "..\Misc\Basic.h"							// base interfaces
#include "..\BugSlay\BugSlayer.h"				// cool asserts :)
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
// in the file 'Specific.h' one can define own project-specific includes
#include "Specific.h"

// TODO: reference additional headers your program requires here

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_STDAFX_H__A9DB83DB_A9FD_11D0_BFD1_444553540000__INCLUDED_)
