#include "StdAfx.h"

#include <mmsystem.h>

#include "WinFrame.h"
#include "SysKeys.h"
#include "../Platform/SDLApplication.h"
#include "../Platform/Event.h"
#include "../Platform/Clock.h"

#include "../Misc/Win32Helper.h"
#include "../Main/iMain.h"
#include "../Main/iMainCommands.h"
#include "../GameTT/iMission.h"
#include "../Scene/Scene.h"
#include "../Input/Input.h"
#include "../Input/InputTypes.h"
#include "../GFX/GFXTypes.h"

#include "resource.h"
using namespace NWin32Helper;
static CCriticalSection msgs;
#define SPLASH_SCREEN_SIZE_X 600
#define SPLASH_SCREEN_SIZE_Y 352

#ifndef WM_MOUSEWHEEL
#define WM_MOUSEWHEEL	0x020A
#endif // WM_MOUSEWHEEL
namespace NWinFrame
{
static HWND hWnd = 0;                   // window handle
static HINSTANCE hInstance = 0;         // instance handle
static NPlatform::SDLApplication sdlApplication;
static ATOM atomWndClassName = 0;       // atom window class name identification (assigned during registration)
static bool bActive = false;
static bool bExit = false;
// Starts released: the mouse is not owned until the window actually holds
// focus, which ReconcileMouseCapture establishes on the first focused pump.
static bool bMouseReleased = true;
static std::list<SWindowsMsg> msgList;  // pumped messages
static std::string szAppTitleName = " Blitzkrieg Game"; // application title ( will be loaded during initialization )
static std::string szWndClassName = "NIVAL_RTS_ENGINE"; // user window class name ( will be loaded during initialization )
static HWND hWndSplashScreen = 0;
static DWORD GetWindowedStyle() { return WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX; }
static bool IsWindowedMode()
{
	return ( GetGlobalVar( "windowed", 0 ) != 0 ) ||
		     ( GetGlobalVar( "GFX.Mode.Current.FullScreen", 0 ) == int( GFXFS_WINDOWED ) );
}
bool IsActive() { return bActive; }
bool IsExit() { return bExit; }
HWND GetHWnd() { return hWnd; }
HINSTANCE GetHInstance() { return hInstance; }
void *GetSDLWindow() { return sdlApplication.BorrowWindow().value; }
void AddMsg( SWindowsMsg::EMsg msg, int x, int y, DWORD dwFlags );
static void AddInputMessage( const int nEventID, const int nParam = 0 );
static void UpdateCursorPos( const LPARAM lParam );
static void AddMovieSkipMessage();	// keyboard: abort the whole movie sequence
static void AddMovieNextMessage();	// mouse: skip only the current movie
static void ApplyMouseClip();
ATOM RegisterClass( HINSTANCE hInst );
bool CheckPreviousApp( LPCSTR pszMainClass, LPCSTR pszMainTitle );
bool InitInstance( HINSTANCE hInst, int nCmdShow, int nWidth, int nHeight );
int TranslateCoords( const LPARAM lParam )
{
	const int x = LOWORD( lParam );
	const int y = HIWORD( lParam );
	return ( ( x & 0x7FFF ) | ( (y & 0x7FFF) << 15 ) ) | 0x40000000;
}
static LRESULT CALLBACK WndProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	switch ( uMsg )
	{
		case WM_PAINT:
			/*RECT rect;
			if ( GetUpdateRect(hWnd, &rect, FALSE) )
				ValidateRect( hWnd, &rect );*/
			break;
    case WM_GETMINMAXINFO:
      ((MINMAXINFO*)lParam)->ptMinTrackSize.x = 100;
      ((MINMAXINFO*)lParam)->ptMinTrackSize.y = 100;
      break;
    case WM_ENTERSIZEMOVE:
			if ( !IsWindowedMode() )
				NI_ASSERT( 0 );
      break;
    case WM_EXITSIZEMOVE:
      break;
		case WM_SETCURSOR:
			if ( NMain::IsInitialized() && ( !IsWindowedMode() || LOWORD(lParam) == HTCLIENT ) )
				GetSingleton<ICursor>()->OnSetCursor();
			break;
    case WM_NCHITTEST:
			if ( !IsWindowedMode() )
				return HTCLIENT;
      break;
    case WM_POWERBROADCAST:
      switch( wParam )
      {
        case PBT_APMQUERYSUSPEND:
          return TRUE;

        case PBT_APMRESUMESUSPEND:
          return TRUE;
      }
      break;
    case WM_SYSCOMMAND:
      switch( wParam & 0xfff0 )
      {
        case SC_MONITORPOWER:
				case SC_SCREENSAVE:
          return 1;

        case SC_MOVE:
        case SC_SIZE:
        case SC_MAXIMIZE:
        case SC_KEYMENU:
					if ( !IsWindowedMode() )
						return 1;
          break;
/*				case SC_RESTORE:
					ShowWindow( hWnd, SW_RESTORE );
					break;*/
      }
      break;
    case WM_CLOSE:
      PostQuitMessage( 0 );
      return 0;
		case WM_ACTIVATEAPP:
			SetActive( wParam != 0 );
			break;
		case WM_ACTIVATE:
			{
				switch ( LOWORD(wParam) )
				{
					case WA_CLICKACTIVE:					// activate window
					case WA_ACTIVE:
						SetActive( true );
						NMain::SwitchGame( true );
						break;
					case WA_INACTIVE:						// deactivate window
						SetActive( false );
						NMain::SwitchGame( false );
						break;
				}
			}
			break;

		// Button transitions are only fed to the input device emulation; the
		// bind system turns them into begin/end_action commands. Posting the
		// CMD_*_ACTION* messages here as well dispatched every mouse action
		// twice (e.g. the move+rotate drag gesture was followed by a second
		// plain move to the release point).
		case WM_LBUTTONDOWN:
			if ( GetSingleton<IInput>()->IsEmulated(DEVICE_TYPE_MOUSE) )
			{
				CaptureMouse();
				SetCapture( hWnd );
				AddMovieNextMessage();
				if ( NMain::IsInitialized() )
					UpdateCursorPos( lParam );
				GetSingleton<IInput>()->EmulateInput( DEVICE_TYPE_MOUSE, INPUT_CONTROL_MOUSE_BUTTON0, 0x80, timeGetTime(), TranslateCoords(lParam) );
			}
			break;
		case WM_LBUTTONUP:
			if ( GetSingleton<IInput>()->IsEmulated(DEVICE_TYPE_MOUSE) )
			{
				if ( NMain::IsInitialized() )
					UpdateCursorPos( lParam );
				GetSingleton<IInput>()->EmulateInput( DEVICE_TYPE_MOUSE, INPUT_CONTROL_MOUSE_BUTTON0, 0x00, timeGetTime(), TranslateCoords(lParam) );
				if ( GetCapture() == hWnd )
					ReleaseCapture();
			}
			break;
		case WM_LBUTTONDBLCLK:
			if ( GetSingleton<IInput>()->IsEmulated(DEVICE_TYPE_MOUSE) )
			{
				AddMovieNextMessage();
				if ( NMain::IsInitialized() )
					UpdateCursorPos( lParam );
				GetSingleton<IInput>()->EmulateInput( DEVICE_TYPE_MOUSE, INPUT_CONTROL_MOUSE_BUTTON0 | 0x4000, 0x80, timeGetTime(), TranslateCoords(lParam) );
			}
			break;
		case WM_RBUTTONDOWN:
			if ( GetSingleton<IInput>()->IsEmulated(DEVICE_TYPE_MOUSE) )
			{
				CaptureMouse();
				SetCapture( hWnd );
				AddMovieNextMessage();
				if ( NMain::IsInitialized() )
					UpdateCursorPos( lParam );
				GetSingleton<IInput>()->EmulateInput( DEVICE_TYPE_MOUSE, INPUT_CONTROL_MOUSE_BUTTON1, 0x80, timeGetTime(), TranslateCoords(lParam) );
			}
			break;
		case WM_RBUTTONUP:
			if ( GetSingleton<IInput>()->IsEmulated(DEVICE_TYPE_MOUSE) )
			{
				if ( NMain::IsInitialized() )
					UpdateCursorPos( lParam );
				GetSingleton<IInput>()->EmulateInput( DEVICE_TYPE_MOUSE, INPUT_CONTROL_MOUSE_BUTTON1, 0x00, timeGetTime(), TranslateCoords(lParam) );
				if ( GetCapture() == hWnd )
					ReleaseCapture();
			}
			break;
		case WM_RBUTTONDBLCLK:
			if ( GetSingleton<IInput>()->IsEmulated(DEVICE_TYPE_MOUSE) ) 
				GetSingleton<IInput>()->EmulateInput( DEVICE_TYPE_MOUSE, INPUT_CONTROL_MOUSE_BUTTON1 | 0x4000, 0x80, timeGetTime(), TranslateCoords(lParam) );
			break;
		case WM_MBUTTONDOWN:
			if ( GetSingleton<IInput>()->IsEmulated(DEVICE_TYPE_MOUSE) ) 
				GetSingleton<IInput>()->EmulateInput( DEVICE_TYPE_MOUSE, INPUT_CONTROL_MOUSE_BUTTON2, 0x80, timeGetTime(), TranslateCoords(lParam) );
			break;
		case WM_MBUTTONUP:
			if ( GetSingleton<IInput>()->IsEmulated(DEVICE_TYPE_MOUSE) ) 
				GetSingleton<IInput>()->EmulateInput( DEVICE_TYPE_MOUSE, INPUT_CONTROL_MOUSE_BUTTON2, 0x00, timeGetTime(), TranslateCoords(lParam) );
			break;
		case WM_MBUTTONDBLCLK:
			if ( GetSingleton<IInput>()->IsEmulated(DEVICE_TYPE_MOUSE) ) 
				GetSingleton<IInput>()->EmulateInput( DEVICE_TYPE_MOUSE, INPUT_CONTROL_MOUSE_BUTTON2 | 0x4000, 0x80, timeGetTime(), TranslateCoords(lParam) );
			break;
		case WM_MOUSEWHEEL:
			if ( GetSingleton<IInput>()->IsEmulated(DEVICE_TYPE_MOUSE) ) 
			{
				static int absZ = 0;
				const short deltaZ = (short)HIWORD( wParam );
				absZ += deltaZ;
				GetSingleton<IInput>()->EmulateInput( DEVICE_TYPE_MOUSE, INPUT_CONTROL_MOUSE_AXIS_Z, absZ, timeGetTime(), TranslateCoords(lParam) );
			}
			break;
		case WM_MOUSEMOVE:
			if ( NMain::IsInitialized() ) 
				UpdateCursorPos( lParam );
			break;
			/*
		case WM_KEYDOWN:
			AddMsg( SWindowsMsg::KEY_DOWN, wParam, lParam & 0xFFFF, (lParam >> 16) & 0xFFFF );
			break;
		case WM_KEYUP:
			AddMsg( SWindowsMsg::KEY_UP, wParam, lParam & 0xFFFF, (lParam >> 16) & 0xFFFF );
			break;
			*/
		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
			switch ( wParam )
			{
				case VK_ESCAPE:
					AddMovieSkipMessage();
					break;
				case VK_SPACE:
					AddMovieSkipMessage();
					break;
				case VK_RETURN:
					AddMovieSkipMessage();
					break;
			}
			break;
		case WM_INPUTLANGCHANGEREQUEST:
			break;
		case WM_INPUTLANGCHANGE:
			{
				CHARSETINFO csi;
				Zero( csi );
				DWORD dwCharSet = wParam;
				BOOL bSuccess = TranslateCharsetInfo( &dwCharSet, &csi, TCI_SRCCHARSET );
				if ( bSuccess )
				{
					if ( IInput *pInput = GetSingleton<IInput>() )
						pInput->SetCodePage( csi.ciACP );
				}
			}
			break;
	}
	return DefWindowProc( hWnd, uMsg, wParam, lParam );
}
static void AddInputMessage( const int nEventID, const int nParam )
{
	if ( NMain::IsInitialized() )
		GetSingleton<IInput>()->AddMessage( SGameMessage( nEventID, nParam ) );
}
static void UpdateCursorPos( const LPARAM lParam )
{
	const int x = LOWORD( lParam );
	const int y = HIWORD( lParam );
	GetSingleton<ICursor>()->SetPos( x, y );
}
static void AddMovieSkipMessage()
{
	AddInputMessage( MC_MOVIE_SKIP_SEQUENCE, 0 );
}
static void AddMovieNextMessage()
{
	AddInputMessage( MC_MOVIE_SKIP_MOVIE, 0 );
}
static void ApplyMouseClip()
{
	if ( bMouseReleased || !bActive || hWnd == 0 || IsIconic( hWnd ) )
	{
		::ClipCursor( 0 );
		return;
	}

	RECT rcClip;
	if ( !GetClientRect( hWnd, &rcClip ) )
	{
		::ClipCursor( 0 );
		return;
	}

	POINT ptTopLeft = { rcClip.left, rcClip.top };
	POINT ptBottomRight = { rcClip.right, rcClip.bottom };
	if ( !ClientToScreen( hWnd, &ptTopLeft ) || !ClientToScreen( hWnd, &ptBottomRight ) )
	{
		::ClipCursor( 0 );
		return;
	}

	rcClip.left = ptTopLeft.x;
	rcClip.top = ptTopLeft.y;
	rcClip.right = ptBottomRight.x;
	rcClip.bottom = ptBottomRight.y;
	::ClipCursor( &rcClip );
}
static void AddMsg( SWindowsMsg::EMsg msg, int x, int y, DWORD dwFlags )
{
	CCriticalSectionLock lock( msgs );
	msgList.push_back( SWindowsMsg() );
	SWindowsMsg &m = msgList.back();
	m.time = static_cast<DWORD>( NPlatform::MonotonicMilliseconds() );
	m.msg = msg;
	m.x = x;
	m.y = y;
	m.dwFlags = dwFlags;
}
bool GetMessage( SWindowsMsg *pRes )
{
	CCriticalSectionLock lock( msgs );
	if ( !msgList.empty() )
	{
		*pRes = msgList.front();
		msgList.pop_front();
		return true;
	}
	pRes->msg = SWindowsMsg::TIME;
	pRes->time = static_cast<DWORD>( NPlatform::MonotonicMilliseconds() );
	return false;
}
bool InitApplication( HINSTANCE hInstance, const char *pszAppName, const char *pszWndName, int nWidth, int nHeight )
{
	::NWinFrame::hInstance = hInstance;
	szAppTitleName = pszAppName ? pszAppName : " Blitzkrieg Game";
	szWndClassName = pszWndName ? pszWndName : "NIVAL_RTS_ENGINE";
	if ( !sdlApplication.Initialize( szAppTitleName.c_str(), nWidth, nHeight ) )
		return false;
	hWnd = static_cast<HWND>( sdlApplication.GetWindowsNativeHandle() );
	bActive = true;
	bExit = false;
	// The system pointer is hidden and the mouse grabbed by
	// ReconcileMouseCapture on the first pump where the window holds focus, so
	// there is nothing to set here.
	return hWnd != nullptr;
	#if 0
	szAppTitleName = pszAppName;
	szWndClassName = pszWndName;
  atomWndClassName = RegisterClass( hInstance );
	NI_ASSERT_TF( atomWndClassName != 0, "Can't register class", return false; );
  bool bRetVal = CheckPreviousApp( reinterpret_cast<LPCSTR>(atomWndClassName), szAppTitleName.c_str() );
	if ( bRetVal == false )
		return bRetVal;
	bRetVal = InitInstance( hInstance, SW_HIDE, nWidth, nHeight );
	NI_ASSERT_TF( bRetVal, "Can't init app instance", return false; );

	return true;
	#endif
}
static void PreRegisterClass( WNDCLASSEX& wcex )
{
	wcex.cbSize = sizeof( WNDCLASSEX );

	wcex.style = CS_DBLCLKS;
	wcex.lpfnWndProc = reinterpret_cast<WNDPROC>( WndProc );
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon   = LoadIcon( hInstance, reinterpret_cast<LPCTSTR>(IDI_MAIN) ); // main icon
	wcex.hIconSm = LoadIcon( hInstance, reinterpret_cast<LPCTSTR>(IDI_MAIN) ); // small icon
	wcex.hCursor = 0;       // LoadCursor( NULL, IDC_ARROW ); <= we don't need cursor in our window!
	wcex.hbrBackground = reinterpret_cast<HBRUSH>( GetStockObject(NULL_BRUSH) ); // <= we don't need any color filling
	wcex.lpszMenuName = 0;  // <= no menu
	wcex.lpszClassName = szWndClassName.c_str();
}
static ATOM RegisterClass( HINSTANCE hInst )
{
	WNDCLASSEX wcex;
	hInstance = hInst;
	PreRegisterClass( wcex );
	return RegisterClassEx( &wcex );
}
static void PreCreateWindow( CREATESTRUCT& cs )
{
  cs.lpCreateParams = 0;
  cs.hInstance = hInstance;
  cs.hMenu = 0;
  cs.hwndParent = 0;
  cs.x = 0;
  cs.y = 0;
  cs.style = IsWindowedMode() ? GetWindowedStyle() : WS_POPUP;// | WS_CLIPSIBLINGS;
  cs.lpszName = szAppTitleName.c_str();
  cs.lpszClass = MAKEINTATOM( atomWndClassName );
  cs.dwExStyle = 0;
}
static bool InitInstance( HINSTANCE hInst, int nCmdShow, int nWidth, int nHeight )
{
  hInstance = hInst;
	CREATESTRUCT cs;
	cs.cx = nWidth;
	cs.cy = nHeight;
	PreCreateWindow( cs );
	if ( IsWindowedMode() )
	{
		RECT rcWindow = { 0, 0, cs.cx, cs.cy };
		AdjustWindowRectEx( &rcWindow, cs.style, FALSE, cs.dwExStyle );
		cs.cx = rcWindow.right - rcWindow.left;
		cs.cy = rcWindow.bottom - rcWindow.top;

		RECT rcWork = { 0, 0, 0, 0 };
		if ( SystemParametersInfo( SPI_GETWORKAREA, 0, &rcWork, 0 ) )
		{
			cs.x = rcWork.left + Max( 0, int( rcWork.right - rcWork.left - cs.cx ) / 2 );
			cs.y = rcWork.top + Max( 0, int( rcWork.bottom - rcWork.top - cs.cy ) / 2 );
		}
	}
  hWnd = CreateWindowEx( cs.dwExStyle, cs.lpszClass, cs.lpszName, cs.style, cs.x, cs.y, cs.cx, cs.cy,
		                     cs.hwndParent, cs.hMenu, cs.hInstance, cs.lpCreateParams );
	NI_ASSERT_TF( hWnd != 0, "Can't create main app window", return false; );
  SetCursor( 0 );

  return TRUE;
}
void ShowAppWindow( bool bShow )
{
	if ( bShow ) sdlApplication.Show(); else sdlApplication.Hide();
}
static bool CheckPreviousApp( LPCSTR pszMainClass, LPCSTR pszMainTitle )
{
  HWND hwndFind, hwndLast, hwndForeGround;
  DWORD dwFindID, dwForeGroundID;
  hwndFind = FindWindow( pszMainClass, pszMainTitle );
  if ( hwndFind )
  {
    hwndForeGround = GetForegroundWindow();
    dwForeGroundID = GetWindowThreadProcessId( hwndForeGround, 0 );
    dwFindID = GetWindowThreadProcessId( hwndFind, 0 );
    if ( (dwFindID != dwForeGroundID) || IsIconic(hwndFind) )
    {
      hwndLast = GetLastActivePopup( hwndFind );
      if ( IsIconic(hwndLast) )
        ShowWindow( hwndLast, SW_RESTORE );
      BringWindowToTop( hwndLast );
      SetForegroundWindow( hwndLast );
    }
    return FALSE;
  }

  return TRUE;
}
void SetActive( bool bActivate ) 
{ 
	bActive = bActivate; 
	if ( bActive )
		CaptureMouse();
	else
		ReleaseMouse();
	if ( IsWindowedMode() )
	{
		if ( bActive && IsIconic(hWnd) )
			ShowWindow( hWnd, SW_RESTORE );
		return;
	}
	if ( !bActive )
		ShowWindow( hWnd, SW_MINIMIZE );
	else
		ShowWindow( hWnd, SW_RESTORE );
}
void CaptureMouse()
{
	bMouseReleased = false;
	sdlApplication.SetMouseGrab( true );
	// The engine draws its own cursor, so the system pointer must be hidden
	// while the game owns the mouse - otherwise both are on screen at once.
	// SDL_HideCursor is the only thing that takes the SDL window's default
	// arrow down; the Win32 SetCursor(0) that did this in the DirectX build
	// never ran here, because the window is SDL's and not one whose class
	// cursor we control.
	sdlApplication.SetCursorVisible( false );
	ApplyMouseClip();
}
void ReleaseMouse()
{
	bMouseReleased = true;
	sdlApplication.SetMouseGrab( false );
	// Give the pointer back when the game no longer owns it, so the desktop
	// and other windows are usable while tabbed out.
	sdlApplication.SetCursorVisible( true );
	::ClipCursor( 0 );
}
// Reconcile the mouse-owned state to whether the window actually holds input
// focus. Capture is otherwise driven by the focus-gained/lost events, but SDL
// only emits those on a transition: a window that comes up already focused - the
// normal case at launch - never gets a focus-gained, so the mouse was never
// grabbed and the system cursor never hidden until the first alt-tab out and
// back. Polling the real focus flag each pump closes that gap and is idempotent.
static void ReconcileMouseCapture()
{
	if ( hWnd == 0 )
		return;
	const bool bFocused = sdlApplication.HasInputFocus();
	if ( bFocused && bMouseReleased )
		CaptureMouse();
	else if ( !bFocused && !bMouseReleased )
		ReleaseMouse();
}
void PumpMessages()
{
	ReconcileMouseCapture();
	NPlatform::PlatformEvent event;
	while ( sdlApplication.PollEvent( event ) )
	{
		switch ( event.type )
		{
			case NPlatform::EventType::quit: bExit = true; break;
			// SwitchGame is what actually pauses: it holds PAUSE_TYPE_INACTIVE on
			// the game timer, drops the gamma ramp and releases the cursor. The
			// WM_ACTIVATE branch above calls it either way; this path only ever
			// set the flag, so switching to another Space left the simulation
			// running against a clock that kept counting while macOS stopped
			// presenting. Coming back, the whole backlog executed at once - on a
			// large map, a lot of orders arriving in one frame.
			case NPlatform::EventType::focusGained:
				SetActive( true );
				NMain::SwitchGame( true );
				break;
			case NPlatform::EventType::focusLost:
				SetActive( false );
				NMain::SwitchGame( false );
				if ( NMain::IsInitialized() ) if ( IInput *input = GetSingleton<IInput>() ) input->ConsumePlatformEvent( event );
				break;
			case NPlatform::EventType::keyDown:
			case NPlatform::EventType::keyUp:
				if ( NMain::IsInitialized() )
				{
					if ( event.key == static_cast<int>( NPlatform::PlatformKey::escape ) || event.key == static_cast<int>( NPlatform::PlatformKey::space ) || event.key == static_cast<int>( NPlatform::PlatformKey::returnKey ) ) AddMovieSkipMessage();
					if ( IInput *input = GetSingleton<IInput>() )
						input->ConsumePlatformEvent( event );
				}
				break;
			case NPlatform::EventType::textInput:
				if ( NMain::IsInitialized() ) if ( IInput *input = GetSingleton<IInput>() ) input->ConsumePlatformEvent( event );
				break;
			case NPlatform::EventType::mouseMotion:
				if ( NMain::IsInitialized() )
				{
					GetSingleton<ICursor>()->SetPos( event.x, event.y );
					if ( IInput *input = GetSingleton<IInput>() )
						input->ConsumePlatformEvent( event );
				}
				break;
			case NPlatform::EventType::mouseButtonDown:
			case NPlatform::EventType::mouseButtonUp:
				if ( NMain::IsInitialized() )
				{
					if ( IInput *input = GetSingleton<IInput>() ) input->ConsumePlatformEvent( event );
				}
				break;
			case NPlatform::EventType::mouseWheel:
				if ( NMain::IsInitialized() )
					if ( IInput *input = GetSingleton<IInput>() ) input->ConsumePlatformEvent( event );
				break;
			default: break;
		}
	}
	/* The SDL owner applies its own native event dispatch; no Win32 queue is pumped here. */
	#if 0
	msgList.clear();
  MSG msg;
	while ( PeekMessage( &msg, 0, 0, 0, PM_NOREMOVE ) )
	{
		if ( ::GetMessage( &msg, 0, 0, 0 ) )
		{
			if ( msg.message == WM_ACTIVATEAPP )
				SetActive( msg.wParam != 0 );
			TranslateMessage( &msg );
			DispatchMessage( &msg );
		}
		else
			bExit = true;
	}
	ApplyMouseClip();
	#endif
}
void ResetExit()
{
	bExit = false;
}
void Exit( int nExitCode )
{
	PostQuitMessage( nExitCode );
}
// The splash bitmap, painted directly onto the dialog. The template names it
// as a static control's SS_BITMAP image, but that reference does not survive
// the resource compiler this build uses: the control came up empty and the
// splash showed as a bare white rectangle where the Blitzkrieg logo belongs.
// Loading the bitmap here and blitting it in WM_PAINT does not depend on the
// control, its id, or its style surviving compilation.
static HBITMAP hSplashBitmap = 0;

static void PaintSplash( HWND hwndDlg, HDC hdc )
{
	if ( hSplashBitmap == 0 )
		return;
	BITMAP bm = {};
	::GetObjectW( hSplashBitmap, sizeof( bm ), &bm );
	HDC hMemDC = ::CreateCompatibleDC( hdc );
	HGDIOBJ hOld = ::SelectObject( hMemDC, hSplashBitmap );
	RECT rc;
	::GetClientRect( hwndDlg, &rc );
	::StretchBlt( hdc, 0, 0, rc.right, rc.bottom, hMemDC, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY );
	::SelectObject( hMemDC, hOld );
	::DeleteDC( hMemDC );
}

INT_PTR CALLBACK SplashScreenDialogProc( HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	switch ( uMsg )
	{
		case WM_INITDIALOG:
			{
				const int nScreenSizeX = GetSystemMetrics( SM_CXSCREEN );
				const int nScreenSizeY = GetSystemMetrics( SM_CYSCREEN );
				const int nX = ( nScreenSizeX - SPLASH_SCREEN_SIZE_X ) / 2;
				const int nY = ( nScreenSizeY - SPLASH_SCREEN_SIZE_Y ) / 2;
				::MoveWindow( hwndDlg, nX, nY, SPLASH_SCREEN_SIZE_X, SPLASH_SCREEN_SIZE_Y, false );
				if ( hSplashBitmap == 0 )
				{
					// The bitmap is linked into the executable, so load it from
					// the process's own module. The dialog's GWLP_HINSTANCE does
					// not reliably name that module under this build's resource
					// compiler, and loading against the wrong instance returned
					// null - the reason the earlier control-based attempt drew
					// nothing.
					hSplashBitmap = reinterpret_cast<HBITMAP>( ::LoadImage( ::GetModuleHandle( 0 ),
						MAKEINTRESOURCE( IDB_SPLASH ), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION ) );
					if ( getenv( "BK_GFX_TRACE" ) )
						fprintf( stderr, "BK_GFX_TRACE: splash bitmap load %s\n", hSplashBitmap ? "ok" : "FAILED" );
				}
			}
			return 1;
		case WM_ERASEBKGND:
			return 1;			// the WM_PAINT blit paints every pixel; skip the flash of the default background
		case WM_PAINT:
			{
				PAINTSTRUCT ps;
				HDC hdc = ::BeginPaint( hwndDlg, &ps );
				PaintSplash( hwndDlg, hdc );
				::EndPaint( hwndDlg, &ps );
			}
			return 1;
		case WM_PRINTCLIENT:
			// PrintWindow (used for offscreen capture and by some compositors)
			// asks the control to draw into a supplied DC rather than through
			// BeginPaint, so the same blit has to answer here or the capture
			// comes back blank.
			PaintSplash( hwndDlg, reinterpret_cast<HDC>( wParam ) );
			return 1;
		case WM_DESTROY:
			if ( hSplashBitmap != 0 )
			{
				::DeleteObject( hSplashBitmap );
				hSplashBitmap = 0;
			}
			return 0;
		default:
			return 0;
	}
}
void ShowSplashScreen( HINSTANCE hInstance, bool bShow )
{
	if ( bShow )
	{
		if ( hWndSplashScreen == 0 )
		{
			hWndSplashScreen = CreateDialog( hInstance, "IDD_SPLASH_SCREEN", GetDesktopWindow(), SplashScreenDialogProc );
			::SetWindowText( hWndSplashScreen, szAppTitleName.c_str() );
			// Topmost: the splash must stay visible over the game window, which
			// IGFX::SetMode() shows and activates while startup is still running.
			::SetWindowPos( hWndSplashScreen, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE );
		}
		ShowWindow( hWndSplashScreen, SW_SHOW );
		UpdateWindow( hWndSplashScreen );
		// The main loop pumps SDL events, not the Win32 message queue this
		// modeless dialog would normally paint through, so WM_PAINT may never be
		// delivered and the splash would stay unpainted for its whole lifetime.
		// Blit the bitmap onto the window directly, once, right after it is
		// shown.
		{
			HDC hdc = ::GetDC( hWndSplashScreen );
			if ( hdc != 0 )
			{
				PaintSplash( hWndSplashScreen, hdc );
				::ReleaseDC( hWndSplashScreen, hdc );
			}
		}
	}
	else if ( hWndSplashScreen != 0 )
	{
		DestroyWindow( hWndSplashScreen );
		hWndSplashScreen = 0;
		if ( hWnd != 0 )
			::SetForegroundWindow( hWnd );
	}
}
}; // namespace NWinFrame
