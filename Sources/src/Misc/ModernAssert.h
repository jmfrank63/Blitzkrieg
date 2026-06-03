#ifndef __MODERN_ASSERT_H__
#define __MODERN_ASSERT_H__
#pragma once

#include <cassert>
#include <cstdlib>

#if defined(_MSC_VER)
  #define DEBUG_BREAK __debugbreak()
#else
  #define DEBUG_BREAK { __asm { int 3 } }
#endif


#if defined(_DEBUG) || defined(_DO_ASSERT) || defined(_DO_ASSERT_SLOW)
  
  #define NI_ASSERT(x)                               { assert(x); }
  
  // Diagnostic text must not be evaluated on passing assertions; many callers
  // build it with lookups that are only safe on the failing path.
  #define NI_ASSERT_T(x, user_text)                  { assert(x); }
  
  #define NI_ASSERT_TF(x, user_text, statement)      \
    {                                                \
      if (!(x)) {                                    \
        statement;                                   \
        assert(false && user_text);                  \
      }                                              \
    }
  
  #define NI_ASSERTHR(x)                             { assert(!FAILED(x)); }
  #define NI_ASSERTHR_T(x, user_text)                { assert(!FAILED(x)); }
  #define NI_ASSERTHR_TF(x, user_text, statement)    \
    {                                                \
      if (FAILED(x)) {                               \
        statement;                                   \
        assert(false && user_text);                  \
      }                                              \
    }
  
  #define NI_FORCE_ASSERT(x, user_text, statement, bForce)      NI_ASSERT_TF(x, user_text, statement)
  #define NI_FORCE_ASSERT_HR(x, user_text, statement, bForce)   NI_ASSERTHR_TF(x, user_text, statement)

#else
  
  #define NI_ASSERT(x)                               { }
  #define NI_ASSERT_T(x, user_text)                  { }
  #define NI_ASSERT_TF(x, user_text, statement)      { }
  #define NI_ASSERTHR(x)                             { }
  #define NI_ASSERTHR_T(x, user_text)                { }
  #define NI_ASSERTHR_TF(x, user_text, statement)    { }
  #define NI_FORCE_ASSERT(x, user_text, statement, bForce)      { }
  #define NI_FORCE_ASSERT_HR(x, user_text, statement, bForce)   { }

#endif // _DEBUG

#if defined(_DEBUG) || defined(_DO_ASSERT_SLOW)
  
  #define NI_ASSERT_SLOW(x)                          { assert(x); }
  #define NI_ASSERT_SLOW_T(x, user_text)             { assert(x); }
  #define NI_ASSERT_SLOW_TF(x, user_text, statement) NI_ASSERT_TF(x, user_text, statement)
  #define NI_ASSERTHR_SLOW(x)                        { assert(!FAILED(x)); }
  #define NI_ASSERTHR_SLOW_T(x, user_text)           { assert(!FAILED(x)); }
  #define NI_ASSERTHR_SLOW_TF(x, user_text, statement) NI_ASSERTHR_TF(x, user_text, statement)

#else
  
  #define NI_ASSERT_SLOW(x)                          { }
  #define NI_ASSERT_SLOW_T(x, user_text)             { }
  #define NI_ASSERT_SLOW_TF(x, user_text, statement) { }
  #define NI_ASSERTHR_SLOW(x)                        { }
  #define NI_ASSERTHR_SLOW_T(x, user_text)           { }
  #define NI_ASSERTHR_SLOW_TF(x, user_text, statement) { }

#endif // _DO_ASSERT_SLOW

#define GET_CALLSTACK_ADDRS(addresses, depth)        \
  memset(addresses, 0, depth * sizeof(DWORD))


struct _EXCEPTION_POINTERS;
struct IBaseCommand;

inline int __stdcall SetCrashHandlerFilter(long (__stdcall*)(struct _EXCEPTION_POINTERS*)) 
{ 
  return 0; // No-op: let debugger handle crashes
}

namespace NBugSlayer 
{
  inline void __stdcall AddEmergencyCommand(IBaseCommand*) { }
  inline void __stdcall RemoveAllEmergencyCommands() { }
  inline void __stdcall ExecuteEmergencyCommands() { }
  
  inline void MemSystemDumpStats() { }
  
  inline void* FastDumbAlloc(size_t) { return nullptr; }
  inline bool FastDumbFree(void*) { return false; }
}

#endif // __MODERN_ASSERT_H__
