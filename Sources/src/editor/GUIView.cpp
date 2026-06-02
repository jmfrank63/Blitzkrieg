#include "StdAfx.h"
#include "GameWnd.h"
#include "GUIView.h"
#include "GUIFrame.h"
#include "frames.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


CGUIView::CGUIView()
{
}

CGUIView::~CGUIView()
{
}


BEGIN_MESSAGE_MAP(CGUIView, CWnd)
ON_WM_PAINT()
END_MESSAGE_MAP()



BOOL CGUIView::PreCreateWindow(CREATESTRUCT& cs) 
{
	if (!CWnd::PreCreateWindow(cs))
		return FALSE;
	
	cs.lpszClass = AfxRegisterWndClass(CS_HREDRAW|CS_VREDRAW|CS_DBLCLKS, 
		::LoadCursor(NULL, IDC_ARROW), HBRUSH(COLOR_WINDOW+1), NULL);
	
	return TRUE;
}

void CGUIView::OnPaint() 
{
/*
	if ( g_frameManager.GetFrame( CFrameManager::E_GUI_FRAME )->IsRunning() )
	{
		RECT valRC;
		g_frameManager.GetGameWnd()->GetWindowRect( &valRC );
		ScreenToClient( &valRC );
		ValidateRect( &valRC );
		
		CWnd::OnPaint();
		g_frameManager.GetFrame( CFrameManager::E_GUI_FRAME )->GFXDraw();
	}
	else
	{
		ValidateRect( 0 );
		CWnd::OnPaint();
	}
*/


	{
		RECT valRC;
		((CWnd *) g_frameManager.GetGameWnd())->GetWindowRect( &valRC );
		ScreenToClient( &valRC );
		ValidateRect( &valRC );
		
		CWnd::OnPaint();
		g_frameManager.GetFrame( CFrameManager::E_GUI_FRAME )->GFXDraw();
	}
}
