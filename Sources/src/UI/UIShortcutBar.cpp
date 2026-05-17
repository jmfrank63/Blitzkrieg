#include "StdAfx.h"
#include "UIShortcutBar.h"
#include "UIMessages.h"


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//CRAP �������� �� ������ ������, �������� ���� ������� GLAD
static const int GLAD = 20;		//��� ��������� ��� ��������� �����������, ����� ����� ���� ������� ���������� ScrollBar � �������� ������� ����

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CUIShortcutBar::SBar::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	
	saver.Add( 1, &pElement );
	saver.Add( 2, &items );
	saver.Add( 3, &bExpandState );
	return 0;
}	
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CUIShortcutBar::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.AddTypedSuper( 1, static_cast<CMultipleWindow*>(this) );
	
	saver.Add( 2, &nBottomSpace );
	saver.Add( 3, &nLeftSpace );
	saver.Add( 4, &nTopSpace );
	saver.Add( 5, &nBarHeight );
	saver.Add( 7, &nVSubSpace );
	saver.Add( 8, &nItemLeftSpace );
	saver.Add( 9, &bScrollBarAlwaysVisible );
	saver.Add( 10, &nScrollBarWidth );
	saver.Add( 11, &pSelectionTexture );
	saver.Add( 12, &selSubRects );
	//13 ������
	saver.Add( 14, &nSelBar );
	saver.Add( 15, &nSelItem );
	saver.Add( 16, &bars );
	//
	saver.Add( 17, &szBarFileName );
	saver.Add( 18, &szItemFileName );
	saver.Add( 19, &szTextFileName );
	saver.Add( 20, &nRightSpace );
	
	// scrollbar pointer
	if ( saver.IsReading() )
	{
		saver.Add( 13, &pScrollBar );
		NI_ASSERT_T( pScrollBar != 0, "No ScrollBar control, can not create CUIShortcutBar" );
	}
	else
	{
		saver.Add( 13, &pScrollBar );
	}
	
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CUIShortcutBar::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.AddTypedSuper( static_cast<CMultipleWindow*>(this) );
	
	saver.Add( "LeftSpace", &nLeftSpace );
	saver.Add( "RightSpace", &nRightSpace );
	saver.Add( "TopSpace", &nTopSpace );
	saver.Add( "BottomSpace", &nBottomSpace );
	saver.Add( "BarHeight", &nBarHeight );
	saver.Add( "VSubSpace", &nVSubSpace );
	saver.Add( "ItemLeftSpace", &nItemLeftSpace );
	saver.Add( "SBVisible", &bScrollBarAlwaysVisible );

	saver.Add( "BarFile", &szBarFileName );
	saver.Add( "ItemFile", &szItemFileName );
	saver.Add( "TextFile", &szTextFileName );

	if ( saver.IsReading() )
	{
		//�������������� pScrollBar
		pScrollBar = checked_cast<IUIScrollBar *> ( GetChildByID( 1 ) );
		InitSBWidth();
		
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
CUIShortcutBar::CUIShortcutBar() : pScrollBar( 0 ), nLeftSpace( 10 ), nRightSpace( 10 ), nTopSpace( 5 ),
	nVSubSpace( 2 ), nItemLeftSpace( 0 ), nScrollBarWidth( 30 ), bScrollBarAlwaysVisible( true ),
	nSelBar( -1 ), nSelItem( -1 ), nBottomSpace( 5 ), nBarHeight( 30 )
{
	SetMouseWheelMultiplyer( 22.5f );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIShortcutBar::InitSBWidth()
{
	NI_ASSERT( pScrollBar != 0 );
/*
	CVec2 vPos, vSize;
	CTRect<float> rc;
	
	pScrollBar->GetWindowPlacement( &vPos, &vSize, &rc );
	nScrollBarWidth = vSize.x;
*/
	CUIScrollBar *pSB = dynamic_cast<CUIScrollBar *> ( pScrollBar.GetPtr() );
	nScrollBarWidth = pSB->vSize.x;
	pScrollBar->SetMinValue( 0 );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IUIElement* CUIShortcutBar::AddBar()
{
	CPtr<IDataStorage> pStorage = GetSingleton<IDataStorage>();
	std::string szName = szBarFileName;
	szName += ".xml";
	CPtr<IDataStream> pStream = pStorage->OpenStream( szName.c_str(), STREAM_ACCESS_READ );
	NI_ASSERT_T( pStream != 0, NStr::Format("CUIShortcutBar error: Can not open stream %s", szName.c_str()) );
	if ( !pStream )
		return 0;
	
	SBar bar;
	CTreeAccessor saver = CreateDataTreeSaver( pStream, IDataTree::READ );
	saver.Add( "Element", &bar.pElement );
	bars.push_back( bar );
	AddChild( bar.pElement );
	return bar.pElement;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IUIElement* CUIShortcutBar::AddItem()
{
	NI_ASSERT_T( bars.size() > 0, "CUIShortcutBar error: You need to add bar before adding items" );
	CPtr<IDataStorage> pStorage = GetSingleton<IDataStorage>();
	std::string szName = szItemFileName;
	szName += ".xml";
	CPtr<IDataStream> pStream = pStorage->OpenStream( szName.c_str(), STREAM_ACCESS_READ );
	NI_ASSERT_T( pStream != 0, NStr::Format("CUIShortcutBar error: Can not open stream %s", szName.c_str()) );
	if ( !pStream )
		return 0;
	
	CTreeAccessor saver = CreateDataTreeSaver( pStream, IDataTree::READ );
	CPtr<IUIElement> pElement;
	saver.Add( "Element", &pElement );
	
	SBar &bar = bars[bars.size() - 1];
	bar.items.push_back( pElement.GetPtr() );
	AddChild( pElement );
	return pElement;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIShortcutBar::AddMultyItems( int nNum )
{
	NI_ASSERT_T( bars.size() > 0, "CUIShortcutBar error: You need to add bar before adding items" );
	CPtr<IDataStorage> pStorage = GetSingleton<IDataStorage>();
	std::string szName = szItemFileName;
	szName += ".xml";
	CPtr<IDataStream> pStream = pStorage->OpenStream( szName.c_str(), STREAM_ACCESS_READ );
	NI_ASSERT_T( pStream != 0, NStr::Format("CUIShortcutBar error: Can not open stream %s", szName.c_str()) );
	if ( !pStream )
		return;
	
	SBar &bar = bars[bars.size() - 1];
	CTreeAccessor saver = CreateDataTreeSaver( pStream, IDataTree::READ );
	
	CPtr<IUIElement> pElementBase;
	saver.Add( "Element", &pElementBase );
	CPtr<IUIElement> pElement;
	
	for ( int i = 0; i < nNum; ++i )
	{
		pElement = pElementBase->Duplicate();
		//saver.Add( "Element", &pElement );
		bar.items.push_back( pElement.GetPtr() );
		AddChild( pElement );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IUIElement* CUIShortcutBar::AddTextItem( const WORD *pszText )
{
	NI_ASSERT_T( bars.size() > 0, "CUIShortcutBar error: You need to add bar before adding items" );
	SBar &bar = bars[bars.size() - 1];
	
	CPtr<IDataStorage> pStorage = GetSingleton<IDataStorage>();
	std::string szName = szTextFileName;
	szName += ".xml";
	CPtr<IDataStream> pStream = pStorage->OpenStream( szName.c_str(), STREAM_ACCESS_READ );
	NI_ASSERT_T( pStream != 0, NStr::Format("CUIShortcutBar error: Can not open stream %s", szName.c_str()) );
	if ( !pStream )
		return 0;

	CTreeAccessor saver = CreateDataTreeSaver( pStream, IDataTree::READ );
	CObj<IUIElement> pText;
	saver.Add( "Element", &pText );

	CSimpleWindow *pWindow = dynamic_cast<CSimpleWindow *> ( pText.GetPtr() );
	pWindow->vSize.x = wndRect.Width() - nLeftSpace - nRightSpace - 2*nItemLeftSpace - nScrollBarWidth;
	pWindow->SetWindowText( 0, pszText );
	
	//���� ���������� ������ ������������ ������
	for ( int i=0; i<pWindow->states.size(); i++ )
	{
		if ( pWindow->states[i].pGfxText )
		{
			pWindow->states[i].pGfxText->SetWidth( pWindow->vSize.x );
			pWindow->states[i].pGfxText->EnableRedLine( bRedLine );
		}
	}
	int nHeight = pWindow->states[0].pGfxText->GetNumLines();				//����� �����
	nHeight *= pWindow->states[0].pGfxText->GetLineSpace();					//������ ����� �������
	pWindow->vSize.y = nHeight;

	bar.items.push_back( pText );
	AddChild( pText );
	return pText;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIShortcutBar::InitialUpdate()
{
	InitSBWidth();
	UpdateScrollBarStatus();
	UpdateItemsCoordinates();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIShortcutBar::UpdateScrollBarStatus()
{
	int nBeginSelectedBar = -1, nEndSelectedBar = -1, nSelectedSize = -1;

	int nV = 0;
	//���������� ����� ���� ������ � ��������
	for ( int i = 0; i < bars.size(); ++i )
	{
		const SBar &bar = bars[i];
		if ( i == nSelBar )
			nBeginSelectedBar = nV;

		nV += nBarHeight;
		if ( bar.bExpandState )
		{
			for ( CWindowList::const_iterator it=bar.items.begin(); it!=bar.items.end(); ++it )
			{
				CVec2 vSize;
				(*it)->GetWindowPlacement( 0, &vSize, 0 );
				nV += vSize.y + nVSubSpace * 2;
			}
			if ( i == nSelBar )
			{
				nEndSelectedBar = nV;
				nSelectedSize = nEndSelectedBar -  nBeginSelectedBar;
			}
		}
	}

	const int nWindowHeight = wndRect.Height() - nTopSpace - nBottomSpace;
	nV -= nWindowHeight;
	//nBeginSelectedBar -= nWindowHeight;
	//nEndSelectedBar -= nWindowHeight;

	if ( nV > 0 )
	{
		//SB �������
		pScrollBar->SetMaxValue( nV );
		if ( pScrollBar->GetPosition() > nV )
			pScrollBar->SetPosition( nV );
		if ( !bScrollBarAlwaysVisible )
			pScrollBar->ShowWindow( UI_SW_SHOW );
		
		if ( nEndSelectedBar > nWindowHeight + pScrollBar->GetPosition() ) // end of the bar is out of the window bounds
		{
			// scroll to bar begin or to end of list
			pScrollBar->SetPosition( Min( nBeginSelectedBar, nEndSelectedBar - nWindowHeight ) );
		}
	}
	else
	{
		//SB ���������
		pScrollBar->SetMaxValue( 0 );
		if ( pScrollBar->GetPosition() > 0 )
			pScrollBar->SetPosition( 0 );
		if ( !bScrollBarAlwaysVisible )
			pScrollBar->ShowWindow( UI_SW_HIDE );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIShortcutBar::UpdateItemsCoordinates()
{
	//������������� ���������� ��� ���� ���������� ���������
	int nY = -pScrollBar->GetPosition();
	CTRect<float> boundRc = wndRect;
	boundRc.y1 += nTopSpace;
	boundRc.y2 -= nBottomSpace;
	
	for ( int i=0; i<bars.size(); i++ )
	{
		const SBar &bar = bars[i];
		if ( nY + nBarHeight - nVSubSpace < 0 || nY + nVSubSpace > wndRect.Height() - nTopSpace - nBottomSpace )
		{
			//bar �� �������
			bar.pElement->ShowWindow( UI_SW_HIDE );
		}
		else
		{
			CVec2 vBarPos( nLeftSpace, nY + nTopSpace + nVSubSpace );
			bar.pElement->SetWindowPlacement( &vBarPos, 0 );
			bar.pElement->ShowWindow( UI_SW_SHOW );
			bar.pElement->SetBoundRect( boundRc );
		}
		nY += nBarHeight;
		
		for ( CWindowList::const_iterator it=bar.items.begin(); it!=bar.items.end(); ++it )
		{
			IUIElement *pE = *it;
			if ( bar.bExpandState )
			{
				CVec2 vSize;
				(*it)->GetWindowPlacement( 0, &vSize, 0 );
				if ( nY + nVSubSpace + vSize.y < 0 || nY + nVSubSpace > wndRect.Height() - nTopSpace - nBottomSpace )
				{
					//item �� �������
					pE->ShowWindow( UI_SW_HIDE );
				}
				else
				{
					CVec2 vItemPos( nLeftSpace + nItemLeftSpace, nY + nTopSpace + nVSubSpace );
					pE->SetWindowPlacement( &vItemPos, 0 );
					pE->ShowWindow( UI_SW_SHOW );
					pE->SetBoundRect( boundRc );
				}
				nY += vSize.y + 2*nVSubSpace;
			}
			else
			{
				pE->ShowWindow( UI_SW_HIDE );
			}
		}
	}

	CTRect<float> rc;
	pParent->GetWindowPlacement( 0, 0, &rc );
	Reposition( rc );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIShortcutBar::Reposition( const CTRect<float> &rcParent )
{
	//������� ������� ����������
	CVec2 size;
	pScrollBar->GetWindowPlacement( 0, &size, 0 );
	pScrollBar->SetWindowPlacement( 0, &CVec2(size.x, wndRect.Height() ) );
	CMultipleWindow::Reposition( rcParent );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIShortcutBar::ProcessMessage( const SUIMessage &msg )
{
	//ListControl ������������ NOTIFY ��������� �� ScrollBar
	switch( msg.nMessageCode )
	{
	case UI_NOTIFY_POSITION_CHANGED:
		UpdateItemsCoordinates();
		return true;
	}
	
	return CMultipleWindow::ProcessMessage( msg );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIShortcutBar::Visit( interface ISceneVisitor *pVisitor )
{
	if ( !IsVisible() )
		return;	
	// ������ ��������
	CSimpleWindow::Visit( pVisitor );
	
	// ������ ���������
	if ( nSelBar != -1 && nSelItem != -1 && pSelectionTexture )
	{
		int nLeft = wndRect.x1 + nLeftSpace;
		int nTop = -pScrollBar->GetPosition();
		for ( int i=0; i<bars.size(); i++ )
		{
			const SBar &bar = bars[i];
			nTop += nBarHeight;
			if ( i == nSelBar )
				break;
			if ( bar.bExpandState )
			{
				for ( CWindowList::const_iterator it=bar.items.begin(); it!=bar.items.end(); ++it )
				{
					CVec2 vSize;
					(*it)->GetWindowPlacement( 0, &vSize, 0 );
					nTop += vSize.y + nVSubSpace * 2;
				}
			}
		}
		
		const SBar &bar = bars[nSelBar];
		int nItemIndex = 0;
		CWindowList::const_iterator it = bar.items.begin();
		for ( ; it != bar.items.end(); ++it )
		{
			if ( nItemIndex == nSelItem )
				break;
			CVec2 vSize;
			(*it)->GetWindowPlacement( 0, &vSize, 0 );
			nTop += vSize.y + nVSubSpace * 2;
			nItemIndex++;
		}
		CVec2 vSize;
		(*it)->GetWindowPlacement( 0, &vSize, 0 );
		
		if ( nTop + nVSubSpace + 2*vSize.y > 0 && nTop < wndRect.Height() - nTopSpace - nBottomSpace )
		{
			// selection �����
			if ( !selSubRects.empty() )
			{
				const int nSize = selSubRects.size();
				nTop += wndRect.y1 + nTopSpace;

				SGFXRect2 *pRects = GetTempBuffer<SGFXRect2>( nSize );
				for ( int i=0; i<nSize; i++ )
				{
					SGFXRect2 &rc = pRects[i];
					rc.rect.x1 = nLeft + selSubRects[i].rc.x1;
					rc.rect.x2 = nLeft + selSubRects[i].rc.x2;
					rc.rect.y1 = nTop + selSubRects[i].rc.y1;
					rc.rect.y2 = nTop + selSubRects[i].rc.y2;
					rc.maps = selSubRects[i].mapa;
					rc.fZ = 0;
					rc.color = 0xffffffff;
					rc.specular = 0xff000000;

					// ��������, ����� ����� ������ ����� selection
					float fY = wndRect.y1 + nTopSpace - rc.rect.y1;
					if ( fY > 0 )
					{
						rc.maps.y1 += fY * selSubRects[i].mapa.Height() / selSubRects[i].rc.Height();
						rc.rect.y1 = wndRect.y1 + nTopSpace;
					}
	
					fY = rc.rect.y2 - wndRect.y2 + nBottomSpace;
					if ( fY > 0 )
					{
						rc.maps.y2 -= fY * selSubRects[i].mapa.Height() / selSubRects[i].rc.Height();
						rc.rect.y2 = wndRect.y2 - nBottomSpace;
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
void CUIShortcutBar::Draw( IGFX *pGFX )
{
	NI_ASSERT_SLOW_T( false, "Can't user Draw() directly - use visitor pattern" );
	return;

	if ( !IsVisible() )
		return;
	
	//������ ��������
	CSimpleWindow::Draw( pGFX );
	
	//������ ���������
	pGFX->SetShadingEffect( 3 );
	if ( nSelBar != -1 && nSelItem != -1 && pSelectionTexture )
	{
		int nLeft = wndRect.x1 + nLeftSpace;
		int nTop = -pScrollBar->GetPosition();
		for ( int i=0; i<bars.size(); i++ )
		{
			const SBar &bar = bars[i];
			nTop += nBarHeight;
			if ( i == nSelBar )
				break;
			if ( bar.bExpandState )
			{
				for ( CWindowList::const_iterator it=bar.items.begin(); it!=bar.items.end(); ++it )
				{
					CVec2 vSize;
					(*it)->GetWindowPlacement( 0, &vSize, 0 );
					nTop += vSize.y + nVSubSpace * 2;
				}
			}
		}
		
		const SBar &bar = bars[nSelBar];
		int nItemIndex = 0;
		CWindowList::const_iterator it=bar.items.begin();
		for ( ; it!=bar.items.end(); ++it )
		{
			if ( nItemIndex == nSelItem )
				break;
			CVec2 vSize;
			(*it)->GetWindowPlacement( 0, &vSize, 0 );
			nTop += vSize.y + nVSubSpace * 2;
			nItemIndex++;
		}
		CVec2 vSize;
		(*it)->GetWindowPlacement( 0, &vSize, 0 );
		
		if ( nTop + nVSubSpace + 2*vSize.y > 0 && nTop < wndRect.Height() - nTopSpace - nBottomSpace )
		{
			//selection �����
			int nSize = selSubRects.size();
			if ( nSize > 0 )
			{
				pGFX->SetTexture( 0, pSelectionTexture );
				nTop += wndRect.y1 + nTopSpace;

				SGFXRect2 *pRects = GetTempBuffer<SGFXRect2>( nSize );
				for ( int i=0; i<nSize; i++ )
				{
					SGFXRect2 &rc = pRects[ i ];
					rc.rect.x1 = nLeft + selSubRects[i].rc.x1;
					rc.rect.x2 = nLeft + selSubRects[i].rc.x2;
					rc.rect.y1 = nTop + selSubRects[i].rc.y1;
					rc.rect.y2 = nTop + selSubRects[i].rc.y2;
					rc.maps = selSubRects[i].mapa;
					rc.fZ = 0;
					rc.color = 0xffffffff;
					rc.specular = 0xff000000;

					//��������, ����� ����� ������ ����� selection
					float fY = wndRect.y1 + nTopSpace - rc.rect.y1;
					if ( fY > 0 )
					{
						rc.maps.y1 += fY * selSubRects[i].mapa.Height() / selSubRects[i].rc.Height();
						rc.rect.y1 = wndRect.y1 + nTopSpace;
					}
	
					fY = rc.rect.y2 - wndRect.y2 + nBottomSpace;
					if ( fY > 0 )
					{
						rc.maps.y2 -= fY * selSubRects[i].mapa.Height() / selSubRects[i].rc.Height();
						rc.rect.y2 = wndRect.y2 - nBottomSpace;
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
void CUIShortcutBar::GetSelectionItem( int *pBar, int *pItem ) 
{ 
	if ( pBar )
		*pBar = nSelBar; 
	if ( pItem )
		*pItem = nSelItem; 
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIShortcutBar::OnLButtonDown( const CVec2 &vPos, EMouseState mouseState )
{
	if ( pScrollBar->IsInside( vPos ) )
		return CMultipleWindow::OnLButtonDown( vPos, mouseState );

	float fX = vPos.x - wndRect.x1 - nLeftSpace;
	if ( fX < 0 || fX > wndRect.Width() - nRightSpace - nScrollBarWidth )
		return true;		//��� ������� selection
	
	float fY = vPos.y - wndRect.y1 - nTopSpace;
	if ( fY < 0 || fY > wndRect.Height() - nTopSpace - nBottomSpace )
		return true;		//��� ������� selection
	
	bool bRet = CMultipleWindow::OnLButtonDown( vPos, mouseState );
	if ( !bRet )
		return bRet;			//����� ��� ������
	if ( bars.size() == 0 )
		return bRet;
	
	int nV = -pScrollBar->GetPosition();
	//������ ���������� item
/*
	int nPrevSelBar = nSelBar;
	int nPrevSelItem = nSelItem;
	nSelBar = nSelItem = -1;
*/
	for ( int i = 0; i < bars.size(); ++i )
	{
		SBar &bar = bars[i];
		if ( nV <= fY && nV + nBarHeight > fY )
		{
			//������������� ��������, ��� ����� ������ ��������
			if ( !bars[i].pElement->IsInside( vPos ) )
			{
				//��� ��������
				return bRet;
			}
			
			SetBarExpandState( i, !bar.bExpandState, true );

			return bRet;
		}
		nV += nBarHeight;
		
		if ( bar.bExpandState )
		{
			int nTempItem = 0;
			CWindowList::iterator it = bar.items.begin();
			for ( ; it!=bar.items.end(); ++it )
			{
				CVec2 vSize;
				(*it)->GetWindowPlacement( 0, &vSize, 0 );
				if ( nV <= fY && nV + vSize.y + nVSubSpace*2 > fY )
					break;
				nV += vSize.y + nVSubSpace*2;
				nTempItem++;
			}

			if ( it != bar.items.end() )
			{
				//����� ������ � ������� ��������
				(*it)->OnLButtonDown( vPos, mouseState );

				
				if ( !(*it)->IsWindowEnabled() )
				{
					return bRet;		//���������� ������, �� ��������
				}

				nSelBar = i;
				nSelItem = nTempItem;

				//�������� ������ ��������� �� ��������� selection state
				SUIMessage msg;
				msg.nMessageCode = UI_NOTIFY_SELECTION_CHANGED;
				msg.nFirst = GetWindowID();
				msg.nSecond = nSelBar;
				GetParent()->ProcessMessage( msg );
				return bRet;
			}
		}
	}

//	NI_ASSERT_T( nSelBar != -1, "CUIShortcutBar::OnLButtonDown() error: Can not find selected bar" )
	return bRet;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIShortcutBar::OnLButtonUp( const CVec2 &vPos, EMouseState mouseState )
{
	if ( pScrollBar->IsInside( vPos ) )
		return CMultipleWindow::OnLButtonUp( vPos, mouseState );

	float fX = vPos.x - wndRect.x1 - nLeftSpace;
	if ( fX < 0 || fX > wndRect.Width() - nRightSpace - nScrollBarWidth )
		return true;		//��� ������� selection
	
	float fY = vPos.y - wndRect.y1 - nTopSpace;
	if ( fY < 0 || fY > wndRect.Height() - nTopSpace - nBottomSpace )
		return true;		//��� ������� selection
	
	//��������, ����� ����� �������� ������ ������ ��� ������������ bar, ����� ����� bar ���� �������� ��������� PUSHED
	if ( nSelBar != -1 )
	{
		IUIElement *pBar = GetBar( nSelBar );
		if ( pBar && pBar->IsInside( vPos ) )
		{
			CSimpleWindow *pWindow = dynamic_cast<CSimpleWindow *> ( pBar );
			pWindow->nCurrentSubState = E_HIGHLIGHTED_STATE;
			return true;
		}
	}
	return CMultipleWindow::OnLButtonUp( vPos, mouseState );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IUIElement* CUIShortcutBar::GetBar( int nBar )
{
	if ( nBar >= bars.size() )
		return 0;
	
	return bars[nBar].pElement;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CUIShortcutBar::GetNumberOfItems( int nBar )
{
	if ( nBar >= bars.size() )
		return 0;

	return bars[nBar].items.size();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IUIElement* CUIShortcutBar::GetItem( int nBar, int nItem )
{
	if ( nBar >= bars.size() )
		return 0;

	SBar &bar = bars[nBar];
	if ( bar.items.size() < nItem )
		return 0;
	int i = 0;
	for ( CWindowList::iterator it=bar.items.begin(); it!=bar.items.end(); ++it )
	{
		if ( i == nItem )
			return *it;
		i++;
	}

	NI_ASSERT( 0 );
	return 0;		//WTF
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIShortcutBar::Clear()
{
	bars.clear();
	RemoveAllChildren();
	AddChild( pScrollBar.GetPtr() );
	nSelBar = nSelItem = -1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIShortcutBar::SetBarExpandState( int nBar, bool bExpand, const bool bNotify )
{
	if ( nBar >= bars.size() )
		return;

	SBar &bar = bars[nBar];
	
	if ( bNotify )
	{
		IUIElement * pItem = GetItem( nBar, 0 );
		if ( !bExpand || !pItem || !pItem->IsWindowEnabled() )
			nSelItem = -1;
		else
			nSelItem  = 0;

		nSelBar = nBar;
		bar.bExpandState = bExpand;
		InitialUpdate();
		
		//������� ������ �������� ������
		/*NI_ASSERT_T( pPushed != 0, "Error in CUIShortcutBar, plz tell me, Slavik :)" );
		// tlling slavik: blya, gde ti nahuy
		if ( pPushed )
		{
			pPushed->SetState( pPushed->GetState() + 1 );
			pPushed = 0;
		}*/

		//�������� ������ ��������� �� expand state
		SUIMessage msg;
		msg.nMessageCode = UI_NOTIFY_BAR_EXPAND;
		msg.nFirst = GetWindowID();
		msg.nSecond = bar.bExpandState;
		GetParent()->ProcessMessage( msg );

		msg.nMessageCode = UI_NOTIFY_SELECTION_CHANGED;
		msg.nFirst = GetWindowID();
		msg.nSecond = nSelBar;
		GetParent()->ProcessMessage( msg );
	}
	else
	{
		bar.bExpandState = bExpand;
		InitialUpdate();
	}

	bar.pElement->SetState( bExpand );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIShortcutBar::SetSelectionItem( int nBar, int nItem )
{
	if ( nBar != -1 )
	{
		NI_ASSERT_T( nBar < bars.size(), "Invalid selection bar index" );
		nSelBar = nBar;
	}
	if ( nItem != -1 )
	{
		SBar &bar = bars[nBar];
		NI_ASSERT_T( nItem < bar.items.size(), "Invalid selection item index" );
		nSelItem = nItem;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIShortcutBar::OnMouseWheel( const CVec2 &vPos, EMouseState mouseState, float fDelta )
{
	if ( !IsInside( vPos ) )
		return false;
	
	if ( !pScrollBar )
		return false;
	
	pScrollBar->SetPosition( pScrollBar->GetPosition() + fDelta*GetMouseWheelMultiplyer() );
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
