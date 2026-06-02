
#include "stdafx.h"
#include "AnimationView.h"
#include "AnimationFrm.h"
#include "GameWnd.h"
#include "frames.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


CAnimationView::CAnimationView()
{
}

CAnimationView::~CAnimationView()
{
}


BEGIN_MESSAGE_MAP(CAnimationView, CWnd)
	ON_WM_PAINT()
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_WM_HSCROLL()
END_MESSAGE_MAP()



BOOL CAnimationView::PreCreateWindow(CREATESTRUCT& cs) 
{
	if (!CWnd::PreCreateWindow(cs))
		return FALSE;
/*
	cs.dwExStyle |= WS_EX_CLIENTEDGE;
	cs.style &= ~WS_BORDER;
*/
	cs.lpszClass = AfxRegisterWndClass(CS_HREDRAW|CS_VREDRAW|CS_DBLCLKS, 
		::LoadCursor(NULL, IDC_ARROW), HBRUSH(COLOR_WINDOW+1), NULL);
/*
	cs.lpszClass = AfxRegisterWndClass(CS_HREDRAW|CS_VREDRAW|CS_DBLCLKS, 
		0, HBRUSH(COLOR_WINDOW+1), NULL);
*/
	return TRUE;
}

int CAnimationView::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CWnd::OnCreate(lpCreateStruct) == -1)
		return -1;

	return 0;
}

void CAnimationView::OnPaint() 
{
	CAnimationFrame *pFrame = static_cast<CAnimationFrame *> ( g_frameManager.GetFrame( CFrameManager::E_ANIMATION_FRAME ) );
	if ( pFrame->IsRunning() )
	{
		RECT valRC;
		g_frameManager.GetGameWnd()->GetWindowRect( &valRC );
		ScreenToClient( &valRC );
		ValidateRect( &valRC );
		
		CWnd::OnPaint();
		pFrame->GFXDraw();
	}
	else
	{
		ValidateRect( 0 );
		CWnd::OnPaint();
		pFrame->UpdateThumbWindows();
	}
}

void CAnimationView::OnSize(UINT nType, int cx, int cy) 
{
	CWnd::OnSize(nType, cx, cy);
	CAnimationFrame *pFrame = static_cast<CAnimationFrame *> ( g_frameManager.GetFrame( CFrameManager::E_ANIMATION_FRAME ) );
	pFrame->ViewSizeChanged();
}

BOOL CAnimationView::PreTranslateMessage( MSG* pMsg )
{
	switch ( pMsg->message )
	{
		case WM_THUMB_LIST_DBLCLK:
		case WM_THUMB_LIST_DELETE:
			GetParent()->PostMessage( pMsg->message, pMsg->wParam );
			return true;
	}

	return CWnd::PreTranslateMessage(pMsg);
}

LRESULT CAnimationView::WindowProc(UINT message, WPARAM wParam, LPARAM lParam) 
{
	if ( message == WM_THUMB_LIST_SELECT )
	{
		GetParent()->PostMessage( message, wParam );
		return true;
	}

	return CWnd::WindowProc(message, wParam, lParam);
}

void CAnimationView::OnHScroll( UINT nSBCode, UINT nPos, CScrollBar* pScrollBar )
{
  SCROLLINFO info;
  if ( !pScrollBar->GetScrollInfo( &info ) )
	{
    NI_ASSERT( 0 );
		return;
	}
	int nNewPos = info.nPos;

	switch (nSBCode)
	{
	case SB_LEFT:      // Scroll to far left.
		nNewPos = info.nMin;
		break;
		
	case SB_RIGHT:      // Scroll to far right.
		nNewPos = info.nMax;
		break;
		
	case SB_ENDSCROLL:   // End scroll.
		break;
		
	case SB_LINELEFT:      // Scroll left.
		if (nNewPos > info.nMin)
			nNewPos--;
		break;
		
	case SB_LINERIGHT:   // Scroll right.
		if (nNewPos < info.nMax)
			nNewPos++;
		break;
		
	case SB_PAGELEFT:    // Scroll one page left.
		if ( info.nMin > info.nPos - info.nPage )
			nNewPos = info.nMin;
		else
			nNewPos = info.nPos - info.nPage;
		break;
		
	case SB_PAGERIGHT:      // Scroll one page right.
		if ( info.nMax < info.nPos + info.nPage )
			nNewPos = info.nMax;
		else
			nNewPos = info.nPos + info.nPage;
		break;

	case SB_THUMBPOSITION: // Scroll to absolute position. nPos is the position
		nNewPos = nPos;      // of the scroll box at the end of the drag operation.
		break;
		
	case SB_THUMBTRACK:   // Drag scroll box to specified position. nPos is the
		nNewPos = nPos;     // position that the scroll box has been dragged to.
		break;
	}
	pScrollBar->SetScrollPos( nNewPos );
	
	
	CAnimationFrame *pFrame = static_cast<CAnimationFrame *> ( g_frameManager.GetFrame( CFrameManager::E_ANIMATION_FRAME ) );
	pFrame->UpdateUnitsCoordinates();

}
