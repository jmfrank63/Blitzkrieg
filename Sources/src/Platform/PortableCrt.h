#pragma once

#if !defined(_WIN32) && !defined(_WIN64)

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>
#include <stdint.h>
#ifdef __cplusplus
#include <typeinfo>
#endif

#ifndef _TRUNCATE
#define _TRUNCATE ((size_t)-1)
#define __forceinline inline __attribute__((always_inline))

typedef wchar_t WCHAR;
typedef WCHAR *LPWSTR;
typedef const WCHAR *LPCWSTR;
typedef const char *LPCTSTR;
typedef unsigned long DWORD;
typedef unsigned int UINT;
typedef long LONG;
typedef unsigned long REGSAM;
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

static inline UINT GetACP(void) { return 65001; }
static inline char *_itoa(int value, char *buffer, int radix) {
    if (radix == 10) snprintf(buffer, 32, "%d", value);
    else snprintf(buffer, 32, "%x", (unsigned int)value);
    return buffer;
}
static inline int WideCharToMultiByte(UINT, DWORD, LPCWSTR source, int source_length,
                                      char *dest, int dest_length, const char *, int *) {
    if (source_length < 0) source_length = (int)wcslen(source);
    size_t converted = wcstombs(dest, source, (size_t)dest_length);
    return converted == (size_t)-1 ? 0 : (int)converted;
}
static inline int MultiByteToWideChar(UINT, DWORD, const char *source, int source_length,
                                      LPWSTR dest, int dest_length) {
    if (source_length < 0) source_length = (int)strlen(source);
    size_t converted = mbstowcs(dest, source, (size_t)dest_length);
    return converted == (size_t)-1 ? 0 : (int)converted;
}

static inline LONG RegCreateKeyEx(HKEY, LPCTSTR, DWORD, LPCTSTR, DWORD, REGSAM,
                                  const void *, HKEY *result, DWORD *disposition) {
    if (result) *result = 0;
    if (disposition) *disposition = 0;
    return ERROR_INVALID_PARAMETER;
}
static inline LONG RegCloseKey(HKEY) { return ERROR_SUCCESS; }
static inline LONG RegQueryValueEx(HKEY, LPCTSTR, DWORD *, DWORD *, unsigned char *, DWORD *) {
    return ERROR_INVALID_PARAMETER;
}
static inline LONG RegSetValueEx(HKEY, LPCTSTR, DWORD, DWORD, const unsigned char *, DWORD) {
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
