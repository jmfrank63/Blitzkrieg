#include "StdAfx.h"
#include "resource.h"
#include "TreeDockWindow.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

CTreeDockWindow::CTreeDockWindow()
	: pwndMainFrame( 0 )
{
}

CTreeDockWindow::~CTreeDockWindow()
{
}

void CTreeDockWindow::SetMainFrameWindow( CWnd *_pwndMainFrame )
{
	pwndMainFrame = _pwndMainFrame;
}

BEGIN_MESSAGE_MAP(CTreeDockWindow, SECControlBar)
	ON_WM_CREATE()
	ON_WM_SIZE()
END_MESSAGE_MAP()


int CTreeDockWindow::OnCreate( LPCREATESTRUCT lpCreateStruct ) 
{
	if ( SECControlBar::OnCreate( lpCreateStruct ) == -1 )
	{
		return -1;
	}


	DWORD dwStyle = TVS_SHOWSELALWAYS |
									TVS_HASBUTTONS |
									TVS_LINESATROOT |
									TVS_HASLINES |
									TVS_SHOWSELALWAYS |
									TVS_DISABLEDRAGDROP |
									WS_CHILD | WS_VISIBLE;

	DWORD dwStyleEx = TVXS_FLYBYTOOLTIPS |
										LVXS_HILIGHTSUBITEMS;

	BOOL bCreated = wndTree.Create( dwStyle, dwStyleEx, CRect( 0, 0, 0, 0 ), this, IDC_EMBEDDED_CONTROL );

	NI_ASSERT_T( bCreated, NStr::Format( _T( "CTreeDockWindow::OnCreate, cant't creat Tree" ) ) );

	wndTree.ModifyStyleEx( 0, WS_EX_CLIENTEDGE, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
	

	return 0;
}

void CTreeDockWindow::OnSize( UINT nType, int cx, int cy ) 
{
	SECControlBar::OnSize( nType, cx, cy );
	
	if( wndTree.GetSafeHwnd() != 0 )
	{
		CRect insideRect;
		GetInsideRect( insideRect );
		wndTree.SetWindowPos( &CWnd::wndTop, insideRect.left, insideRect.top, insideRect.Width(), insideRect.Height(), SWP_NOACTIVATE | SWP_SHOWWINDOW );
	}
}
