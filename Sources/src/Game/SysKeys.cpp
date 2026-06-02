#include "StdAfx.h"

#include "SysKeys.h"
#ifndef LLKHF_EXTENDED
#define LLKHF_EXTENDED       0x00000001
#endif
#ifndef LLKHF_INJECTED
#define LLKHF_INJECTED       0x00000010
#endif
#ifndef LLKHF_ALTDOWN
#define LLKHF_ALTDOWN        0x00000020
#endif
#ifndef LLKHF_UP
#define LLKHF_UP             0x00000080
#endif

#ifndef LLMHF_INJECTED
#define LLMHF_INJECTED       0x00000001
#endif

#ifndef WH_KEYBOARD_LL
#define WH_KEYBOARD_LL     13
#endif
#ifndef WH_MOUSE_LL
#define WH_MOUSE_LL        14
#endif
namespace NSysKeys
{
struct KBDLLHOOKSTRUCT
{
  DWORD vkCode;
  DWORD scanCode;
  DWORD flags;
  DWORD time;
  DWORD dwExtraInfo;
};
static HHOOK hHook = 0;
static UINT nPreviousState = 0;
static bool bCurrEnable = true;
LRESULT CALLBACK LowLevelKeyboardProc( INT nCode, WPARAM wParam, LPARAM lParam )
{
  KBDLLHOOKSTRUCT *pkbhs = (KBDLLHOOKSTRUCT *)lParam;
  BOOL bControlKeyDown = 0;

  switch ( nCode )
  {
    case HC_ACTION:
      bControlKeyDown = GetAsyncKeyState( VK_CONTROL ) >> ( (sizeof(SHORT) * 8) - 1 );
      if ( (pkbhs->vkCode == VK_ESCAPE) && bControlKeyDown )
        return 1;
      if ( (pkbhs->vkCode == VK_ESCAPE) && (pkbhs->flags & LLKHF_ALTDOWN) )
        return 1;
			if ( (pkbhs->vkCode == VK_LWIN) || (pkbhs->vkCode == VK_RWIN) || (pkbhs->vkCode == VK_APPS) ) 
        return 1;
      break;
  }
  return CallNextHookEx( hHook, nCode, wParam, lParam );
}
extern "C" WINBASEAPI BOOL WINAPI IsDebuggerPresent(void);
void EnableSystemKeys( bool bEnable, HINSTANCE hInstance )
{
	if ( (bCurrEnable == bEnable) /*|| IsDebuggerPresent()*/ ) 
		return;
	const DWORD dwOSVersion = GetVersion();
	if ( dwOSVersion & 0x80000000 ) 
	{
		if ( bEnable )
			SystemParametersInfo( SPI_SETSCREENSAVERRUNNING, nPreviousState, &nPreviousState, 0 );
		else
			SystemParametersInfo( SPI_SETSCREENSAVERRUNNING, TRUE, &nPreviousState, 0 );
	}
	else if ( (dwOSVersion & 0xff) >= 5 ) 
	{
		if ( bEnable && (NSysKeys::hHook != 0) ) 
			UnhookWindowsHookEx( NSysKeys::hHook );
		else
			NSysKeys::hHook = SetWindowsHookEx( WH_KEYBOARD_LL, NSysKeys::LowLevelKeyboardProc, hInstance, 0 );
	}
	bCurrEnable = bEnable;
}
};
