#include "StdAfx.h"
#include "UIList.h"
#include "UIListSorter.h"
#include "UIMessages.h"
#include "..\GameTT\CommonId.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SUIListRow::operator &( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &subItems );
	saver.Add( 2, &nUserData );
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SUIListHeader::operator &( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &subItems );
	saver.Add( 2, &nUserData );
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SColumnProperties::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &nWidth );
	saver.Add( 2, &szFileName );
	saver.Add( 3, &nSorterType );
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int SColumnProperties::operator&( IDataTree &ss  )
{
	CTreeAccessor saver = &ss;
	saver.Add( "Width", &nWidth );
	saver.Add( "FileName", &szFileName );
	saver.Add( "Sorter", &nSorterType );
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IUIElement* SUIListRow::GetElement( int nIndex ) const
{
	NI_ASSERT_T( subItems.size() > nIndex, "Invalid index in vector operation" );
	return subItems[nIndex];
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IUIElement* SUIListHeader::GetElement( int nIndex ) const
{
	NI_ASSERT_T( subItems.size() > nIndex, "Invalid index in vector operation" );
	return subItems[nIndex].pElement;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CUIList::CUIList() : pScrollBar( 0 ), nLeftSpace( 10 ), nTopSpace( 5 ), nItemHeight( 30 ), nHeaderSize( 0 ),
	nSortedHeaderIndex( -1 ), bSortAscending( false ), nHeaderTopSpace( 0 ), nHSubSpace( 2 ),
	nVSubSpace( 2 ), bLeftScrollBar( false ), nScrollBarWidth( 30 ), nSelection( -1 ), bScrollBarAlwaysVisible( true )
{
	SetMouseWheelMultiplyer( 25.0f/4.8f );		//������� �������
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
	saver.Add( 18, &nSelection );
	saver.Add( 19, &nSortedHeaderIndex );
	saver.Add( 20, &bSortAscending );
	saver.Add( 21, &nHeaderTopSpace );
	
	// scrollbar pointer
	if ( saver.IsReading() )
	{
		saver.Add( 13, &pScrollBar );
		NI_ASSERT_T( pScrollBar != 0, "No ScrollBar control, can not create CUIShortcutBar" );
		InitSortFunctors();
	}
	else
	{
		saver.Add( 13, &pScrollBar );
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
	saver.Add( "HSubSpace", &nHSubSpace );
	saver.Add( "VSubSpace", &nVSubSpace );
	saver.Add( "LeftSB", &bLeftScrollBar );
	saver.Add( "SBVisible", &bScrollBarAlwaysVisible );
	saver.Add( "SBWidth", &nScrollBarWidth );
	saver.Add( "HeaderSize", &nHeaderSize );
	saver.Add( "ColumnProps", &columnProperties );
	saver.Add( "HeaderTopSpace", &nHeaderTopSpace );
	
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
				headers.subItems[i].pElement = pElement;
			}
		}
		// BLYAD, I ESHE V DVUH MESTAH!
		/*else
			headers.subItems.clear();*/

		//�������������� pScrollBar
		pScrollBar = checked_cast<IUIScrollBar *> ( GetChildByID( 1 ) );
		
		std::string szName;
		saver.Add( "Selection_Texture", &szName );
		if ( szName.size() == 0 )
			pSelectionTexture = 0;
		else
			pSelectionTexture = GetSingleton<ITextureManager>()->GetTexture( szName.c_str() );

		LoadTileRectangles( &saver, selSubRects, "SelTileRects", pSelectionTexture );
		InitItemHeight();
		InitSortFunctors();
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
IUIElement* CUIList::CreateComponent( const char *pszFileName )
{
	CPtr<IDataStorage> pStorage = GetSingleton<IDataStorage>();
	std::string szName = pszFileName;
	szName += ".xml";
	CPtr<IDataStream> pStream = pStorage->OpenStream( szName.c_str(), STREAM_ACCESS_READ );
	NI_ASSERT_T( pStream != 0, NStr::Format("CUIList error: Can not open stream %s", szName.c_str()) );
	if ( !pStream )
		return 0;

	CTreeAccessor saver = CreateDataTreeSaver( pStream, IDataTree::READ );
	CPtr<IUIElement> pElement;
	saver.Add( "Element", &pElement );
	AddChild( pElement );
	return pElement;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIList::SetSelectionItem( int nSel ) 
{ 
	if ( listItems.size() > nSel ) 
	{
		nSelection = nSel; 
		EnsureSelectionVisible();
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIList::InitItemHeight()
{
	nItemHeight = 0;
	CVec2 vSize;
	for ( int i=0; i<columnProperties.size(); i++ )
	{
		CPtr<IUIElement> pElement = CreateComponent( columnProperties[i].szFileName.c_str() );
		pElement->GetWindowPlacement( 0, &vSize, 0 );
		if ( vSize.y > nItemHeight )
			nItemHeight = vSize.y;
		RemoveChild( pElement );
	}
	nItemHeight += nVSubSpace * 2;

	NI_ASSERT( pScrollBar != 0 );
	CVec2 size;
	pScrollBar->GetWindowPlacement( 0, &size, 0 );
	nScrollBarWidth = size.x;
	pScrollBar->SetMinValue( 0 );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIList::AddItem( int nData )
{
	SUIListRow *pRow = new SUIListRow;
	listItems.push_back( pRow );
	pRow->subItems.resize( columnProperties.size() );
	pRow->SetUserData( nData );
	for ( int i=0; i<columnProperties.size(); i++ )
	{
		pRow->subItems[i] = CreateComponent( columnProperties[i].szFileName.c_str() );
	}
	
/*
��� ������ �����, ������ ��� ������ ��� ��� ���������� item ����� ����������� ������ ����� ������
	//����������� items
	if ( nSortedHeaderIndex != -1 )
	{
		bSortAscending = !bSortAscending;		//����� ������ Sort() ���������� �������������
		Sort( nSortedHeaderIndex );
	}
*/
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIList::RemoveItem( int nIndex )
{
	NI_ASSERT_T( nIndex < listItems.size() && nIndex >= 0, NStr::Format("Wrong item (%d) to remove (max %d)", nIndex, listItems.size()) );
	if ( nSelection == nIndex )
	{
		//delete selected element
		RemoveFocusFromItem( nSelection );
		nSelection = -1;
	}
	
	if ( nIndex <= nSelection )
	{
		nSelection--;
	}

	//remove item from list control
	CUIListItems::iterator it = listItems.begin() + nIndex;
	for ( int i=0; i<(*it)->subItems.size(); i++ )
	{
		RemoveChild( (*it)->subItems[i] );
	}
	listItems.erase( it );
	
	NotifySelectionChanged();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IUIListRow* CUIList::GetItem( int nIndex )
{
	if ( nIndex == -1 )
	{
		return &headers;
	}

	NI_ASSERT_T( nIndex < listItems.size() && nIndex >= 0, NStr::Format("Wrong item (%d) to get (max %d)", nIndex, listItems.size()) );
	CUIListItems::iterator it = listItems.begin() + nIndex;
	return *it;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CUIList::GetItemByID( int nID )
{
	for ( int i=0; i<listItems.size(); i++ )
	{
		if ( listItems[i]->nUserData == nID )
			return i;
	}

	return -1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIList::InitialUpdate()
{
//	InitItemHeight();
	UpdateScrollBarStatus();
	UpdateItemsCoordinates();

	if ( nSortedHeaderIndex != -1 )
	{
		bSortAscending = !bSortAscending;		//����� ������ Sort() ���������� �������������
		Sort( nSortedHeaderIndex );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIList::UpdateItemsCoordinates()
{
	//������������� ���������� ��� ���� ���������� ���������
	int nY = -pScrollBar->GetPosition();
	CTRect<float> rect;
	GetWindowPlacement( 0, 0, &rect );
	CTRect<float> boundRc = rect;
	if ( nHeaderSize > 0 )
		boundRc.y1 += nHeaderSize + nHeaderTopSpace;
	else
		boundRc.y1 += nTopSpace;
	boundRc.y2 -= nTopSpace;
	CVec2 vElementPos;
	CVec2 vElementSize;

	for ( CUIListItems::iterator item=listItems.begin(); item!=listItems.end(); ++item )
	{
		if ( nY + nItemHeight - nVSubSpace < 0 || nY + nVSubSpace > rect.Height() - 2*nTopSpace )
		{
			//item �� �������
			for ( SUIListRow::CUIListSubItems::iterator it=(*item)->subItems.begin(); it!=(*item)->subItems.end(); ++it )
			{
				(*it)->ShowWindow( UI_SW_HIDE );
			}
		}
		else
		{
			//item �������
			int left = nLeftSpace;
			if ( bLeftScrollBar && pScrollBar && pScrollBar->IsVisible() )
				left += nScrollBarWidth;
			int nActiveRaw = 0;
			for ( SUIListRow::CUIListSubItems::iterator it=(*item)->subItems.begin(); it!=(*item)->subItems.end(); ++it )
			{
				IUIElement *pElement = *it;
				pElement->ShowWindow( UI_SW_SHOW );
				vElementSize.x = columnProperties[nActiveRaw].nWidth - nHSubSpace * 2;
				vElementSize.y = nItemHeight - nVSubSpace * 2;

				vElementPos.x = left + nHSubSpace;
				vElementPos.y = nY + nHeaderSize + nHeaderTopSpace + nTopSpace + nVSubSpace;
				pElement->SetWindowPlacement( &vElementPos, &vElementSize );
				pElement->SetBoundRect( boundRc );

				left += columnProperties[nActiveRaw].nWidth;
				nActiveRaw++;
			}
		}
		nY += nItemHeight;
	}

	CTRect<float> rc;
	GetParent()->GetWindowPlacement( 0, 0, &rc );
	Reposition( rc );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIList::UpdateScrollBarStatus()
{
	if ( !pScrollBar )
		return;

	CTRect<float> rect;
	GetWindowPlacement( 0, 0, &rect );
	int nMaxSBValue = listItems.size() * nItemHeight - ( rect.Height() - nTopSpace * 2 - nHeaderSize - nHeaderTopSpace );
	if ( nMaxSBValue < 0 )
		nMaxSBValue = 0;
	pScrollBar->SetMaxValue( nMaxSBValue );

	if ( !bScrollBarAlwaysVisible )
	{
		if ( nMaxSBValue > 0 )
			pScrollBar->ShowWindow( UI_SW_SHOW );
		else
			pScrollBar->ShowWindow( UI_SW_HIDE );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIList::Reposition( const CTRect<float> &rcParent )
{
	CTRect<float> rect = GetScreenRect();

	//������� ������� ����������
	{
		CVec2 size;
		pScrollBar->GetWindowPlacement( 0, &size, 0 );
		pScrollBar->SetWindowPlacement( &CVec2(0, 0), &CVec2(size.x, rect.Height()) );
	}

	//���������� ���������
	int left = nLeftSpace;
	CVec2 vPos, vSize;
	vPos.y = nHeaderTopSpace;
	vSize.y = nHeaderSize;
	for ( int i=0; i<headers.subItems.size(); i++ )
	{
		vPos.x = left;
		vSize.x = columnProperties[i].nWidth;
		headers.subItems[i].pElement->SetWindowPlacement( &vPos, &vSize );

		left += columnProperties[i].nWidth;
	}

	CMultipleWindow::Reposition( rcParent );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIList::NotifySelectionChanged()
{
	SUIMessage msg;
	msg.nMessageCode = UI_NOTIFY_SELECTION_CHANGED;
	msg.nFirst = GetWindowID();
	msg.nSecond = nSelection;
	GetParent()->ProcessMessage( msg );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIList::NotifyDoubleClick( int nItem )
{
	SUIMessage msg;
	msg.nMessageCode = UI_NOTIFY_LIST_DOUBLE_CLICK;
	msg.nFirst = GetWindowID();
	msg.nSecond = nItem;
	GetParent()->ProcessMessage( msg );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIList::EnsureSelectionVisible()
{
	CTRect<float> rect = GetScreenRect();

	//���������� ������� ������ selection
	int nY = -pScrollBar->GetPosition();
	nY += nSelection * nItemHeight;

	int nTemp = 0;
	if ( nHeaderSize == 0 )
		nTemp = nTopSpace;
	if ( nY >= nTemp && nY + nItemHeight <= rect.Height() - 2 * nTopSpace - nHeaderSize - nHeaderTopSpace )
	{
		//selection ��������� �������
		return;
	}

	int nScroll = -nY;
/*
	if ( nHeaderSize > 0 )
		nScroll = -nY - nHSubSpace;
	else
		nScroll = -nY - nHSubSpace;
*/
	if ( nScroll > 0 )
	{
		pScrollBar->SetPosition( pScrollBar->GetPosition() - nScroll );
		UpdateItemsCoordinates();
		return;
	}

	nScroll = rect.Height() - 2 * nTopSpace - nHeaderSize - nHeaderTopSpace - nY - nItemHeight;
	if ( nScroll < 0 )
	{
		pScrollBar->SetPosition( pScrollBar->GetPosition() - nScroll );
		UpdateItemsCoordinates();
		return;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIList::OnChar( int nAsciiCode, int nVirtualKey, bool bPressed, DWORD keyState )
{
	if ( !IsVisible() )
		return false;
	if ( !bPressed )
		return false;

	switch ( nVirtualKey )
	{
		case VK_UP:
		//case VK_LEFT:
		if ( pFocused.GetPtr() == pScrollBar.GetPtr() )
			return pScrollBar->OnChar( nAsciiCode, nVirtualKey, bPressed, keyState );
		{
			if ( nSelection > 0 )
			{
				RemoveFocusFromItem( nSelection );
				nSelection--;
				EnsureSelectionVisible();
				NotifySelectionChanged();
				return true;
			}
			else
				return false;
			return true;
		}

		case VK_DOWN:
		//case VK_RIGHT:
		if ( pFocused.GetPtr() == pScrollBar.GetPtr() )
			return pScrollBar->OnChar( nAsciiCode, nVirtualKey, bPressed, keyState );
		{
			if ( nSelection < listItems.size() - 1 )
			{
				RemoveFocusFromItem( nSelection );
				nSelection++;
				EnsureSelectionVisible();
				NotifySelectionChanged();
				return true;
			}
			else
				return false;
		}
	}

	return CMultipleWindow::OnChar( nAsciiCode, nVirtualKey, bPressed, keyState );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIList::ProcessMessage( const SUIMessage &msg )
{
	//ListControl ������������ NOTIFY ��������� �� ScrollBar
	switch( msg.nMessageCode )
	{
	case UI_NOTIFY_POSITION_CHANGED:
		if ( msg.nFirst != 1 && msg.nFirst != 3 )
			return false;

		UpdateItemsCoordinates();
		return true;

	case MESSAGE_KEY_UP:
		if ( pFocused.GetPtr() == pScrollBar.GetPtr() )
			return pScrollBar->ProcessMessage( msg );
		{
			if ( nSelection > 0 )
			{
				RemoveFocusFromItem( nSelection );
				nSelection--;
				EnsureSelectionVisible();
				NotifySelectionChanged();
				return true;
			}
			else
				return false;
			return true;
		}
	case MESSAGE_KEY_DOWN:
		if ( pFocused.GetPtr() == pScrollBar.GetPtr() )
			return pScrollBar->ProcessMessage( msg );
		{
			if ( nSelection < listItems.size() - 1 )
			{
				RemoveFocusFromItem( nSelection );
				nSelection++;
				EnsureSelectionVisible();
				NotifySelectionChanged();
				return true;
			}
			else
				return false;
		}

	case UI_NOTIFY_STATE_CHANGED_MESSAGE:
		if ( msg.nFirst >= 10 && msg.nFirst < 10 + headers.subItems.size() )
		{
			//������ �� ���� �� ����������, ���������� ������������� ���� �������
			const int nColumn = msg.nFirst - 10;
			Sort( nColumn );
			//notify about resort
			SUIMessage msg;
			msg.nMessageCode = UI_NOTIFY_LIST_RESORTED;
			msg.nFirst = GetWindowID();
			msg.nSecond = (nColumn << 8) | int(bSortAscending);
			GetParent()->ProcessMessage( msg );
			return true;
		}
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
	if ( pSelectionTexture && nSelection != -1 )
	{
		int nY = -pScrollBar->GetPosition();
		nY += nSelection * nItemHeight;

		if ( nY + nItemHeight + nTopSpace + nHeaderSize + nHeaderTopSpace > 0 && nY < rect.Height() - nTopSpace - nHeaderSize - nHeaderTopSpace )
		{
			// selection �����
			if ( !selSubRects.empty() )
			{
				const int nSize = selSubRects.size();
				int top = rect.top + nY + nTopSpace + nHeaderSize + nHeaderTopSpace;
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
					
					//��������, ����� ����� ������ ����� selection
					int nAdd = 0;
					if ( nHeaderSize > 0 )
						nAdd = nHeaderSize + nHeaderTopSpace;
					else
						nAdd = nTopSpace;
					float fY = rect.y1 - rc.rect.y1 + nAdd;
					if ( fY > 0 )
					{
						rc.maps.y1 += fY * selSubRects[i].mapa.Height() / selSubRects[i].rc.Height();
						rc.rect.y1 = rect.y1 + nAdd;
					}
	
					fY = rc.rect.y2 - rect.y2 + nTopSpace;
					if ( fY > 0 )
					{
						rc.maps.y2 -= fY * selSubRects[i].mapa.Height() / selSubRects[i].rc.Height();
						rc.rect.y2 = rect.y2 - nTopSpace;
					}
				}
				pVisitor->VisitUIRects( pSelectionTexture, 3, pRects, nSize );
			}
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
	pGFX->SetShadingEffect( 3 );
	CSimpleWindow::Draw( pGFX );

	//������ ���������� �����
	CTRect<float> rect = GetScreenRect();
	if ( pSelectionTexture && nSelection != -1 )
	{
		int nY = -pScrollBar->GetPosition();
		nY += nSelection * nItemHeight;

		if ( nY + nItemHeight + nTopSpace + nHeaderSize + nHeaderTopSpace > 0 && nY < rect.Height() - nTopSpace - nHeaderSize - nHeaderTopSpace )
		{
			//selection �����
			SGFXRect2 rc;
			pGFX->SetTexture( 0, pSelectionTexture );
			
			int nSize = selSubRects.size();
			if ( nSize > 0 )
			{
				int top = rect.top + nY + nTopSpace + nHeaderSize + nHeaderTopSpace;
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
					
					//��������, ����� ����� ������ ����� selection
					int nAdd = 0;
					if ( nHeaderSize > 0 )
						nAdd = nHeaderSize + nHeaderTopSpace;
					else
						nAdd = nTopSpace;
					float fY = rect.y1 - rc.rect.y1 + nAdd;
					if ( fY > 0 )
					{
						rc.maps.y1 += fY * selSubRects[i].mapa.Height() / selSubRects[i].rc.Height();
						rc.rect.y1 = rect.y1 + nAdd;
					}
	
					fY = rc.rect.y2 - rect.y2 + nTopSpace;
					if ( fY > 0 )
					{
						rc.maps.y2 -= fY * selSubRects[i].mapa.Height() / selSubRects[i].rc.Height();
						rc.rect.y2 = rect.y2 - nTopSpace;
					}
				}
				pGFX->DrawRects( pRects, nSize );
			}
		}
	}
	
	//������ �����
	for ( CWindowList::reverse_iterator ri=childList.rbegin(); ri!=childList.rend(); ri++ )
		(*ri)->Draw( pGFX );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIList::OnLButtonDown( const CVec2 &vPos, EMouseState mouseState )
{
	if ( pScrollBar->IsInside( vPos ) )
		return CMultipleWindow::OnLButtonDown( vPos, mouseState );

	bool bRet = CMultipleWindow::OnLButtonDown( vPos, mouseState );
	if ( !bRet )
		return bRet;			//����� ��� ������
	
	//��� �������������� ��������� Selection
	CTRect<float> rect = GetScreenRect();
	float fSelIndex = vPos.y - rect.y1 - nHeaderSize - nHeaderTopSpace;
	//������ fSelIndex �������� �������� ����� ������������ ������ items
	//���� fSelIndex < 0, ������ ������ ��� ������� items, �������� � ������� header
	if ( fSelIndex < 0 )
		return true;
	
	fSelIndex = ( fSelIndex + pScrollBar->GetPosition() - nTopSpace ) / nItemHeight;
	if ( fSelIndex < 0 || fSelIndex >= listItems.size() )
		return true;
	
	//������ fSelIndex ��� ������ item ��� ������
	if ( nSelection == (int) fSelIndex )
		return true;		//���������� ������� item ����� ����� �� ������, ������ �� ����������
	
	//selection ���������
	RemoveFocusFromItem( nSelection );
	nSelection = fSelIndex;
	NotifySelectionChanged();
	
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIList::OnLButtonDblClk( const CVec2 &vPos )
{
	if ( pScrollBar->IsInside( vPos ) )
		return CMultipleWindow::OnLButtonDblClk( vPos );
	
	bool bRet = CMultipleWindow::OnLButtonDblClk( vPos );
	if ( !bRet )
		return bRet;			//����� ��� ������

	//��� �������������� double click ������ ������
	CTRect<float> rect = GetScreenRect();
	float fSelIndex = vPos.y - rect.y1 - nHeaderSize - nHeaderTopSpace;
	//������ fSelIndex �������� �������� ����� ������������ ������ items
	//���� fSelIndex < 0, ������ ������ ��� ������� items, �������� � ������� header
	if ( fSelIndex < 0 )
		return true;
	
	fSelIndex = ( fSelIndex + pScrollBar->GetPosition() - nTopSpace ) / nItemHeight;
	if ( fSelIndex < 0 || fSelIndex >= listItems.size() )
		return true;
	
	//������ fSelIndex ��� ������ item ��� ������
	//�������� ��������� � double click
	NotifyDoubleClick( fSelIndex );
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIList::SetSortFunctor( int nColumn, IUIListSorter *pSorter )
{
	NI_ASSERT_T( nColumn < columnProperties.size(), "Invalid column index in CUIList::SetSortFunctor()" );
	headers.subItems[nColumn].pSorter = pSorter;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SSortWrapper
{
private:
	int nColumn;
	CPtr<IUIListSorter> pSorter;
	bool bReverse;
	
public:
	SSortWrapper( int n, IUIListSorter *p, bool bR ) : nColumn( n ), pSorter( p ), bReverse( bR ) {}
	bool operator()( const SUIListRow *pR1, const SUIListRow *pR2 ) const
	{
		return (*pSorter)( nColumn, pR1, pR2, bReverse );
			/*bReverse ^*/ 
	}
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIList::ReSort()
{
	//����������� items
	if ( nSortedHeaderIndex == -1 )
		return false;			//�� �����, �� ������ ������� �����������

	bSortAscending = !bSortAscending;		//����� ������ Sort() ���������� �������������
	return Sort( nSortedHeaderIndex );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIList::Sort( int nColumn, const int nSortType )
{
	// -1 - descending
	//	1 - ascending
	//	0 - doesnt matter (inner determie )

	NI_ASSERT_T( nColumn < columnProperties.size(), "Invalid column index in CUIList::Sort()" );
	if ( !headers.subItems[nColumn].pSorter )
		return false;

	//����� ����������� �������� ������� ���������� �������
	SUIListRow *pSelectedRow = 0;
	if ( nSelection < listItems.size() && nSelection >= 0 )
		pSelectedRow = listItems[nSelection].GetPtr();

	if ( nSortedHeaderIndex != nColumn )
	{
		//������� ��������� � nSortedHeaderIndex
		if ( nSortedHeaderIndex != -1 )
			headers.subItems[nSortedHeaderIndex].pElement->SetState( 0, false );
		nSortedHeaderIndex = nColumn;
		headers.subItems[nSortedHeaderIndex].pElement->SetState( 1, false );
		
		switch( nSortType )
		{
		case 0:
			bSortAscending = true;
			break;
		case 1:
			bSortAscending = false;
			break;
		case -1:
			bSortAscending= true;
			break;
		}
	}
	else
	{
		switch( nSortType )
		{
		case 0:
			bSortAscending = !bSortAscending;
			break;
		case 1:
			bSortAscending = false;
			break;
		case -1:
			bSortAscending= true;
			break;
		}
		headers.subItems[nSortedHeaderIndex].pElement->SetState( 1 + bSortAscending, false );
	}

	SSortWrapper sw( nColumn, headers.subItems[nColumn].pSorter, bSortAscending );
	std::stable_sort( listItems.begin(), listItems.end(), sw );
	UpdateItemsCoordinates();

	//��������������� ������� selection
	if ( pSelectedRow )
	{
		int i = 0;
		for ( ; i<listItems.size(); i++ )
		{
			if ( listItems[i].GetPtr() == pSelectedRow )
			{
				nSelection = i;
				break;
			}
		}
		NI_ASSERT_T( i != listItems.size(), "Error: can not find selected element after sort" );
	}
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIList::InitSortFunctors()
{
	// EBAT' V ROT
	//if ( nHeaderSize == 0 )
		//return;
	NI_ASSERT_T( columnProperties.size() == headers.subItems.size(), "CUIList error: The size of headers does not equal the size of ColumnProperties" );
	
	for ( int i=0; i<columnProperties.size(); i++ )
	{
		switch ( columnProperties[i].nSorterType )
		{
		case 1:
			headers.subItems[i].pSorter = new CUIListTextSorter;
			break;
		case 2:
			headers.subItems[i].pSorter = new CUIListNumberSorter;
			break;
		case 3:
			headers.subItems[i].pSorter = new CUIListUserDataSorter;
			break;
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIList::RemoveFocusFromItem( int nIndex )
{
	if ( nIndex < 0 || nIndex >= listItems.size() )
		return;

	CUIListItems::iterator it = listItems.begin() + nIndex;
	for ( int i=0; i<(*it)->subItems.size(); i++ )
	{
		(*it)->subItems[i]->SetFocus( false );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIList::MoveSelectionItemUp()
{
	if ( nSelection < 0 || nSelection >= listItems.size() )
		return;
	
	CUIListItems::iterator it = listItems.begin() + nSelection;
	for ( int i=0; i<(*it)->subItems.size(); i++ )
	{
		MoveWindowUp( (*it)->subItems[i] );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIList::OnMouseWheel( const CVec2 &vPos, EMouseState mouseState, float fDelta )
{
	if ( !IsInside( vPos ) )
		return false;
	
	if ( !pScrollBar )
		return false;
	
	pScrollBar->SetPosition( pScrollBar->GetPosition() + fDelta*GetMouseWheelMultiplyer() );
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
