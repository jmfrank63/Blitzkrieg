#include "StdAfx.h"

#include <mmsystem.h>

#include "WinFrame.h"
#include "SysKeys.h"

#include "..\Misc\Win32Helper.h"
#include "..\Main\iMain.h"
#include "..\Main\iMainCommands.h"
#include "..\GameTT\iMission.h"
#include "..\Scene\Scene.h"
#include "..\Input\Input.h"
#include "..\Input\InputTypes.h"

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
static ATOM atomWndClassName = 0;       // atom window class name identification (assigned during registration)
static bool bActive = false;
static bool bExit = false;
static std::list<SWindowsMsg> msgList;  // pumped messages
static std::string szAppTitleName = " Blitzkrieg Game"; // application title ( will be loaded during initialization )
static std::string szWndClassName = "NIVAL_RTS_ENGINE"; // user window class name ( will be loaded during initialization )
static HWND hWndSplashScreen = 0;
bool IsActive() { return bActive; }
bool IsExit() { return bExit; }
HWND GetHWnd() { return hWnd; }
HINSTANCE GetHInstance() { return hInstance; }
void AddMsg( SWindowsMsg::EMsg msg, int x, int y, DWORD dwFlags );
static void AddInputMessage( const int nEventID, const int nParam = 0 );
static void AddMouseActionMessage( const int nEventID, const LPARAM lParam );
static void UpdateCursorPos( const LPARAM lParam );
static void AddMovieSkipMessage();
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
			NI_ASSERT( 0 );
      break;
    case WM_EXITSIZEMOVE:
      break;
		case WM_SETCURSOR:
			if ( NMain::IsInitialized() ) 
				GetSingleton<ICursor>()->OnSetCursor();
			break;
    case WM_NCHITTEST:
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
      switch( wParam )
      {
        case SC_MOVE:
        case SC_SIZE:
        case SC_MAXIMIZE:
        case SC_KEYMENU:
        case SC_MONITORPOWER:
				case SC_SCREENSAVE:
          return 1; // in both modes is prevented
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
						::ClipCursor( 0 );
						break;
				}
			}
			break;

		case WM_LBUTTONDOWN:
			if ( GetSingleton<IInput>()->IsEmulated(DEVICE_TYPE_MOUSE) )
			{
				SetCapture( hWnd );
				AddMouseActionMessage( CMD_BEGIN_ACTION1, lParam );
				AddMovieSkipMessage();
				GetSingleton<IInput>()->EmulateInput( DEVICE_TYPE_MOUSE, INPUT_CONTROL_MOUSE_BUTTON0, 0x80, timeGetTime(), TranslateCoords(lParam) );
			}
			break;
		case WM_LBUTTONUP:
			if ( GetSingleton<IInput>()->IsEmulated(DEVICE_TYPE_MOUSE) )
			{
				AddMouseActionMessage( CMD_END_ACTION1, lParam );
				GetSingleton<IInput>()->EmulateInput( DEVICE_TYPE_MOUSE, INPUT_CONTROL_MOUSE_BUTTON0, 0x00, timeGetTime(), TranslateCoords(lParam) );
				if ( GetCapture() == hWnd )
					ReleaseCapture();
			}
			break;
		case WM_LBUTTONDBLCLK:
			if ( GetSingleton<IInput>()->IsEmulated(DEVICE_TYPE_MOUSE) )
			{
				AddMouseActionMessage( CMD_MOUSE0_DBLCLK, lParam );
				AddMovieSkipMessage();
				GetSingleton<IInput>()->EmulateInput( DEVICE_TYPE_MOUSE, INPUT_CONTROL_MOUSE_BUTTON0 | 0x4000, 0x80, timeGetTime(), TranslateCoords(lParam) );
			}
			break;
		case WM_RBUTTONDOWN:
			if ( GetSingleton<IInput>()->IsEmulated(DEVICE_TYPE_MOUSE) )
			{
				SetCapture( hWnd );
				AddMouseActionMessage( CMD_BEGIN_ACTION2, lParam );
				AddMovieSkipMessage();
				GetSingleton<IInput>()->EmulateInput( DEVICE_TYPE_MOUSE, INPUT_CONTROL_MOUSE_BUTTON1, 0x80, timeGetTime(), TranslateCoords(lParam) );
			}
			break;
		case WM_RBUTTONUP:
			if ( GetSingleton<IInput>()->IsEmulated(DEVICE_TYPE_MOUSE) )
			{
				AddMouseActionMessage( CMD_END_ACTION2, lParam );
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
				BOOL bSuccess = TranslateCharsetInfo( (DWORD*)dwCharSet, &csi, TCI_SRCCHARSET );
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
static void AddMouseActionMessage( const int nEventID, const LPARAM lParam )
{
	if ( NMain::IsInitialized() )
	{
		UpdateCursorPos( lParam );
		AddInputMessage( nEventID, TranslateCoords(lParam) );
	}
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
static void AddMsg( SWindowsMsg::EMsg msg, int x, int y, DWORD dwFlags )
{
	CCriticalSectionLock lock( msgs );
	msgList.push_back( SWindowsMsg() );
	SWindowsMsg &m = msgList.back();
	m.time = GetTickCount();
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
	pRes->time = GetTickCount();
	return false;
}
bool InitApplication( HINSTANCE hInstance, const char *pszAppName, const char *pszWndName, int nWidth, int nHeight )
{
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
  cs.style = WS_POPUP;// | WS_CLIPSIBLINGS;
  cs.lpszName = szAppTitleName.c_str();
  cs.lpszClass = LPCSTR( atomWndClassName );
  cs.dwExStyle = 0;
}
static bool InitInstance( HINSTANCE hInst, int nCmdShow, int nWidth, int nHeight )
{
  hInstance = hInst;
	CREATESTRUCT cs;
	cs.cx = nWidth;
	cs.cy = nHeight;
	PreCreateWindow( cs );
  hWnd = CreateWindowEx( cs.dwExStyle, cs.lpszClass, cs.lpszName, cs.style, cs.x, cs.y, cs.cx, cs.cy,
		                     cs.hwndParent, cs.hMenu, cs.hInstance, cs.lpCreateParams );
	NI_ASSERT_TF( hWnd != 0, "Can't create main app window", return false; );
  SetCursor( 0 );

  return TRUE;
}
void ShowAppWindow( bool bShow )
{
  ShowWindow( hWnd, bShow ? SW_SHOW : SW_HIDE );
  UpdateWindow( hWnd );
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
	if ( !bActive )
		ShowWindow( hWnd, SW_MINIMIZE );
	else
		ShowWindow( hWnd, SW_RESTORE );
}
void PumpMessages()
{
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
}
void ResetExit()
{
	bExit = false;
}
void Exit( int nExitCode )
{
	PostQuitMessage( nExitCode );
}
BOOL CALLBACK SplashScreenDialogProc( HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam )
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
				HWND hwndPicture = ::GetDlgItem( hwndDlg, IDC_PICTURE );
				if ( ::IsWindow(hwndPicture) ) 
					::MoveWindow( hwndPicture, 0, 0, SPLASH_SCREEN_SIZE_X, SPLASH_SCREEN_SIZE_Y, false );
			}
			return 1;
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
		}
		ShowWindow( hWndSplashScreen, SW_SHOW );
		UpdateWindow( hWndSplashScreen );
	}
	else if ( hWndSplashScreen != 0 )
	{
		DestroyWindow( hWndSplashScreen );
		hWndSplashScreen = 0;
	}
}
}; // namespace NWinFrame
