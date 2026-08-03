#ifndef BLITZKRIEG_PLATFORM_LEGACY_TYPES_H
#define BLITZKRIEG_PLATFORM_LEGACY_TYPES_H

// Windows supplies these value types through the platform headers. Non-native
// builds get only the fixed-width legacy values required by shared interfaces;
// opaque operating-system handles deliberately remain undefined.
#if !defined(_WIN32) && !defined(_WIN64)
using BYTE = unsigned char;
using WORD = unsigned short;
using DWORD = unsigned int;
using BOOL = int;
using LONG = int;
using LPARAM = long long;
using HWND = void*;

struct GUID {
    DWORD Data1;
    WORD Data2;
    WORD Data3;
    BYTE Data4[8];
};

struct RECT {
    LONG left;
    LONG top;
    LONG right;
    LONG bottom;
};

struct POINT {
    LONG x;
    LONG y;
};

struct SIZE {
    LONG cx;
    LONG cy;
};
#endif

#ifndef BLITZKRIEG_QWORD_DEFINED
#define BLITZKRIEG_QWORD_DEFINED
using QWORD = unsigned long long;
#endif

#endif
