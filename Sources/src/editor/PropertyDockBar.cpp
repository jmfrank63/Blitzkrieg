#include "stdafx.h"
#include "resource.h"
#include "PropertyDockBar.h"


CPropertyDockBar::CPropertyDockBar()
{
}

CPropertyDockBar::~CPropertyDockBar()
{
}


BEGIN_MESSAGE_MAP(CPropertyDockBar, SECControlBar)
	ON_WM_CREATE()
	ON_WM_SIZE()
END_MESSAGE_MAP()



int CPropertyDockBar::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (SECControlBar::OnCreate(lpCreateStruct) == -1)
		return -1;

 	RECT r;
	GetClientRect( &r );
	r.left = r.left + 25;
	m_tree.Create( NULL, "MultiTreeCtrl", WS_CHILD | WS_VISIBLE | WS_BORDER, r, this, 0);

	HD_ITEM hdi;
	hdi.mask = HDI_TEXT | HDI_FORMAT;
	hdi.mask |= HDI_WIDTH;
	hdi.cxy = ( r.right - r.left ) * 0.3f;
	hdi.fmt = HDF_STRING | HDF_CENTER;
	hdi.pszText = (LPTSTR)"Tree";
	int m_nReturn = m_tree.m_tree.m_wndHeader.InsertItem( 0, &hdi);
	
	hdi.cxy = ( r.right - r.left ) * 2.8f;
	hdi.pszText = (LPTSTR)"Properties";
	m_nReturn = m_tree.m_tree.m_wndHeader.InsertItem( 1, &hdi);
	
	return 0;
}

void CPropertyDockBar::OnSize(UINT nType, int cx, int cy) 
{
	SECControlBar::OnSize(nType, cx, cy);

	if( m_tree.ifInit )
	{
		CRect rectInside;
		GetInsideRect(rectInside);
		m_tree.SetWindowPos( NULL, rectInside.left, rectInside.top,
			rectInside.Width(), rectInside.Height(),
			SWP_NOZORDER|SWP_NOACTIVATE );
			
			
		int nWidth = rectInside.right - rectInside.left;
		if ( nWidth < 0 )
			nWidth = -nWidth;

		HD_ITEM hdi;
		hdi.mask |= HDI_WIDTH;
		hdi.cxy = nWidth * 0.6f;
		m_tree.m_tree.m_wndHeader.SetItem( 0, &hdi);
		
		hdi.cxy = nWidth * 0.4f;
		m_tree.m_tree.m_wndHeader.SetItem( 1, &hdi);
	}
}

BOOL CPropertyDockBar::PreTranslateMessage(MSG* pMsg) 
{
/*
	switch ( pMsg->message )
	{
	case WM_KEY_FRAME_RCLICK:
		{
			CMenu menu;
			menu.LoadMenu( IDR_KEYFRAME_ZOOM_MENU );
			CMenu *popupMenu = menu.GetSubMenu( 0 );
			popupMenu->TrackPopupMenu( TPM_LEFTALIGN | TPM_LEFTBUTTON, pMsg->wParam, pMsg->lParam, this );
			return true;
		}
	
	case WM_KEY_FRAME_UPDATE:
		if ( pActiveKeyItem )
		{
			pActiveKeyItem->SetFramesList( m_pKeyFramer->GetFramesList() );
			g_frameManager.GetParticleFrame()->SetChangedFlag( true );
		}
		return true;
	}
*/
	
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

/*
void CPropertyDockBar::SetActiveKeyFrameTreeItem( CKeyFrameTreeItem *pItem )
{
	NI_ASSERT( pItem != 0 );

	pActiveKeyItem = pItem;
	m_pKeyFramer->SetFramesList( pItem->framesList );
	m_pKeyFramer->SetXResizeMode( pItem->bResizeMode );
	m_pKeyFramer->SetDimentions( pItem->fMinValX, pItem->fMaxValX, pItem->fStepX, pItem->fMinValY, pItem->fMaxValY, pItem->fStepY );
}
*/

void CPropertyDockBar::AddRootVariable( std::string &str, int variable)
{
	if( str == "Player" )
	{
		HTREEITEM hPA = m_tree.m_tree.InsertItemEx( str.c_str(), TVI_ROOT, TVI_LAST, numComboBoxItem );
		CNumComboBoxTreeItem *ptr = reinterpret_cast< CNumComboBoxTreeItem *>(m_tree.m_tree.GetTreeItemPtr( hPA ));
		if( ptr != 0 ) ptr->SetNumData( variable );
		m_varHandles.insert( std::make_pair( str, hPA ) );
		
	}
	else
	{
		HTREEITEM hPA = m_tree.m_tree.InsertItem(TVIF_TEXT, _T( str.c_str() ), 0, 0, 0, 0, 0, 0 , NULL);
		CSTreeItem* ptr = reinterpret_cast< CSTreeItem *>( m_tree.m_tree.GetTreeItemPtr( hPA ));
		if( ptr != 0 ) ptr->SetOleData( long( variable ) );
		m_varHandles.insert( std::make_pair( str, hPA ) );
		
	}
}
HTREEITEM CPropertyDockBar::AddEmptyNode( std::string &str, HTREEITEM hPARoot )
{
		HTREEITEM hPA = m_tree.m_tree.InsertItemEx( str.c_str(), hPARoot, TVI_LAST, emptyItem );
		CEmptyTreeItem *ptr = reinterpret_cast< CEmptyTreeItem *>(m_tree.m_tree.GetTreeItemPtr( hPA ));
		m_varHandles.insert( std::make_pair( str, hPA ) );
		return hPA;
}

int CPropertyDockBar::GetVariable( std::string &name)
{
	
	if( m_varHandles.find( name ) != m_varHandles.end() )
	{
		if( name == "Player" )
		{
			CNumComboBoxTreeItem* ptr = reinterpret_cast< CNumComboBoxTreeItem *>(m_tree.m_tree.GetTreeItemPtr( m_varHandles[name] ));	
			if( ptr != 0 ) 
			{
				return	ptr->GetNumData();
			}
		}
		else
		{
			CSTreeItem* ptr = reinterpret_cast< CSTreeItem *>(m_tree.m_tree.GetTreeItemPtr( m_varHandles[name] ));	
			if( ptr != 0 ) 
			{
				return	int(ptr->GetOleData().intVal);
			}
		}
	}
	return -1;
}

void CPropertyDockBar::ClearVariables()
{
	m_pCurrentObject = 0;
	m_tree.m_tree.DeleteAllItems();
	m_varHandles.clear();
}

void CPropertyDockBar::AddManipulatorVariable( std::string &str, IManipulator *ptr )
{
	std::vector<std::string> szVector;
	NStr::SplitString( str, szVector, '.' );
	HTREEITEM hPARoot = TVI_ROOT;
	std::string tmpStr;
	if( szVector.size() != 1 )
	{
		for( std::vector<std::string>::iterator it = szVector.begin(); it != szVector.end() - 1; ++it )
		{
			tmpStr += (*it);
			if( m_insertedNodes.find( tmpStr ) == m_insertedNodes.end() )
			{
				hPARoot = AddEmptyNode( *it, hPARoot);
				m_insertedNodes.insert( std::make_pair( tmpStr, hPARoot ) );
			}
			else
			{
				hPARoot = m_insertedNodes[ tmpStr ];
			}
			tmpStr += std::string(".");
		}
	}	
	AddPropertieNode( *(szVector.end() - 1), str, ptr, hPARoot);
}

HTREEITEM	CPropertyDockBar::AddPropertieNode( std::string &str, std::string &propName, IManipulator *pManipulator, HTREEITEM hPARoot )
{
	CPropertieTreeItem *ptr ;	
	CComboBoxTreeItemPropertieTreeItem *ptr2;
	CDirChosePropertieTreeItem	*ptr3;
	CFileChosePropertieTreeItem	*ptr4;
	
	
	HTREEITEM hPA;
	if ( pManipulator->GetPropertyDesc(propName.c_str())->ePropType != SPropertyDesc::VAL_COMBO
		&& pManipulator->GetPropertyDesc(propName.c_str())->ePropType != SPropertyDesc::VAL_BROWSEDIR 
		&& pManipulator->GetPropertyDesc(propName.c_str())->ePropType != SPropertyDesc::VAL_BROWSEFILE )
	{
		hPA = m_tree.m_tree.InsertItemEx( str.c_str(), hPARoot, TVI_LAST, propertieItem );
		ptr	= reinterpret_cast< CPropertieTreeItem *>(m_tree.m_tree.GetTreeItemPtr( hPA ));
		if( ptr != 0 ) ptr->Setup( propName, pManipulator );
	}
	if ( pManipulator->GetPropertyDesc(propName.c_str())->ePropType == SPropertyDesc::VAL_COMBO )
	{
		hPA = m_tree.m_tree.InsertItemEx( str.c_str(), hPARoot, TVI_LAST, propertieItemCombo );
		ptr2 = reinterpret_cast< CComboBoxTreeItemPropertieTreeItem *>(m_tree.m_tree.GetTreeItemPtr( hPA ));
		if( ptr2 != 0 ) ptr2->Setup( propName, pManipulator );
	}
	if ( pManipulator->GetPropertyDesc(propName.c_str())->ePropType == SPropertyDesc::VAL_BROWSEDIR )
	{
		hPA = m_tree.m_tree.InsertItemEx( str.c_str(), hPARoot, TVI_LAST, propertieItemDir );
		ptr3 = reinterpret_cast< CDirChosePropertieTreeItem *>(m_tree.m_tree.GetTreeItemPtr( hPA ));
		if( ptr3 != 0 ) ptr3->Setup( propName, pManipulator );
	}
	if ( pManipulator->GetPropertyDesc(propName.c_str())->ePropType == SPropertyDesc::VAL_BROWSEFILE )
	{
		hPA = m_tree.m_tree.InsertItemEx( str.c_str(), hPARoot, TVI_LAST, propertieItemFile );
		ptr4 = reinterpret_cast< CFileChosePropertieTreeItem *>(m_tree.m_tree.GetTreeItemPtr( hPA ));
		if( ptr4 != 0 ) ptr4->Setup( propName, pManipulator );
	}
	
	
	m_varHandles.insert( std::make_pair( str, hPA ) );
	return hPA;
}

void CPropertyDockBar::AddObjectWithProp( IManipulator *pMan )
{
	ClearVariables();
	m_insertedNodes.clear();
	m_pCurrentObject = pMan;
	for ( CPtr<IManipulatorIterator> pIt = pMan->Iterate(); !pIt->IsEnd(); pIt->Next() )
	{
		const SPropertyDesc *pDesc = pIt->GetPropertyDesc();
		AddManipulatorVariable( std::string(pDesc->pszName), pMan );
	}
}
