#include "StdAfx.h"
#include "MineView.h"
#include "frames.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


CMineView::CMineView()
{
}

CMineView::~CMineView()
{
}


BEGIN_MESSAGE_MAP(CMineView, CWnd)
ON_WM_PAINT()
END_MESSAGE_MAP()



BOOL CMineView::PreCreateWindow(CREATESTRUCT& cs) 
{
	if (!CWnd::PreCreateWindow(cs))
		return FALSE;
	
	cs.lpszClass = AfxRegisterWndClass(CS_HREDRAW|CS_VREDRAW|CS_DBLCLKS, 
		::LoadCursor(NULL, IDC_ARROW), HBRUSH(COLOR_WINDOW+1), NULL);
	
	return TRUE;
}


void CMineView::OnPaint() 
{
	CWnd::OnPaint();
}
