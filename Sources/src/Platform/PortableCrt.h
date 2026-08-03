#pragma once

#if !defined(_WIN32) && !defined(_WIN64)

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>
#include <stdint.h>
#include <limits.h>

#ifndef _TRUNCATE
#define _TRUNCATE ((size_t)-1)
#define __forceinline inline __attribute__((always_inline))

typedef wchar_t WCHAR;
typedef WCHAR *LPWSTR;
typedef const WCHAR *LPCWSTR;
typedef const char *LPCTSTR;
typedef unsigned int UINT;
typedef unsigned long REGSAM;
typedef unsigned long ULONG;
typedef long HRESULT;
typedef int REFIID;
typedef void *HANDLE;
typedef void *HMODULE;
typedef void *HINSTANCE;
typedef void *HCURSOR;
#ifndef BLITZKRIEG_FILETIME_DEFINED
typedef struct { unsigned long dwLowDateTime; unsigned long dwHighDateTime; } FILETIME;
#define BLITZKRIEG_FILETIME_DEFINED
#endif
typedef struct { wchar_t *pwcsName; unsigned long type; unsigned long cbSize; FILETIME mtime; FILETIME ctime; FILETIME atime; unsigned long grfMode; unsigned long grfLocksSupported; unsigned long clsid; unsigned long grfStateBits; unsigned long reserved; } STATSTG;
typedef struct { unsigned long dwSignature; unsigned long dwStrucVersion; unsigned long dwFileVersionMS; unsigned long dwFileVersionLS; unsigned long dwProductVersionMS; unsigned long dwProductVersionLS; unsigned long dwFileFlagsMask; unsigned long dwFileFlags; unsigned long dwFileOS; unsigned long dwFileType; unsigned long dwFileSubtype; unsigned long dwFileDateMS; unsigned long dwFileDateLS; } VS_FIXEDFILEINFO;
typedef struct { long long QuadPart; } LARGE_INTEGER;
typedef struct { unsigned long long QuadPart; } ULARGE_INTEGER;
#define IID_IUnknown 0
#define IID_IStream 1
#define E_NOINTERFACE 0x80004002L
#define S_OK 0L
#define FALSE 0
#define TRUE 1
#define THREAD_PRIORITY_NORMAL 0
#define THREAD_PRIORITY_BELOW_NORMAL -1
#define TEXT(x) x
#define MAKELPARAM(a,b) ((long)(((unsigned short)(a)) | ((unsigned long)((unsigned short)(b)) << 16)))
#define MAKEINTRESOURCE(x) ((const char *)(uintptr_t)(x))
#define IDC_WAIT ((const char *)32514)
#define STG_E_INVALIDFUNCTION 0x80030001L
#define STG_E_ACCESSDENIED 0x80030005L
#define _PC_24 0
#ifdef __cplusplus
struct IUnknown { virtual ~IUnknown() = default; };
struct IStream : IUnknown {
    virtual HRESULT QueryInterface(REFIID, void **) = 0;
    virtual ULONG AddRef() = 0;
    virtual ULONG Release() = 0;
    virtual HRESULT Read(void *, ULONG, ULONG *) = 0;
    virtual HRESULT Write(const void *, ULONG, ULONG *) = 0;
    virtual HRESULT Seek(LARGE_INTEGER, unsigned int, ULARGE_INTEGER *) = 0;
    virtual HRESULT SetSize(ULARGE_INTEGER) = 0;
    virtual HRESULT CopyTo(IStream *, ULARGE_INTEGER, ULARGE_INTEGER *, ULARGE_INTEGER *) = 0;
};
#endif
static inline unsigned long GetTickCount(void) { return 0; }
static inline HANDLE GetCurrentThread(void) { return 0; }
static inline int SetThreadPriority(HANDLE, int) { return 1; }
static inline HMODULE GetModuleHandleA(const char *) { return 0; }
static inline void *GetProcAddress(HMODULE, const char *) { return 0; }
static inline unsigned long GetCurrentDirectory(unsigned long, char *) { return 0; }
static inline unsigned long GetModuleFileName(HMODULE, char *, unsigned long) { return 0; }
static inline int GetFileVersionInfoSize(const char *, unsigned int *) { return 0; }
static inline int GetFileVersionInfo(const char *, unsigned long, unsigned long, void *) { return 0; }
#ifdef __cplusplus
static inline int GetFileVersionInfoSize(const wchar_t *, unsigned int *) { return 0; }
static inline int GetFileVersionInfo(const wchar_t *, unsigned long, unsigned long, void *) { return 0; }
#endif
static inline int VerQueryValue(const void *, const char *, void **, unsigned int *) { return 0; }
static inline int CoCreateGuid(void *) { return -1; }
static inline int DosDateTimeToFileTime(unsigned short, unsigned short, FILETIME *) { return 0; }
static inline int LocalFileTimeToFileTime(const FILETIME *, FILETIME *) { return 0; }
typedef void *HKEY;

#define HKEY_LOCAL_MACHINE ((HKEY)(uintptr_t)1)
#define HKEY_CURRENT_USER ((HKEY)(uintptr_t)2)
#define KEY_READ 0x20019
#define KEY_WRITE 0x20006
#define REG_OPTION_NON_VOLATILE 0
#define REG_SZ 1
#define ERROR_SUCCESS 0
#define ERROR_INVALID_DATA 13
#define ERROR_INVALID_PARAMETER 87
#define CP_ACP 0
#define _stricmp strcasecmp
#define _strnicmp strncasecmp
#define __int64 long long
#ifndef __stdcall
#define __stdcall
#endif
#define _EM_INVALID 0
#define _EM_ZERODIVIDE 0
#define _EM_OVERFLOW 0
#define _EM_UNDERFLOW 0
#define _EM_INEXACT 0
#define _EM_DENORMAL 0
#define _MCW_EM 0
#define _RC_NEAR 0
#define _RC_CHOP 0
#define _DN_SAVE 0
#define _PC_24 0
#define _MCW_RC 0
#define _MCW_DN 0
static inline unsigned int _control87(unsigned int, unsigned int) { return 0; }
static inline void Sleep(unsigned long) {}
static inline int ClipCursor(const void *) { return 1; }
static inline int SetCursorPos(int, int) { return 1; }
static inline HCURSOR LoadCursor(HINSTANCE, const char *) { return 0; }
static inline HCURSOR SetCursor(HCURSOR cursor) { return cursor; }

static inline UINT GetACP(void) { return 65001; }
static inline char *_itoa(int value, char *buffer, int radix) {
    if (radix == 10) snprintf(buffer, 32, "%d", value);
    else snprintf(buffer, 32, "%x", (unsigned int)value);
    return buffer;
}
static inline int WideCharToMultiByte(UINT, unsigned long, LPCWSTR source, int source_length,
                                      char *dest, int dest_length, const char *, int *) {
    if (source_length < 0) source_length = (int)wcslen(source);
    size_t converted = wcstombs(dest, source, (size_t)dest_length);
    return converted == (size_t)-1 ? 0 : (int)converted;
}
static inline int MultiByteToWideChar(UINT, unsigned long, const char *source, int source_length,
                                      LPWSTR dest, int dest_length) {
    if (source_length < 0) source_length = (int)strlen(source);
    size_t converted = mbstowcs(dest, source, (size_t)dest_length);
    return converted == (size_t)-1 ? 0 : (int)converted;
}

static inline int RegCreateKeyEx(HKEY, LPCTSTR, unsigned long, LPCTSTR, unsigned long, REGSAM,
                                  const void *, HKEY *result, unsigned int *disposition) {
    if (result) *result = 0;
    if (disposition) *disposition = 0;
    return ERROR_INVALID_PARAMETER;
}
static inline int RegCloseKey(HKEY) { return ERROR_SUCCESS; }
static inline int RegQueryValueEx(HKEY, LPCTSTR, unsigned int *, unsigned int *, unsigned char *, unsigned int *) {
    return ERROR_INVALID_PARAMETER;
}
static inline int RegSetValueEx(HKEY, LPCTSTR, unsigned long, unsigned long, const unsigned char *, unsigned long) {
    return ERROR_INVALID_PARAMETER;
}
#endif

static inline int fopen_s(FILE **file, const char *path, const char *mode) {
    if (file == NULL) return 22;
    *file = fopen(path, mode);
    return *file == NULL;
}

static inline int freopen_s(FILE **file, const char *path, const char *mode, FILE *stream) {
    if (file == NULL) return 22;
    *file = freopen(path, mode, stream);
    return *file == NULL;
}

static inline int vsprintf_s(char *buffer, size_t size, const char *format, va_list args) {
    int result = vsnprintf(buffer, size, format, args);
    if (result < 0 || (size != 0 && (size_t)result >= size)) {
        if (size != 0) buffer[size - 1] = '\0';
        return -1;
    }
    return result;
}

static inline int sprintf_s(char *buffer, size_t size, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int result = vsprintf_s(buffer, size, format, args);
    va_end(args);
    return result;
}

static inline int strcpy_s(char *destination, size_t size, const char *source) {
    if (destination == NULL || source == NULL || size == 0) return 22;
    size_t length = strlen(source);
    if (length >= size) {
        destination[0] = '\0';
        return 34;
    }
    memcpy(destination, source, length + 1);
    return 0;
}

static inline int strncpy_s(char *destination, size_t size, const char *source, size_t count) {
    if (destination == NULL || source == NULL || size == 0) return 22;
    size_t limit = count == _TRUNCATE ? size - 1 : count;
    size_t length = strlen(source);
    if (length > limit) {
        if (count != _TRUNCATE) {
            destination[0] = '\0';
            return 34;
        }
        length = limit;
    }
    if (length >= size) length = size - 1;
    memcpy(destination, source, length);
    destination[length] = '\0';
    return 0;
}

static inline int _vsnprintf_s(char *buffer, size_t size, size_t count, const char *format, va_list args) {
    (void)count;
    return vsprintf_s(buffer, size, format, args);
}

#endif
