
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


BEGIN_MESSAGE_MAP(CEditorApp, CWinApp)
	ON_COMMAND(ID_APP_ABOUT, OnAppAbout)
	ON_COMMAND(ID_HELP, OnHelp)
END_MESSAGE_MAP() 


CEditorApp::CEditorApp()
{
	int nBreakId = -1;
	m_pMainFrame = 0;
}


CEditorApp theApp;

BOOL CEditorApp::InitInstance()
{
	if ( !NMain::CanLaunch() )
		return false;
	
	std::string szCommandLine( m_lpCmdLine );
	NStr::TrimBoth( szCommandLine, '\"' );

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
	SetCrashHandlerFilter( CrashHandlerFilter );
#endif // defined( _DO_SEH ) && !defined( _DEBUG )

#ifdef _AFXDLL
	Enable3dControls();			// Call this when using MFC in a shared DLL
#else
	Enable3dControlsStatic();	// Call this when linking to MFC statically
#endif

	CString strRegistryPathName;
	strRegistryPathName.LoadString( IDS_REGISTRY_PATH );
	SetRegistryKey( strRegistryPathName );
	m_pRecentFileList = new CRecentFileList( 0, "RFL", "file%2d", 15 );
	
	m_pMainFrame = new CMainFrame;

	CMDIFrameWnd* pFrame = m_pMainFrame;
	m_pMainWnd = pFrame;

	if (!pFrame->LoadFrame(IDR_EDITORTYPE))
	{
		delete pFrame;
		m_pMainWnd = 0;
		return FALSE;
	}


	HINSTANCE hInst = AfxGetResourceHandle();
	m_hMDIMenu  = ::LoadMenu(hInst, MAKEINTRESOURCE(IDR_EDITORTYPE));
	hMDIAccel = ::LoadAccelerators(hInst, MAKEINTRESOURCE(IDR_EDITORTYPE));

	m_nCmdShow = SW_SHOWMAXIMIZED;

	pFrame->ShowWindow(m_nCmdShow);
	pFrame->UpdateWindow();

  /**/
	if ( pSplashWnd )
	{
		pSplashWnd->Dismiss();
	}
	/**/

	if ( !szCommandLine.empty() )
	{
		g_frameManager.GetTemplateEditorFrame()->OnFileLoadMap( szCommandLine );
	}

	return TRUE;
}

void CEditorApp::SaveRegisterData()
{

}


int CEditorApp::ExitInstance() 
{
#if defined( _DO_SEH ) && !defined( _DEBUG )
	SetCrashHandlerFilter( 0 );
#endif // defined( _DO_SEH ) && !defined( _DEBUG )
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


class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

	enum { IDD = IDD_ABOUT };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
END_MESSAGE_MAP()

void CEditorApp::OnAppAbout()
{
	CAboutDlg aboutDlg;
	aboutDlg.DoModal();
}



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
