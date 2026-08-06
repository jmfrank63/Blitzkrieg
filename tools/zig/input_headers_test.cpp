// P03-M01 header audit.  This translation unit is intentionally compiled with
// BK_INPUT_EVENT_ONLY=1; it must remain usable by a portable C++ toolchain.

#if !defined(BK_INPUT_EVENT_ONLY)
#error "input_headers_test.cpp must be compiled with BK_INPUT_EVENT_ONLY defined"
#endif

// DirectInput declarations are C++ identifiers rather than preprocessor
// macros, so poison the identifiers before including the headers.  Any
// remaining use in a portable header then becomes a compile-time error without
// requiring dinput.h, Windows SDK headers, or a Windows target.
#define IDirectInput8 BK_INPUT_HEADER_AUDIT_FORBIDDEN_IDirectInput8
#define IDirectInputDevice8 BK_INPUT_HEADER_AUDIT_FORBIDDEN_IDirectInputDevice8
#define DIDEVICEOBJECTDATA BK_INPUT_HEADER_AUDIT_FORBIDDEN_DIDEVICEOBJECTDATA
#define DIDEVICEINSTANCE BK_INPUT_HEADER_AUDIT_FORBIDDEN_DIDEVICEINSTANCE
#define DIDEVCAPS BK_INPUT_HEADER_AUDIT_FORBIDDEN_DIDEVCAPS
#define DIDEVICEOBJECTINSTANCE BK_INPUT_HEADER_AUDIT_FORBIDDEN_DIDEVICEOBJECTINSTANCE
#define DIPROPHEADER BK_INPUT_HEADER_AUDIT_FORBIDDEN_DIPROPHEADER
#define DIDATAFORMAT BK_INPUT_HEADER_AUDIT_FORBIDDEN_DIDATAFORMAT
#define DIJOYSTATE BK_INPUT_HEADER_AUDIT_FORBIDDEN_DIJOYSTATE
#define DIJOYSTATE2 BK_INPUT_HEADER_AUDIT_FORBIDDEN_DIJOYSTATE2
#define DirectInput8Create BK_INPUT_HEADER_AUDIT_FORBIDDEN_DirectInput8Create
#define DirectInputCreate BK_INPUT_HEADER_AUDIT_FORBIDDEN_DirectInputCreate
#define GUID_SysKeyboard BK_INPUT_HEADER_AUDIT_FORBIDDEN_GUID_SysKeyboard
#define GUID_SysMouse BK_INPUT_HEADER_AUDIT_FORBIDDEN_GUID_SysMouse
#define GUID_Joystick BK_INPUT_HEADER_AUDIT_FORBIDDEN_GUID_Joystick

#include "../../Sources/src/Input/StdAfx.h"

// Public and private Input headers participating in the module boundary.  The
// first four are also reached by StdAfx/Specific, but remain explicit so a
// future include-graph change cannot silently remove them from this audit.
#include "../../Sources/src/Input/Specific.h"
#include "../../Sources/src/Input/Input.h"
#include "../../Sources/src/Input/InputTypes.h"
#include "../../Sources/src/Input/InputAPI.h"
#include "../../Sources/src/Input/InputCodes.h"
#include "../../Sources/src/Input/InputBind.h"
#include "../../Sources/src/Input/InputBinder.h"
#include "../../Sources/src/Input/InputHelper.h"
#include "../../Sources/src/Input/InputObjectFactory.h"
#include "../../Sources/src/Input/InputSlider.h"
#include "../../Sources/src/Input/Visitors.h"

// No DirectInput header is allowed to become visible through the include set.
#if defined(DIRECTINPUT_VERSION) || defined(DIRECTINPUT_HEADER_VERSION)
#error "DirectInput preprocessor declarations leaked into event-only Input headers"
#endif

#undef GUID_Joystick
#undef GUID_SysMouse
#undef GUID_SysKeyboard
#undef DirectInputCreate
#undef DirectInput8Create
#undef DIJOYSTATE2
#undef DIJOYSTATE
#undef DIDATAFORMAT
#undef DIPROPHEADER
#undef DIDEVICEOBJECTINSTANCE
#undef DIDEVCAPS
#undef DIDEVICEINSTANCE
#undef DIDEVICEOBJECTDATA
#undef IDirectInputDevice8
#undef IDirectInput8

int main()
{
    return 0;
}
