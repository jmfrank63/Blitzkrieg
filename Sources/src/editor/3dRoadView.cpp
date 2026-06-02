#include "StdAfx.h"
#include "3DRoadView.h"
#include "3DRoadFrm.h"
#include "frames.h"


C3DRoadView::C3DRoadView()
{
}

C3DRoadView::~C3DRoadView()
{
}


BEGIN_MESSAGE_MAP(C3DRoadView, CWnd)
ON_WM_PAINT()
END_MESSAGE_MAP()



BOOL C3DRoadView::PreCreateWindow(CREATESTRUCT& cs) 
{
	if (!CWnd::PreCreateWindow(cs))
		return FALSE;
	
	cs.lpszClass = AfxRegisterWndClass(CS_HREDRAW|CS_VREDRAW|CS_DBLCLKS, 
		::LoadCursor(NULL, IDC_ARROW), HBRUSH(COLOR_WINDOW+1), NULL);
	
	return TRUE;
}


void C3DRoadView::OnPaint() 
{
	C3DRoadFrame *pFrame = static_cast<C3DRoadFrame *> ( g_frameManager.GetFrame( CFrameManager::E_3DROAD_FRAME ) );
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
