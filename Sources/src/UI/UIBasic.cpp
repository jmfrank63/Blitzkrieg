#include "StdAfx.h"
#include "..\Main\TextSystem.h"
#include "UIBasic.h"
#include "UIBasicM.h"
#include "UIMessages.h"
#include "..\Scene\Scene.h"
#include "..\Input\Input.h"
#include "..\GameTT\CommonID.h"
#include "..\GameTT\MessageReaction.h"
#include "..\Common\PauseGame.h"
#include "..\Main\ScenarioTracker.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CMultipleWindow::CMessageList CMultipleWindow::staticMessageList;
CMultipleWindow::CLuaValues CMultipleWindow::staticLuaValues;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IManipulator *CSimpleWindow::GetManipulator()
{
	if ( !pManipulator )
	{
		CUIWindowManipulator *pWindowManipulator = new CUIWindowManipulator;
		pWindowManipulator->SetWindow( this );
		pManipulator = pWindowManipulator;
	}
	
	return pManipulator;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSimpleWindow::GetTextSize( const int nState, int *pSizeX, int *pSizeY ) const
{
	NI_ASSERT_T( states.size() > nState, NStr::Format( "wrong state number %d", nState ) );
	if ( states[nState].pGfxText )
	{
		if ( pSizeY )
			*pSizeY = states[nState].pGfxText->GetNumLines() * states[nState].pGfxText->GetLineSpace();
		if ( pSizeX )
		{
			//*pSizeX = states[nState].pGfxText->GetWidth();
			*pSizeX = vSize.x;
		}
	}
	else
	{
		if ( pSizeX )
			*pSizeX = 0;
		if ( pSizeY )
			*pSizeY = 0;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSimpleWindow::CopyInternals( CSimpleWindow * pWnd )
{
	//*pWnd = *this;
	pWnd->wndRect = wndRect;							//���������� ������ ������������ ������
	pWnd->nPositionFlag = nPositionFlag;									//������ ����� ��������
	pWnd->vPos = vPos;													//���������� ����� ������� ����� ������ ������������ ��������� ����� ��������
	pWnd->vSize = vSize;												//������� ������
	pWnd->nID = nID;														//���������� ������������� ������
	pWnd->pParent = pParent;					//��������
	pWnd->bWindowActive = bWindowActive;									//������� �� ����				//??
	pWnd->nCmdShow = nCmdShow;												//������ ����������� ����
	
	pWnd->nCurrentState = nCurrentState;									//������� ���������
	pWnd->nCurrentSubState = nCurrentSubState;								//������� ������������ ������: NORMAL, HIGHLIGHTED, PUSHED
	pWnd->bShowBackground = bShowBackground;								//���������� ��� ��� �������� ( ����� ����� ��� ������ ������ � ������� )
	
	pWnd->szHighSound = szHighSound;						//����, ������������� ����� ����� ��������� �� �������, �������� ��� ������ ���� ������ ��� ������ state, ��
	
	pWnd->nTextAlign = nTextAlign;
	pWnd->dwTextColor = dwTextColor;
	pWnd->vShiftText = vShiftText;
	pWnd->nFontSize = nFontSize;
	pWnd->vTextPos = vTextPos;
	pWnd->bRedLine = bRedLine;
	pWnd->bSingleLine = bSingleLine;

	pWnd->dwShadowColor = dwShadowColor;
	pWnd->vShadowShift = vShadowShift;
	pWnd->szToolKey = szToolKey;
	
	pWnd->rcBound = rcBound;
	pWnd->bBounded = bBounded;
	
	pWnd->nBlink = nBlink;
	pWnd->dwLastBlinkTime = dwLastBlinkTime;
	pWnd->dwCurrentBlinkColor = dwCurrentBlinkColor;
	pWnd->bBlinking = bBlinking;											//���� true �� ������ �������� ������
	pWnd->dwBlinkTime = dwBlinkTime;
	pWnd->nBlinkColorIndex = nBlinkColorIndex;								// color number (for blinking)


	pWnd->states.resize( states.size() );
	for ( int i = 0; i < states.size(); ++i )
		states[i].CopyInternals( &pWnd->states[i] );

	pWnd->InitDependentInfo();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CSimpleWindow::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.Add( "WindowPos", &vPos );
	saver.Add( "WindowSize", &vSize );
	saver.Add( "PositionFlag", &nPositionFlag );
	saver.Add( "ElementID", &nID );
	saver.Add( "VisibleFlag", &nCmdShow );
	saver.Add( "ActiveFlag", &bWindowActive );
	saver.Add( "TextColor", &dwTextColor );
	saver.Add( "States", &states );
	saver.Add( "ShadowColor", &dwShadowColor );
	saver.Add( "ShadowShift", &vShadowShift );
	saver.Add( "ShiftText", &vShiftText );
	
	if ( saver.IsReading() )
	{
		for ( int i = 0; i < states.size(); i++ )
		{
			for ( int a = 0; a < 3; a++ )
			{
				if ( states[i].subStates[a].textColor == 0 )
					states[i].subStates[a].textColor = dwTextColor;
			}
			if ( states[i].subStates[3].textColor == 0 )
				states[i].subStates[3].textColor = 0xffa0a0a0;
		}

		if ( states.size() == 0 )
		{
			states.resize( 1 );
		}
	}

	saver.Add( "CurrentState", &nCurrentState );
	saver.Add( "Background", &bShowBackground );
	saver.Add( "TextAlign", &nTextAlign );
	saver.Add( "FontSize", &nFontSize );
	saver.Add( "TextPos", &vTextPos );
	saver.Add( "BoundFlag", &bBounded );
	saver.Add( "BoundRect", &rcBound );
	saver.Add( "RedLine", &bRedLine );
	saver.Add( "SingleLine", &bSingleLine );
	saver.Add( "Blink", &nBlink );

	saver.Add( "ToolTip", &szToolKey );
		
	if ( saver.IsReading() )
	{
		bool bDisabled = false;
		saver.Add( "Disabled", &bDisabled );
		if ( bDisabled )
			EnableWindow( false );
	}
	else
	{
		bool bDisabled = ( nCurrentSubState == E_DISABLED_STATE );
		saver.Add( "Disabled", &bDisabled );
	}
	
	if ( saver.IsReading() )
	{
		InitDependentInfo();
	}

	//CRAP
	CTRect<float> rc;
	rc.left = rc.top = 0.0f;
	rc.right = vSize.x;
	rc.bottom = vSize.y;
	for ( int i = 0; i < states.size(); i++ )
	{
		for ( int k = 0; k < 4; k++ )
		{
			CUIWindowSubState &subState = states[i].subStates[k];
			for ( int a=0; a<subState.subRects.size(); a++ )
			{
				if ( subState.subRects[a].rc.x1 == -1 && subState.subRects[a].rc.x2 == -1 )
					subState.subRects[a].rc = rc;
			}
		}
	}
	//end of CRAP

	saver.Add( "HighSound", &szHighSound );
	NI_ASSERT_T( states.size() > 0, "Error: window states size is zero" );
	
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSimpleWindow::InitDependentInfo()
{
		std::string szFontName;
		switch ( nFontSize )
		{
		case 0:
			szFontName = "fonts\\small";
			break;
		case 1:
			szFontName = "fonts\\medium";
			break;
		case 2:
			szFontName = "fonts\\large";
			break;
		}

		IGFXFont *pFont = GetSingleton<IFontManager>()->GetFont( szFontName.c_str() );
		for ( int i = 0; i < states.size(); i++ )
		{
			if ( states[i].pGfxText )
			{
				states[i].pGfxText->SetColor( dwTextColor );
				states[i].pGfxText->SetFont( pFont );
				states[i].pGfxText->EnableRedLine( bRedLine );
			}
		}

		//��������� ������� ��� ���������� states
		for ( int i = 0; i < states.size(); i++ )
		{
			if ( states[i].szToolKey.size() == 0 )
				states[i].szToolKey = szToolKey;		//������ tooltip
			if ( states[i].szToolKey.size() > 0 )
				states[i].pToolText = GetSingleton<ITextManager>()->GetString( states[i].szToolKey.c_str() );
			else
				states[i].pToolText = CreateObject<ITextString>( TEXT_STRING );
		}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSimpleWindow::UpdateSubRects()
{
	CTRect<float> rc;
	rc.left = rc.top = 0.0f;
	rc.right = wndRect.Width();
	rc.bottom = wndRect.Height();
	for ( int i=0; i<states.size(); i++ )
	{
		for ( int k=0; k<4; k++ )
		{
			CUIWindowSubState &subState = states[i].subStates[k];
			for ( int a=0; a<subState.subRects.size(); a++ )
			{
				subState.subRects[a].rc = rc;
			}
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CSimpleWindow::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &vPos );
	saver.Add( 2, &vSize );
	saver.Add( 3, &nPositionFlag );
	saver.Add( 4, &nID );
	saver.Add( 5, &nCmdShow );
	saver.Add( 6, &bWindowActive );
	saver.Add( 7, &states );
	saver.Add( 8, &nCurrentState );
	saver.Add( 9, &nCurrentSubState );
	saver.Add( 10, &bShowBackground );
	saver.Add( 11, &nTextAlign );
	saver.Add( 12, &dwTextColor );
	saver.Add( 14, &nFontSize );
	//saver.Add( 15, &pHighSound );
	saver.Add( 15, &szHighSound );
	saver.Add( 16, &wndRect );
	saver.Add( 18, &szToolKey );
//	saver.Add( 19, &pToolText );
	//20 is empty for saves compatibility
	saver.Add( 21, &bBounded );
	saver.Add( 22, &rcBound );
	saver.Add( 23, &vTextPos );
	saver.Add( 24, &bRedLine );
	saver.Add( 25, &dwShadowColor );
	saver.Add( 26, &vShadowShift );
	saver.Add( 27, &vShiftText );
	saver.Add( 28, &nBlink );
	saver.Add( 29, &dwLastBlinkTime );
	saver.Add( 30, &dwCurrentBlinkColor );
	saver.Add( 31, &bBlinking );
	saver.Add( 32, &bSingleLine );
	saver.Add( 33, &dwBlinkTime );
	saver.Add( 34, &nBlinkColorIndex );
	
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSimpleWindow::IsInside( const CVec2 &vPos )
{
	const CUIWindowSubState &subState = states[nCurrentState].subStates[nCurrentSubState];
	IUIMask *pCurrentMask = subState.pMask;
	if ( !pCurrentMask )
		return wndRect.IsInside( vPos );
	
	if ( !wndRect.IsInside( vPos ) )
		return false;			//��� �������������� ������
	
	//����������� �������� ���������� � ���������� �����
	int nSizeX = subState.pTexture->GetSizeX( 0 );
	int nSizeY = subState.pTexture->GetSizeY( 0 );
	
	CTRect<int> rc;
	rc.x1 = subState.subRects[0].mapa.x1 * nSizeX - 0.5f;
	rc.x2 = subState.subRects[0].mapa.x2 * nSizeX - 0.5f - rc.x1;
	rc.y1 = subState.subRects[0].mapa.y1 * nSizeY - 0.5f;
	rc.y2 = subState.subRects[0].mapa.y2 * nSizeY - 0.5f - rc.y1;
	
	const CArray2D<BYTE> *pMask = pCurrentMask->GetMask();
	int nx = (float) (vPos.x-wndRect.x1)/(wndRect.Width())*rc.x2 + rc.x1;
	int ny = (float) (vPos.y-wndRect.y1)/(wndRect.Height())*rc.y2 + rc.y1;
	
	const BYTE *pBuf = ((CArray2D<BYTE> *)pMask)->GetBuffer();
	if ( pBuf[ny*nSizeX + nx] )
		return true;
	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSimpleWindow::SetWindowTexture( IGFXTexture *pTexture )
{
	for ( int i=0; i<4; i++ )
		states[0].subStates[i].pTexture = pTexture;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IGFXTexture* CSimpleWindow::GetWindowTexture()
{
	NI_ASSERT_T( !states.empty(), "Empty UI states" );
	return states[0].subStates[0].pTexture;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSimpleWindow::SetWindowMap( const CTRect<float> &maps )
{
	for ( int i=0; i<4; i++ )
	{
		CUIWindowSubState &current = states[0].subStates[i];
		for ( int a=0; a<current.subRects.size(); a++ )
		{
			current.subRects[a].mapa = maps;
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSimpleWindow::SetWindowPlacement( const CVec2 *_vPos, const CVec2 *_vSize )
{
	if ( _vPos != 0 )
		vPos = *_vPos;
	if ( _vSize != 0 )
	{
		//{�� ����� ��� ����� �����???
		vSize = *_vSize;
		CTRect<float> rc;
		rc.left = rc.top = 0.0f;
		rc.right = vSize.x;
		rc.bottom = vSize.y;
		for ( int i=0; i<states.size(); i++ )
		{
			for ( int k=0; k<4; k++ )
			{
				CUIWindowSubState &subState = states[i].subStates[k];
				if ( subState.subRects.size() == 1 )
				{
					for ( int a=0; a<subState.subRects.size(); a++ )
					{
						subState.subRects[a].rc = rc;
					}
				}
			}
		}
		//}

		
		// updater text sizes		
		for ( int i=0; i<states.size(); i++ )
		{
			if ( states[i].pGfxText )
				states[i].pGfxText->SetWidth( vSize.x );
		}

	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSimpleWindow::SetWindowID( int _nID )
{
	nID = _nID;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IText* CSimpleWindow::GetHelpContext( const CVec2 &vPos, CTRect<float> *pRect )
{
	if ( !IsVisible() || !states[nCurrentState].pToolText )
		return 0;

	const std::wstring szTT = reinterpret_cast<const wchar_t*>(states[nCurrentState].pToolText->GetString());
	if ( szTT.empty() )
		return 0;

	if ( pRect )
		*pRect = wndRect;
	return states[nCurrentState].pToolText;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSimpleWindow::SetHelpContext( int nState, const WORD *pszToolTipText )
{
	states[nState].pToolText->SetText( pszToolTipText );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSimpleWindow::InitText()
{
	CTRect<float> textRC = wndRect;
	if ( bBounded )
	{
		textRC.y1 = rcBound.y1;
		textRC.y2 = rcBound.y2;
	}

	for ( int i=0; i<states.size(); i++ )
	{
		if ( !states[i].pGfxText )
		{
			states[i].pGfxText = CreateObject<IGFXText>( GFX_TEXT );
			states[i].pGfxText->EnableRedLine( bRedLine );
		}
		
		IText *pText = states[i].pGfxText->GetText();
		if ( !pText )
		{
			pText = CreateObject<IText>( TEXT_STRING );
			states[i].pGfxText->SetText( pText );
		}
		
		IGFXFont *pFont = GetSingleton<IFontManager>()->GetFont( "fonts\\medium" );
		states[i].pGfxText->SetFont( pFont );
		states[i].pGfxText->SetWidth( textRC.Width() );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSimpleWindow::SetWindowText( int nState, const WORD *pszText )
{
	CTRect<float> textRC = wndRect;
	if ( bBounded )
	{
		textRC.y1 = rcBound.y1;
		textRC.y2 = rcBound.y2;
	}

	if ( nState == -1 )
	{
		for ( int i=0; i<states.size(); i++ )
		{
			IText *pText = states[i].pGfxText->GetText();
			pText->SetText( pszText );
			states[i].pGfxText->SetText( pText );
			states[i].pGfxText->SetWidth( textRC.Width() );
		}
		return;
	}

	NI_ASSERT_T( nState < states.size(), NStr::Format("Can't set text for state %d (max %d states)", nState, states.size()) );
	IText *pText = states[nState].pGfxText->GetText();
	pText->SetText( pszText );
	states[nState].pGfxText->SetText( pText );
	states[nState].pGfxText->SetWidth( textRC.Width() );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const WORD* CSimpleWindow::GetWindowText( int nState )
{
	NI_ASSERT_T( nState < states.size(), NStr::Format("Can't get text from state %d (max %d states)", nState, states.size()) );
	IText *pText = states[nState].pGfxText->GetText();
	return pText->GetString();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSimpleWindow::SetTextColor( DWORD dwColor )
{
	for ( int i=0; i<states.size(); i++ )
		states[i].pGfxText->SetColor( dwColor );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSimpleWindow::Update( const NTimer::STime &currTime )
{
	if ( !bBlinking )
		return false;

	if ( currTime - dwLastBlinkTime < dwBlinkTime )
	{
		//������
		DWORD dwSubBlinkTime = GetGlobalVar( "BlinkSubTime", 1 );
		int nStage = (currTime - dwLastBlinkTime) / dwSubBlinkTime;
		if ( nStage & 0x01 )
		{
			//���������� ����
			//dwCurrentBlinkColor = 0xff000000;
			dwCurrentBlinkColor = states[nCurrentState].subStates[nCurrentSubState].specular;
		}
		else
		{
			//������������ ������
			std::string szBlinkKey = "BlinkColor";
			szBlinkKey += NStr::Format( "%d", nBlinkColorIndex );
			dwCurrentBlinkColor = GetGlobalVar( szBlinkKey.c_str(), (int) 0xffff0000 );
			//if ( nBlinkColorIndex == 0 )
				//GetSingleton<IScene>()->AddSound( "int_ok", VNULL3, SFX_INTERFACE, SAM_ADD_N_FORGET );
		}
	}
	else
	{
		dwCurrentBlinkColor = 0xff000000;
		bBlinking = false;
	}
	
	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSimpleWindow::BlinkMe( const int _nBlinkTime, const int _nBlinkColorIndex )
{
	nBlinkColorIndex = _nBlinkColorIndex;
	if ( _nBlinkTime )
		dwBlinkTime = _nBlinkTime;
	else
		dwBlinkTime = GetGlobalVar( "BlinkTime", 0 );
		
	dwLastBlinkTime = GetSingleton<IGameTimer>()->GetAbsTime();
	bBlinking = true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSimpleWindow::SetFocus( bool bFocus )
{
	if ( bFocus )
	{
		//������������� ����� ������� ����� ������
		IUIElement *pWnd = dynamic_cast<IUIElement *> ( this );
		GetParent()->SetFocusedWindow( pWnd );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSimpleWindow::ShowWindow( int _nCmdShow )
{
	IUIElement *pWnd = dynamic_cast<IUIElement *> ( this );
	if ( _nCmdShow != UI_SW_HIDE )
	{
		if ( pParent && pWnd )
		{
			if ( UI_SW_SHOW_DONT_MOVE_UP == _nCmdShow )
			{
			}
			else if ( _nCmdShow == UI_SW_SHOW || _nCmdShow == UI_SW_MAXIMIZE || _nCmdShow == UI_SW_SHOW_MODAL )
				pParent->MoveWindowUp( pWnd );			//����� �������, �������� ���������
			else if ( _nCmdShow == UI_SW_LAST || _nCmdShow == UI_SW_MINIMIZE  )
				pParent->MoveWindowDown( pWnd );		//���� �������
			//		else if ( _nCmdShow == UI_SW_HIDE )
			//			pParent->MoveWindowDown( pWnd );		//���� ���� ����������, ����� ��� �������� ���� �������
		}
	}
/*
	else
	{
		if ( pWnd && pParent )
			pParent->MoveWindowDown( pWnd );		//���� �������
	}
*/
	nCmdShow = _nCmdShow;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSimpleWindow::Reposition( const CTRect<float> &rcParent )
{
	//���� �������� reposition, ������ ���� �������� ���������� ��������.
	//��������� ������������ ������������ parent ����� �� �������� ���� ����������
/*
	// �������� ����� �������� � �������:
	CVec2 vParent;
	CVec2 vAxis( 1, 1 );
	switch ( (nPositionFlag >> 8) & 0x0f )
	{
		case UIPLACE_LEFT:
			vParent.x = rcParent.x1;
			break;
		case UIPLACE_RIGHT:
			vParent.x = rcParent.x2 - 1;
			vAxis.x = -1;
			break;
		case UIPLACE_HMID:
			vParent.x = rcParent.x1 + rcParent.Width()/2;
			break;
	}
	switch ( (nPositionFlag >> 8) & 0xf0 )
	{
		case UIPLACE_TOP:
			vParent.y = rcParent.y1;
			break;
		case UIPLACE_BOTTOM:
			vParent.y = rcParent.y2 - 1;
			vAxis.y = -1;
			break;
		case UIPLACE_VMID:
			vParent.y = rcParent.y1 + rcParent.Height()/2;
			break;
	}
	// �������� ����� �������� � ������:
	CVec2 vChild;
	switch ( nPositionFlag & 0x0f )
	{
		case UIPLACE_LEFT:
			vChild.x = vPos.x*vAxis.x;
			break;
		case UIPLACE_RIGHT:
			vChild.x = vPos.x*vAxis.x - vSize.x + 1;
			break;
		case UIPLACE_HMID:
			vChild.x = vPos.x*vAxis.x - vSize.x/2;
			break;
	}
	switch ( nPositionFlag & 0xf0 )
	{
		case UIPLACE_TOP:
			vChild.y = vPos.y*vAxis.y;
			break;
		case UIPLACE_BOTTOM:
			vChild.y = vPos.y*vAxis.y - vSize.y + 1;
			break;
		case UIPLACE_VMID:
			vChild.y = vPos.y*vAxis.y - vSize.y/2;
			break;
	}
	// 
	wndRect.x1 = vParent.x + vChild.x;
	wndRect.y1 = vParent.y + vChild.y;
	wndRect.x2 = wndRect.x1 + vSize.x;
	wndRect.y2 = wndRect.y1 + vSize.y;
*/

	switch ( nPositionFlag & 0xf )
	{
		case UIPLACE_LEFT:
			wndRect.x1 = rcParent.x1 + vPos.x;
			wndRect.x2 = wndRect.x1 + vSize.x;
			break;
		case UIPLACE_RIGHT:
			wndRect.x2 = rcParent.x2 - vPos.x;
			wndRect.x1 = wndRect.x2 - vSize.x;
			break;
		case UIPLACE_HMID:
			wndRect.x1 = (int) (rcParent.x1 + vPos.x + rcParent.Width()/2 - vSize.x/2);
			wndRect.x2 = (int) (rcParent.x1 + vPos.x + rcParent.Width()/2 + vSize.x/2);
			break;
	}

	switch ( nPositionFlag & 0xf0 )
	{
		case UIPLACE_TOP:
			wndRect.y1 = rcParent.y1 + vPos.y;
			wndRect.y2 = wndRect.y1 + vSize.y;
			break;
		case UIPLACE_BOTTOM:
			wndRect.y2 = rcParent.y2 - vPos.y;
			wndRect.y1 = wndRect.y2 - vSize.y;
			break;
		case UIPLACE_VMID:
			wndRect.y1 = (int) (rcParent.y1 + vPos.y + rcParent.Height()/2 - vSize.y/2);
			wndRect.y2 = (int) (rcParent.y1 + vPos.y + rcParent.Height()/2 + vSize.y/2);
			break;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CSimpleWindow::GetWindowPlacement( CVec2 *pPos, CVec2 *pSize, CTRect<float> *pScreenRect )
{
//	NI_ASSERT_T( pPos && pSize && pScreenRect, "Can't get window placement" );

	if ( pPos != 0 )
		*pPos = vPos;
	if ( pSize != 0 )
		*pSize = vSize;
	if ( pScreenRect != 0 )
		*pScreenRect = wndRect;

	return nPositionFlag;
}

void CSimpleWindow::UpdateLocalCoordinates()
{
/*
	if ( !pParent )
		return;					//����� ���� ��� screen ?

	CMultipleWindow *pRealParent = dynamic_cast<CMultipleWindow *> ( pParent.GetPtr() );
	NI_ASSERT( pRealParent != 0 );

	//��������� ����� �������� ��������� ���������, ��������� ����������� ������������ � pParent
	const CTRect<float> &rcParent = pRealParent->GetScreenRect();
	switch ( nPositionFlag & 0xf )
	{
		case UIPLACE_LEFT:
			vPos.x = wndRect.x1 - rcParent.x1;
			vSize.x = wndRect.x2 - wndRect.x1;
			break;
		case UIPLACE_RIGHT:
			vPos.x = rcParent.x2 - wndRect.x1;
			vSize.x = wndRect.x2 - wndRect.x1;
			break;
	}
	
	switch ( nPositionFlag & 0xf0 )
	{
		case UIPLACE_TOP:
			vPos.y = wndRect.y1 - rcParent.y1;
			vSize.y = wndRect.y2 - wndRect.y1;
			break;
		case UIPLACE_BOTTOM:
			vPos.y = rcParent.y2 - wndRect.y1;
			vSize.y = wndRect.y2 - wndRect.y1;
			break;
	}
*/

/*
	//������ ���� ��� multiple window ���� ������� reposition ��� ���� children
	Reposition( rcParent );			//��� ����� ������� ��� ������� ����� �����������
*/
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSimpleWindow::EnableWindow( bool bEnable )
{
	bWindowActive = bEnable;
	nCurrentSubState = bEnable? E_NORMAL_STATE : E_DISABLED_STATE;

	if ( bEnable )
	{
		for ( int i=0; i<states.size(); i++ )
			states[i].pGfxText->SetColor( dwTextColor );
	}
	else
	{
		// voobshe-to cvet disabled zadaetsa v xml
		/*for ( int i=0; i<states.size(); i++ )
			states[i].pGfxText->SetColor( 0xff808080 );*/
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSimpleWindow::Visit( interface ISceneVisitor *pVisitor )
{
	if ( !nCmdShow )
		return;
	// visit background
	VisitBackground( pVisitor );
	// visit text
	VisitText( pVisitor );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSimpleWindow::VisitBackground( ISceneVisitor *pVisitor )
{
	if ( !bShowBackground || states[nCurrentState].subStates[nCurrentSubState].subRects.empty() ) 
		return;
	//
	const CUIWindowSubState &currentSubState = states[nCurrentState].subStates[nCurrentSubState];		
	const int nSize = currentSubState.subRects.size();
	SGFXRect2 *pRects = GetTempBuffer<SGFXRect2>( nSize );
	for ( int i = 0; i < nSize; ++i )
	{
		SGFXRect2 &rc = pRects[i];
		const SWindowSubRect &sub = currentSubState.subRects[i];
		rc.rect.x1 = wndRect.x1 + sub.rc.x1;
		rc.rect.x2 = wndRect.x1 + sub.rc.x2;
		rc.rect.y1 = wndRect.y1 + sub.rc.y1;
		rc.rect.y2 = wndRect.y1 + sub.rc.y2;
		rc.maps = sub.mapa;
		rc.color = currentSubState.color;
		rc.specular = currentSubState.specular;
		if ( bBlinking )
			rc.specular = dwCurrentBlinkColor;
		rc.fZ = 0;
		
		if ( bBounded )
		{
			// ��������, ����� ����� ������ ����� ��������
			float fTemp = rcBound.x1 - rc.rect.x1;
			if ( fTemp > 0 )
			{
				rc.maps.x1 += fTemp * rc.maps.Width() / rc.rect.Width();
				rc.rect.x1 = rcBound.x1;
			}
			
			fTemp = rc.rect.x2 - rcBound.x2;
			if ( fTemp > 0 )
			{
				rc.maps.x2 -= fTemp * rc.maps.Width() / rc.rect.Width();
				rc.rect.x2 = rcBound.x2;
			}
			
			fTemp = rcBound.y1 - rc.rect.y1;
			if ( fTemp > 0 )
			{
				rc.maps.y1 += fTemp * rc.maps.Height() / rc.rect.Height();
				rc.rect.y1 = rcBound.y1;
			}
			
			fTemp = rc.rect.y2 - rcBound.y2;
			if ( fTemp > 0 )
			{
				rc.maps.y2 -= fTemp * rc.maps.Height() / rc.rect.Height();
				rc.rect.y2 = rcBound.y2;
			}
		}
	}
	pVisitor->VisitUIRects( currentSubState.pTexture, 3, pRects, nSize );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSimpleWindow::VisitText( ISceneVisitor *pVisitor )
{
	IGFXText *pTempGFXText = states[nCurrentState].pGfxText;
	if ( !pTempGFXText || (pTempGFXText->GetText()->GetLength() <= 0) )
		return;
	//
	int nY = 0;
	DWORD flag = FNT_FORMAT_LEFT;
	CTRect<float> textRC = wndRect;
	//
	if ( bBounded )
	{
		textRC.y1 = rcBound.y1;
		textRC.y2 = rcBound.y2;
	}
	//
	switch ( nTextAlign & 0xf )
	{
		case UIPLACE_LEFT:
			textRC.x1 += vTextPos.x;
			break;
		case UIPLACE_RIGHT:
			textRC.x2 -= vTextPos.x;
			flag = FNT_FORMAT_RIGHT;
			break;
		case UIPLACE_HMID:
			textRC.x1 += vTextPos.x;
			textRC.x2 += vTextPos.x;
			flag = FNT_FORMAT_CENTER;
			break;
	}
	//
	if ( bBounded )
		nY = wndRect.y1 - rcBound.y1;
	//
	switch ( nTextAlign & 0xf0 )
	{
		case UIPLACE_TOP:
			textRC.y1 += vTextPos.y;
			break;
		case UIPLACE_BOTTOM:
			textRC.y2 -= vTextPos.y;
			nY += wndRect.Height() - pTempGFXText->GetLineSpace();
			break;
		case UIPLACE_VMID:
			{
				textRC.y1 += vTextPos.y;
				textRC.y2 += vTextPos.y;
				pTempGFXText->SetWidth( textRC.Width() );
				const int nNumLines = bSingleLine ? 1 : pTempGFXText->GetNumLines();
				nY += ( wndRect.Height() - nNumLines * pTempGFXText->GetLineSpace() ) / 2;
				break;
			}
	}
	//
	if ( nTextAlign & 0xf00 )
		flag = FNT_FORMAT_JUSTIFY;

	if ( nCurrentSubState == E_PUSHED_STATE )
	{
		textRC.x1 += vShiftText.x;
		textRC.x2 += vShiftText.x;
		nY += vShiftText.y;
	}

	if ( vShadowShift.x != 0 || vShadowShift.y != 0 )
	{
		// ������ ����
		CTRect<float> shadowRC = textRC;
		shadowRC.x1 += vShadowShift.x;
		shadowRC.y1 += vShadowShift.y;
		shadowRC.x2 += vShadowShift.x;
		shadowRC.y2 += vShadowShift.y;
		pVisitor->VisitUIText( pTempGFXText, shadowRC, nY, dwShadowColor, flag | (bSingleLine ? FNT_FORMAT_SINGLE_LINE : 0) );
	}
	//
	pVisitor->VisitUIText( pTempGFXText, textRC, nY, 
			                   states[nCurrentState].subStates[nCurrentSubState].textColor, 
			                   flag | (bSingleLine ? FNT_FORMAT_SINGLE_LINE : 0) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSimpleWindow::Draw( IGFX *pGFX )
{
	NI_ASSERT_SLOW_T( false, "Can't user Draw() directly - use visitor pattern" );
	return;
	
	if ( !nCmdShow )
		return;

	pGFX->SetShadingEffect( 3 );
	DrawBackground( pGFX );
	DrawText( pGFX );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSimpleWindow::DrawBackground( IGFX *pGFX )
{
	if ( !bShowBackground )
		return;
	
	const CUIWindowSubState &currentSubState = states[nCurrentState].subStates[nCurrentSubState];
	pGFX->SetTexture( 0, currentSubState.pTexture );
	
	const int nSize = currentSubState.subRects.size();
	if ( nSize > 0 )
	{
		SGFXRect2 *pRects = GetTempBuffer<SGFXRect2>( nSize );
		for ( int i=0; i<nSize; i++ )
		{
			SGFXRect2 &rc = pRects[ i ];
			const SWindowSubRect &sub = currentSubState.subRects[i];
			rc.rect.x1 = wndRect.x1 + sub.rc.x1;
			rc.rect.x2 = wndRect.x1 + sub.rc.x2;
			rc.rect.y1 = wndRect.y1 + sub.rc.y1;
			rc.rect.y2 = wndRect.y1 + sub.rc.y2;
			rc.maps = sub.mapa;
			rc.color = currentSubState.color;
			rc.specular = currentSubState.specular;
			if ( bBlinking )
				rc.specular = dwCurrentBlinkColor;
			rc.fZ = 0;
			
			if ( bBounded )
			{
				// ��������, ����� ����� ������ ����� ��������
				float fTemp;
				fTemp = rcBound.x1 - rc.rect.x1;
				if ( fTemp > 0 )
				{
					rc.maps.x1 += fTemp * rc.maps.Width() / rc.rect.Width();
					rc.rect.x1 = rcBound.x1;
				}
				
				fTemp = rc.rect.x2 - rcBound.x2;
				if ( fTemp > 0 )
				{
					rc.maps.x2 -= fTemp * rc.maps.Width() / rc.rect.Width();
					rc.rect.x2 = rcBound.x2;
				}
				
				fTemp = rcBound.y1 - rc.rect.y1;
				if ( fTemp > 0 )
				{
					rc.maps.y1 += fTemp * rc.maps.Height() / rc.rect.Height();
					rc.rect.y1 = rcBound.y1;
				}
				
				fTemp = rc.rect.y2 - rcBound.y2;
				if ( fTemp > 0 )
				{
					rc.maps.y2 -= fTemp * rc.maps.Height() / rc.rect.Height();
					rc.rect.y2 = rcBound.y2;
				}
			}
		}
		pGFX->DrawRects( pRects, nSize );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSimpleWindow::DrawText( IGFX *pGFX )
{
	if ( !states[nCurrentState].pGfxText )
		return;
	IGFXText *pTempGFXText = states[nCurrentState].pGfxText;

	int nY = 0;
	DWORD flag = FNT_FORMAT_LEFT;
	CTRect<float> textRC = wndRect;
	if ( bBounded )
	{
		textRC.y1 = rcBound.y1;
		textRC.y2 = rcBound.y2;
	}

	switch ( nTextAlign & 0xf )
	{
	case UIPLACE_LEFT:
		textRC.x1 += vTextPos.x;
		break;
	case UIPLACE_RIGHT:
		textRC.x2 -= vTextPos.x;
		flag = FNT_FORMAT_RIGHT;
		break;
	case UIPLACE_HMID:
		textRC.x1 += vTextPos.x;
		textRC.x2 += vTextPos.x;
		flag = FNT_FORMAT_CENTER;
		break;
	}
	
	if ( bBounded )
		nY = wndRect.y1 - rcBound.y1;
	switch ( nTextAlign & 0xf0 )
	{
	case UIPLACE_TOP:
		textRC.y1 += vTextPos.y;
		break;
	case UIPLACE_BOTTOM:
		textRC.y2 -= vTextPos.y;
		nY += wndRect.Height() - pTempGFXText->GetLineSpace();
		break;
	case UIPLACE_VMID:
		{
			textRC.y1 += vTextPos.y;
			textRC.y2 += vTextPos.y;
			pTempGFXText->SetWidth( textRC.Width() );
			const int nNumLines = bSingleLine ? 1 : pTempGFXText->GetNumLines();
			nY += ( wndRect.Height() - nNumLines * pTempGFXText->GetLineSpace() ) / 2;
			break;
		}
	}

	if ( nTextAlign & 0xf00 )
	{
		flag = FNT_FORMAT_JUSTIFY;
	}

	if ( nCurrentSubState == E_PUSHED_STATE )
	{
		textRC.x1 += vShiftText.x;
		textRC.x2 += vShiftText.x;
		nY += vShiftText.y;
	}

	if ( vShadowShift.x != 0 || vShadowShift.y != 0 )
	{
		//������ ����
		CTRect<float> shadowRC = textRC;
		shadowRC.x1 += vShadowShift.x;
		shadowRC.y1 += vShadowShift.y;
		shadowRC.x2 += vShadowShift.x;
		shadowRC.y2 += vShadowShift.y;
		pTempGFXText->SetColor( dwShadowColor );
		pGFX->DrawText( pTempGFXText, shadowRC, nY, flag | (bSingleLine ? FNT_FORMAT_SINGLE_LINE : 0) );
	}
	
	pTempGFXText->SetColor( states[nCurrentState].subStates[nCurrentSubState].textColor );
	pGFX->DrawText( pTempGFXText, textRC, nY, flag | (bSingleLine ? FNT_FORMAT_SINGLE_LINE : 0) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSimpleWindow::OnMouseMove( const CVec2 &vPos, EMouseState mState )
{
	if ( !bWindowActive )
	{
		if ( IsInside( vPos ) )
			return true;
		return false;
	}

	//���� ����� ������ ����� �� ������
	if ( mState == E_MOUSE_FREE )
	{
		if ( !IsInside( vPos ) )
		{
			//����� ��� ������
			nCurrentSubState = E_NORMAL_STATE;
			return false;
		}
		else
		{
			//����� ������ ������
			if ( nCurrentSubState == E_NORMAL_STATE )
			{
				nCurrentSubState = E_HIGHLIGHTED_STATE;
				
				// ����������� high ����
				GetSingleton<IScene>()->AddSound( szHighSound.c_str(), VNULL3, SFX_INTERFACE, SAM_ADD_N_FORGET, ESCT_GENERIC );
			}
			return true;
		}
	}
	
	//���� ����� ��� ������ ������ ����� ������
	if ( mState & E_LBUTTONDOWN || mState & E_RBUTTONDOWN )
	{
		if ( !IsInside( vPos ) )
		{
			//����� ��� ������
			nCurrentSubState = E_NORMAL_STATE;
			return true;
		}
		else
		{
			//����� ������ ������
			nCurrentSubState = E_PUSHED_STATE;
			return true;
		}
	}
	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSimpleWindow::OnMouseWheel( const CVec2 &vPos, EMouseState mouseState, float fDelta )
{
	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSimpleWindow::OnLButtonDblClk( const CVec2 &vPos )
{
	if ( !bWindowActive )
		return false;
	if ( IsInside( vPos ) )
		return true;
	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSimpleWindow::OnLButtonDown( const CVec2 &vPos, EMouseState mouseState )
{
	if ( !bWindowActive )
	{
		if ( IsInside( vPos ) )
			return true;
		return false;
	}

	if ( !IsInside( vPos ) )
	{
		//����� ��� ������
		nCurrentSubState = E_NORMAL_STATE;
		return false;
	}
	else
	{
		//����� ������ ������
		if ( nCurrentSubState != E_PUSHED_STATE )
		{
			//����������� push ����
			GetSingleton<IScene>()->AddSound( states[nCurrentState].szPushSound.c_str(), VNULL3, SFX_INTERFACE, SAM_ADD_N_FORGET );
		}
		nCurrentSubState = E_PUSHED_STATE;

		//����������� �� ������� ������ �� ������
		SUIMessage msg;
		msg.nMessageCode = UI_NOTIFY_WINDOW_CLICKED;
		msg.nFirst = nID;
		msg.nSecond = nCurrentState;
		pParent->ProcessMessage( msg );
		
		return true;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSimpleWindow::OnLButtonUp( const CVec2 &vPos, EMouseState mouseState )
{
	if ( !bWindowActive )
		return false;
	
	if ( !IsInside( vPos ) )
	{
		//����� ��� ������
		nCurrentSubState = E_NORMAL_STATE;
		return false;
	}
	else
	{
		//����� ������ ������
		if ( nCurrentSubState == E_PUSHED_STATE )
		{
			nCurrentSubState = E_HIGHLIGHTED_STATE;
			SetState( ( nCurrentState + 1 ) % states.size(), true );

			//�������� ���� ���������
			if ( ( nBlink & 2 ) || ( ( nBlink & 1 ) && states.size() == 1 ) )
				BlinkMe();
		}
		return true;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSimpleWindow::OnRButtonDown( const CVec2 &vPos, EMouseState mouseState )
{
	if ( !bWindowActive )
		return false;
	
	if ( IsInside( vPos ) )
	{	
		
			//����������� �� ������� ������ �� ������
		SUIMessage msg;
		msg.nMessageCode = UI_NOTIFY_WINDOW_RCLICKED;
		msg.nFirst = nID;
		msg.nSecond = nCurrentState;
		pParent->ProcessMessage( msg );
		
		return true;

		return true;
	}
	else
		return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CSimpleWindow::OnRButtonUp( const CVec2 &vPos, EMouseState mouseState )
{
	if ( !bWindowActive )
		return false;
	
	if ( IsInside( vPos ) )
		return true;
	else
		return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSimpleWindow::SetState( int nState, bool bNotify )
{
	const int nNewState = nState % states.size();
	if ( (nCurrentState == nNewState) && (states.size() > 1) )
		return;

	if ( bWindowActive )
	{
		//����������� click ����
		if ( !states[nCurrentState].szClickSound.empty() ) 
			GetSingleton<IScene>()->AddSound( states[nCurrentState].szClickSound.c_str(), VNULL3, SFX_INTERFACE, SAM_ADD_N_FORGET );

	}

	nCurrentState = nNewState;
	if ( !bWindowActive )			//disable window does not generate messages
		return;

	if ( bNotify && pParent )
	{
		//����������� �� ��������� state
		SUIMessage msg;
		msg.nMessageCode = UI_NOTIFY_STATE_CHANGED_MESSAGE;
		msg.nFirst = nID;
		msg.nSecond = nCurrentState;
		pParent->ProcessMessage( msg );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CSimpleWindow::DestroyWindow()
{
	IUIElement *pElement = dynamic_cast<IUIElement *> ( this );
	pParent->RemoveChild( pElement );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IUIElement* CSimpleWindow::PickElement( const CVec2 &vPos, int nRecursion )
{
	if ( IsVisible() && IsInside( vPos ) )
		return dynamic_cast<IUIElement*> ( this );
	else
		return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static int Error_out( struct lua_State *state )
{
	Script script(state);
	Script::Object obj = script.GetObject(script.GetTop());
	NI_ASSERT_T( false, NStr::Format("Script error: %s", obj.GetString() ) );
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static int GetUserProfileVar( struct lua_State *state )
{
	Script script(state);
	NI_ASSERT_T( script.GetTop() == 2, "Script function must have 2 arguments on the stack" );			//��� ���������
	const std::string szStr = script.GetObject( -2 );
	const int nValue = script.GetObject( -1 );
	script.PushNumber( GetSingleton<IUserProfile>()->GetVar( szStr.c_str(), nValue ) );
	return 1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static int SetUserProfileVar( struct lua_State *state )
{
	Script script(state);
	NI_ASSERT_T( script.GetTop() == 2, "Script function must have 2 arguments on the stack" );			//��� ���������
	const std::string szStr = script.GetObject( -2 );
	const int nValue = script.GetObject( -1 );
	GetSingleton<IUserProfile>()->AddVar( szStr.c_str(), nValue );
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static int OutputValue( struct lua_State *state )
{
	Script script(state);
	NI_ASSERT_T( script.GetTop() == 2, "Script function must have 2 arguments on the stack" );			//��� ���������
	std::string szStr = script.GetObject( -2 );
	int nValue = script.GetObject( -1 );
	NStr::DebugTrace( "****Debug LUA script: %s %d\n", szStr.c_str(), nValue );
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static int InitCommonScript( struct lua_State *state )
{
	Script script(state);
	int nRes = GetGlobalVar( "NumberOfButtons", 0 );
	script.PushNumber( nRes );
	return 1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static int SetProcessedFlag( struct lua_State *state )			//������������� ���� PROCESSED ��� ���������
{
	Script script(state);
	NI_ASSERT_T( script.GetTop() == 1, "Script function must have 1 argument on the stack" );			//���� ��������
	int nMessageCode = script.GetObject( -1 );
	script.PushNumber( nMessageCode | PROCESSED_FLAG );
	return 1;										//���� ������������ ��������
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static int IsActiveBit( struct lua_State *state )
{
	Script script(state);
	NI_ASSERT_T( script.GetTop() == 2, "Script function must have 2 arguments on the stack" );			//��� ���������
	DWORD n = script.GetObject( -2 );
	int nBit = script.GetObject( -1 );
	int nRes = (bool) ( n & ( 1 << nBit ) );
	script.PushNumber( nRes );
	
	return 1;										//���� ������������ ��������
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static int ProcessMessageWithLink( struct lua_State *state )
{
	Script script(state);
	const int nEventID = script.GetObject( 1 );
	const int nParam = script.GetObject( 2 );
	
	script.PushNumber( GetSingleton<IMessageLinkContainer>()->ProcessMessage( SGameMessage( nEventID, nParam ) ) );
	return 1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CMultipleWindow::IsGameButtonProcessing( lua_State *pLuaState )
{
	Script script( pLuaState );
	const bool bNoConrtol = GetSingleton<IGameTimer>()->GetPauseReason() >= PAUSE_TYPE_NO_CONTROL;
	script.PushNumber( !bNoConrtol );
	return 1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMultipleWindow::CopyInternals( CMultipleWindow * pWnd )
{
	//*pWnd = *this;
	CSimpleWindow::CopyInternals( pWnd );
	pWnd->childList.clear();							//child windows

	pWnd->pHighlighted = 0;			//������������ ����
	pWnd->pPushed = 0;						//������� ���� (����� ������)
	pWnd->pRPushed = 0;					//���� � ������� ������ ������� ����
	pWnd->pFocused = 0;					//���� � �������, ��� ������ ������ ��� edit box �������� ��������� TEXT_MODE
	
	pWnd->fMouseWheelMultiplyer = fMouseWheelMultiplyer;

	pWnd->szLuaFileName = szLuaFileName;
	
	pWnd->bAnimation = bAnimation;						//���� ���������� ����, �� ������ � ���������
	pWnd->bAnimationRunning = bAnimationRunning;			//���� ����, ��� ���������� ��������, ������� ��� ��������
	pWnd->dwLastOpenTime = dwLastOpenTime;				//����� ����� �������� �������� ��������
	pWnd->dwLastCloseTime = dwLastCloseTime;			//����� ����� �������� �������� ��������
	pWnd->dwAnimationTime = dwAnimationTime;			//����� �������� �������� ��� ��������
	pWnd->vMinPos = vMinPos;
	pWnd->vMaxPos = vMaxPos;
	pWnd->vBeginPos = vBeginPos;
	pWnd->nAnimationCmdShow = nAnimationCmdShow;
	
	pWnd->bModal = bModal;

	
	for ( CWindowList::const_iterator it = childList.begin(); it != childList.end(); ++it )
		pWnd->childList.push_back( (*it)->Duplicate( ) );

	pWnd->InitDependentInfo();
	pWnd->InitDependentInfoMW();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CMultipleWindow::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.AddTypedSuper( static_cast<CSimpleWindow*>(this) );
	saver.Add( "Children", &childList );
	saver.Add( "ModalFlag", &bModal );
	saver.Add( "Animation", &bAnimation );
	saver.Add( "MinPos", &vMinPos );
	saver.Add( "MaxPos", &vMaxPos );
	saver.Add( "MouseWheelMul", &fMouseWheelMultiplyer );
	saver.Add( "AnimTime", &dwAnimationTime );
	saver.Add( "Script", &szLuaFileName );
	
	if ( saver.IsReading() )
	{
		InitDependentInfoMW();
	}
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMultipleWindow::InitDependentInfoMW()
{
	//������� �� ���� ������� ����� ��� ��� ���������� parent
		IUIContainer *pContainer = dynamic_cast<IUIContainer *> ( this );
		if ( pContainer )
		{
			for ( CWindowList::iterator it=childList.begin(); it!=childList.end(); ++it )
				(*it)->SetParent( pContainer );
		}

		//���� � ������ �� child ���������� ModalFlag, �� �� ������������� ���� ���� ��� �������� ������ � ������ ���� child ������ � ������
		CWindowList::iterator it=childList.begin();
		for ( ; it!=childList.end(); ++it )
		{
			CMultipleWindow *pContainer = dynamic_cast<CMultipleWindow *> ( it->GetPtr() );
			if ( pContainer && pContainer->bModal )
			{
				pContainer->bModal = false;
				break;
			}
		}

		if ( it != childList.end() )
		{
			bModal = true;
			CObj<IUIElement> pObj = *it;
			childList.erase( it );
			childList.push_front( pObj );
		}
	
	// �������� ��������� LUA
#if defined( _DO_ASSERT ) || defined( _DO_ASSERT_SLOW )
			std::unordered_map<int, int> mapa;
			
			//���������, ����� �� ���� ������� � �������������� ID
			for ( CWindowList::iterator it = childList.begin(); it != childList.end(); ++it )
			{
				if ( (*it)->GetWindowID() > 10 )
					mapa[(*it)->GetWindowID()]++;
			}
			
			for ( std::unordered_map<int, int>::iterator it = mapa.begin(); it != mapa.end(); ++it )
			{
				NI_ASSERT_T( it->second == 1, NStr::Format("Duplicate window id %d", it->first) );
			}
#endif		// defined( _DO_ASSERT ) || defined( _DO_ASSERT_SLOW )
		
		luaScript.Clear();
		luaScript.Init();

		if ( szLuaFileName.size() == 0 )
		{
			bLua = false;
			return;
		}

		luaScript.Register( "AddMessage", CMultipleWindow::AddMessage );
		luaScript.Register( "_ERRORMESSAGE", Error_out );
		luaScript.Register( "SetProcessedFlag", SetProcessedFlag );
		luaScript.Register( "OutputValue", OutputValue );
		luaScript.Register( "InitCommonScript", InitCommonScript );
		luaScript.Register( "IsActiveBit", IsActiveBit );
		luaScript.Register( "ProcessMessageWithLink", ProcessMessageWithLink );
		luaScript.Register( "SaveLuaValue", CMultipleWindow::SaveLuaValue );
		luaScript.Register( "IsGameButtonProcessing", CMultipleWindow::IsGameButtonProcessing );
		luaScript.Register( "SetUserProfileVar", SetUserProfileVar );
		luaScript.Register( "GetUserProfileVar", GetUserProfileVar );
		// load LUA script data
		bLua = false;													// ������� ������������, ��� ������������� �� ������
		{
			CPtr<IDataStream> pStream = GetSingleton<IDataStorage>()->OpenStream( (szLuaFileName + ".lua").c_str(), STREAM_ACCESS_READ );
			if ( pStream )
			{
				int nSize = pStream->GetSize();
				std::vector<char> buffer( nSize + 10 );
				pStream->Read( &(buffer[0]), nSize );
				bLua = luaScript.DoBuffer( &(buffer[0]), nSize, "UI" ) == 0;	// ����������� lua ����

				Script::Object obj = luaScript.GetGlobal( "LuaInit" );
				if ( !obj.IsNil() )
				{
					luaScript.PushNumber( nID );
					int nRes = luaScript.Call( 1, 0 );			//�������� LUA �������, ���������� ����� ����������, 0 �����������
					NI_ASSERT_T( nRes == 0, "LUA script call failed" );
				}
			}
		}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CMultipleWindow::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.AddTypedSuper( 0, static_cast<CSimpleWindow*>(this) );
	saver.Add( 1, &childList );
	saver.Add( 2, &bAnimation );
	saver.Add( 3, &bAnimationRunning );
	saver.Add( 4, &dwLastOpenTime );
	saver.Add( 5, &dwLastCloseTime );
	saver.Add( 6, &dwAnimationTime );
	saver.Add( 7, &vMinPos );
	saver.Add( 8, &vMaxPos );
	saver.Add( 9, &vBeginPos );
	saver.Add( 10, &nAnimationCmdShow );
	saver.Add( 11, &bModal );
	saver.Add( 12, &fMouseWheelMultiplyer );

	//20 ����� ��� serialize LUA
	
	if ( saver.IsReading() )
	{
		//������� �� ���� ������� ����� ��� ��� ���������� parent
		IUIContainer *pContainer = dynamic_cast<IUIContainer *> ( this );
		if ( pContainer )
		{
			for ( CWindowList::iterator it=childList.begin(); it!=childList.end(); ++it )
				(*it)->SetParent( pContainer );
		}
	}
	
	// �������� ��������� LUA
	saver.Add( 1, &szLuaFileName );
	if ( saver.IsReading() )
	{
		if ( szLuaFileName.size() == 0 )
		{
			bLua = false;
			return 0;
		}
		luaScript.Clear();
		luaScript.Init();
		luaScript.Register( "AddMessage", CMultipleWindow::AddMessage );
		luaScript.Register( "_ERRORMESSAGE", Error_out );
		luaScript.Register( "SetProcessedFlag", SetProcessedFlag );
		luaScript.Register( "OutputValue", OutputValue );
		luaScript.Register( "InitCommonScript", InitCommonScript );
		luaScript.Register( "IsActiveBit", IsActiveBit );
		luaScript.Register( "ProcessMessageWithLink", ProcessMessageWithLink );
		luaScript.Register( "SaveLuaValue", CMultipleWindow::SaveLuaValue );
		luaScript.Register( "IsGameButtonProcessing", CMultipleWindow::IsGameButtonProcessing );
		luaScript.Register( "SetUserProfileVar", SetUserProfileVar );
		luaScript.Register( "GetUserProfileVar", GetUserProfileVar );
		// load LUA script data
		bLua = false;													// ������� ������������, ��� ������������� �� ������
		{
			CPtr<IDataStream> pStream = GetSingleton<IDataStorage>()->OpenStream( (szLuaFileName + ".lua").c_str(), STREAM_ACCESS_READ );
			if ( pStream )
			{
				int nSize = pStream->GetSize();
				std::vector<char> buffer( nSize + 10 );
				pStream->Read( &(buffer[0]), nSize );
				bLua = luaScript.DoBuffer( &(buffer[0]), nSize, "UI" ) == 0;	// ����������� lua ����
			}
		}
	}
	
	if ( bLua )
	{
		if ( saver.IsReading() )
		{
			Script::Object obj = luaScript.GetGlobal( "LuaLoad" );
			if ( !obj.IsNil() )
			{
				staticLuaValues.clear();
				saver.Add( 20, &staticLuaValues );
				for ( int i=0; i<staticLuaValues.size(); i++ )
				{
					luaScript.PushNumber( staticLuaValues[i].nID );
					luaScript.PushNumber( staticLuaValues[i].nVal );
				}
				int nRes = luaScript.Call( staticLuaValues.size()*2, 0 );			//�������� LUA �������, ���������� ����� ����������, 0 �����������
				NI_ASSERT_T( nRes == 0, "LUA script call failed" );
			}

			obj = luaScript.GetGlobal( "LuaInit" );
			if ( !obj.IsNil() )
			{
				luaScript.PushNumber( nID );
				int nRes = luaScript.Call( 1, 0 );			//�������� LUA �������, ���������� ����� ����������, 0 �����������
				NI_ASSERT_T( nRes == 0, "LUA script call failed" );
			}
		}
		else
		{
			staticLuaValues.clear();
			Script::Object obj = luaScript.GetGlobal( "LuaSave" );
			if ( !obj.IsNil() )
			{
				int nRes = luaScript.Call( 0, 0 );			//�������� LUA �������, 0 ����������, 0 �����������
				NI_ASSERT_T( nRes == 0, "LUA script call failed" );
				saver.Add( 20, &staticLuaValues );
			}
		}
	}

	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMultipleWindow::SetBoundRect( const CTRect<float> &rc )
{
	CSimpleWindow::SetBoundRect( rc );

	for ( CWindowList::iterator it=childList.begin(); it!=childList.end(); ++it )
		(*it)->SetBoundRect( rc );			//�������� ����� ����� ����������� ����� rc � ����������� ��������������� CMultipleWindow
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMultipleWindow::ShowWindow( int _nCmdShow )
{
	//CRAP{ KOSTILI K POKAZU HELP EKRANOV V NEPOLNOEKRANNIH INTERFEISAH
	if ( _nCmdShow && GetChildByID( TUTORIAL_WINDOW_ID ) )
		GetSingleton<IInput>()->AddMessage( SGameMessage(TUTORIAL_TRY_SHOW_IF_NOT_SHOWN) );
	//CRAP}

	if ( bAnimation )
	{
		//���� ������ � ���������
		if ( _nCmdShow != 0 && _nCmdShow != UI_SW_MINIMIZE )
		{
			//���������� ������
			nAnimationCmdShow = _nCmdShow;
			DWORD dwCurrentTime = GetSingleton<IGameTimer>()->GetAbsTime();
			if ( dwCurrentTime - dwLastCloseTime < dwAnimationTime )
				dwLastOpenTime = dwCurrentTime + ( dwCurrentTime - dwLastCloseTime - dwAnimationTime );
			else
				dwLastOpenTime = dwCurrentTime;
			dwLastCloseTime = 0;
			bAnimationRunning = true;
			CSimpleWindow::ShowWindow( _nCmdShow );
		}
		else
		{
			//�������� ������
			nAnimationCmdShow = _nCmdShow;
			DWORD dwCurrentTime = GetSingleton<IGameTimer>()->GetAbsTime();
			if ( dwCurrentTime - dwLastOpenTime < dwAnimationTime )
				dwLastCloseTime = dwCurrentTime + ( dwCurrentTime - dwLastOpenTime - dwAnimationTime );
			else
				dwLastCloseTime = dwCurrentTime;
			dwLastOpenTime = 0;
			bAnimationRunning = true;

			//������ �� �������, ��� ��� ��� ��� �������� ��������
			//�������� ���� ���� �������
			if ( pParent )
			{
				IUIElement *pWnd = dynamic_cast<IUIElement *> ( this );
				pParent->MoveWindowDown( pWnd );
			}
		}
	}
	else
		CSimpleWindow::ShowWindow( _nCmdShow );		//��� ��������

	if ( UI_SW_SHOW_MODAL == _nCmdShow )
	{
		CMultipleWindow *pPapa = dynamic_cast<CMultipleWindow *> ( GetParent() );
		if ( pPapa )
			pPapa->SetModalFlag( true );
	}
	else if ( UI_SW_HIDE_MODAL == _nCmdShow )
	{
		CMultipleWindow *pPapa = dynamic_cast<CMultipleWindow *> ( GetParent() );
		if ( pPapa )
			pPapa->SetModalFlag( false );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMultipleWindow::Update( const NTimer::STime &currTime )
{
	if ( bAnimation )
	{
		//��� ���������� ���� ������� ����������
		CTRect<float> rc = GetScreenRect();
		int nWidth = vMaxPos.x - vMinPos.x;
		int nHeight = vMaxPos.y - vMinPos.y;
		
		if ( currTime - dwLastOpenTime < dwAnimationTime )
		{
			//���� � �������� ��������
			// fX � fY ��� �������� �������� ����� ����
			float fX = (int) ( (float) nWidth * ( currTime - dwLastOpenTime ) / dwAnimationTime );
			rc.left = vBeginPos.x + vMinPos.x + fX;
			rc.right = rc.left + wndRect.Width();

			float fY = (int) ( (float) nHeight * ( currTime - dwLastOpenTime ) / dwAnimationTime );
			rc.top = vBeginPos.y + vMinPos.y + fY;
			rc.bottom = rc.top + wndRect.Height();
			
			SetScreenRect( rc );
			for ( CWindowList::iterator it=childList.begin(); it!=childList.end(); ++it )
				(*it)->Reposition( rc );
			return true;
		}
		else if ( bAnimationRunning && dwLastOpenTime != 0 )
		{
			//���� ���� ��������� ���� ��������
			bAnimationRunning = false;
			dwLastOpenTime = 0;
			rc.left = vBeginPos.x + vMaxPos.x;
			rc.right = rc.left + wndRect.Width();
			rc.top = vBeginPos.y + vMaxPos.y;
			rc.bottom = rc.top + wndRect.Height();
			
			SetScreenRect( rc );
			for ( CWindowList::iterator it=childList.begin(); it!=childList.end(); ++it )
				(*it)->Reposition( rc );
			
			if ( pParent )
			{
				//����������� �� ��������� ��������
				SUIMessage msg;
				msg.nMessageCode = UI_NOTIFY_ANIMATION_FINISHED;
				msg.nFirst = nID;
				msg.nSecond = nCmdShow;
				pParent->ProcessMessage( msg );
			}
			return true;
		}
		
		if ( currTime - dwLastCloseTime < dwAnimationTime )
		{
			//���� � �������� ��������
			// fX � fY ��� �������� �������� ����� ����
			float fX = (int) ( (float) nWidth * ( currTime - dwLastCloseTime ) / dwAnimationTime );
			rc.left = vBeginPos.x + vMaxPos.x - fX;
			rc.right = rc.left + wndRect.Width();
			
			float fY = (int) ( (float) nHeight * ( currTime - dwLastCloseTime ) / dwAnimationTime );
			rc.top = vBeginPos.y + vMaxPos.y - fY;
			rc.bottom = rc.top + wndRect.Height();
			
			SetScreenRect( rc );
			for ( CWindowList::iterator it=childList.begin(); it!=childList.end(); ++it )
				(*it)->Reposition( rc );
			return true;
		}
		else if ( bAnimationRunning && dwLastCloseTime != 0 )
		{
			//���� ���� ��������� ���� ��������
			bAnimationRunning = false;
			dwLastCloseTime = 0;
			if ( nAnimationCmdShow != UI_SW_MINIMIZE )
				CSimpleWindow::ShowWindow( UI_SW_HIDE );
			else
			{
				//���� ���� ��������� ���� ��������
				nCmdShow = nAnimationCmdShow;
				rc.left = vBeginPos.x + vMinPos.x;
				rc.right = rc.left + wndRect.Width();
				rc.top = vBeginPos.y + vMinPos.y;
				rc.bottom = rc.top + wndRect.Height();
				SetScreenRect( rc );
				for ( CWindowList::iterator it=childList.begin(); it!=childList.end(); ++it )
					(*it)->Reposition( rc );
			}

			if ( pParent )
			{
				//����������� �� ��������� ��������
				SUIMessage msg;
				msg.nMessageCode = UI_NOTIFY_ANIMATION_FINISHED;
				msg.nFirst = nID;
				msg.nSecond = nCmdShow;
				pParent->ProcessMessage( msg );
			}
			return true;
		}
	}
	
	for ( CWindowList::iterator it=childList.begin(); it!=childList.end(); ++it )
		(*it)->Update( currTime );
	
	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMultipleWindow::SetFocus( bool bFocus )
{
	CSimpleWindow::SetFocus( bFocus );
	if ( !bFocus )
	{
		//��������� ���� � ������ �����
		if ( pFocused )
		{
			pFocused->SetFocus( false );
		}
	}

/*
	if ( bFocus )
		return;

	if ( pFocused )
	{
		pFocused->SetFocus( bFocus );
//		pFocused = 0;
	}
*/

/*
	//��������� ������������ ���� �������
	for ( CWindowList::iterator it = childList.begin(); it != childList.end(); ++it )
	{
		(*it)->SetFocus( bFocus );
	}
*/
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMultipleWindow::Reposition( const CTRect<float> &rcParent )
{
	CSimpleWindow::Reposition( rcParent );
	vBeginPos.x = wndRect.x1;
	vBeginPos.y = wndRect.y1;
	
	if ( bAnimation )
	{
		if ( nCmdShow == UI_SW_MINIMIZE )
		{
			wndRect.x1 += vMinPos.x;
			wndRect.x2 += vMinPos.x;
			wndRect.y1 += vMinPos.y;
			wndRect.y2 += vMinPos.y;
		}
		else if ( nCmdShow == UI_SW_MAXIMIZE || nCmdShow == UI_SW_SHOW || nCmdShow == UI_SW_LAST )
		{
			wndRect.x1 += vMaxPos.x;
			wndRect.x2 += vMaxPos.x;
			wndRect.y1 += vMaxPos.y;
			wndRect.y2 += vMaxPos.y;
		}
	}

	
	for ( CWindowList::iterator it = childList.begin(); it != childList.end(); ++it )
		(*it)->Reposition( GetScreenRect() );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMultipleWindow::Visit( interface ISceneVisitor *pVisitor )
{
	if ( !IsVisible() )
		return;
	CSimpleWindow::Visit( pVisitor );
	for ( CWindowList::reverse_iterator ri = childList.rbegin(); ri != childList.rend(); ++ri )
		(*ri)->Visit( pVisitor );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMultipleWindow::Draw( IGFX *pGFX )
{
	NI_ASSERT_SLOW_T( false, "Can't user Draw() directly - use visitor pattern" );
	return;

	if ( !IsVisible() )
		return;
	CSimpleWindow::Draw( pGFX );
	
	for ( CWindowList::reverse_iterator ri = childList.rbegin(); ri != childList.rend(); ++ri )
		(*ri)->Draw( pGFX );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IText* CMultipleWindow::GetHelpContext( const CVec2 &vPos, CTRect<float> *pRect )
{
	if ( !IsVisible() )
		return 0;
	
	for ( CWindowList::iterator it=childList.begin(); it!=childList.end(); ++it )
	{
		if ( (*it)->IsVisible() && (*it)->IsInside( vPos ) )
		{
			return (*it)->GetHelpContext( vPos, pRect );
		}
	}
	
	return CSimpleWindow::GetHelpContext( vPos, pRect );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMultipleWindow::SetFocusedWindow( IUIElement *pWnd )
{
	if ( pWnd )
	{
		pFocused = pWnd;
		if ( GetParent() )
		{
			IUIElement *pNewFocus = dynamic_cast<IUIElement *> ( this );
			GetParent()->SetFocusedWindow( pNewFocus );
		}
	}

/*
	SUIMessage msg;
	msg.nMessageCode = UI_SHOW_WINDOW;
	msg.nFirst = pWnd->GetWindowID();
	msg.nSecond = UI_SW_MAXIMIZE;
	messageList.push_back( msg );
*/
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMultipleWindow::EnableWindow( bool bEnable )
{
	CSimpleWindow::EnableWindow( bEnable );
/*
	for ( CWindowList::iterator it=childList.begin(); it!=childList.end(); ++it )
		(*it)->EnableWindow( bEnable );
*/
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMultipleWindow::OnMouseMove( const CVec2 &vPos, EMouseState mState )
{
	if ( bModal )
	{
		//������ ������� �������
		IUIElement *pModalElement = GetFirstModal();
		if ( pModalElement )
			return pModalElement->OnMouseMove( vPos, mState );
		else if ( !childList.empty() )
		{
			return childList.front()->OnMouseMove( vPos, mState );
		}
		else
		{
			if ( GetParent() == 0 )
				return false;
			else
				return true;
		}
	}

	//���� ����� ������ ����� �� ������
	if ( mState == E_MOUSE_FREE )
	{
		if ( !IsInside( vPos ) )
		{
			//����� ��� ������
			if ( pHighlighted )
			{
				pHighlighted->OnMouseMove( vPos, mState );
				pHighlighted = 0;
				return true;
			}
			return false;
		}
		else
		{
			//������ ����� ������������ ������
			IUIElement *pNewH = 0;
			for ( CWindowList::iterator it=childList.begin(); it!=childList.end(); ++it )
			{
				if ( (*it)->IsVisible() && (*it)->IsInside( vPos ) )
				{
					pNewH = *it;
					break;
				}
			}
			if ( pHighlighted && pHighlighted.GetPtr() != pNewH )
			{
				//���������� ������������ ������ ������� ����� �����
//				pHighlighted->OnMouseMove( vPos, mState );
				pHighlighted->OnMouseMove( CVec2(-1, -1), mState );
			}
		pHighlighted = pNewH;
			if ( pHighlighted )
				return pHighlighted->OnMouseMove( vPos, mState );

/*
			if ( pHighlighted && pHighlighted->IsInside( vPos ) )
			{
				//����� ������ ������������� ����
				return pHighlighted->OnMouseMove( vPos, mState );
			}
			else
			{
				//����� ��� ������������� ����, ������ ����� ������������ ������
				if ( pHighlighted )
				{
					pHighlighted->OnMouseMove( vPos, mState );
					pHighlighted = 0;
					return true;
				}
				for ( CWindowList::iterator it=childList.begin(); it!=childList.end(); ++it )
				{
					if ( (*it)->IsVisible() && (*it)->IsInside( vPos ) )
					{
						pHighlighted = *it;
						return pHighlighted->OnMouseMove( vPos, mState );
					}
				}
			}
*/
			
			if ( GetParent() == 0 )
				return false;
			else
				return true;
		}
	}
	
	//���� ����� ��� ������ ������ ����� ������
	if ( mState & E_LBUTTONDOWN || mState & E_RBUTTONDOWN )
	{
		if ( !IsInside( vPos ) )
		{
			//����� ��� ������
			if ( pPushed )
			{
				pPushed->OnMouseMove( vPos, mState );
				return true;
			}
		}
		else
		{
			//����� ������ ������
			if ( pPushed && pPushed->OnMouseMove( vPos, mState ) )
				return true;
			if ( GetParent() )
				return true;
		}
	}
	
	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMultipleWindow::OnMouseWheel( const CVec2 &vPos, EMouseState mouseState, float fDelta )
{
	if ( pHighlighted )
	{
		bool bRes = pHighlighted->OnMouseWheel( vPos, mouseState, fDelta );
		if ( bRes )
			return bRes;
	}

	if ( bModal )
	{
		//������ ������� �������
		IUIElement *pModalElement = GetFirstModal();
		if ( pModalElement )
			return pModalElement->OnMouseWheel( vPos, mouseState, fDelta );
		else if ( !childList.empty() )
		{
			return childList.front()->OnMouseWheel( vPos, mouseState, fDelta );
		}
		else
		{
			if ( GetParent() == 0 )
				return false;
			else
				return true;
		}
	}
	
/*
	//���� �� ����� ����������, ����� ������ � ������� ������������ Mouse Wheel, �� �� ������
	//�������� ������� ��������� OnMouseWheel() � UIScrollText.cpp, UIList.cpp, UIShortcutBar.cpp
	if ( pFocused )
	{
		return pFocused->OnMouseWheel( vPos, mouseState, fDelta );
	}
*/

	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMultipleWindow::OnLButtonDblClk( const CVec2 &vPos )
{
	if ( pHighlighted )
	{
		return pHighlighted->OnLButtonDblClk( vPos );
	}
	return CSimpleWindow::OnLButtonDblClk( vPos );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IUIElement * CMultipleWindow::GetFirstModal()
{
	for ( CWindowList::iterator it = childList.begin(); it != childList.end(); ++it )
	{
		if ( (*it)->IsModal() )
			return *it;
	}
	return 0;
}		
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMultipleWindow::OnLButtonDown( const CVec2 &vPos, EMouseState mouseState )
{
	if ( bModal )
	{
		//������ ������� modal �������
		IUIElement *pModalElement = GetFirstModal();
		if ( pModalElement )
			return pModalElement->OnLButtonDown( vPos, mouseState );
		else if ( !childList.empty() )
		{
			childList.front()->OnLButtonDown( vPos, mouseState );
			return true;
		}
	}

	pPushed = 0;
	if ( !IsInside( vPos ) )
	{
		if ( pFocused )
		{
			pFocused->SetFocus( false );
			pFocused = 0;
		}
/*
		//����� ��� ������
		if ( !childList.empty() )
			childList.back()->OnKillFocus();
*/
		return false;
	}
	else
	{
		//����� ������ ������, ������� ���������� ������ ��� ������
		for ( CWindowList::iterator it=childList.begin(); it!=childList.end(); ++it )
		{
			if ( (*it)->IsVisible() && (*it)->IsInside( vPos ) )
			{
				if ( !(*it)->IsWindowEnabled() )
				{
					return true;
				}
				else
				{
					//������� ������ ������ zorder
					pPushed = *it;
					if ( pFocused )
						pFocused->SetFocus( false );
					pFocused = pPushed;
					pFocused->SetFocus( true );
					CObj<IUIElement> pTemp = *it;
					childList.erase( it );
					childList.push_front( pTemp );
					return pPushed->OnLButtonDown( vPos, mouseState );
				}
			}
		}
		//�� ����� ������ � childs ��� ������
		if ( pFocused )
		{
			pFocused->SetFocus( false );
			pFocused = 0;
		}
	}
	if ( GetParent() )
		return true;			//CRAP		��� � ������ ���� ����������� ���������������� LButtonDown
	//����� ������ �������� �� ���������� child ��� ������
	//� multiple window ������ ������� true ���� ����� ������ ������ ����
	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMultipleWindow::OnLButtonUp( const CVec2 &vPos, EMouseState mouseState )
{
	if ( bModal )
	{
		IUIElement *pModalElement = GetFirstModal();
		if ( pModalElement )
			return pModalElement->OnLButtonUp( vPos, mouseState );
		else if ( !childList.empty() )
		{
			return childList.front()->OnLButtonUp( vPos, mouseState );
		}
	}

	if ( pPushed )
	{
		pPushed->OnLButtonUp( vPos, mouseState );
		pPushed = 0;
		return true;
	}
	
	return false;

//	return OnMouseMove( vPos, mouseState );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMultipleWindow::OnRButtonDown( const CVec2 &vPos, EMouseState mouseState )
{
	if ( bModal )
	{
		//������ ������� �������
		IUIElement *pModalElement = GetFirstModal();
		if ( pModalElement )
			return pModalElement->OnRButtonDown( vPos, mouseState );
		else if ( !childList.empty() )
		{
			return childList.front()->OnRButtonDown( vPos, mouseState );
		}
	}

	pRPushed = 0;
	if ( !IsInside( vPos ) )
	{
		if ( pFocused )
		{
			pFocused->SetFocus( false );
			pFocused = 0;
		}
/*
		//����� ��� ������
		if ( !childList.empty() )
			childList.back()->OnKillFocus();
*/
		return false;
	}
	else
	{
		//����� ������ ������, ������� ���������� ������ ��� ������
		for ( CWindowList::iterator it=childList.begin(); it!=childList.end(); ++it )
		{
			if ( (*it)->IsVisible() && (*it)->IsInside( vPos ) )
			{
				if ( !(*it)->IsWindowEnabled() )
				{
					return true;
				}
				else
				{
					//������� ������ ������ zorder
					pRPushed = *it;
					if ( pFocused && pFocused != pRPushed )
						pFocused->SetFocus( false );
					pFocused = pRPushed;
					pFocused->SetFocus( true );
					childList.push_front( *it );
					childList.erase( it );
					return pRPushed->OnRButtonDown( vPos, mouseState );
				}
			}
		}
		//�� ����� ������ � childs ��� ������
		if ( pFocused )
		{
			pFocused->SetFocus( false );
			pFocused = 0;
		}
		
		if ( GetParent() )
			return true;			//CRAP		��� � ������ ���� ����������� ���������������� RButtonDown
		//����� ������ �������� �� ���������� child ��� ������
		//� multiple window ������ ������� true ���� ����� ������ ������ ����
	}

	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMultipleWindow::OnRButtonUp( const CVec2 &vPos, EMouseState mouseState )
{
	if ( bModal )
	{
		//������ ������� �������
		IUIElement *pModalElement = GetFirstModal();
		if ( pModalElement )
			return pModalElement->OnRButtonUp( vPos, mouseState );
		else if ( !childList.empty() )
			return childList.front()->OnRButtonUp( vPos, mouseState );

	}

	if ( pRPushed )
	{
		pRPushed->OnRButtonUp( vPos, mouseState );
		pRPushed = 0;
		return true;
	}
	
	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMultipleWindow::OnChar( int nAsciiCode, int nVirtualKey, bool bPressed, DWORD keyState )
{
	if ( bModal )
	{
		//������ ������� �������
		if ( !childList.empty() )
		{
			return childList.front()->OnChar( nAsciiCode, nVirtualKey, bPressed, keyState );
		}
	}

	if ( !bPressed )
		return false;

	//��������� ������������ ������ � �������� ���� �����
	if ( pFocused )
	{
		if ( pFocused->OnChar( nAsciiCode, nVirtualKey, bPressed, keyState ) )
			return true;
	}

	//������ � ������� �� ���������� ���������, ��������� �������� ��� �� ������ �����
	for ( CWindowList::iterator it = childList.begin(); it != childList.end(); ++it )
	{
		if ( (*it)->OnChar( nAsciiCode, nVirtualKey, bPressed, keyState ) )
			return true;
	}

/*
	if ( !childList.empty() )
		return childList.front()->OnChar( nAsciiCode, nVirtualKey, bPressed, keyState );
*/

	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IUIElement* CMultipleWindow::PickElement( const CVec2 &vPos, int nRecursion )
{
	//��� ������� ������������ � ���������, ��� ����������� ���������� ��� ������
	if ( GetParent() == 0 )			//UIScreen, ������ ������� �� children
	{
		for ( CWindowList::iterator it = childList.begin(); it != childList.end(); ++it )
		{
			IUIElement *pElement = (*it)->PickElement( vPos, nRecursion - 1 );
			if ( pElement )
				return pElement;
		}
		return 0;
	}

	if ( IsVisible() && IsInside( vPos ) )
	{
		if ( nRecursion > 0 )
		{
			for ( CWindowList::iterator it = childList.begin(); it != childList.end(); ++it )
			{
				IUIElement *pElement = (*it)->PickElement( vPos, nRecursion - 1 );
				if ( pElement )
					return pElement;
			}
		}
		return dynamic_cast<IUIElement*>( this );
	}
	
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IUIElement* CMultipleWindow::GetChildByID( int nChildID )
{
	//������� ����� �����
	for ( CWindowList::iterator it=childList.begin(); it!=childList.end(); ++it )
	{
		if ( (*it)->GetWindowID() == nChildID )
			return *it;
	}

	//������� ����� ����� �����
	for ( CWindowList::iterator it=childList.begin(); it!=childList.end(); ++it )
	{
		IUIContainer *pContainer = dynamic_cast<IUIContainer *> ( it->GetPtr() );
		if ( pContainer )
		{
			IUIElement *pElement = pContainer->GetChildByID( nChildID );
			if ( pElement )
				return pElement;
		}
	}
	
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
IUIElement* CMultipleWindow::GetChildByIndex( int nIndex )
{
	int i = 0;
	for ( CWindowList::iterator it = childList.begin(); it != childList.end(); ++it, ++i )
	{
		if ( i == nIndex )
			return *it;
	}
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMultipleWindow::MoveWindowUp( IUIElement *pWnd )
{
	for ( CWindowList::iterator it=childList.begin(); it!=childList.end(); ++it )
	{
		if ( (*it).GetPtr() == pWnd )
		{
			childList.push_front( *it );
			childList.erase( it );
			return;
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMultipleWindow::MoveWindowDown( IUIElement *pWnd )
{
	for ( CWindowList::iterator it=childList.begin(); it!=childList.end(); ++it )
	{
		if ( (*it).GetPtr() == pWnd )
		{
			childList.push_back( *it );
			childList.erase( it );
			return;
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMultipleWindow::ProcessMessage( const SUIMessage &_msg )
{
	bool bRet = false;
	messageList.push_back( _msg );

	//������ ������������ ���������
	while ( !messageList.empty() )
	{
		const SUIMessage msg = messageList.front();
		messageList.pop_front();

		//CRAP{ KOSTILI for hepl button
		if ( msg.nMessageCode == UI_NOTIFY_STATE_CHANGED_MESSAGE && msg.nFirst == TUTORIAL_BUTTON_ID )
		{
			GetSingleton<IInput>()->AddMessage( SGameMessage(PROCESSED_FLAG | TUTORIAL_BUTTON_ID) );
			continue;
		}
		//CRAP}

		if ( msg.nMessageCode == 268435457 )
			int k = 0;
		
		if ( IsProcessedMessage( msg ) )
		{
			//������ ��� ������� ������ ���� ������ ��� ���������
			if ( GetParent() == 0 )
			{
				// ��� ������ ���� Screen!!! (�.�. � ���� ���� �������)
				NI_ASSERT_SLOW( dynamic_cast<IUIScreen*>(this) != 0 );
				ProcessMessage( msg );			//������ ��� ������ ���� screen
			}
			else
				GetParent()->ProcessMessage( msg );

			bRet = true;
			continue;
		}
		
		//������ ��������� �������������� ����� multiple window
		switch( msg.nMessageCode )
		{
			case UI_SET_STATE_WO_NOTIFY:
				{
					IUIElement *pElement = GetChildByID( msg.nFirst );
					NI_ASSERT_T( pElement != 0, NStr::Format( "There is no control with id %d", msg.nFirst ) );
					pElement->SetState( msg.nSecond, false );
					bRet = true;
					continue;
				}
				break;
			case UI_SET_STATE_MESSAGE:
				{
					IUIElement *pElement = GetChildByID( msg.nFirst );
					NI_ASSERT_T( pElement != 0, NStr::Format( "There is no control with id %d", msg.nFirst ) );
					pElement->SetState( msg.nSecond );
					bRet = true;
					continue;
				}
			case UI_NEXT_STATE_MESSAGE:
				{
					IUIElement *pElement = GetChildByID( msg.nFirst );
					pElement->SetState( pElement->GetState() + 1 );
					bRet = true;
					continue;
				}
			case UI_DISABLE_WINDOW_FORCE:
				{
					IUIElement *pElement = GetChildByID( msg.nFirst );
					pElement->EnableWindow( false );
					bRet = true;
					continue;
				}
			case UI_ENABLE_WINDOW_FORCE:
				{
					IUIElement *pElement = GetChildByID( msg.nFirst );
					pElement->EnableWindow( true );
					bRet = true;
					continue;
				}
			case UI_ENABLE_WINDOW:
				{
					IUIElement *pElement = GetChildByID( msg.nFirst );
					pElement->EnableWindow( msg.nSecond );
					bRet = true;
					continue;
				}
			case UI_BLINK_WINDOW:
				{
					IUIElement *pElement = GetChildByID( msg.nFirst );
					CSimpleWindow *pWindow = dynamic_cast<CSimpleWindow *>( pElement );
					if ( pWindow /* && ( pWindow->nBlink & 4 ) */ )
						pWindow->BlinkMe( (msg.nSecond) & 0xffff, (msg.nSecond>>16) & 0xffff );
					bRet = true;
					continue;
				}
			
				//it's OK, break is not present not to copy code.
			case UI_MODAL_FLAG_FORCE_REMOVE:
			case UI_MODAL_FLAG_FORCE_SET:
				bModal = UI_MODAL_FLAG_FORCE_SET == msg.nMessageCode;
			case UI_SET_MODAL_FLAG:
				if ( msg.nMessageCode == UI_SET_MODAL_FLAG )
					bModal = msg.nSecond;

				bRet = true;
				//������� ������� ������������ � ������� ������
				if ( pHighlighted )
				{
					pHighlighted->OnMouseMove( CVec2(-1, -1), E_MOUSE_FREE );
					pHighlighted = 0;
				}
				if ( pPushed )
				{
					pPushed->OnMouseMove( CVec2(-1, -1), E_MOUSE_FREE );
					pPushed = 0;
				}
				{
				//��������� ������� ����������� ������� � ���� �����
				CTRect<float> screenRect = GetSingleton<IGFX>()->GetScreenRect();
				GetSingleton<ICursor>()->SetBounds( 0, 0, screenRect.Width(), screenRect.Height() );
				}
				
				continue;
			case UI_HIDE_WINDOW_FORCE:
				if ( IUIElement *pElement = GetChildByID(msg.nFirst) )
				{
					pElement->ShowWindow( UI_SW_HIDE );
					bRet = true;
				}
				continue;
			case UI_SHOW_WINDOW_FORCE:
				if ( IUIElement *pElement = GetChildByID(msg.nFirst) )
				{
					pElement->ShowWindow( UI_SW_SHOW );
					bRet = true;
				}
				continue;

			case UI_SHOW_WINDOW:
				if ( IUIElement *pElement = GetChildByID(msg.nFirst) )
				{
					pElement->ShowWindow( msg.nSecond );
					bRet = true;
				}
				continue;
		}

		//����� LUA
		if ( bLua )
		{
			luaScript.GetGlobal( "LuaProcessMessage" );
			luaScript.PushNumber( msg.nMessageCode );
			luaScript.PushNumber( msg.nFirst );
			luaScript.PushNumber( msg.nSecond );
			int nRes = luaScript.Call( 3, 1 );			//�������� LUA �������, 3 ���������, 1 ���������
			NI_ASSERT_T( nRes == 0, "LUA script call LuaProcessMessage failed" );
			
			nRes = luaScript.GetObject( -1 );
			luaScript.Pop();									// remove return value from the LUA stack
			if ( nRes )
			{
				while ( !staticMessageList.empty() )
				{
					const SUIMessage &luaMsg = staticMessageList.front();
					messageList.push_back( luaMsg );
					staticMessageList.pop_front();
				}
				bRet = true;
				continue;					//��������� ������������ � LUA
			}
			else
				staticMessageList.clear();
		}

		if ( IsNotifyParentMessage( msg ) )
		{
			//������ ��� ������� ����� �� ����������, �� �� ����� ���������� �����
			//�������� ������� ����� ��� ���������
			IUIElement *pPapa = GetParent();
			if ( pPapa )
				GetParent()->ProcessMessage( msg );
			
			bRet = true;
			continue;
		}

		if ( !IsNotifyMessage( msg ) )					//��� notify ��������� ������ ������������ ������ LUA
		{
			//������ ���� ���������� ��� ���������
			for ( CWindowList::iterator child = childList.begin(); child != childList.end(); ++child )
			{
				if ( (*child)->ProcessMessage( msg ) != false )
				{
					bRet = true;
					break;
				}
			}
		}
	}

	return bRet;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CMultipleWindow::AddMessage( lua_State *pLuaState )
{
	Script script( pLuaState );
/*
	SUIMessage msg;
	msg.nMessageCode = nMessageCode;
	msg.nFirst = nFirst;
	msg.nSecond = nSecond;
	staticMessageList.push_back( msg );
*/

	int nNumberOfParams = script.GetTop();			//����� ����������
	NI_ASSERT_SLOW_T( nNumberOfParams == 3, "The number of parameters for function AddMessage should be 3" );
	NI_ASSERT_SLOW_T( script.IsNumber( -3 ), "Script error in AddMessage: the 1st parameter isn't a number" );
	NI_ASSERT_SLOW_T( script.IsNumber( -2 ), "Script error in AddMessage: the 2nd parameter isn't a number" );
	NI_ASSERT_SLOW_T( script.IsNumber( -1 ), "Script error in AddMessage: the 3rd parameter isn't a number" );

	SUIMessage msg;
	msg.nMessageCode = script.GetObject( -3 );
	msg.nFirst = script.GetObject( -2 );
	msg.nSecond = script.GetObject( -1 );
	
#ifdef _DEBUG
	if ( msg.nMessageCode == UI_NEXT_STATE_MESSAGE )
		int i = 0;
#endif		//_DEBUG

	staticMessageList.push_back( msg );					//� �������� ProcessMessage � ������ �������� ��� ����� ��������� �� ����� ������

	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CMultipleWindow::SaveLuaValue( lua_State *pLuaState )
{
	Script script( pLuaState );

	int nNumberOfParams = script.GetTop();			//����� ����������
	NI_ASSERT_SLOW_T( nNumberOfParams == 2, "The number of parameters for function AddMessage should be 2" );
	NI_ASSERT_SLOW_T( script.IsNumber( -2 ), "Script error in AddMessage: the 1st parameter isn't a number" );
	NI_ASSERT_SLOW_T( script.IsNumber( -1 ), "Script error in AddMessage: the 2nd parameter isn't a number" );

	SLuaValue val;
	val.nID = script.GetObject( -2 );
	val.nVal = script.GetObject( -1 );
	staticLuaValues.push_back( val );

	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMultipleWindow::IsInsideChild( const CVec2 &_vPos )
{
	for ( CWindowList::iterator it = childList.begin(); it != childList.end(); ++it )
	{
		if ( (*it)->IsInside( _vPos ) )
			return true;
	}
	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
