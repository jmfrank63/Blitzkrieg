#pragma once

#if !defined(_WIN32) && !defined(_WIN64)

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#ifndef _TRUNCATE
#define _TRUNCATE ((size_t)-1)
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
