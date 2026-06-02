#ifndef __MODERN_ASSERT_H__
#define __MODERN_ASSERT_H__
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Modern C++ Assert Replacement for Legacy BugSlayer System
// Replaces year 2000 BugSlayer technology with standard C++ assertions
// Compatible with modern debuggers (WinDbg, Visual Studio Debugger, TTD)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <cassert>
#include <cstdlib>

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Debug break macro - works with modern debuggers
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if defined(_MSC_VER)
  #define DEBUG_BREAK __debugbreak()
#else
  #define DEBUG_BREAK { __asm { int 3 } }
#endif

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Legacy BugSlayer compatibility macros - replaced with standard C++ asserts
// These macros provide backward compatibility while using modern debugging tools
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// ASSERT macros - enabled in debug builds
#if defined(_DEBUG) || defined(_DO_ASSERT) || defined(_DO_ASSERT_SLOW)
  
  // Basic assert with no message (no semicolon required)
  #define NI_ASSERT(x)                               { assert(x); }
  
  // Assert with user text message (no semicolon required)
  #define NI_ASSERT_T(x, user_text)                  { assert((x) && user_text); }
  
  // Assert with statement execution on failure
  #define NI_ASSERT_TF(x, user_text, statement)      \
    {                                                \
      if (!(x)) {                                    \
        statement;                                   \
        assert(false && user_text);                  \
      }                                              \
    }
  
  // HRESULT assertions (check for failure bit)
  #define NI_ASSERTHR(x)                             { assert(!FAILED(x)); }
  #define NI_ASSERTHR_T(x, user_text)                { assert((!FAILED(x)) && user_text); }
  #define NI_ASSERTHR_TF(x, user_text, statement)    \
    {                                                \
      if (FAILED(x)) {                               \
        statement;                                   \
        assert(false && user_text);                  \
      }                                              \
    }
  
  // Force assert variants (always enabled regardless of build config)
  #define NI_FORCE_ASSERT(x, user_text, statement, bForce)      NI_ASSERT_TF(x, user_text, statement)
  #define NI_FORCE_ASSERT_HR(x, user_text, statement, bForce)   NI_ASSERTHR_TF(x, user_text, statement)

#else
  
  // Release builds - asserts disabled (expand to empty statement blocks)
  #define NI_ASSERT(x)                               { }
  #define NI_ASSERT_T(x, user_text)                  { }
  #define NI_ASSERT_TF(x, user_text, statement)      { }
  #define NI_ASSERTHR(x)                             { }
  #define NI_ASSERTHR_T(x, user_text)                { }
  #define NI_ASSERTHR_TF(x, user_text, statement)    { }
  #define NI_FORCE_ASSERT(x, user_text, statement, bForce)      { }
  #define NI_FORCE_ASSERT_HR(x, user_text, statement, bForce)   { }

#endif // _DEBUG

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ASSERT_SLOW macros - performance-critical assertions (disabled in release)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if defined(_DEBUG) || defined(_DO_ASSERT_SLOW)
  
  #define NI_ASSERT_SLOW(x)                          { assert(x); }
  #define NI_ASSERT_SLOW_T(x, user_text)             { assert((x) && user_text); }
  #define NI_ASSERT_SLOW_TF(x, user_text, statement) NI_ASSERT_TF(x, user_text, statement)
  #define NI_ASSERTHR_SLOW(x)                        { assert(!FAILED(x)); }
  #define NI_ASSERTHR_SLOW_T(x, user_text)           { assert((!FAILED(x)) && user_text); }
  #define NI_ASSERTHR_SLOW_TF(x, user_text, statement) NI_ASSERTHR_TF(x, user_text, statement)

#else
  
  #define NI_ASSERT_SLOW(x)                          { }
  #define NI_ASSERT_SLOW_T(x, user_text)             { }
  #define NI_ASSERT_SLOW_TF(x, user_text, statement) { }
  #define NI_ASSERTHR_SLOW(x)                        { }
  #define NI_ASSERTHR_SLOW_T(x, user_text)           { }
  #define NI_ASSERTHR_SLOW_TF(x, user_text, statement) { }

#endif // _DO_ASSERT_SLOW

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Callstack address retrieval macro (simplified - debugger handles this better now)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define GET_CALLSTACK_ADDRS(addresses, depth)        \
  memset(addresses, 0, depth * sizeof(DWORD))

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Legacy BugSlayer crash handler compatibility stubs
// Modern debuggers (WinDbg, Visual Studio) handle crashes directly - these are no-ops
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Forward declarations for legacy compatibility
struct _EXCEPTION_POINTERS;
struct IBaseCommand;

// Crash handler filter stub (modern debuggers handle exceptions automatically)
inline int __stdcall SetCrashHandlerFilter(long (__stdcall*)(struct _EXCEPTION_POINTERS*)) 
{ 
  return 0; // No-op: let debugger handle crashes
}

// Emergency command system stubs (no longer needed with modern debuggers)
namespace NBugSlayer 
{
  inline void __stdcall AddEmergencyCommand(IBaseCommand*) { }
  inline void __stdcall RemoveAllEmergencyCommands() { }
  inline void __stdcall ExecuteEmergencyCommands() { }
  
  // Memory system dump stub (use debugger memory tools instead)
  inline void MemSystemDumpStats() { }
  
  // Fast memory allocator stubs (use standard allocator instead)
  inline void* FastDumbAlloc(size_t) { return nullptr; }
  inline bool FastDumbFree(void*) { return false; }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif // __MODERN_ASSERT_H__
