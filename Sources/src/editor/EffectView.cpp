
#include "StdAfx.h"
#include "EffectView.h"
#include "EffectFrm.h"
#include "frames.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


CEffectView::CEffectView()
{
}

CEffectView::~CEffectView()
{
}


BEGIN_MESSAGE_MAP(CEffectView, CWnd)
ON_WM_PAINT()
END_MESSAGE_MAP()



BOOL CEffectView::PreCreateWindow(CREATESTRUCT& cs) 
{
	if (!CWnd::PreCreateWindow(cs))
		return FALSE;
	
	cs.lpszClass = AfxRegisterWndClass(CS_HREDRAW|CS_VREDRAW|CS_DBLCLKS, 
		::LoadCursor(NULL, IDC_ARROW), HBRUSH(COLOR_WINDOW+1), NULL);
	
	return TRUE;
}


void CEffectView::OnPaint() 
{
	CEffectFrame *pFrame = static_cast<CEffectFrame *> ( g_frameManager.GetFrame( CFrameManager::E_EFFECT_FRAME ) );
	if ( pFrame->IsRunning() )
	{
		RECT valRC;
		((CWnd *) g_frameManager.GetGameWnd())->GetWindowRect( &valRC );
		ScreenToClient( &valRC );
		ValidateRect( &valRC );
		
		CWnd::OnPaint();
		pFrame->GFXDraw();
	}
	else
	{
/*
		ValidateRect( 0 );
		CWnd::OnPaint();
		g_frameManager.GetFrame( CFrameManager::E_EFFECT_FRAME )->UpdateChildWindows();
*/
		CWnd::OnPaint();
		ValidateRect( 0 );
	}
}

BOOL CEffectView::PreTranslateMessage( MSG* pMsg )
{
/*
	switch ( pMsg->message )
	{
		case WM_THUMB_LIST_DBLCLK:
		case WM_THUMB_LIST_DELETE:
			GetParent()->PostMessage( pMsg->message, pMsg->wParam );
			return true;
	}
*/

	return CWnd::PreTranslateMessage(pMsg);
}
