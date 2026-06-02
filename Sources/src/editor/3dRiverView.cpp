#include "StdAfx.h"
#include "3DRiverView.h"
#include "3DRiverFrm.h"
#include "frames.h"


C3DRiverView::C3DRiverView()
{
}

C3DRiverView::~C3DRiverView()
{
}


BEGIN_MESSAGE_MAP(C3DRiverView, CWnd)
ON_WM_PAINT()
END_MESSAGE_MAP()



BOOL C3DRiverView::PreCreateWindow(CREATESTRUCT& cs) 
{
	if (!CWnd::PreCreateWindow(cs))
		return FALSE;
	
	cs.lpszClass = AfxRegisterWndClass(CS_HREDRAW|CS_VREDRAW|CS_DBLCLKS, 
		::LoadCursor(NULL, IDC_ARROW), HBRUSH(COLOR_WINDOW+1), NULL);
	
	return TRUE;
}


void C3DRiverView::OnPaint() 
{
	C3DRiverFrame *pFrame = static_cast<C3DRiverFrame *> ( g_frameManager.GetFrame( CFrameManager::E_3DRIVER_FRAME ) );
	{
		RECT valRC;
		((CWnd *) g_frameManager.GetGameWnd())->GetWindowRect( &valRC );
		ScreenToClient( &valRC );
		ValidateRect( &valRC );
		
		CWnd::OnPaint();
		pFrame->GFXDraw();
	}
/*
	else
	{
		CWnd::OnPaint();
		ValidateRect( 0 );
	}
*/
}
