
#include "StdAfx.h"
#include "SquadView.h"
#include "frames.h"
#include "SquadFrm.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


CSquadView::CSquadView()
{
}

CSquadView::~CSquadView()
{
}


BEGIN_MESSAGE_MAP(CSquadView, CWnd)
ON_WM_PAINT()
END_MESSAGE_MAP()



BOOL CSquadView::PreCreateWindow(CREATESTRUCT& cs) 
{
	if (!CWnd::PreCreateWindow(cs))
		return FALSE;
	
	cs.lpszClass = AfxRegisterWndClass(CS_HREDRAW|CS_VREDRAW|CS_DBLCLKS, 
		::LoadCursor(NULL, IDC_ARROW), HBRUSH(COLOR_WINDOW+1), NULL);
	
	return TRUE;
}


void CSquadView::OnPaint() 
{
	{
		RECT valRC;
		((CWnd *) g_frameManager.GetGameWnd())->GetWindowRect( &valRC );
		ScreenToClient( &valRC );
		ValidateRect( &valRC );
		
		CWnd::OnPaint();
		g_frameManager.GetFrame( CFrameManager::E_SQUAD_FRAME )->GFXDraw();
	}
}
