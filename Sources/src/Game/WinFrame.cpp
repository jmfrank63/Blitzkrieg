#include "StdAfx.h"

#include <mmsystem.h>

#include "WinFrame.h"
#include "SysKeys.h"

#include "..\Misc\Win32Helper.h"
#include "..\Main\iMain.h"
#include "..\Scene\Scene.h"
#include "..\Input\Input.h"
#include "..\Input\InputTypes.h"

#include "resource.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NWin32Helper;
static CCriticalSection msgs;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define SPLASH_SCREEN_SIZE_X 600
#define SPLASH_SCREEN_SIZE_Y 352

#ifndef WM_MOUSEWHEEL
#define WM_MOUSEWHEEL	0x020A
#endif // WM_MOUSEWHEEL
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
namespace NWinFrame
{
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static HWND hWnd = 0;                   // window handle
static HINSTANCE hInstance = 0;         // instance handle
static ATOM atomWndClassName = 0;       // atom window class name identification (assigned during registration)
static bool bActive = false;
static bool bExit = false;
static std::list<SWindowsMsg> msgList;  // pumped messages
static std::string szAppTitleName = " Blitzkrieg Game"; // application title ( will be loaded during initialization )
static std::string szWndClassName = "NIVAL_RTS_ENGINE"; // user window class name ( will be loaded during initialization )
// splash screen data
static HWND hWndSplashScreen = 0;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool IsActive() { return bActive; }
bool IsExit() { return bExit; }
HWND GetHWnd() { return hWnd; }
HINSTANCE GetHInstance() { return hInstance; }
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void AddMsg( SWindowsMsg::EMsg msg, int x, int y, DWORD dwFlags );
ATOM RegisterClass( HINSTANCE hInst );
bool CheckPreviousApp( LPCSTR pszMainClass, LPCSTR pszMainTitle );
bool InitInstance( HINSTANCE hInst, int nCmdShow, int nWidth, int nHeight );
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int TranslateCoords( const LPARAM lParam )
{
	const int x = LOWORD( lParam );
	const int y = HIWORD( lParam );
	return ( ( x & 0x7FFF ) | ( (y & 0x7FFF) << 15 ) ) | 0x40000000;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// did not know how to return NCHitTest
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
    // Halt frame movement while the app is sizing or moving
			NI_ASSERT( 0 );
      break;
    case WM_EXITSIZEMOVE:
      break;
		case WM_SETCURSOR:
			if ( NMain::IsInitialized() ) 
				GetSingleton<ICursor>()->OnSetCursor();
			//SetCursor( 0 );
//			SetCursor( LoadCursor(0, IDC_ARROW) );
			break;
    case WM_NCHITTEST:
      // Prevent the user from selecting the menu in fullscreen mode
      //if( !m_bWindowed )
      return HTCLIENT;
      break;
    case WM_POWERBROADCAST:
      switch( wParam )
      {
        case PBT_APMQUERYSUSPEND:
          // At this point, the app should save any data for open
          // network connections, files, etc., and prepare to go into
          // a suspended mode.
          return TRUE;

        case PBT_APMRESUMESUSPEND:
          // At this point, the app should recover any data, network
          // connections, files, etc., and resume running from when
          // the app was suspended.
          return TRUE;
      }
      break;
    case WM_SYSCOMMAND:
      // Prevent moving/sizing and power loss in fullscreen mode
      switch( wParam )
      {
        case SC_MOVE:
        case SC_SIZE:
        case SC_MAXIMIZE:
        case SC_KEYMENU:
        case SC_MONITORPOWER:
				case SC_SCREENSAVE:
          //if( FALSE == m_bWindowed )
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
			//if ( !(HIWORD(wParam)) )          // if window is not minimized
			{
				switch ( LOWORD(wParam) )
				{
					case WA_CLICKACTIVE:					// activate window
					case WA_ACTIVE:
						SetActive( true );
						NMain::SwitchGame( true );
						//NSysKeys::EnableSystemKeys( false, hInstance );
						break;
					case WA_INACTIVE:						// deactivate window
						SetActive( false );
						NMain::SwitchGame( false );
						::ClipCursor( 0 );
						//NSysKeys::EnableSystemKeys( true, hInstance );
						break;
				}
			}
			break;

		case WM_LBUTTONDOWN:
			if ( GetSingleton<IInput>()->IsEmulated(DEVICE_TYPE_MOUSE) ) 
				GetSingleton<IInput>()->EmulateInput( DEVICE_TYPE_MOUSE, INPUT_CONTROL_MOUSE_BUTTON0, 0x80, timeGetTime(), TranslateCoords(lParam) );
			break;
		case WM_LBUTTONUP:
			if ( GetSingleton<IInput>()->IsEmulated(DEVICE_TYPE_MOUSE) ) 
				GetSingleton<IInput>()->EmulateInput( DEVICE_TYPE_MOUSE, INPUT_CONTROL_MOUSE_BUTTON0, 0x00, timeGetTime(), TranslateCoords(lParam) );
			break;
		case WM_LBUTTONDBLCLK:
			if ( GetSingleton<IInput>()->IsEmulated(DEVICE_TYPE_MOUSE) ) 
				GetSingleton<IInput>()->EmulateInput( DEVICE_TYPE_MOUSE, INPUT_CONTROL_MOUSE_BUTTON0 | 0x4000, 0x80, timeGetTime(), TranslateCoords(lParam) );
			break;
		case WM_RBUTTONDOWN:
			if ( GetSingleton<IInput>()->IsEmulated(DEVICE_TYPE_MOUSE) ) 
				GetSingleton<IInput>()->EmulateInput( DEVICE_TYPE_MOUSE, INPUT_CONTROL_MOUSE_BUTTON1, 0x80, timeGetTime(), TranslateCoords(lParam) );
			break;
		case WM_RBUTTONUP:
			if ( GetSingleton<IInput>()->IsEmulated(DEVICE_TYPE_MOUSE) ) 
				GetSingleton<IInput>()->EmulateInput( DEVICE_TYPE_MOUSE, INPUT_CONTROL_MOUSE_BUTTON1, 0x00, timeGetTime(), TranslateCoords(lParam) );
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
			/*
		case WM_MOUSEMOVE:
			AddMsg( SWindowsMsg::MOUSE_MOVE, lParam & 0xFFFF, (lParam >> 16) & 0xFFFF, wParam );
			break;
		case WM_KEYDOWN:
			AddMsg( SWindowsMsg::KEY_DOWN, wParam, lParam & 0xFFFF, (lParam >> 16) & 0xFFFF );
			break;
		case WM_KEYUP:
			AddMsg( SWindowsMsg::KEY_UP, wParam, lParam & 0xFFFF, (lParam >> 16) & 0xFFFF );
			break;
			*/
		case WM_INPUTLANGCHANGEREQUEST:
			// lParam (Low Word) - Language Identifier
			break;
		case WM_INPUTLANGCHANGE:
			// wParam - character set
			// lParam - input locale identifier (HKL)
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
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool InitApplication( HINSTANCE hInstance, const char *pszAppName, const char *pszWndName, int nWidth, int nHeight )
{
	szAppTitleName = pszAppName;
	szWndClassName = pszWndName;
  // create and register class style
  atomWndClassName = RegisterClass( hInstance );
	NI_ASSERT_TF( atomWndClassName != 0, "Can't register class", return false; );
  // check for this app already runed
  bool bRetVal = CheckPreviousApp( reinterpret_cast<LPCSTR>(atomWndClassName), szAppTitleName.c_str() );
	if ( bRetVal == false )
		return bRetVal;
	// Perform application initialization
	bRetVal = InitInstance( hInstance, SW_HIDE, nWidth, nHeight );
	NI_ASSERT_TF( bRetVal, "Can't init app instance", return false; );

	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// PURPOSE: fills window class description struct.
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
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// PURPOSE: Registers the window class.
//
// COMMENTS:
//   This function and its usage is only necessary if you want this code
//   to be compatible with Win32 systems prior to the 'RegisterClassEx'
//   function that was added to Windows 95. It is important to call this function
//   so that the application will get 'well formed' small icons associated
//   with it.
static ATOM RegisterClass( HINSTANCE hInst )
{
	WNDCLASSEX wcex;
	hInstance = hInst;
	PreRegisterClass( wcex );
	return RegisterClassEx( &wcex );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// PURPOSE: fills create struct
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
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// PURPOSE: Saves instance handle and creates main window
static bool InitInstance( HINSTANCE hInst, int nCmdShow, int nWidth, int nHeight )
{
  // Store instance handle in our global variable
  hInstance = hInst;
	//
	CREATESTRUCT cs;
	cs.cx = nWidth;
	cs.cy = nHeight;
	PreCreateWindow( cs );
  // create window with sysmenu on the taskbar
  hWnd = CreateWindowEx( cs.dwExStyle, cs.lpszClass, cs.lpszName, cs.style, cs.x, cs.y, cs.cx, cs.cy,
		                     cs.hwndParent, cs.hMenu, cs.hInstance, cs.lpCreateParams );
  // return with FALSE if window wasn't created
	NI_ASSERT_TF( hWnd != 0, "Can't create main app window", return false; );
  // show & update window
  //ShowWindow( hWnd, nCmdShow );
  //UpdateWindow( hWnd );
  // eliminate cursor once for this widow
  SetCursor( 0 );
//  SetCursor( LoadCursor(0, IDC_ARROW) );

  return TRUE;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ShowAppWindow( bool bShow )
{
  ShowWindow( hWnd, bShow ? SW_SHOW : SW_HIDE );
  UpdateWindow( hWnd );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// PURPOSE: prevent application from running more then one application window
static bool CheckPreviousApp( LPCSTR pszMainClass, LPCSTR pszMainTitle )
{
  HWND hwndFind, hwndLast, hwndForeGround;
  DWORD dwFindID, dwForeGroundID;
  // Check if application is already running
  hwndFind = FindWindow( pszMainClass, pszMainTitle );
  if ( hwndFind )
  {
    // Bring previously running application's main
    // window to the user's attention
    hwndForeGround = GetForegroundWindow();
    dwForeGroundID = GetWindowThreadProcessId( hwndForeGround, 0 );
    dwFindID = GetWindowThreadProcessId( hwndFind, 0 );
    // Don't do anything if window is already in foreground
    // Unless it is iconized.
    if ( (dwFindID != dwForeGroundID) || IsIconic(hwndFind) )
    {
      hwndLast = GetLastActivePopup( hwndFind );
      if ( IsIconic(hwndLast) )
        ShowWindow( hwndLast, SW_RESTORE );
      BringWindowToTop( hwndLast );
      SetForegroundWindow( hwndLast );
    }
    // Prevent additional instance's of this application
    // from running
    return FALSE;
  }

  return TRUE;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void SetActive( bool bActivate ) 
{ 
	bActive = bActivate; 
	if ( !bActive )
		ShowWindow( hWnd, SW_MINIMIZE );
	else
		ShowWindow( hWnd, SW_RESTORE );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void PumpMessages()
{
	msgList.clear();
  // Now we are ready to recieve and process Windows messages.
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
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void ResetExit()
{
	bExit = false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Exit( int nExitCode )
{
	PostQuitMessage( nExitCode );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** splash screen
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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
		//ShowWindow( hWndSplashScreen, SW_HIDE );
		//UpdateWindow( hWndSplashScreen );
		//CloseWindow( hWndSplashScreen );
		DestroyWindow( hWndSplashScreen );
		hWndSplashScreen = 0;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
}; // namespace NWinFrame
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
