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

// Declared here to avoid forcing <windows.h> on every includer.
extern "C" __declspec(dllimport) int __stdcall IsDebuggerPresent(void);
extern "C" __declspec(dllimport) void __stdcall OutputDebugStringA(const char*);

#define NI_ASSERT_STRINGIZE2(x) #x
#define NI_ASSERT_STRINGIZE(x)  NI_ASSERT_STRINGIZE2(x)

// The CRT assert dialog opens behind the fullscreen game window where it can't
// be clicked, and its "Retry" button is just a debug break anyway. Break at the
// assert site directly when a debugger is attached; fall back to the normal CRT
// assert (dialog/abort) otherwise.
#define NI_ASSERT_FAIL(expr_text)                                             \
    {                                                                         \
      if (IsDebuggerPresent()) {                                              \
        OutputDebugStringA("NI_ASSERT failed: " expr_text " at "              \
                           __FILE__ "(" NI_ASSERT_STRINGIZE(__LINE__) ")\n"); \
        DEBUG_BREAK;                                                          \
      }                                                                       \
      else { assert(false && expr_text); }                                    \
    }

#if defined(_DEBUG) || defined(_DO_ASSERT) || defined(_DO_ASSERT_SLOW)

  #define NI_ASSERT(x)                               { if (!(x)) NI_ASSERT_FAIL(#x); }

  // Diagnostic text must not be evaluated on passing assertions; many callers
  // build it with lookups that are only safe on the failing path.
  #define NI_ASSERT_T(x, user_text)                  { if (!(x)) NI_ASSERT_FAIL(#x); }

  #define NI_ASSERT_TF(x, user_text, statement)      \
    {                                                \
      if (!(x)) {                                    \
        statement;                                   \
        NI_ASSERT_FAIL(#x);                          \
      }                                              \
    }

  #define NI_ASSERTHR(x)                             { if (FAILED(x)) NI_ASSERT_FAIL(#x); }
  #define NI_ASSERTHR_T(x, user_text)                { if (FAILED(x)) NI_ASSERT_FAIL(#x); }
  #define NI_ASSERTHR_TF(x, user_text, statement)    \
    {                                                \
      if (FAILED(x)) {                               \
        statement;                                   \
        NI_ASSERT_FAIL(#x);                          \
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
  
  #define NI_ASSERT_SLOW(x)                          { if (!(x)) NI_ASSERT_FAIL(#x); }
  #define NI_ASSERT_SLOW_T(x, user_text)             { if (!(x)) NI_ASSERT_FAIL(#x); }
  #define NI_ASSERT_SLOW_TF(x, user_text, statement) NI_ASSERT_TF(x, user_text, statement)
  #define NI_ASSERTHR_SLOW(x)                        { if (FAILED(x)) NI_ASSERT_FAIL(#x); }
  #define NI_ASSERTHR_SLOW_T(x, user_text)           { if (FAILED(x)) NI_ASSERT_FAIL(#x); }
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
