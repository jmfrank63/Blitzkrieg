#include "StdAfx.h"
#include "MedalView.h"
#include "frames.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


CMedalView::CMedalView()
{
}

CMedalView::~CMedalView()
{
}


BEGIN_MESSAGE_MAP(CMedalView, CWnd)
ON_WM_PAINT()
END_MESSAGE_MAP()



BOOL CMedalView::PreCreateWindow(CREATESTRUCT& cs) 
{
	if (!CWnd::PreCreateWindow(cs))
		return FALSE;
	
	cs.lpszClass = AfxRegisterWndClass(CS_HREDRAW|CS_VREDRAW|CS_DBLCLKS, 
		::LoadCursor(NULL, IDC_ARROW), HBRUSH(COLOR_WINDOW+1), NULL);
	
	return TRUE;
}


void CMedalView::OnPaint() 
{
	CWnd::OnPaint();
}
