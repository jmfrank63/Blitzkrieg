#include "StdAfx.h"
#include "MissionView.h"
#include "frames.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


CMissionView::CMissionView()
{
}

CMissionView::~CMissionView()
{
}


BEGIN_MESSAGE_MAP(CMissionView, CWnd)
ON_WM_PAINT()
END_MESSAGE_MAP()



BOOL CMissionView::PreCreateWindow(CREATESTRUCT& cs) 
{
	if (!CWnd::PreCreateWindow(cs))
		return FALSE;
	
	cs.lpszClass = AfxRegisterWndClass(CS_HREDRAW|CS_VREDRAW|CS_DBLCLKS, 
		::LoadCursor(NULL, IDC_ARROW), HBRUSH(COLOR_WINDOW+1), NULL);
	
	return TRUE;
}


void CMissionView::OnPaint() 
{
	CWnd::OnPaint();
}
