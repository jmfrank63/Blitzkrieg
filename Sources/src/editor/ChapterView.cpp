#include "StdAfx.h"
#include "ChapterView.h"
#include "frames.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


CChapterView::CChapterView()
{
}

CChapterView::~CChapterView()
{
}


BEGIN_MESSAGE_MAP(CChapterView, CWnd)
ON_WM_PAINT()
END_MESSAGE_MAP()



BOOL CChapterView::PreCreateWindow(CREATESTRUCT& cs) 
{
	if (!CWnd::PreCreateWindow(cs))
		return FALSE;
	
	cs.lpszClass = AfxRegisterWndClass(CS_HREDRAW|CS_VREDRAW|CS_DBLCLKS, 
		::LoadCursor(NULL, IDC_ARROW), HBRUSH(COLOR_WINDOW+1), NULL);
	
	return TRUE;
}


void CChapterView::OnPaint() 
{
	CWnd::OnPaint();
}
