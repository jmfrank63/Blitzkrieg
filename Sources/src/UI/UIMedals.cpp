#include "StdAfx.h"
#include "..\GameTT\iMission.h"
#include "..\Main\GameStats.h"
#include "UIMessages.h"
#include "UIMedals.h"

const int LEFT_SPACE = 3;				//������ ����� �� ���� ��������
const int TOP_SPACE = 2;				//������ ������ � ����� �� ���� ��������
const int MULT = 20;						//�� ������� ������������ ScrollBar ��� ������� �� ������
const int MEDAL = 10;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIMedals::UpdatePositions()
{
	int nY = -pScrollBar->GetPosition();

	//��������� ������������ ������� ��� ������ ������
	for ( int i=0; i<nMedalsCount; i++ )
	{
		//������
		CUIStaticBridge *pStatic = checked_cast<CUIStaticBridge *> ( GetChildByID( MEDAL + i * 2 ) );
		NI_ASSERT( pStatic != 0 );
		
		if ( nY + pStatic->vPos.y + pStatic->vSize.y <= nSpace || nY + pStatic->vPos.y >= wndRect.Height() - nSpace )
		{
			//������ ������ ���������
			pStatic->ShowWindow( UI_SW_HIDE );
		}
		else
		{
			pStatic->ShowWindow( UI_SW_SHOW );
			float fTextureSizeY = pStatic->states[0].subStates[0].pTexture->GetSizeY( 0 );
			CTRect<float> mapa = medalMaps[ i ];
			CTRect<float> rect;
			rect.x1 = pStatic->vPos.x + wndRect.x1;
			rect.y1 = pStatic->vPos.y + wndRect.y1 + nY;
			rect.x2 = rect.x1 + pStatic->vSize.x;
			rect.y2 = rect.y1 + pStatic->vSize.y;
			
			//��������, ����� ���� ����� ������ ����� ����
			if ( rect.y1 < wndRect.y1 + nSpace )
			{
				//������� ���� �������
				mapa.y1 += ( wndRect.y1 + nSpace - rect.y1 ) / fTextureSizeY;
				rect.y1 = wndRect.y1 + nSpace;
			}
			else if ( rect.y2 > wndRect.y2 - nSpace )
			{
				//������ ���� �������
				mapa.y2 -= ( rect.y2 - wndRect.y2 + nSpace ) / fTextureSizeY;
				rect.y2 = wndRect.y2 - nSpace;
			}

			for ( int k=0; k<3; k++ )
			{
				pStatic->states[0].subStates[k].subRects[0].mapa = mapa;
//				pStatic->states[0].subStates[k].subRects[0].rc = rect;
			}
			pStatic->wndRect = rect;
			pStatic->UpdateSubRects();
		}

		//����� ��� �������
		pStatic = checked_cast<CUIStaticBridge *> ( GetChildByID( MEDAL + i * 2 + 1 ) );
		NI_ASSERT( pStatic != 0 );
		if ( nY + pStatic->vPos.y + pStatic->vSize.y <= nSpace || nY + pStatic->vPos.y >= wndRect.Height() - nSpace )
		{
			//������ ������ ���������
			pStatic->ShowWindow( UI_SW_HIDE );
		}
		else
		{
			pStatic->ShowWindow( UI_SW_SHOW );
			CTRect<float> rect;
			rect.x1 = pStatic->vPos.x + wndRect.x1;
			rect.y1 = pStatic->vPos.y + wndRect.y1 + nY;
			rect.x2 = rect.x1 + pStatic->vSize.x;
			rect.y2 = rect.y1 + pStatic->vSize.y;
			
			/*
			//��������, ����� ���� ����� ������ ����� ����
			if ( rect.y1 < wndRect.y1 + nSpace )
			{
				//������� ���� �������
				mapa.y1 += ( wndRect.y1 + nSpace - rect.y1 ) / fTextureSizeY;
				rect.y1 = wndRect.y1 + nSpace;
			}
			else if ( rect.y2 > wndRect.y2 - nSpace )
			{
				//������ ���� �������
				mapa.y2 -= ( rect.y2 - wndRect.y2 ) / fTextureSizeY;
				rect.y2 = wndRect.y2 - nSpace;
			}
			
			for ( int k=0; k<3; k++ )
			{
				pStatic->states[0].subStates[k].subRects[0].mapa = mapa;
				//				pStatic->states[0].subStates[k].subRects[0].rc = rect;
			}
			*/
			pStatic->wndRect = rect;
			//pStatic->UpdateSubRects();
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIMedals::ComputeHPositions()
{
	int nX = nHSubSpace + nSpace;
	int nY = nSpace;
	CTRect<float> rect = wndRect;
	rect.top = wndRect.top;
	rect.bottom = rect.top + 10;	//10 - temp

	//��������� ������� ��� ������ ������
	int nMaxY = 0;
	for ( int i=0; i<nMedalsCount; i++ )
	{
		//������
		CUIStaticBridge *pStatic = checked_cast<CUIStaticBridge *> ( GetChildByID( MEDAL + i * 2 ) );
		NI_ASSERT( pStatic != 0 );
		if ( nX + pStatic->vSize.x + nHSubSpace + nSpace + wndRect.x1 > wndRect.x2 )
		{
			nX = nHSubSpace + nSpace;
			nY = nMaxY + nVSubSpace;
			nMaxY = 0;
		}
		pStatic->vPos.x = nX;		//������� �� ���������� � ����� ������������ ��� ��������������, � rect ����� ����������
		pStatic->vPos.y = nY;
		rect.x1 = wndRect.x1 + nX;
		rect.x2 = rect.x1 + pStatic->vSize.x;
		rect.y1 = wndRect.y1 + nY;
		rect.y2 = rect.y1 + pStatic->vSize.y;
		pStatic->wndRect = rect;
		nX = rect.x2 + nHSubSpace;
		
		//����� ��� �������
		pStatic = checked_cast<CUIStaticBridge *> ( GetChildByID( MEDAL + i * 2 + 1 ) );
		NI_ASSERT( pStatic != 0 );
		float fTemp = ( rect.x2 - rect.x1 - pStatic->vSize.x ) / 2;
		rect.x1 += fTemp;
		rect.x2 -= fTemp;
		rect.y1 = rect.y2 + nVTextSpace;
		rect.y2 = rect.y1 + pStatic->vSize.y;
		pStatic->wndRect = rect;
		pStatic->vPos.x = rect.x1 - wndRect.x1;		//������� �� ���������� � ����� ������������ ��� ��������������, � rect ����� ����������
		pStatic->vPos.y = rect.y1 - wndRect.y1;
		
		if ( rect.y2 - wndRect.y1 > nMaxY )
			nMaxY = rect.y2 - wndRect.y1;
	}

	pScrollBar->SetMinValue( 0 );
	int nTemp = nY - wndRect.Height() + nSpace;
	if ( nTemp > 0 )
		pScrollBar->SetMaxValue( nTemp );
	else
		pScrollBar->SetMaxValue( 0 );
	pScrollBar->SetStep( 1 );
	pScrollBar->SetButtonStep( MULT );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIMedals::UpdateScrollbar()
{
	//��������, ������� �� ScrollBar
	//���������� ����� ������ �������

#ifdef OLD
	int nSum = 0;
	int i = 0;
	for ( CTextList::iterator it=textes.begin(); it!=textes.end(); ++it )
	{
		if ( !it->pGfxText )
			continue;
		
		int nHeight = it->pGfxText->GetNumLines();				//����� ����� � ������ ������
		nHeight *= it->pGfxText->GetLineSpace();					//������ ����� �������
		//������ � nHeight ���������� ������ ������� ������ � ��������
		nSum += nHeight;
		i++;
		if (!( i & 0x01 ))
			nSum += nSubSpace;
	}


	if ( nSum <= wndRect.Height() - TOP_SPACE * 2 )
	{
		/*
		pScrollBar->ShowWindow( UI_SW_HIDE );
		return;
		*/
		//��������� ������ ������ �������
		pScrollBar->SetMinValue( 0 );
		pScrollBar->SetMaxValue( 0 );
	}


	//ScrollBar �������
	//������ ������������ ������ ������
	nSum = 0;
	i = 0;
	for ( CTextList::iterator it=textes.begin(); it!=textes.end(); ++it )
	{
		if ( !it->pGfxText )
			continue;
		it->pGfxText->SetWidth( wndRect.Width() - nSpace*2 - pScrollBar->GetSize().x );
		
		int nHeight = it->pGfxText->GetNumLines();				//����� ����� � ������ ������
		nHeight *= it->pGfxText->GetLineSpace();					//������ ����� �������
		//������ � nHeight ���������� ������ ������� ������ � ��������
		nSum += nHeight;
		i++;
		if (!( i & 0x01 ))
			nSum += nSubSpace;
	}

	pScrollBar->ShowWindow( UI_SW_SHOW );
	pScrollBar->SetMinValue( 0 );
	int nTemp = nSum - wndRect.Height() + TOP_SPACE * 2;
	if ( nTemp > 0 )
		pScrollBar->SetMaxValue( nTemp );
	else
		pScrollBar->SetMaxValue( 0 );
	pScrollBar->SetStep( 1 );
	pScrollBar->SetButtonStep( MULT );
#endif		//OLD
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIMedals::Visit( interface ISceneVisitor *pVisitor )
{
	if ( !nCmdShow )
		return;
	CSimpleWindow::Visit( pVisitor );

	// ������ �����
	for ( CWindowList::reverse_iterator ri = childList.rbegin(); ri != childList.rend(); ++ri )
	{
		CSimpleWindow *pWindow = dynamic_cast<CSimpleWindow *> ( ri->GetPtr() );
		if ( !pWindow->IsVisible() )
			continue;

		const int nCurId = pWindow->GetWindowID();
		if ( nCurId < 10 || !(nCurId & 0x01) )
			(*ri)->Visit( pVisitor );
		else
		{
			//������ �����
			if ( !pWindow->states[pWindow->nCurrentState].pGfxText )
				continue;

			CTRect<float> textRC = wndRect;
			textRC.y1 += nSpace;
			textRC.y2 -= nSpace;
			textRC.x1 = pWindow->wndRect.x1;
			textRC.x2 = pWindow->wndRect.x2;
			const int nY = pWindow->wndRect.y1 - wndRect.y1;

			pVisitor->VisitUIText( pWindow->states[pWindow->nCurrentState].pGfxText, textRC, nY, 0, FNT_FORMAT_LEFT );
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIMedals::Draw( IGFX *pGFX )
{
	NI_ASSERT_SLOW_T( false, "Can't user Draw() directly - use visitor pattern" );
	return;

	if ( !nCmdShow )
		return;
	CSimpleWindow::Draw( pGFX );

	//������ �����
	pGFX->SetShadingEffect( 3 );
	for ( CWindowList::reverse_iterator ri=childList.rbegin(); ri!=childList.rend(); ri++ )
	{
		CSimpleWindow *pWindow = dynamic_cast<CSimpleWindow *> ( ri->GetPtr() );
		if ( !pWindow->IsVisible() )
			continue;

		int nCurId = pWindow->GetWindowID();
		if ( nCurId < 10 || !(nCurId & 0x01) )
			(*ri)->Draw( pGFX );
		else
		{
			//������ �����
			if ( !pWindow->states[pWindow->nCurrentState].pGfxText )
				continue;

			CTRect<float> textRC = wndRect;
			textRC.y1 += nSpace;
			textRC.y2 -= nSpace;
			textRC.x1 = pWindow->wndRect.x1;
			textRC.x2 = pWindow->wndRect.x2;
			int nY = pWindow->wndRect.y1 - wndRect.y1;

			pGFX->DrawText( pWindow->states[pWindow->nCurrentState].pGfxText, textRC, nY, FNT_FORMAT_LEFT );
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CUIMedals::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.AddTypedSuper( static_cast<CMultipleWindow*>(this) );
	saver.Add( "LeftRightSpace", &nSpace );
	saver.Add( "HSubSpace", &nHSubSpace );
	saver.Add( "VSubSpace", &nVSubSpace );
	saver.Add( "VTextSpace", &nVTextSpace );
	
	if ( saver.IsReading() )
	{
		//�������������� pScrollBar
		pScrollBar = dynamic_cast<CUIScrollBar *>( GetChildByID(1) );
		NI_ASSERT_T( pScrollBar != 0, "Can't find scroll bar" );
	}
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CUIMedals::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.AddTypedSuper( 1, static_cast<CMultipleWindow*>(this) );
	
	if ( !saver.IsReading() )
	{
		CPtr<IUIElement> pElement = dynamic_cast<IUIElement *> ( pScrollBar );
		saver.Add( 3, &pElement );
	}
	else
	{
		CPtr<IUIElement> pElement;
		saver.Add( 3, &pElement );
		pScrollBar = dynamic_cast<CUIScrollBar *> ( pElement.GetPtr() );
		NI_ASSERT_T( pScrollBar != 0, "No ScrollBar control, can not create CUIMedals" );
	}
	
	saver.Add( 4, &nSpace );
	saver.Add( 5, &nHSubSpace );
	saver.Add( 6, &nVSubSpace );
	saver.Add( 7, &nNextPosX );
	saver.Add( 8, &nNextPosY );
	saver.Add( 9, &nMedalsCount );
	saver.Add( 10, &medalMaps );
	saver.Add( 11, &nVTextSpace );
	
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIMedals::InitMaps()
{
	medalMaps.clear();
	for ( int i=0; i<nMedalsCount; i++ )
	{
		//������
		CUIStaticBridge *pStatic = checked_cast<CUIStaticBridge *> ( GetChildByID( MEDAL + i * 2 ) );
		NI_ASSERT( pStatic != 0 );
		medalMaps.push_back( pStatic->states[0].subStates[0].subRects[0].mapa );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIMedals::Reposition( const CTRect<float> &rcParent )
{
	//������� ������� ����������
	CVec2 size = pScrollBar->GetSize();
	pScrollBar->SetPos( CVec2(size.x, 0) );
	pScrollBar->SetSize( CVec2(size.x, GetSize().y ) );
	CMultipleWindow::Reposition( rcParent );
	/*
	//������� �����
	IText *pText = states[0].pGfxText->GetText();
	SetWindowText( 0, pText->GetString() );
	*/
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIMedals::ProcessMessage( const SUIMessage &msg )
{
	//Scroll Text ������������ NOTIFY ��������� �� ScrollBar
	switch( msg.nMessageCode )
	{
	case UI_NOTIFY_POSITION_CHANGED:
		// �������� �����
		UpdatePositions();
		return true;
	case UI_NOTIFY_WINDOW_CLICKED:
		if ( msg.nFirst >= 10 )
		{
			SUIMessage newMsg;
			newMsg.nMessageCode = MC_MEDAL_CLICKED | PROCESSED_FLAG;
			newMsg.nFirst = (msg.nFirst - 10)/2;
			GetParent()->ProcessMessage( newMsg );
		}
		return true;
	}
	
	return CMultipleWindow::ProcessMessage( msg );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIMedals::ClearMedals()
{
	CObj<IUIElement> pSave = GetChildByID( 1 );		//��� Scroll Bar
	RemoveAllChildren();
	AddChild( pSave );
	nMedalsCount = 0;
	nNextPosX = nSpace + nHSubSpace;
	nNextPosY = nVSubSpace;
	pScrollBar->SetPosition( 0 );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIMedals::AddMedal( IGFXTexture *pTexture, const CTRect<float> &mapImageRect, const WORD *pszMedalsName )
{
	IObjectFactory *pFactory = GetCommonFactory();
	CPtr<CUIStaticBridge> pMedal = dynamic_cast<CUIStaticBridge*>( pFactory->CreateObject( UI_STATIC ) );
	pMedal->SetShowBackgroundFlag( true );
	SWindowSubRect subRect;
	subRect.mapa.x1 = 0;
	subRect.mapa.y1 = 0;
	subRect.mapa.x2 = mapImageRect.x2;
	subRect.mapa.y2 = mapImageRect.y2;
	subRect.rc.x1 = 0;
	subRect.rc.y1 = 0;
	subRect.rc.x2 = mapImageRect.x1;
	subRect.rc.y2 = mapImageRect.y1;
	for ( int i=0; i<4; i++ )
	{
		pMedal->states[0].subStates[i].pTexture = pTexture;
		pMedal->states[0].subStates[i].subRects.push_back( subRect );
	}
	pMedal->vPos = VNULL2;
	pMedal->vSize = CVec2( mapImageRect.x1, mapImageRect.y1 );
	pMedal->SetWindowID( 2 * nMedalsCount + MEDAL );			//������ ID - ����������� �������
	AddChild( pMedal );
	
	pMedal = dynamic_cast<CUIStaticBridge*>( pFactory->CreateObject( UI_STATIC ) );
	pMedal->InitText();
	pMedal->SetWindowText( 0, pszMedalsName );
	pMedal->vPos = VNULL2;
	pMedal->vSize.x = pMedal->states[0].pGfxText->GetWidth();
	pMedal->vSize.y = pMedal->states[0].pGfxText->GetLineSpace();
	pMedal->SetWindowID( 2 * nMedalsCount + 1 + MEDAL );	//�������� ID - �����
	AddChild( pMedal );
	
	nMedalsCount++;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIMedals::UpdateMedals()
{
	InitMaps();
	UpdateScrollbar();
	ComputeHPositions();
	UpdatePositions();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIMedals::ShowWindow( int _nCmdShow )
{
	CSimpleWindow::ShowWindow( _nCmdShow );
	if ( !_nCmdShow )
		return;

	//��������� ��������
	ClearMedals();
	const SMedalStats *pStats = NGDB::GetGameStats<SMedalStats>( "medals\\1\\1", IObjectsDB::MEDAL );
/*	SMedalStats stats;
	
	CPtr<IDataStorage> pStorage = GetSingleton<IDataStorage>();
	CPtr<IDataStream> pStream = pStorage->OpenStream( "medals\\1\\1", STREAM_ACCESS_READ );
	NI_ASSERT( pStream != 0 );
	CPtr<IDataTree> pDT = CreateDataTreeSaver( pStream, IDataTree::READ );
	NI_ASSERT( pDT != 0 );
	stats.operator &( *pDT );
*/

	NI_ASSERT( pStats != 0 );
	ITextureManager *pTM = GetSingleton<ITextureManager>();
	ITextManager *pTextM = GetSingleton<ITextManager>();

	for ( int i=0; i<10; i++ )
	{
//		std::string szTemp = pStats->szHeaderText;
//		szTemp += ".txt";
		CPtr<IText> p1 = pTextM->GetDialog( pStats->szHeaderText.c_str() );
		IGFXTexture *pTexture = pTM->GetTexture( pStats->szTexture.c_str() );
		AddMedal( pTexture, pStats->mapImageRect, p1->GetString() );
	}

	UpdateMedals();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
