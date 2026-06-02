
#include "StdAfx.h"
#include "SpriteView.h"
#include "SpriteFrm.h"
#include "frames.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


CSpriteView::CSpriteView()
{
}

CSpriteView::~CSpriteView()
{
}


BEGIN_MESSAGE_MAP(CSpriteView, CWnd)
	ON_WM_PAINT()
	ON_WM_SIZE()
END_MESSAGE_MAP()



BOOL CSpriteView::PreCreateWindow(CREATESTRUCT& cs) 
{
	if (!CWnd::PreCreateWindow(cs))
		return FALSE;

	cs.lpszClass = AfxRegisterWndClass(CS_HREDRAW|CS_VREDRAW|CS_DBLCLKS, 
		::LoadCursor(NULL, IDC_ARROW), HBRUSH(COLOR_WINDOW+1), NULL);

	return TRUE;
}


void CSpriteView::OnPaint() 
{
	CSpriteFrame *pFrame = static_cast<CSpriteFrame *> ( g_frameManager.GetFrame( CFrameManager::E_SPRITE_FRAME ) );
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
		ValidateRect( 0 );
		CWnd::OnPaint();
		pFrame->UpdateThumbWindows();
	}
}

void CSpriteView::OnSize(UINT nType, int cx, int cy) 
{
	CWnd::OnSize(nType, cx, cy);
	CSpriteFrame *pFrame = static_cast<CSpriteFrame *> ( g_frameManager.GetFrame( CFrameManager::E_SPRITE_FRAME ) );
	pFrame->ViewSizeChanged();
}

BOOL CSpriteView::PreTranslateMessage( MSG* pMsg )
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

LRESULT CSpriteView::WindowProc(UINT message, WPARAM wParam, LPARAM lParam) 
{
	if ( message == WM_THUMB_LIST_SELECT )
	{
		GetParent()->PostMessage( message, wParam );
		return true;
	}

	return CWnd::WindowProc(message, wParam, lParam);
}
