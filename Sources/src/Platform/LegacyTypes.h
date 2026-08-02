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
#endif

#ifndef BLITZKRIEG_QWORD_DEFINED
#define BLITZKRIEG_QWORD_DEFINED
using QWORD = unsigned long long;
#endif

#endif
