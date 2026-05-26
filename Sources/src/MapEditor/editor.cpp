// editor.cpp : Defines the class behaviors for the application.
//

#include "stdafx.h"
#include <crtdbg.h>
#include <afxadv.h>

#include "EditorWindowSingleton.h"
#include "editor.h"

#include "MainFrm.h"
#include "frames.h"
#include "TemplateEditorFrame1.h"

#include "..\Main\iMain.h"
#include "..\RandomMapGen\Registry_Types.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

namespace
{
	class CStartupSplashWnd : public CWnd
	{
	public:
		BOOL Create( UINT nBitmapID )
		{
			if ( !m_bitmap.LoadBitmap( nBitmapID ) )
			{
				return FALSE;
			}

			BITMAP bitmapInfo = {};
			m_bitmap.GetBitmap( &bitmapInfo );

			CString strClassName = AfxRegisterWndClass( CS_SAVEBITS | CS_HREDRAW | CS_VREDRAW, ::LoadCursor( 0, IDC_APPSTARTING ), (HBRUSH)::GetStockObject( BLACK_BRUSH ), 0 );
			const int nPosX = ( ::GetSystemMetrics( SM_CXSCREEN ) - bitmapInfo.bmWidth ) / 2;
			const int nPosY = ( ::GetSystemMetrics( SM_CYSCREEN ) - bitmapInfo.bmHeight ) / 2;

			if ( !CWnd::CreateEx( WS_EX_TOOLWINDOW, strClassName, _T( "" ), WS_POPUP, nPosX, nPosY, bitmapInfo.bmWidth, bitmapInfo.bmHeight, 0, 0 ) )
			{
				m_bitmap.DeleteObject();
				return FALSE;
			}

			ShowWindow( SW_SHOWNOACTIVATE );
			UpdateWindow();
			return TRUE;
		}

		void Dismiss()
		{
			if ( GetSafeHwnd() )
			{
				DestroyWindow();
			}
		}

	protected:
		afx_msg void OnPaint()
		{
			CPaintDC dc( this );

			CDC memDC;
			memDC.CreateCompatibleDC( &dc );
			CBitmap *pOldBitmap = memDC.SelectObject( &m_bitmap );

			BITMAP bitmapInfo = {};
			m_bitmap.GetBitmap( &bitmapInfo );
			dc.BitBlt( 0, 0, bitmapInfo.bmWidth, bitmapInfo.bmHeight, &memDC, 0, 0, SRCCOPY );

			memDC.SelectObject( pOldBitmap );
		}

		afx_msg BOOL OnEraseBkgnd( CDC* )
		{
			return TRUE;
		}

	private:
		CBitmap m_bitmap;

		DECLARE_MESSAGE_MAP()
	};

	BEGIN_MESSAGE_MAP( CStartupSplashWnd, CWnd )
		ON_WM_PAINT()
		ON_WM_ERASEBKGND()
	END_MESSAGE_MAP()
}

/////////////////////////////////////////////////////////////////////////////
// CEditorApp

BEGIN_MESSAGE_MAP(CEditorApp, CWinApp)
	//{{AFX_MSG_MAP(CEditorApp)
	ON_COMMAND(ID_APP_ABOUT, OnAppAbout)
	ON_COMMAND(ID_HELP, OnHelp)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP() 

/////////////////////////////////////////////////////////////////////////////
// CEditorApp construction

CEditorApp::CEditorApp()
{
	//_CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
	//	_CrtSetBreakAlloc( 80 );
	int nBreakId = -1;
	//_CrtSetBreakAlloc( nBreakId );
	m_pMainFrame = 0;
}

/////////////////////////////////////////////////////////////////////////////
// The one and only CEditorApp object

CEditorApp theApp;

/////////////////////////////////////////////////////////////////////////////
// CEditorApp initialization
BOOL CEditorApp::InitInstance()
{
	// check for network drive
	if ( !NMain::CanLaunch() )
		return false;
	
	std::string szCommandLine( m_lpCmdLine );
	NStr::TrimBoth( szCommandLine, '\"' );

	//если проинсталлирована игра, включаем поддержку игры
	NMain::SetGameDirectory();
	/**
	if ( !NMain::SetGameDirectory() )
	{
		if ( !szCommandLine.empty() )
		{
			CString strTitle;
			strTitle.LoadString( IDR_EDITORTYPE );
			MessageBox( ::GetDesktopWindow(), _T( "Blitzkrieg Map Editor not installed. Please install Blitzkrieg Map Editor!" ), strTitle, MB_OK | MB_ICONEXCLAMATION );
			return false;	
		}
	}
	/**/
	
	CEditorWindowSingletonChecker editorWindowSingletonChecker;
	if ( szCommandLine.empty() )
	{
		if ( editorWindowSingletonChecker.BringAppOnTop() )
		{
			return false;
		}
	}
	else
	{
		if ( editorWindowSingletonChecker.OpenFileOnApp( szCommandLine ) )
		{
			return false;
		}
	}
	/**/
	CStartupSplashWnd splashWnd;
	CStartupSplashWnd *pSplashWnd = 0;
	if ( splashWnd.Create( IDB_EDITOR_STARTUP ) )
	{
		pSplashWnd = &splashWnd;
	}
	/**/
	
#if defined( _DO_SEH ) && !defined( _DEBUG )
	// set StructuredExceptionHandler 
	SetCrashHandlerFilter( CrashHandlerFilter );
#endif // defined( _DO_SEH ) && !defined( _DEBUG )
	// Standard initialization
	// If you are not using these features and wish to reduce the size
	//  of your final executable, you should remove from the following
	//  the specific initialization routines you do not need.

#ifdef _AFXDLL
	Enable3dControls();			// Call this when using MFC in a shared DLL
#else
	Enable3dControlsStatic();	// Call this when linking to MFC statically
#endif

	// Change the registry key under which our settings are stored.
	// TODO: You should modify this string to be something appropriate
	// such as the name of your company or organization.
//	SetRegistryKey(_T("Local AppWizard-Generated Applications"));
	CString strRegistryPathName;
	strRegistryPathName.LoadString( IDS_REGISTRY_PATH );
	SetRegistryKey( strRegistryPathName );
	m_pRecentFileList = new CRecentFileList( 0, "RFL", "file%2d", 15 );
	
	// To create the main window, this code creates a new frame window
	// object and then sets it as the application's main window object.
	m_pMainFrame = new CMainFrame;

	CMDIFrameWnd* pFrame = m_pMainFrame;
	m_pMainWnd = pFrame;

	// create main MDI frame window
//	if (!pFrame->LoadFrame(IDR_MAINFRAME))
	if (!pFrame->LoadFrame(IDR_EDITORTYPE))
	{
		delete pFrame;
		m_pMainWnd = 0;
		return FALSE;
	}

	// try to load shared MDI menus and accelerator table
	//TODO: add additional member variables and load calls for
	//	additional menu types your application may need. 

	HINSTANCE hInst = AfxGetResourceHandle();
	m_hMDIMenu  = ::LoadMenu(hInst, MAKEINTRESOURCE(IDR_EDITORTYPE));
	hMDIAccel = ::LoadAccelerators(hInst, MAKEINTRESOURCE(IDR_EDITORTYPE));

	m_nCmdShow = SW_SHOWMAXIMIZED;

	// The main window has been initialized, so show and update it.
	pFrame->ShowWindow(m_nCmdShow);
	pFrame->UpdateWindow();

  /**/
	if ( pSplashWnd )
	{
		pSplashWnd->Dismiss();
	}
	/**/

	//загрузка начальной карты
	if ( !szCommandLine.empty() )
	{
		g_frameManager.GetTemplateEditorFrame()->OnFileLoadMap( szCommandLine );
	}

	return TRUE;
}

void CEditorApp::SaveRegisterData()
{

}

/////////////////////////////////////////////////////////////////////////////
// CEditorApp message handlers

int CEditorApp::ExitInstance() 
{
#if defined( _DO_SEH ) && !defined( _DEBUG )
	// set StructuredExceptionHandler 
	SetCrashHandlerFilter( 0 );
#endif // defined( _DO_SEH ) && !defined( _DEBUG )
	//TODO: handle additional resources you may have added
	if (m_hMDIMenu != NULL)
		FreeResource(m_hMDIMenu);
	if (hMDIAccel != NULL)
		FreeResource(hMDIAccel);

	return CWinApp::ExitInstance();
}

void CEditorApp::ShowSECControlBar( SECControlBar *pControlBar, int nCommand )
{
	m_pMainFrame->ShowSECControlBar( pControlBar, nCommand );
}

/**
void CEditorApp::SetMainWindowTitle( const char *pszTitle )
{
	m_pMainFrame->SetMainWindowTitle( pszTitle );
}

void CEditorApp::SetMainWindowText( const char *pszText )
{
	m_pMainFrame->SetMainWindowText( pszText );
}
/**/

/////////////////////////////////////////////////////////////////////////////
// CAboutDlg dialog used for App About

class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// Dialog Data
	//{{AFX_DATA(CAboutDlg)
	enum { IDD = IDD_ABOUT };
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CAboutDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	//{{AFX_MSG(CAboutDlg)
		// No message handlers
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
	//{{AFX_DATA_INIT(CAboutDlg)
	//}}AFX_DATA_INIT
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAboutDlg)
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
	//{{AFX_MSG_MAP(CAboutDlg)
		// No message handlers
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

// App command to run the dialog
void CEditorApp::OnAppAbout()
{
	CAboutDlg aboutDlg;
	aboutDlg.DoModal();
}

/////////////////////////////////////////////////////////////////////////////
// CEditorApp message handlers


#ifdef OLD
BOOL CEditorApp::ProcessMessageFilter(int code, LPMSG lpMsg) 
{
	if ( lpMsg->message == WM_KEYDOWN && lpMsg->wParam == VK_ESCAPE || lpMsg->wParam == VK_DELETE )
	{
		::PostMessage( lpMsg->hwnd, WM_KEYDOWN, lpMsg->wParam, lpMsg->lParam );
		return TRUE;
	}

	return CWinApp::ProcessMessageFilter(code, lpMsg);
}
#endif		//OLD

BOOL CEditorApp::OnIdle(LONG lCount) 
{
	
	CWinApp::OnIdle(lCount);
	return TRUE;
}

BOOL CEditorApp::SaveAllModified() 
{
	return TRUE;
}

void CEditorApp::OnHelp() 
{
	if ( m_pMainWnd != 0 )
  {
    if ( CMainFrame *pFrame = static_cast<CMainFrame*>( m_pMainWnd ) )
		{
			pFrame->OnHelp();
		}
  }
}
