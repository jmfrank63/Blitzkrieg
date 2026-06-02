#include "stdafx.h"
#include "TemplateTreeDock.h"
#include "resource.h"


CTemplateTreeDockBar::CTemplateTreeDockBar()
{
	pTemplateTree = new CTemplateTreeCtrl;
}

CTemplateTreeDockBar::~CTemplateTreeDockBar()
{
	delete pTemplateTree;
}


BEGIN_MESSAGE_MAP(CTemplateTreeDockBar, SECControlBar)
	ON_WM_SIZE()
	ON_WM_CREATE()
END_MESSAGE_MAP()



int CTemplateTreeDockBar::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (SECControlBar::OnCreate(lpCreateStruct) == -1)
		return -1;

  pTemplateTree->Create( 0, "Template Tree", WS_CHILD | WS_VISIBLE, CRect(0,0,0,0), this, 100 );
	return 0;
}

void CTemplateTreeDockBar::OnSize(UINT nType, int cx, int cy) 
{
	SECControlBar::OnSize(nType, cx, cy);

  CRect rectInside;
	GetInsideRect(rectInside);
	pTemplateTree->SetWindowPos( NULL, rectInside.left, rectInside.top,
		rectInside.Width(), rectInside.Height(), SWP_NOZORDER|SWP_NOACTIVATE );
}

BOOL CTemplateTreeDockBar::PreTranslateMessage(MSG* pMsg) 
{
/*
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
*/
	return SECControlBar::PreTranslateMessage( pMsg );
}

void CTemplateTreeDockBar::SaveTemplateTree( IDataTree *pDT )
{
	pTemplateTree->SaveTemplateTree( pDT );
}

void CTemplateTreeDockBar::LoadTemplateTree( IDataTree *pDT )
{
	pTemplateTree->LoadTemplateTree( pDT );
}
