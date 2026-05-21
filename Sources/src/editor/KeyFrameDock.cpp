#include "stdafx.h"
#include "resource.h"
#include "KeyFrameDock.h"
#include "KeyFrame.h"
#include "ParticleFrm.h"
#include "frames.h"

/////////////////////////////////////////////////////////////////////////////
// CKeyFrameDockWnd

CKeyFrameDockWnd::CKeyFrameDockWnd()
{
	m_pKeyFramer = new CKeyFrameEditor();
}

CKeyFrameDockWnd::~CKeyFrameDockWnd()
{
	if ( m_pKeyFramer )
	{
		delete m_pKeyFramer;
		m_pKeyFramer = 0;
	}
}


BEGIN_MESSAGE_MAP(CKeyFrameDockWnd, SECControlBar)
	//{{AFX_MSG_MAP(CKeyFrameDockWnd)
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_COMMAND(ID_KEYFRAME_DELETE_NODE, OnKeyframeDeleteNode)
	ON_COMMAND(ID_KEYFRAME_RESET_ALL, OnKeyframeResetAll)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()


/////////////////////////////////////////////////////////////////////////////
// CKeyFrameDockWnd message handlers

int CKeyFrameDockWnd::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (SECControlBar::OnCreate(lpCreateStruct) == -1)
		return -1;

  m_pKeyFramer->Create( 0, "Key Framer", WS_CHILD | WS_VISIBLE, CRect(0,0,0,0), this, 100 );
	return 0;
}

void CKeyFrameDockWnd::ClearControl()
{
	m_pKeyFramer->ClearAll();
}

void CKeyFrameDockWnd::SetDimentions( float fMinX, float fMaxX, float fStepX, float fMinY, float fMaxY, float fStepY )
{
	m_pKeyFramer->SetDimentions( fMinX, fMaxX, fStepX, fMinY, fMaxY, fStepY );
}

void CKeyFrameDockWnd::SetFramesList( CFramesList frames )
{
	m_pKeyFramer->SetFramesList( frames );
}

void CKeyFrameDockWnd::ResetNodes()
{
	m_pKeyFramer->ResetNodes();
}

void CKeyFrameDockWnd::SetXResizeMode( bool bResizeMode )
{
	m_pKeyFramer->SetXResizeMode( bResizeMode );
}

void CKeyFrameDockWnd::OnSize(UINT nType, int cx, int cy) 
{
	SECControlBar::OnSize(nType, cx, cy);

  CRect rectInside;
	GetInsideRect(rectInside);
	::SetWindowPos( m_pKeyFramer->GetSafeHwnd(), NULL, rectInside.left, rectInside.top,
		rectInside.Width(), rectInside.Height(),
		SWP_NOZORDER|SWP_NOACTIVATE );
}

BOOL CKeyFrameDockWnd::PreTranslateMessage(MSG* pMsg) 
{
	switch ( pMsg->message )
	{
	case WM_KEY_FRAME_RCLICK:
		{
			//���������� ����
			CMenu menu;
			menu.LoadMenu( IDR_KEYFRAME_ZOOM_MENU );
			CMenu *popupMenu = menu.GetSubMenu( 0 );
			popupMenu->TrackPopupMenu( TPM_LEFTALIGN | TPM_LEFTBUTTON, pMsg->wParam, pMsg->lParam, this );
			return true;
		}
	
	case WM_KEY_FRAME_UPDATE:
		if ( pActiveKeyItem )
		{
			//��������� ������ ��������
			pActiveKeyItem->SetFramesList( m_pKeyFramer->GetFramesList() );
			//������ �������� ������ � ParticleFrame
			g_frameManager.GetFrame( CFrameManager::E_PARTICLE_FRAME )->SetChangedFlag( true );
		}
		return true;

/*
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
*/
	}

/*
	switch ( pMsg->message )
	{
		case WM_KEYDOWN:
			::PostMessage( m_pKeyFramer->GetSafeHwnd(), WM_KEYDOWN, pMsg->wParam, pMsg->lParam );
			return true;
	}
*/

	return SECControlBar::PreTranslateMessage( pMsg );
}

void CKeyFrameDockWnd::OnKeyframeDeleteNode() 
{
	m_pKeyFramer->DeleteActiveNode();
}

void CKeyFrameDockWnd::OnKeyframeResetAll() 
{
	ResetNodes();
}

void CKeyFrameDockWnd::SetActiveKeyFrameTreeItem( CKeyFrameTreeItem *pItem )
{
	NI_ASSERT( pItem != 0 );

	pActiveKeyItem = pItem;
	m_pKeyFramer->SetFramesList( pItem->framesList );
	m_pKeyFramer->SetXResizeMode( pItem->bResizeMode );
	m_pKeyFramer->SetDimentions( pItem->fMinValX, pItem->fMaxValX, pItem->fStepX, pItem->fMinValY, pItem->fMaxValY, pItem->fStepY );
}

