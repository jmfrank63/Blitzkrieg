
#include "StdAfx.h"
#include "editor.h"
#include "GameWnd.h"
#include "MainFrm.h"
#include "frames.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


CGameWnd::CGameWnd()
{
}

CGameWnd::~CGameWnd()
{
}

BEGIN_MESSAGE_MAP(CGameWnd, CWnd)
	ON_WM_MOUSEMOVE()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_LBUTTONDBLCLK()
	ON_WM_RBUTTONDOWN()
	ON_WM_RBUTTONUP()
	ON_WM_RBUTTONDBLCLK()
	ON_WM_MBUTTONDOWN()
	ON_WM_MBUTTONUP()
	ON_WM_MBUTTONDBLCLK()
	ON_WM_PAINT()
	ON_WM_CREATE()
END_MESSAGE_MAP()



void CGameWnd::OnMouseMove(UINT nFlags, CPoint point) 
{
	::SendMessage( g_frameManager.GetActiveWnd()->GetSafeHwnd(), WM_MOUSEMOVE, nFlags, MAKELPARAM(point.x, point.y) );
}

void CGameWnd::OnLButtonDown(UINT nFlags, CPoint point) 
{
	::SendMessage( g_frameManager.GetActiveWnd()->GetSafeHwnd(), WM_LBUTTONDOWN, nFlags, MAKELPARAM(point.x, point.y) );
}

void CGameWnd::OnLButtonUp(UINT nFlags, CPoint point) 
{
	::SendMessage( g_frameManager.GetActiveWnd()->GetSafeHwnd(), WM_LBUTTONUP, nFlags, MAKELPARAM(point.x, point.y) );
}

void CGameWnd::OnLButtonDblClk(UINT nFlags, CPoint point) 
{
	::SendMessage( g_frameManager.GetActiveWnd()->GetSafeHwnd(), WM_LBUTTONDBLCLK, nFlags, MAKELPARAM(point.x, point.y) );
}

void CGameWnd::OnRButtonDown(UINT nFlags, CPoint point) 
{
	::SendMessage( g_frameManager.GetActiveWnd()->GetSafeHwnd(), WM_RBUTTONDOWN, nFlags, MAKELPARAM(point.x, point.y) );
}

void CGameWnd::OnRButtonUp(UINT nFlags, CPoint point) 
{
	::SendMessage( g_frameManager.GetActiveWnd()->GetSafeHwnd(), WM_RBUTTONUP, nFlags, MAKELPARAM(point.x, point.y) );
}

void CGameWnd::OnRButtonDblClk(UINT nFlags, CPoint point) 
{
	::SendMessage( g_frameManager.GetActiveWnd()->GetSafeHwnd(), WM_RBUTTONDBLCLK, nFlags, MAKELPARAM(point.x, point.y) );
}

void CGameWnd::OnMButtonDown(UINT nFlags, CPoint point) 
{
	::SendMessage( g_frameManager.GetActiveWnd()->GetSafeHwnd(), WM_MBUTTONDOWN, nFlags, MAKELPARAM(point.x, point.y) );
}

void CGameWnd::OnMButtonUp(UINT nFlags, CPoint point) 
{
	::SendMessage( g_frameManager.GetActiveWnd()->GetSafeHwnd(), WM_MBUTTONUP, nFlags, MAKELPARAM(point.x, point.y) );
}

void CGameWnd::OnMButtonDblClk(UINT nFlags, CPoint point) 
{
	::SendMessage( g_frameManager.GetActiveWnd()->GetSafeHwnd(), WM_MBUTTONDBLCLK, nFlags, MAKELPARAM(point.x, point.y) );
}

void CGameWnd::OnPaint() 
{

	ValidateRect( 0 );
}

BOOL CGameWnd::PreCreateWindow(CREATESTRUCT& cs) 
{
	if (!CWnd::PreCreateWindow(cs))
		return FALSE;
/*
	cs.dwExStyle |= WS_EX_CLIENTEDGE;
	cs.style &= ~WS_BORDER;
*/
/*
	cs.lpszClass = AfxRegisterWndClass(CS_HREDRAW|CS_VREDRAW|CS_DBLCLKS, 
		::LoadCursor(NULL, IDC_ARROW), HBRUSH(COLOR_WINDOW+1), NULL);
*/
	cs.lpszClass = AfxRegisterWndClass(CS_HREDRAW|CS_VREDRAW|CS_DBLCLKS, 
		0, HBRUSH(COLOR_WINDOW+1), NULL);

	return TRUE;
}

int CGameWnd::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CWnd::OnCreate(lpCreateStruct) == -1)
		return -1;
	
	m_mainFramePtr = static_cast<CMainFrame *>( GetParent() ); // first time this is MainFrame
	
 	return 0;
}