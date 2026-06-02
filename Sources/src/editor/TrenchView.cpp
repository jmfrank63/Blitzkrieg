#include "StdAfx.h"
#include "TrenchView.h"
#include "TrenchFrm.h"
#include "frames.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


CTrenchView::CTrenchView()
{
}

CTrenchView::~CTrenchView()
{
}


BEGIN_MESSAGE_MAP(CTrenchView, CWnd)
ON_WM_PAINT()
END_MESSAGE_MAP()



BOOL CTrenchView::PreCreateWindow(CREATESTRUCT& cs) 
{
	if (!CWnd::PreCreateWindow(cs))
		return FALSE;
	
	cs.lpszClass = AfxRegisterWndClass(CS_HREDRAW|CS_VREDRAW|CS_DBLCLKS, 
		::LoadCursor(NULL, IDC_ARROW), HBRUSH(COLOR_WINDOW+1), NULL);
	
	return TRUE;
}


void CTrenchView::OnPaint() 
{
/*
	{
		RECT valRC;
		((CWnd *) g_frameManager.GetGameWnd())->GetWindowRect( &valRC );
		ScreenToClient( &valRC );
		ValidateRect( &valRC );
		
		CWnd::OnPaint();
		g_frameManager.GetMeshFrame()->GFXDraw();
	}
	else
	{
		CWnd::OnPaint();
		ValidateRect( 0 );
	}
*/

	CWnd::OnPaint();
	ValidateRect( 0 );
}
