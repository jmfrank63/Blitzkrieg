#include "StdAfx.h"
#include "CampaignView.h"
#include "CampaignFrm.h"
#include "frames.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


CCampaignView::CCampaignView()
{
}

CCampaignView::~CCampaignView()
{
}


BEGIN_MESSAGE_MAP(CCampaignView, CWnd)
ON_WM_PAINT()
ON_WM_HSCROLL()
ON_WM_VSCROLL()
END_MESSAGE_MAP()



BOOL CCampaignView::PreCreateWindow(CREATESTRUCT& cs) 
{
	if (!CWnd::PreCreateWindow(cs))
		return FALSE;
	
	cs.lpszClass = AfxRegisterWndClass(CS_HREDRAW|CS_VREDRAW|CS_DBLCLKS, 
		::LoadCursor(NULL, IDC_ARROW), HBRUSH(COLOR_WINDOW+1), NULL);
	
	return TRUE;
}

void CCampaignView::OnPaint() 
{
	RECT valRC;
	((CWnd *) g_frameManager.GetGameWnd())->GetWindowRect( &valRC );
	ScreenToClient( &valRC );
	ValidateRect( &valRC );
	
	CWnd::OnPaint();
	g_frameManager.GetFrame( CFrameManager::E_CAMPAIGN_FRAME )->GFXDraw();
}

void CCampaignView::OnHScroll( UINT nSBCode, UINT nPos, CScrollBar* pScrollBar )
{
	UpdateScrolls( nSBCode, nPos, pScrollBar );
}

void CCampaignView::OnVScroll( UINT nSBCode, UINT nPos, CScrollBar* pScrollBar )
{
	UpdateScrolls( nSBCode, nPos, pScrollBar );
}

void CCampaignView::UpdateScrolls( UINT nSBCode, UINT nPos, CScrollBar* pScrollBar )
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
	
	
	CCampaignFrame *pFrame = static_cast<CCampaignFrame *> ( g_frameManager.GetFrame( CFrameManager::E_CAMPAIGN_FRAME ) );
	pFrame->UpdateImageCoordinates();
}
