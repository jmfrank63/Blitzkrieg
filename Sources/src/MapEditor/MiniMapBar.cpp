#include "StdAfx.h"
#include "editor.h"
#include "MiniMapBar.h"
#include "frames.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

CMiniMapBar::CMiniMapBar() : pwndMainFrame( false )
{
}

CMiniMapBar::~CMiniMapBar()
{
}

void CMiniMapBar::SetMainFrameWindow( CTemplateEditorFrame *_pwndMainFrame )
{
	pwndMainFrame = _pwndMainFrame;
	wndMiniMapDialog.m_frame = pwndMainFrame;
}

BEGIN_MESSAGE_MAP( CMiniMapBar, SECControlBar)
	ON_WM_CREATE()
	ON_WM_SIZE()
END_MESSAGE_MAP()

int CMiniMapBar::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if ( SECControlBar::OnCreate(lpCreateStruct) == -1 )
	{
		return -1;
	}

	wndMiniMapDialog.m_frame = pwndMainFrame;
	wndMiniMapDialog.Create( CMiniMapDialog::IDD, this );
	wndMiniMapDialog.ShowWindow( SW_SHOW );

	g_frameManager.SetMiniMapWindow( &wndMiniMapDialog );
	return 0;
}

void CMiniMapBar::OnSize( UINT nType, int cx, int cy ) 
{
	SECControlBar::OnSize( nType, cx, cy );

	if( wndMiniMapDialog.GetSafeHwnd() != 0 )
	{
		CRect insideRect;
		GetInsideRect( insideRect );
		wndMiniMapDialog.SetWindowPos( 0, insideRect.left, insideRect.top, insideRect.Width(), insideRect.Height(), SWP_NOZORDER | SWP_NOACTIVATE );
	}
}

