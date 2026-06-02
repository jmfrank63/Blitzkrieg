#define _CRT_SECURE_NO_WARNINGS
#if !defined(AFX_STDAFX_H__722566A7_527F_471B_AB5D_252854280081__INCLUDED_)
#define AFX_STDAFX_H__722566A7_527F_471B_AB5D_252854280081__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define _NOTHREADS 1
#define _MBCS 1

#include <afxwin.h>											// MFC core and standard components
#include <afxext.h>											// MFC extensions
#include <afxdtctl.h>										// MFC support for Internet Explorer 4 Common Controls
#ifndef _AFX_NO_AFXCMN_SUPPORT
#include <afxcmn.h>											// MFC support for Windows Common Controls
#endif // _AFX_NO_AFXCMN_SUPPORT
#include <comutil.h>

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

#include "resource.h"


#endif // !defined(AFX_STDAFX_H__722566A7_527F_471B_AB5D_252854280081__INCLUDED_)
