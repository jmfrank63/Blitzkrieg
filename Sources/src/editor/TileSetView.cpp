#include "StdAfx.h"
#include "TileSetView.h"
#include "TileSetFrm.h"
#include "frames.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


CTileSetView::CTileSetView()
{
}

CTileSetView::~CTileSetView()
{
}


BEGIN_MESSAGE_MAP(CTileSetView, CWnd)
ON_WM_PAINT()
ON_WM_SIZE()
END_MESSAGE_MAP()



BOOL CTileSetView::PreCreateWindow(CREATESTRUCT& cs) 
{
	if (!CWnd::PreCreateWindow(cs))
		return FALSE;
	
	cs.lpszClass = AfxRegisterWndClass(CS_HREDRAW|CS_VREDRAW|CS_DBLCLKS, 
		::LoadCursor(NULL, IDC_ARROW), HBRUSH(COLOR_WINDOW+1), NULL);
	
	return TRUE;
}


void CTileSetView::OnPaint() 
{
/*
	if ( g_frameManager.GetFrame( CFrameManager::E_TILESET_FRAME )->IsRunning() )
	{
		RECT valRC;
		((CWnd *) g_frameManager.GetGameWnd())->GetWindowRect( &valRC );
		ScreenToClient( &valRC );
		ValidateRect( &valRC );
		
		CWnd::OnPaint();
		g_frameManager.GetFrame( CFrameManager::E_TILESET_FRAME )->GFXDraw();
	}
	else
*/
	{
		ValidateRect( 0 );
		CWnd::OnPaint();
		CTileSetFrame *pFrame = static_cast<CTileSetFrame *> ( g_frameManager.GetFrame( CFrameManager::E_TILESET_FRAME ) );
		pFrame->UpdateThumbWindows();
	}
}

void CTileSetView::OnSize(UINT nType, int cx, int cy) 
{
	CWnd::OnSize(nType, cx, cy);
	CTileSetFrame *pFrame = static_cast<CTileSetFrame *> ( g_frameManager.GetFrame( CFrameManager::E_TILESET_FRAME ) );
	pFrame->ViewSizeChanged();
}

BOOL CTileSetView::PreTranslateMessage( MSG* pMsg )
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

LRESULT CTileSetView::WindowProc(UINT message, WPARAM wParam, LPARAM lParam) 
{
	if ( message == WM_THUMB_LIST_SELECT )
	{
		GetParent()->PostMessage( message, wParam );
		return true;
	}
	
	return CWnd::WindowProc(message, wParam, lParam);
}
