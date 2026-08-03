
#include "StdAfx.h"
#include "ThumbListDockBar.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


CThumbListDockBar::CThumbListDockBar()
{
}

CThumbListDockBar::~CThumbListDockBar()
{
}


BEGIN_MESSAGE_MAP(CThumbListDockBar, SECControlBar)
ON_WM_CREATE()
ON_WM_SIZE()
END_MESSAGE_MAP()



int CThumbListDockBar::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (SECControlBar::OnCreate(lpCreateStruct) == -1)
		return -1;

	DWORD dwStyle = LVS_SHOWSELALWAYS | LVS_SINGLESEL | LVS_ALIGNLEFT | LVS_ICON |
		WS_CHILD | WS_VISIBLE | WS_BORDER;
	m_wndThumbList.Create( 0, "Thumbnail List", dwStyle,
		CRect(0, 0, 0, 0), this, 1005 );
	return 0;
}

void CThumbListDockBar::OnSize(UINT nType, int cx, int cy) 
{
	SECControlBar::OnSize(nType, cx, cy);

	if( m_wndThumbList.GetSafeHwnd() != NULL )
	{
		CRect r;
		GetInsideRect(r);
		m_wndThumbList.SetWindowPos( &CWnd::wndTop, r.left, r.top, r.Width(), r.Height(), SWP_NOACTIVATE|SWP_SHOWWINDOW );
	}
}
