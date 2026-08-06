#ifndef BLITZKRIEG_PLATFORM_COMPILER_H
#define BLITZKRIEG_PLATFORM_COMPILER_H

#if defined(_WIN32) || defined(_WIN64)
#define BK_CDECL __cdecl
#define BK_STDCALL __stdcall
#define BK_EXPORT __declspec(dllexport)
#define BK_IMPORT __declspec(dllimport)
#else
#define BK_CDECL
#if defined(__i386__) && (defined(__GNUC__) || defined(__clang__))
#define BK_STDCALL __attribute__((stdcall))
#else
#define BK_STDCALL
#endif
#if defined(__GNUC__) || defined(__clang__)
#define BK_EXPORT __attribute__((visibility("default")))
#define BK_IMPORT
#else
#define BK_EXPORT
#define BK_IMPORT
#endif
#endif

#define BK_NORETURN [[noreturn]]
#define BK_CAPI_CALL BK_CDECL
#define BK_CAPI_EXPORT BK_EXPORT

#ifndef STDCALL
#define STDCALL BK_STDCALL
#endif

#endif
