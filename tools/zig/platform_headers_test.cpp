#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#endif

#include "Platform/Compiler.h"
#include "Platform/LegacyTypes.h"
#include "GFX/GFXPlatform.h"

static_assert(sizeof(BYTE) == 1 && static_cast<BYTE>(-1) > 0);
static_assert(sizeof(WORD) == 2 && static_cast<WORD>(-1) > 0);
static_assert(sizeof(DWORD) == 4 && static_cast<DWORD>(-1) > 0);
static_assert(sizeof(QWORD) == 8 && static_cast<QWORD>(-1) > 0);
static_assert(sizeof(BOOL) == 4 && static_cast<BOOL>(-1) < 0);
static_assert(sizeof(RECT) == 16);
static_assert(sizeof(POINT) == 8);
static_assert(sizeof(GFXNativeWindow) == sizeof(void *));
static_assert(sizeof(RECT{}.left) == sizeof(LONG));
static_assert(sizeof(POINT{}.x) == sizeof(LONG));

using StdcallFunction = void (STDCALL *)();
static_assert(sizeof(StdcallFunction) == sizeof(void *));

BK_NORETURN void compiler_contract_noreturn();
BK_EXPORT int BK_CDECL compiler_contract_export(int value) { return value; }

int main() {
    using ExportFunction = int (BK_CDECL *)(int);
    static_assert(sizeof(&compiler_contract_export) == sizeof(ExportFunction));
    return sizeof(StdcallFunction) == sizeof(void *) ? 0 : 1;
}
