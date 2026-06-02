
#include "StdAfx.h"
#include "BuildView.h"
#include "BuildFrm.h"
#include "frames.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


CBuildingView::CBuildingView()
{
}

CBuildingView::~CBuildingView()
{
}


BEGIN_MESSAGE_MAP(CBuildingView, CWnd)
ON_WM_PAINT()
END_MESSAGE_MAP()



BOOL CBuildingView::PreCreateWindow(CREATESTRUCT& cs) 
{
	if (!CWnd::PreCreateWindow(cs))
		return FALSE;
	
	cs.lpszClass = AfxRegisterWndClass(CS_HREDRAW|CS_VREDRAW|CS_DBLCLKS, 
		::LoadCursor(NULL, IDC_ARROW), HBRUSH(COLOR_WINDOW+1), NULL);
	
	return TRUE;
}


void CBuildingView::OnPaint() 
{

	{
		RECT valRC;
		((CWnd *) g_frameManager.GetGameWnd())->GetWindowRect( &valRC );
		ScreenToClient( &valRC );
		ValidateRect( &valRC );
		
		CWnd::OnPaint();
		g_frameManager.GetFrame( CFrameManager::E_BUILDING_FRAME )->GFXDraw();
	}
/*	else
	{
		CWnd::OnPaint();
		ValidateRect( 0 );
	}
*/
}
