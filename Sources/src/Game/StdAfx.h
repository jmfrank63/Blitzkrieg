#define _CRT_SECURE_NO_WARNINGS





#if !defined(AFX_STDAFX_H__A9DB83DB_A9FD_11D0_BFD1_444553540000__INCLUDED_)
#define AFX_STDAFX_H__A9DB83DB_A9FD_11D0_BFD1_444553540000__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define _NOTHREADS 1
#if defined(_WIN32) || defined(_WIN64)
#define WIN32_LEAN_AND_MEAN							// Exclude rarely-used stuff from Windows headers
#endif
#include <assert.h>
#ifdef GetObject
#undef GetObject
#endif // GetObject
#ifdef CreateObject
#undef CreateObject
#endif // CreateObject

#include <math.h>
#if defined(_MSC_VER)
#pragma warning( disable : 4503 4018 4786 4800 4290 4146 4244 4284 )
#endif
#include <algorithm>
#include <list>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <queue>
using int64 = long long;
#ifndef interface
#define interface struct
#endif // interface
#if _MSC_VER > 1000
#define ONCE once
#else
#define ONCE message ""
#endif // _MSC_VER > 1000
#ifdef _DEBUG
#ifndef ASSERT
#define ASSERT( x ) assert( x )
#endif // ASSERT
#else
#ifndef ASSERT
#define ASSERT( x )
#endif // ASSERT
#endif // _DEBUG

















#include "Specific.h"



#endif // !defined(AFX_STDAFX_H__A9DB83DB_A9FD_11D0_BFD1_444553540000__INCLUDED_)

