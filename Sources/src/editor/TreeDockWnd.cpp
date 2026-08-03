
#include "StdAfx.h"
#include "PropView.h"
#include "TreeDockWnd.h"
#include "KeyFrameDock.h"
#include "TreeItem.h"
#include "resource.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif



CTreeDockWnd::CTreeDockWnd()
{
	pPropView = 0;
	pKeyFrameDockWnd = 0;
	pTree = 0;
}

CTreeDockWnd::~CTreeDockWnd()
{
	if ( pTree )
		delete pTree;
}


BEGIN_MESSAGE_MAP(CTreeDockWnd, SECControlBar)
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_WM_RBUTTONDOWN()
	ON_WM_PAINT()
	ON_WM_SETFOCUS()
END_MESSAGE_MAP()



int CTreeDockWnd::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (SECControlBar::OnCreate(lpCreateStruct) == -1)
		return -1;

	return 0;
}

CETreeCtrl* CTreeDockWnd::AddTree( const char *szName, int nID, bool bViz )
{
	pTree = new CETreeCtrl();
	pTree->Create( 0, szName, WS_CHILD | WS_VISIBLE | TVS_SHOWSELALWAYS, CRect(0, 0, 0, 0), this, nID );

	CRect rectInside;
	GetInsideRect(rectInside);
	pTree->SetWindowPos( &CWnd::wndTop, rectInside.left, rectInside.top,
		rectInside.Width(), rectInside.Height(),
		SWP_NOACTIVATE|SWP_SHOWWINDOW);

	return pTree;			//Элементы дерева будут заполняться извне
}

CETreeCtrl* CTreeDockWnd::GetTreeWithIndex( int nIndex )
{
	return pTree;
}

void CTreeDockWnd::DeleteTree( int nIndex )
{
	if ( pTree )
	{
		delete pTree;
		pTree = 0;
	}
	Invalidate();
}

void CTreeDockWnd::OnSize(UINT nType, int cx, int cy) 
{
	SECControlBar::OnSize(nType, cx, cy);

	if ( !pTree )
		return;

  CRect rectInside;
	GetInsideRect(rectInside);
	pTree->SetWindowPos( &CWnd::wndTop, rectInside.left, rectInside.top,
		rectInside.Width(), rectInside.Height(),
		SWP_NOACTIVATE|SWP_SHOWWINDOW);
}

BOOL CTreeDockWnd::PreTranslateMessage(MSG* pMsg) 
{
	CTreeItem *pItem = 0;
	switch ( pMsg->message )
	{
	case WM_USERTREESEL:
		pItem = (CTreeItem *) pMsg->lParam;
		NI_ASSERT ( pItem != 0 );
		NI_ASSERT( pPropView != 0 );
		pItem->MyLButtonClick();
		pPropView->SetItemProperty( pItem->GetItemName(), pItem );
		
		if ( pKeyFrameDockWnd )
		{
			CKeyFrameTreeItem *pKeyFrameTreeItem = dynamic_cast<CKeyFrameTreeItem *> ( pItem );
			if ( pKeyFrameTreeItem )
				pKeyFrameDockWnd->SetActiveKeyFrameTreeItem( pKeyFrameTreeItem );
			else
				pKeyFrameDockWnd->ClearControl();
		}

		return true;

	case WM_USERKEYDOWN:
		pItem = (CTreeItem *) pMsg->lParam;
		NI_ASSERT ( pItem != 0 );
		pItem->MyKeyDown( pMsg->wParam );
		return true;

	case WM_USERRBUTTONCLICK:
		pItem = (CTreeItem *) pMsg->lParam;
		NI_ASSERT ( pItem != 0 );
		pItem->MyRButtonClick();
		return true;
	}
	return SECControlBar::PreTranslateMessage( pMsg );
}

void CTreeDockWnd::OnRButtonDown(UINT nFlags, CPoint point) 
{
/*
  CMenu menu;
  
  if ( !menu.LoadMenu( IDR_TREEWND_MENU ) )
    return;
  CMenu *pPopup = menu.GetSubMenu( 0 );
  if ( !pPopup )
    return;
  ClientToScreen( &point );
  pPopup->TrackPopupMenu( TPM_LEFTBUTTON, point.x, point.y, this );
*/
  SECControlBar::OnRButtonDown(nFlags, point);
}

void CTreeDockWnd::OnPaint() 
{
  if ( pTree )
  {

    SECControlBar::OnPaint();
    return;
  }
	CPaintDC dc(this); // device context for painting
  CRect r;
  GetInsideRect( r );
  dc.FillSolidRect( r, GetSysColor( COLOR_MENU ) );
}

void CTreeDockWnd::SaveTrees( IStructureSaver *pSS )
{
	if ( pTree )
		pTree->SaveTree( pSS );
}

void CTreeDockWnd::LoadTrees( IStructureSaver *pSS )
{
	pPropView->ClearControl();

	if ( pTree )
		pTree->LoadTree( pSS );
	Invalidate();
}

void CTreeDockWnd::SaveTrees( IDataTree *pDT )
{
	if ( pTree )
		pTree->SaveTree( pDT );
}

void CTreeDockWnd::LoadTrees( IDataTree *pDT )
{
	if ( pPropView )
		pPropView->ClearControl();

	if ( pTree )
		pTree->LoadTree( pDT );
	Invalidate();
}

void CTreeDockWnd::OnSetFocus(CWnd* pOldWnd) 
{
	SECControlBar::OnSetFocus(pOldWnd);
	
	pTree->SetFocus();
}
