#include "StdAfx.h"
#include "UITree.h"
#include "UIMessages.h"


#ifdef __OLD
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//CRAP �������� �� ������ ������, �������� ���� ������� GLAD
static const int GLAD = 20;		//��� ��������� ��� ��������� �����������, ����� ����� ���� ������� ���������� ScrollBar � �������� ������� ����

IUITreeItem* SUITreeItem::AddTreeItem( IUIElement *_pIcon, IUIElement *_pInfo, int nUserData )
{
	SUITreeItem *pNew = new SUITreeItem;
	pNew->pIcon = _pIcon;
	pNew->pInfo = _pInfo;
	pNew->nData = nUserData;
	treeItems.push_back( pNew );
	return pNew;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SUITreeItem::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &pTreeCtrl );
	saver.Add( 2, &pIcon );
	saver.Add( 3, &pInfo );
	saver.Add( 4, &pParent );
	saver.Add( 5, &nData );
	saver.Add( 6, &treeItems );

	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SUITreeItem::operator&( IDataTree &ss  )
{
	CTreeAccessor saver = &ss;
/*
	saver.Add( 1, &pTreeCtrl );
	saver.Add( 2, &pIcon );
	saver.Add( 3, &pInfo );
	saver.Add( 4, &pParent );
*/
	saver.Add( "UserData", &nData );
	saver.Add( "TreeItems", &treeItems );

	return 0;
}
/*
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SUIListRaw::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &subItems );
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SUIListRaw::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "SubItems", &subItems );
	return 0;
}
*/
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IUIElement* SUIListRaw::GetElement( int nIndex )
{
	NI_ASSERT_T( subItems.size() > nIndex, "Invalid index in vector operation" );
/*
	IUIElement *pElement = dynamic_cast<IUIElement*> ( subItems[nIndex].GetPtr() );
	NI_ASSERT( pElement != 0 );
	return pElement;
*/
	return subItems[nIndex];
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CUIList::~CUIList()
{
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CUIList::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.AddTypedSuper( 1, static_cast<CMultipleWindow*>(this) );
	
	saver.Add( 3, &nLeftSpace );
	saver.Add( 4, &nTopSpace );
	saver.Add( 5, &nItemHeight );
	saver.Add( 6, &nHSubSpace );
	saver.Add( 7, &nVSubSpace );
	saver.Add( 8, &bLeftScrollBar );
	saver.Add( 9, &bScrollBarAlwaysVisible );
	saver.Add( 10, &nScrollBarWidth );
	saver.Add( 11, &pSelectionTexture );
	saver.Add( 12, &selSubRects );
	saver.Add( 14, &nHeaderSize );
	saver.Add( 15, &headers.subItems );
	saver.Add( 16, &listItems );
	saver.Add( 17, &columnProperties );
	
	// scrollbar pointer
	if ( !saver.IsReading() )
	{
		CPtr<IUIElement> pElement = pScrollBar != 0 ? dynamic_cast<IUIElement*>( pScrollBar ) : 0;
		saver.Add( 13, &pElement );
	}
	else
	{
		CPtr<IUIElement> pElement;
		saver.Add( 13, &pElement );
		pScrollBar = pElement != 0 ? dynamic_cast_ptr<CUIScrollBar*>( pElement ) : 0;
		NI_ASSERT_T( pScrollBar != 0, "No ScrollBar control, can not create CUIList" );
	}
	
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CUIList::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.AddTypedSuper( static_cast<CMultipleWindow*>(this) );
	
	saver.Add( "LeftSpace", &nLeftSpace );
	saver.Add( "TopSpace", &nTopSpace );
	saver.Add( "ItemHeight", &nItemHeight );
	saver.Add( "HSubSpace", &nHSubSpace );
	saver.Add( "VSubSpace", &nVSubSpace );
	saver.Add( "LeftSB", &bLeftScrollBar );
	saver.Add( "SBVisible", &bScrollBarAlwaysVisible );
	saver.Add( "SBWidth", &nScrollBarWidth );
	saver.Add( "HeaderSize", &nHeaderSize );
	saver.Add( "ColumnProps", &columnProperties );
	
	if ( saver.IsReading() )
	{
		//�������������� ������ headers
		//if ( nHeaderSize > 0 )
		{
			headers.subItems.resize( columnProperties.size() );
			for ( int i=0; i<columnProperties.size(); i++ )
			{
				IUIElement *pElement = GetChildByID( 10 + i );
				NI_ASSERT_T( pElement != 0, NStr::Format( "Can not find list control header %d", i+10 ) );
				headers.subItems[i] = pElement;
			}
		}
		/*else*/ // NAHUYUA ETO DELAT?
		//headers.subItems.clear();

		//�������������� pScrollBar
		IUIElement *pScrollElement = GetChildByID( 1 );
		pScrollBar = pScrollElement == 0 ? 0 : dynamic_cast<CUIScrollBar *>( pScrollElement );
		
		std::string szName;
		saver.Add( "Selection_Texture", &szName );
		if ( szName.size() == 0 )
			pSelectionTexture = 0;
		else
			pSelectionTexture = GetSingleton<ITextureManager>()->GetTexture( szName.c_str() );

		LoadTileRectangles( &saver, selSubRects, "SelTileRects", pSelectionTexture );
	}
	else
	{
		/*
		if ( pSelectionTexture != 0 )
		{
			std::string szName = GetSingleton<ITextureManager>()->GetTextureName( pTexture );
			szName.erase( szName.rfind( '.' ), -1 );
			saver.Add( "Selection_Texture", &szName );
		}

		if ( pSelectionTexture == 0 )
		{
			CTRect<int> rc;
			rc.x1 = 0;
			rc.x2 = 128;
			rc.y1 = 0;
			rc.y2 = 128;
			saver.Add( "Selection_Texture", &rc );
			
			return;
		}
		*/
	}
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CSimpleWindow* CUIList::CreateComponent( int nWindowType )
{
	IObjectFactory *pFactory = GetCommonFactory();
	IRefCount *pRef = pFactory->CreateObject( nWindowType );
	NI_ASSERT_T( pRef != 0, "List control can not create object" );
	
	CSimpleWindow *pSimple = dynamic_cast<CSimpleWindow *> ( pRef );
	pSimple->InitText();
	NI_ASSERT_T( pSimple != 0, "dynamic_cast failed in CUIList::AddItem()" );
	IUIElement *pElement = checked_cast<IUIElement *>( pRef );
	
	AddChild( pElement );
	return pSimple;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIList::AddItem()
{
	listItems.resize( listItems.size() + 1 );
	SUIListRaw &raw = listItems[ listItems.size() - 1 ];
	int left = nLeftSpace;
	if ( bLeftScrollBar && !bScrollBarAlwaysVisible && pScrollBar && pScrollBar->IsVisible() )
		left += nScrollBarWidth;
	int top = nItemHeight * ( listItems.size() - 1 ) + nTopSpace + nHeaderSize;

	CTRect<float> rc, rect = GetScreenRect();
	int nNumberOfPossible = (rect.bottom - rect.top - nTopSpace * 2 - nHeaderSize) / nItemHeight;
	bool show = ( listItems.size() <= nNumberOfPossible );
	raw.subItems.resize( columnProperties.size() );
	for ( int i=0; i<columnProperties.size(); i++ )
	{
		rc.left = left + nHSubSpace;
		rc.top = top + nVSubSpace;
		rc.right = left + columnProperties[i].nWidth - nHSubSpace;
		rc.bottom = 0;		//top + columnProperties[i].nHeight- nVSubSpace;

		CSimpleWindow *pSimple = CreateComponent( columnProperties[i].nWindowType );
		pSimple->SetScreenRect( rc );
		pSimple->ShowWindow( show );
		raw.subItems[i] = dynamic_cast<IUIElement *> ( pSimple );
		left += columnProperties[i].nWidth;
	}
	
	UpdateScrollBarStatus();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIList::RemoveItem( int nIndex )
{
	NI_ASSERT_T( nIndex < listItems.size() && nIndex >= 0, NStr::Format("Wrong item (%d) to remove (max %d)", nIndex, listItems.size()) );
	CUIListItems::iterator it = listItems.begin() + nIndex;
	listItems.erase( it );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IUIListRaw* CUIList::GetItem( int nIndex )
{
	if ( nIndex == -1 )
	{
		return &headers;
	}

	NI_ASSERT_T( nIndex < listItems.size() && nIndex >= 0, NStr::Format("Wrong item (%d) to get (max %d)", nIndex, listItems.size()) );
	CUIListItems::iterator it = listItems.begin() + nIndex;
	return &(*it);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIList::OnUserChangeScrollBarPosition()
{
	NI_ASSERT_T( pScrollBar != 0, "Can't find scroll bar" );
	
	//������������� ���������� ��� ���� ���������� ���������
	CTRect<float> rect = GetScreenRect();
	int nNumberOfPossible = (rect.bottom - rect.top - nTopSpace * 2 - nHeaderSize) / nItemHeight;
	int nFirstVisible = pScrollBar->GetPosition() / GLAD;
	//��������� ������� ��� nFirstVisible + nNumberOfPossible - 1
	
	CTRect<float> rc;
	int top = rect.top + nTopSpace + nHeaderSize - nFirstVisible * nItemHeight;
	int nItem = 0;
	for ( CUIListItems::iterator item=listItems.begin(); item!=listItems.end(); ++item )
	{
		int left = rect.left + nLeftSpace;
		if ( bLeftScrollBar && !bScrollBarAlwaysVisible && pScrollBar && pScrollBar->IsVisible() )
			left += nScrollBarWidth;
		int col = 0;			//������ �������
		for ( SUIListRaw::CUIListSubItems::iterator it=item->subItems.begin(); it!=item->subItems.end(); ++it )
		{
			CSimpleWindow *pWindow = dynamic_cast<CSimpleWindow *> ( it->GetPtr() );
			rc.left = left + nHSubSpace;
			rc.top = top + nVSubSpace;
			rc.right = left + columnProperties[col].nWidth - nHSubSpace;
			rc.bottom = top + nItemHeight - nVSubSpace;
			pWindow->SetScreenRect( rc );
			if ( nItem < nFirstVisible || nItem >= nFirstVisible + nNumberOfPossible )
				pWindow->ShowWindow( UI_SW_HIDE );
			else
				pWindow->ShowWindow( UI_SW_SHOW );
			left += columnProperties[col].nWidth;
			col++;
		}
		top += nItemHeight;
		nItem++;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
void CUIList::SetNumberOfColumns( int nNumber )
{
	if ( nNumber == nNumberOfColumns )
		return;
	
	//�������� ���������� Properties
	columnProperties.resize( nNumber );
	for ( CUIListItems::iterator item=listItems.begin(); item!=listItems.end(); ++item )
	{
		CUIListSubItems *subItem = (*item);
		if ( nNumber < nNumberOfColumns )
		{
			int index = 0;
			for ( CUIListSubItems::iterator it=subItem->begin(); it!=subItem->end(); ++it )
			{
				if ( index >= nNumber )
					(*it)->DestroyWindow();
				index++;
			}
		}
		subItem->resize( nNumber );
	}
	
	if ( nNumberOfColumns < nNumber )
	{
		//������� � ��������� ����� ����������
		for ( CUIListItems::iterator it=listItems.begin(); it!=listItems.end(); ++it )
		{
			CUIListSubItems *subItem = (*it);
			for ( int i=nNumberOfColumns; i<nNumber; i++ )
			{
				(*subItem)[i] = CreateComponent( columnProperties[i].nWindowType );
			}
		}
	}
	
	nNumberOfColumns = nNumber;
	UpdateAll();
}
*/
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIList::UpdateAll()
{
	UpdateItemsCoordinates();
	UpdateScrollBarCoordinates();
	UpdateScrollBarStatus();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIList::UpdateItemsCoordinates()
{
	//������������� ���������� ��� ���� ���������� ���������
	CTRect<float> rect = GetScreenRect();
	int nNumberOfPossible = (rect.bottom - rect.top - nTopSpace * 2 - nHeaderSize) / nItemHeight;
	CTRect<float> rc;
	
	int top = rect.top + nTopSpace + nHeaderSize;
	int nItem = 0;
	for ( CUIListItems::iterator item=listItems.begin(); item!=listItems.end(); ++item )
	{
		int left = rect.left + nLeftSpace;
		if ( bLeftScrollBar && !bScrollBarAlwaysVisible && pScrollBar && pScrollBar->IsVisible() )
			left += nScrollBarWidth;
		int col = 0;			//������ �������
		for ( SUIListRaw::CUIListSubItems::iterator it=item->subItems.begin(); it!=item->subItems.end(); ++it )
		{
			CSimpleWindow *pWindow = dynamic_cast<CSimpleWindow *> ( it->GetPtr() );
			rc.left = left + nHSubSpace;
			rc.top = top + nVSubSpace;
			rc.right = left + columnProperties[col].nWidth - nHSubSpace;
			rc.bottom = top + nItemHeight - nVSubSpace;
			pWindow->SetScreenRect( rc );
			if ( nItem >= nNumberOfPossible )
				pWindow->ShowWindow( UI_SW_HIDE );
			else
				pWindow->ShowWindow( UI_SW_SHOW );
			left += columnProperties[col].nWidth;
			col++;
		}
		top += nItemHeight;
		nItem++;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIList::UpdateScrollBarCoordinates()
{
/*
	if ( !pScrollBar )
		return;

	CTRect<float> rect = GetScreenRect();
	CTRect<float> rc;
	if ( !bLeftScrollBar )
	{
		rc.left = rect.right - nScrollBarWidth;
		rc.right = rect.right;
		rc.top = rect.top;
		rc.bottom = rect.bottom;
	}
	else
	{
		rc.left = rect.left;
		rc.right = nScrollBarWidth;
		rc.top = rect.top;
		rc.bottom = rect.bottom;
	}
	pScrollBar->SetScreenRect( rc );
*/
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIList::UpdateScrollBarStatus()
{
	if ( !pScrollBar )
		return;

	CTRect<float> rect = GetScreenRect();
	int nNumberOfPossible = (rect.bottom - rect.top - nTopSpace * 2 - nHeaderSize) / nItemHeight;
	if ( bScrollBarAlwaysVisible )
	{
		pScrollBar->ShowWindow( UI_SW_SHOW );
		if ( nNumberOfPossible < listItems.size() )
		{
			pScrollBar->SetMaxValue( (listItems.size() - nNumberOfPossible)*GLAD );
			pScrollBar->SetStep( GLAD );
		}
		else
			pScrollBar->SetMaxValue( 0 );
		return;
	}

	if ( nNumberOfPossible < listItems.size() )
	{
		if ( !pScrollBar->IsVisible() )
		{
			pScrollBar->ShowWindow( UI_SW_SHOW );
			if ( bLeftScrollBar )
			{
				rect.left -= nScrollBarWidth;
				UpdateItemsCoordinates();
			}
			else
				rect.right += nScrollBarWidth;
			UpdateScrollBarCoordinates();
		}
		pScrollBar->SetMaxValue( (listItems.size() - nNumberOfPossible)*GLAD );
		pScrollBar->SetStep( GLAD );
	}
	else
	{
		if ( pScrollBar->IsVisible() )
		{
			pScrollBar->ShowWindow( UI_SW_HIDE );
			if ( bLeftScrollBar )
			{
				rect.left += nScrollBarWidth;
				UpdateItemsCoordinates();
			}
			else
				rect.right -= nScrollBarWidth;
			UpdateScrollBarCoordinates();
		}
		pScrollBar->SetMaxValue( 0 );
		pScrollBar->SetValue( 0 );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
void CUIList::SetColumnProp( int nColumn, const SColumnProperties &prop )
{
	NI_ASSERT_T( nNumberOfColumns > nColumn, "Can't set column props" );
	columnProperties[nColumn].nWidth = prop.nWidth;
	if ( columnProperties[nColumn].nWindowType != prop.nWindowType )
	{
		//������� ����� ���������� � ���� ��������
		columnProperties[nColumn].nWindowType = prop.nWindowType;
		for ( CUIListItems::iterator item=listItems.begin(); item!=listItems.end(); ++item )
		{
			CUIListSubItems *subItem = (*item);
			CSimpleWindow *pChild = (*subItem)[nColumn];
			IUIElement *pElement = dynamic_cast<IUIElement*> ( pChild );
			RemoveChild( pElement );
			CSimpleWindow *pSimple = CreateComponent( prop.nWindowType );
			(*subItem)[nColumn] = pSimple;
		}
	}
	UpdateAll();
}
*/
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIList::Reposition( const CTRect<float> &rcParent )
{
	if ( !bLeftScrollBar )
	{
		pScrollBar->SetPositionFlag( UIPLACE_TOP | UIPLACE_RIGHT );
		pScrollBar->SetPos( CVec2(nScrollBarWidth, 0) );
		pScrollBar->SetSize( CVec2(nScrollBarWidth, GetSize().y ) );
	}
	else
	{
		pScrollBar->SetPositionFlag( UIPLACE_TOP | UIPLACE_LEFT );
		pScrollBar->SetPos( CVec2(0, 0) );
		pScrollBar->SetSize( CVec2(nScrollBarWidth, GetSize().y ) );
	}
	
	int left = nLeftSpace;
	if ( bLeftScrollBar && !bScrollBarAlwaysVisible && pScrollBar && pScrollBar->IsVisible() )
		left += nScrollBarWidth;
	int top = nTopSpace;

	//���������� ���������
	for ( int i=0; i<headers.subItems.size(); i++ )
	{
		CSimpleWindow *pWindow = dynamic_cast<CSimpleWindow *> ( headers.subItems[i].GetPtr() );
		pWindow->SetPos( CVec2( left + nHSubSpace, top + nVSubSpace ) );
		pWindow->SetSize( CVec2( columnProperties[i].nWidth - 2*nHSubSpace, nHeaderSize - 2*nVSubSpace ) );
//		pWindow->UpdateSubRects();

		left += columnProperties[i].nWidth;
	}

	CMultipleWindow::Reposition( rcParent );
	UpdateItemsCoordinates();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIList::ProcessMessage( const SUIMessage &msg )
{
	//ListControl ������������ NOTIFY ��������� �� ScrollBar
	switch( msg.nMessageCode )
	{
	case UI_NOTIFY_POSITION_CHANGED:
		OnUserChangeScrollBarPosition();
		return true;
	}
	
	return CMultipleWindow::ProcessMessage( msg );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIList::Visit( interface ISceneVisitor *pVisitor )
{
	if ( !IsVisible() )
		return;
	
	// ������ ��������
	CSimpleWindow::Visit( pVisitor );
	
	// ������ ���������� �����
	CTRect<float> rect = GetScreenRect();
	int nNumberOfPossible = (rect.bottom - rect.top - nTopSpace * 2 - nHeaderSize) / nItemHeight;
	int nFirstVisible = pScrollBar->GetPosition() / GLAD;
	// ��������� ������� ��� nFirstVisible + nNumberOfPossible - 1
	
	if ( pSelectionTexture && !( nSelection < nFirstVisible || nSelection > nFirstVisible + nNumberOfPossible - 1 ) )
	{
		// selection �����
		if ( !selSubRects.empty() )
		{
			const int nSize = selSubRects.size();
			int top = rect.top + nTopSpace + nHeaderSize - nFirstVisible * nItemHeight;
			top += nSelection * nItemHeight;
			int left = rect.left + nLeftSpace;
			
			SGFXRect2 *pRects = GetTempBuffer<SGFXRect2>( nSize );
			for ( int i=0; i<nSize; i++ )
			{
				SGFXRect2 &rc = pRects[i];
				rc.rect.x1 = left + selSubRects[i].rc.x1;
				rc.rect.x2 = left + selSubRects[i].rc.x2;
				rc.rect.y1 = top + selSubRects[i].rc.y1;
				rc.rect.y2 = top + selSubRects[i].rc.y2;
				rc.maps = selSubRects[i].mapa;
				rc.fZ = 0;
				rc.color = 0xffffffff;
				rc.specular = 0xff000000;
			}
			pVisitor->VisitUIRects( pSelectionTexture, 3, pRects, nSize );
		}
	}
	
	// ������ �����
	for ( CWindowList::reverse_iterator ri = childList.rbegin(); ri != childList.rend(); ++ri )
		(*ri)->Visit( pVisitor );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIList::Draw( IGFX *pGFX )
{
	NI_ASSERT_SLOW_T( false, "Can't user Draw() directly - use visitor pattern" );
	return;
	if ( !IsVisible() )
		return;
	
	//������ ��������
	CSimpleWindow::Draw( pGFX );
	
	//������ ���������� �����
	pGFX->SetShadingEffect( 3 );
	CTRect<float> rect = GetScreenRect();
	int nNumberOfPossible = (rect.bottom - rect.top - nTopSpace * 2 - nHeaderSize) / nItemHeight;
	int nFirstVisible = pScrollBar->GetPosition() / GLAD;
	//��������� ������� ��� nFirstVisible + nNumberOfPossible - 1
	
	if ( pSelectionTexture && !( nSelection < nFirstVisible || nSelection > nFirstVisible + nNumberOfPossible - 1 ) )
	{
		//selection �����
		SGFXRect2 rc;
		pGFX->SetTexture( 0, pSelectionTexture );
		
		int nSize = selSubRects.size();
		if ( nSize > 0 )
		{
			int top = rect.top + nTopSpace + nHeaderSize - nFirstVisible * nItemHeight;
			top += nSelection * nItemHeight;
			int left = rect.left + nLeftSpace;
			
			SGFXRect2 *pRects = GetTempBuffer<SGFXRect2>( nSize );
			for ( int i=0; i<nSize; i++ )
			{
				SGFXRect2 &rc = pRects[ i ];
				rc.rect.x1 = left + selSubRects[i].rc.x1;
				rc.rect.x2 = left + selSubRects[i].rc.x2;
				rc.rect.y1 = top + selSubRects[i].rc.y1;
				rc.rect.y2 = top + selSubRects[i].rc.y2;
				rc.maps = selSubRects[i].mapa;
				rc.fZ = 0;
				rc.color = 0xffffffff;
				rc.specular = 0xff000000;
			}
			pGFX->DrawRects( pRects, nSize );
		}
	}
	
	//������ �����
	for ( CWindowList::reverse_iterator ri=childList.rbegin(); ri!=childList.rend(); ri++ )
		(*ri)->Draw( pGFX );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIList::OnLButtonDown( const CVec2 &vPos, EMouseState mouseState )
{
	bool bRet = CMultipleWindow::OnLButtonDown( vPos, mouseState );
	if ( !bRet )
		return bRet;			//����� ��� ������
	
	//��� �������������� ��������� Selection
	CTRect<float> rect = GetScreenRect();
	int nNumberOfPossible = (rect.bottom - rect.top - nTopSpace * 2 - nHeaderSize) / nItemHeight;
	int nFirstVisible = pScrollBar->GetPosition() / GLAD;
	//��������� ������� ��� nFirstVisible + nNumberOfPossible - 1
	
	if ( vPos.x < rect.left + nLeftSpace || vPos.x > rect.right - nScrollBarWidth - nLeftSpace )
		return true;
	
	int nSel = ( vPos.y - rect.top - nTopSpace - nHeaderSize ) / nItemHeight;
	nSelection = nFirstVisible + nSel;
	
	//���������, ����� selection �� ����� �� ������� ������
	if ( nSelection >= listItems.size() )
	{
		nSelection = listItems.size() - 1;
		return true;
	}

	//����������� �� ��������� selection
	SUIMessage msg;
	msg.nMessageCode = UI_NOTIFY_SELECTION_CHANGED;
	msg.nFirst = GetWindowID();
	msg.nSecond = nSelection;
	GetParent()->ProcessMessage( msg );

	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#endif		//OLD
