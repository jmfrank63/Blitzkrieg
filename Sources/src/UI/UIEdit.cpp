#include "StdAfx.h"

#include "..\Main\iMainCommands.h"
#include "..\GameTT\iMission.h"
#include "UIEdit.h"
#include "UIMessages.h"

const int CURSOR_ANIMATION_TIME = 400;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIEditBox::OnMouseMove( const CVec2 &vPos, EMouseState mouseState )
{
	bool bRes = CSimpleWindow::OnMouseMove( vPos, mouseState );
	
	if ( mouseState == E_MOUSE_FREE )
		return bRes;

	//��� ������, ��� bRes true ����� �������� ����� ���� ����������, � ������ ����� ��� ������ ������, ��� ������ ����������� ����
	//���� ����� ������ ����� ������
	if ( mouseState & E_LBUTTONDOWN )
	{
		nCursorPos = GetSelection( vPos.x );
		if ( nBeginText + nCursorPos > m_nBeginDragSel )
		{
			m_nBeginSel = m_nBeginDragSel;
			m_nEndSel = nBeginText + nCursorPos;
		}
		else
		{
			m_nBeginSel = nBeginText + nCursorPos;
			m_nEndSel = m_nBeginDragSel;
		}
		EnsureCursorVisible();
	}

	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIEditBox::OnLButtonDown( const CVec2 &vPos, EMouseState mouseState )
{
	bool bRes = CSimpleWindow::OnLButtonDown( vPos, mouseState );
	if ( !bRes )
		return bRes;
	
	//��� ������, ��� bRes true ����� �������� ����� ���� ����������, � ������ ����� ��� ������ ������, ��� ������ ����������� ����
	//���� ����� ������ ����� ������
	if ( mouseState & E_LBUTTONDOWN )
	{
		NI_ASSERT( IsInside( vPos ) );
		nCursorPos = GetSelection( vPos.x );
		m_nBeginDragSel = m_nBeginSel = m_nEndSel = nCursorPos + nBeginText;
	}
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIEditBox::OnRButtonDown( const CVec2 &vPos, EMouseState mouseState )
{
	bool bRes = CSimpleWindow::OnRButtonDown( vPos, mouseState );
	if ( !bRes )
		return bRes;
	
	//��� ������, ��� bRes true ����� �������� ����� ���� ����������, � ������ ����� ��� ������ ������, ��� ������ ����������� ����
	//���� ������ ������ ����� ������
	if ( mouseState & E_LBUTTONDOWN )
	{
		NI_ASSERT( IsInside( vPos ) );
		nCursorPos = GetSelection( vPos.x );
		m_nBeginDragSel = m_nBeginSel = m_nEndSel = nCursorPos + nBeginText;
	}
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CUIEditBox::GetSelection( int nX )
{
	NI_ASSERT_T( (nTextAlign & 0xf) == UIPLACE_LEFT, "Error: EditBox supports only UIPLACE_LEFT (0x01) alignment, mouse selection may be bug" );

	int nCur = 0, nPrev = 0;
	int i = 0;
	for ( ; i<=wszFullText.size(); i++ )
	{
		nCur = states[nCurrentState].pGfxText->GetWidth( i );
		nCur += wndRect.left + vTextPos.x;
		if ( nCur > nX )
		{
			if ( nX - nPrev < nCur - nX && i > 0 )			//����� � ����� ����� ��� � ������
				i--;
			break;
		}
		nPrev = nCur;
	}

	if ( nCur <= nX )		//������ ������ ���� ������
		i--;
	NI_ASSERT_T( i >= 0 && i <= wszFullText.size(), "Error in CUIEditBox::GetSelection()" );
	return i;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIEditBox::SetCursor( int nPos )
{
	if ( nPos < 0 )
		nCursorPos = wszFullText.length();
	else
		nCursorPos = nPos; 
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIEditBox::SetFocus( bool bFocus )
{
	//����������� � ����� ������
	CSimpleWindow::SetFocus( bFocus );
/*
	if ( bFocus )
	{
		CMultipleWindow *pPapa = dynamic_cast<CMultipleWindow *> ( pParent.GetPtr() );
		if ( pPapa )
		{
			IUIElement *pElement = dynamic_cast<IUIElement *> ( this );
			pPapa->SetFocusedWindow( pElement );
		}
	}
*/

	//�������� ������ ��������� ����� ������������ TEXT_MODE
	SUIMessage msg;
	if ( bFocus )
	{
		bFocused = true;
		//������������ text mode
		msg.nMessageCode = MC_SET_TEXT_MODE | PROCESSED_FLAG;
		msg.nFirst = GetWindowID();
	}
	else
	{
		bFocused = false;
		//������ text mode
		msg.nMessageCode = MC_CANCEL_TEXT_MODE | PROCESSED_FLAG;
		msg.nFirst = GetWindowID();
		
		//��������� selection
		m_nBeginSel = m_nEndSel = -1;
	}
	GetParent()->ProcessMessage( msg );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIEditBox::DeleteSelection()
{
	if ( m_nEndSel == m_nBeginSel )
		return false;

	if ( m_nBeginSel != -1 )
	{
		if ( m_nEndSel < 0 || m_nEndSel > wszFullText.size() )
			m_nEndSel = wszFullText.size();
		if ( m_nBeginSel > m_nEndSel )
			m_nBeginSel = m_nEndSel;
		wszFullText.erase( m_nBeginSel, m_nEndSel - m_nBeginSel );
		nCursorPos = m_nBeginSel - nBeginText;
		m_nBeginSel = m_nEndSel = -1;
		return true;
	}

	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIEditBox::IsValidSymbol( int nAsciiCode )
{
	
	if ( bGameSpySymbols )
	{
		//��������, ��� ������ ������������� ����������� GameSpy NickName
		static const char szValidSymbols[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789[]\\`_^{|}-";
		static const int nLen = strlen( szValidSymbols );
		for ( int i=0; i<nLen; i++ )
		{
			if ( nAsciiCode == szValidSymbols[i] )
				return true;
		}
		return false;
	}
	
	if ( bFileNameSymbols )
	{
		//��������, ��� ������ ������������� ����������� FileName symbols
		static const char szValidSymbols[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789[]`_^{}-!@#$%^&()+=~";
		static const int nLen = strlen( szValidSymbols );
		for ( int i=0; i<nLen; i++ )
		{
			if ( nAsciiCode == szValidSymbols[i] )
				return true;
		}
		return false;
	}

	if ( bNumericMode )
	{
		if ( nAsciiCode >= '0' && nAsciiCode <= '9' )
			return true;
		return false;
	}

	if ( bLocalPlayerNameMode )
	{
		static const char szInValidSymbols[] = "&'\"<>";
		static const int nLen = strlen( szInValidSymbols );
		for ( int i = 0; i < nLen; i++ )
		{
			if ( nAsciiCode == szInValidSymbols[i] )
				return false;
		}
	}
	
	return nAsciiCode >= 32;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIEditBox::ProcessMessage( const SUIMessage &msg ) 
{ 
	if ( CMD_NUMPAD_ENTER == msg.nMessageCode )
		return OnChar( 0, VK_RETURN, true, E_KEYBOARD_FREE );
	return CSimpleWindow::ProcessMessage( msg );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIEditBox::OnChar( int nAsciiCode, int nVirtualKey, bool bPressed, DWORD keyState )
{
	if ( !IsVisible() )
		return false;
	
	if ( !bPressed )
		return false;
	NotifyTextChanged();

	//���� �������� ������, �� ������ ������� ���
	//	if ( isprint( nAsciiCode ) )
	std::wstring wszOldText = wszFullText;
	int nOldCursorPos = nCursorPos;
	if ( ( keyState == E_KEYBOARD_FREE || keyState == E_SHIFT_KEY_DOWN ) && IsValidSymbol(nAsciiCode) )
	{
		DeleteSelection();
		wszFullText.insert( Min( int(wszFullText.size()), nBeginText+nCursorPos), 1, nAsciiCode );
		nCursorPos++;
		if ( !IsTextInsideEditBox() )
		{
			wszFullText = wszOldText;
			nCursorPos = nOldCursorPos;
		}
		EnsureCursorVisible();
		return true;
	}
	
	if ( nVirtualKey == VK_TAB && !bNumericMode && !bGameSpySymbols && !bFileNameSymbols && ( keyState == E_KEYBOARD_FREE || keyState == E_SHIFT_KEY_DOWN ) )
	{
		DeleteSelection();
		wszFullText.insert( nBeginText+nCursorPos, 4, VK_SPACE );
		nCursorPos += 4;
		if ( !IsTextInsideEditBox() )
		{
			wszFullText = wszOldText;
			nCursorPos = nOldCursorPos;
		}
		EnsureCursorVisible();
		return true;
	}
	
	SUIMessage msg;
	//���� �� �������� ������, �� ������������ �������������� ����������
	switch( nVirtualKey )
	{
	case VK_RETURN:
		if ( E_KEYBOARD_FREE == keyState )
		{
			SetFocus( false );		//��������� ����� � ������ �������� text mode 
			msg.nMessageCode = UI_NOTIFY_EDIT_BOX_RETURN;
			msg.nFirst = GetWindowID();
			msg.nSecond = 0;
			GetParent()->ProcessMessage( msg );
		}
		break;

	case VK_BACK:
		if ( !DeleteSelection() && nBeginText+nCursorPos > 0 )
		{
			wszFullText.erase( nBeginText+nCursorPos-1, 1 );
			nCursorPos--;
			EnsureCursorVisible();
		}
		break;

	case VK_DELETE:
		if ( !DeleteSelection() && nBeginText+nCursorPos < wszFullText.size() )
		{
			wszFullText.erase( nBeginText+nCursorPos, 1 );
			EnsureCursorVisible();
		}
		break;

	case VK_LEFT:
		m_nBeginSel = m_nEndSel = -1;
		if ( nBeginText+nCursorPos == 0 )
			break;
		if ( keyState == E_KEYBOARD_FREE )
		{
			//�� ���� ������� �����
			nCursorPos--;
			EnsureCursorVisible();
		}
		if ( keyState & E_CTRL_KEY_DOWN )
		{
			//���� ������ crtl � ������� �����, �� ���������� ����� �� ���� �����
			while( nBeginText+nCursorPos > 0 && isspace(wszFullText[nBeginText+nCursorPos-1]) )
				nCursorPos--;
			if ( nBeginText+nCursorPos > 0 )
			{
				if ( isalpha(wszFullText[nBeginText+nCursorPos-1]) )
					while( nBeginText+nCursorPos > 0 && isalpha(wszFullText[nBeginText+nCursorPos-1]) )
						nCursorPos--;
					else
						while( nBeginText+nCursorPos > 0 && !isalpha(wszFullText[nBeginText+nCursorPos-1]) )
							nCursorPos--;
			}
		}
		EnsureCursorVisible();
		break;

	case VK_RIGHT:
		m_nBeginSel = m_nEndSel = -1;
		if ( nBeginText+nCursorPos == wszFullText.size() )
			break;
		if ( keyState == E_KEYBOARD_FREE )
		{
			//�� ���� ������� ������
			nCursorPos++;
		}
		else if ( keyState & E_CTRL_KEY_DOWN )
		{
			//���� ������ crtl � ������� ������, �� ���������� ������ �� ���� �����
			if ( nBeginText+nCursorPos < wszFullText.size() )
			{
				if ( isalpha(wszFullText[nBeginText+nCursorPos]) )
					while( nBeginText+nCursorPos < wszFullText.size() && isalpha(wszFullText[nBeginText+nCursorPos]) )
						nCursorPos++;
					else
						while( nBeginText+nCursorPos < wszFullText.size() && !isalpha(wszFullText[nBeginText+nCursorPos]) )
							nCursorPos++;
			}
			
			while( nBeginText+nCursorPos < wszFullText.size() && isspace(wszFullText[nBeginText+nCursorPos]) )
				nCursorPos++;
		}
		EnsureCursorVisible();
		break;
		
	case VK_HOME:
		m_nBeginSel = m_nEndSel = -1;
		if ( keyState == E_KEYBOARD_FREE )
		{
			//�� ������ ������
			nBeginText = 0;
			nCursorPos = 0;
			EnsureCursorVisible();
		}
		break;
		
	case VK_END:
		m_nBeginSel = m_nEndSel = -1;
		if ( keyState == E_KEYBOARD_FREE )
		{
			//�� ����� ������
			nCursorPos = wszFullText.size() - nBeginText;
			EnsureCursorVisible();
		}
		break;
		
	case VK_ESCAPE:
		m_nBeginSel = m_nEndSel = -1;
		SetFocus( false );		//��������� ����� � ������ �������� text mode 
		msg.nMessageCode = UI_NOTIFY_EDIT_BOX_ESCAPE;
		msg.nFirst = GetWindowID();
		msg.nSecond = 0;
		GetParent()->ProcessMessage( msg );
		return true;
		
	default:
		return false;
	}

	if ( !IsTextInsideEditBox() )
	{
		wszFullText = wszOldText;
		nCursorPos = nOldCursorPos;
		EnsureCursorVisible();
	}

	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CUIEditBox::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.AddTypedSuper( static_cast<CSimpleWindow*>(this) );
	saver.Add( "LastAnim", &dwLastCursorAnimatedTime );
	saver.Add( "ShowCursor", &bShowCursor );
	saver.Add( "Focus", &bFocused );
	saver.Add( "SelColor", &dwSelColor );
	saver.Add( "TextScroll", &bTextScroll );
	saver.Add( "NumericMode", &bNumericMode );
	saver.Add( "MaxLength", &nMaxLength );
	saver.Add( "GameSpySymbols", &bGameSpySymbols );
	saver.Add( "FileNameSymbols", &bFileNameSymbols );
	saver.Add( "LocalPlayerName", &bLocalPlayerNameMode );
	
	if ( saver.IsReading() )
	{
		//������� IText � ���������� ��� � pGfxText
		for ( int i=0; i<states.size(); i++ )
		{
			IText *pText = CreateObject<IText>( TEXT_STRING );
			states[i].pGfxText->SetText( pText );
		}
	}
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CUIEditBox::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.AddTypedSuper( 1, static_cast<CSimpleWindow*>(this) );
	saver.Add( 2, &dwLastCursorAnimatedTime );
	saver.Add( 3, &bShowCursor );
	saver.Add( 4, &bFocused );
	saver.Add( 5, &nCursorPos );
	saver.Add( 6, &dwSelColor );
	saver.Add( 7, &m_nBeginSel );
	saver.Add( 8, &m_nEndSel );
	saver.Add( 9, &m_nBeginDragSel );
	saver.Add( 10, &wszFullText );
	saver.Add( 11, &nBeginText );
	saver.Add( 12, &bTextScroll );
	saver.Add( 13, &bNumericMode );
	saver.Add( 14, &nMaxLength );
	saver.Add( 15, &bGameSpySymbols );
	saver.Add( 16, &bFileNameSymbols );
	saver.Add( 17, &bLocalPlayerNameMode );
	
	if ( saver.IsReading() )
	{
		//������� IText � ���������� ��� � pGfxText
		for ( int i=0; i<states.size(); i++ )
		{
			IText *pText = CreateObject<IText>( TEXT_STRING );
			states[i].pGfxText->SetText( pText );
		}
	}
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIEditBox::Visit( interface ISceneVisitor *pVisitor )
{
	if ( !nCmdShow )
		return;

	VisitBackground( pVisitor );
	
	// ������ ���������
	if ( m_nBeginSel != -1 && m_nBeginSel != m_nEndSel )
	{
		int nBegin = 0;
		if ( m_nBeginSel - nBeginText >= -1 )
			nBegin = states[nCurrentState].pGfxText->GetWidth( m_nBeginSel - nBeginText );
		int nEnd = states[nCurrentState].pGfxText->GetWidth( m_nEndSel - nBeginText );
		
		SGFXRect2 rc;
		rc.rect.left = wndRect.left + nBegin + vTextPos.x;
		rc.rect.right = wndRect.left + nEnd + vTextPos.x;
		if ( rc.rect.right > wndRect.right - 1 )
			rc.rect.right = wndRect.right - 1;
		int nH = states[nCurrentState].pGfxText->GetLineSpace();
		switch ( nTextAlign & 0xf0 )
		{
			case UIPLACE_TOP:
				rc.rect.top = 0;
				break;
			case UIPLACE_BOTTOM:
				rc.rect.top = wndRect.Height() - nH;
				break;
			case UIPLACE_VMID:
				rc.rect.top = (int) ( wndRect.Height() - nH ) / 2;
				break;
		}
		rc.rect.top += wndRect.top;
		rc.rect.bottom = rc.rect.top + nH;
		rc.maps.x1 = rc.maps.y1 = rc.maps.x2 = rc.maps.y2 = 0;
		
		if ( bBounded )
		{
			//��������, ����� ����� ������ ����� ���������
			float fTemp;
			fTemp = rcBound.y1 - rc.rect.y1;
			if ( fTemp > 0 )
			{
				rc.rect.y1 = rcBound.y1;
			}

			fTemp = rc.rect.y2 - rcBound.y2;
			if ( fTemp > 0 )
			{
				rc.rect.y2 = rcBound.y2;
			}
		}

		rc.color = dwSelColor;
		rc.fZ = 0;
		pVisitor->VisitUIRects( 0, 3, &rc, 1 );
	}
	
	// ������ �����
	VisitText( pVisitor );

	// ������ ������
	if ( bFocused && bShowCursor )
	{
		int nWidth = states[nCurrentState].pGfxText->GetWidth( nCursorPos );
		SGFXRect2 rc;
		rc.rect.left = wndRect.left + nWidth + vTextPos.x - 1;
		rc.rect.right = rc.rect.left + 2;
		if ( rc.rect.left < wndRect.right - 1 )
		{
			//������ �� ������� �� ���� ������
			int nH = states[nCurrentState].pGfxText->GetLineSpace();
			switch ( nTextAlign & 0xf0 )
			{
				case UIPLACE_TOP:
					rc.rect.top = 0;
					break;
				case UIPLACE_BOTTOM:
					rc.rect.top = wndRect.Height() - nH;
					break;
				case UIPLACE_VMID:
					rc.rect.top = (int) ( wndRect.Height() - nH ) / 2;
					break;
			}
			rc.rect.top += wndRect.top;
			rc.rect.bottom = rc.rect.top + nH;
			rc.maps.x1 = rc.maps.y1 = rc.maps.x2 = rc.maps.y2 = 0;
			
			if ( bBounded )
			{
				// ��������, ����� ����� ������ ����� �������
				float fTemp = rcBound.y1 - rc.rect.y1;
				if ( fTemp > 0 )
				{
					rc.rect.y1 = rcBound.y1;
				}
				
				fTemp = rc.rect.y2 - rcBound.y2;
				if ( fTemp > 0 )
				{
					rc.rect.y2 = rcBound.y2;
				}
			}

			rc.color = dwTextColor;
			rc.fZ = 0;
			pVisitor->VisitUIRects( 0, 3, &rc, 1 );
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIEditBox::Draw( IGFX *pGFX )
{
	NI_ASSERT_SLOW_T( false, "Can't user Draw() directly - use visitor pattern" );
	return;

	if ( !nCmdShow )
		return;

	pGFX->SetShadingEffect( 3 );
	DrawBackground( pGFX );
	
	//������ ���������
	if ( m_nBeginSel != -1 && m_nBeginSel != m_nEndSel )
	{
		int nBegin = 0;
		if ( m_nBeginSel - nBeginText >= -1 )
			nBegin = states[nCurrentState].pGfxText->GetWidth( m_nBeginSel - nBeginText );
		int nEnd = states[nCurrentState].pGfxText->GetWidth( m_nEndSel - nBeginText );
		
		SGFXRect2 rc;
		rc.rect.left = wndRect.left + nBegin + vTextPos.x;
		rc.rect.right = wndRect.left + nEnd + vTextPos.x;
		if ( rc.rect.right > wndRect.right - 1 )
			rc.rect.right = wndRect.right - 1;
		int nH = states[nCurrentState].pGfxText->GetLineSpace();
		switch ( nTextAlign & 0xf0 )
		{
		case UIPLACE_TOP:
			rc.rect.top = 0;
			break;
		case UIPLACE_BOTTOM:
			rc.rect.top = wndRect.Height() - nH;
			break;
		case UIPLACE_VMID:
			rc.rect.top = (int) ( wndRect.Height() - nH ) / 2;
			break;
		}
		rc.rect.top += wndRect.top;
		rc.rect.bottom = rc.rect.top + nH;
		rc.maps.x1 = rc.maps.y1 = rc.maps.x2 = rc.maps.y2 = 0;
		
		if ( bBounded )
		{
			//��������, ����� ����� ������ ����� ���������
			float fTemp;
			fTemp = rcBound.y1 - rc.rect.y1;
			if ( fTemp > 0 )
			{
				rc.rect.y1 = rcBound.y1;
			}

			fTemp = rc.rect.y2 - rcBound.y2;
			if ( fTemp > 0 )
			{
				rc.rect.y2 = rcBound.y2;
			}
		}

		rc.color = dwSelColor;
		rc.fZ = 0;
		pGFX->SetTexture( 0, 0 );
		pGFX->DrawRects( &rc, 1 );
	}
	
	//������ �����
	DrawText( pGFX );

	//������ ������
	if ( bFocused && bShowCursor )
	{
		int nWidth = states[nCurrentState].pGfxText->GetWidth( nCursorPos );
		SGFXRect2 rc;
		rc.rect.left = wndRect.left + nWidth + vTextPos.x - 1;
		rc.rect.right = rc.rect.left + 2;
		if ( rc.rect.left < wndRect.right - 1 )
		{
			//������ �� ������� �� ���� ������
			int nH = states[nCurrentState].pGfxText->GetLineSpace();
			switch ( nTextAlign & 0xf0 )
			{
			case UIPLACE_TOP:
				rc.rect.top = 0;
				break;
			case UIPLACE_BOTTOM:
				rc.rect.top = wndRect.Height() - nH;
				break;
			case UIPLACE_VMID:
				rc.rect.top = (int) ( wndRect.Height() - nH ) / 2;
				break;
			}
			rc.rect.top += wndRect.top;
			rc.rect.bottom = rc.rect.top + nH;
			rc.maps.x1 = rc.maps.y1 = rc.maps.x2 = rc.maps.y2 = 0;
			
			if ( bBounded )
			{
				//��������, ����� ����� ������ ����� �������
				float fTemp;
				fTemp = rcBound.y1 - rc.rect.y1;
				if ( fTemp > 0 )
				{
					rc.rect.y1 = rcBound.y1;
				}
				
				fTemp = rc.rect.y2 - rcBound.y2;
				if ( fTemp > 0 )
				{
					rc.rect.y2 = rcBound.y2;
				}
			}

			rc.color = dwTextColor;
			rc.fZ = 0;
			pGFX->SetTexture( 0, 0 );
			pGFX->DrawRects( &rc, 1 );
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIEditBox::Update( const NTimer::STime &currTime )
{
	if ( currTime - dwLastCursorAnimatedTime > CURSOR_ANIMATION_TIME )
	{
		dwLastCursorAnimatedTime = currTime;
		bShowCursor = !bShowCursor;
	}
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIEditBox::NotifyTextChanged()
{
	SUIMessage msg;
	msg.nMessageCode = UI_NOTIFY_EDIT_BOX_TEXT_CHANGED;
	msg.nFirst = GetWindowID();
	msg.nSecond = 0;
	GetParent()->ProcessMessage( msg );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIEditBox::SetWindowText( int nState, const WORD *pszText )
{
	static_assert( sizeof(wchar_t) == sizeof(WORD), "wchar_t and WORD size mismatch" );
	wszFullText.assign( reinterpret_cast<const wchar_t*>(pszText) );
	nBeginText = 0;
	nCursorPos = 0;
	m_nBeginDragSel = m_nBeginSel = m_nEndSel = -1;
	CSimpleWindow::SetWindowText( nState, pszText );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIEditBox::EnsureCursorVisible()
{
	IGFXText *pGFXText = states[nCurrentState].pGfxText;
	IText *pText = pGFXText->GetText();
	pText->SetText( wszFullText.c_str() + nBeginText );
	pGFXText->SetText( pText );

	if ( nCursorPos <= 0 && nBeginText > 0 )
	{
		NI_ASSERT_T( bTextScroll, "Edit box error: nCursorPos < 0 and bTextScroll == true" );
		//������ ����� ������ ���� edit box, �������� ����� ������, ����� ������ ���� �������
		nBeginText += nCursorPos;
		nCursorPos = 0;
		if ( nBeginText < 0 )
			nBeginText = 0;
		if ( nBeginText > 0 )
		{
			nBeginText--;
			nCursorPos++;
		}
		if ( nBeginText > 0 )
		{
			nBeginText--;
			nCursorPos++;
		}
		pText->SetText( wszFullText.c_str() + nBeginText );
		pGFXText->SetText( pText );
	}
	else if ( pGFXText->GetWidth( nCursorPos ) > wndRect.Width() - vTextPos.x - 2 )
	{
		//������ ������ ������� ���� edit box, �������� ����� �����, ����� ������ ���� �������
		while ( pGFXText->GetWidth( nCursorPos ) > wndRect.Width() - vTextPos.x - 2 )		//2 is the width of cursor
		{
			if ( bTextScroll )
			{
				nBeginText++;
				nCursorPos--;
			}
			else
			{
				wszFullText.erase( wszFullText.size() - 1 );
			}
			pText->SetText( wszFullText.c_str() + nBeginText );
			pGFXText->SetText( pText );
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CUIEditBox::IsTextInsideEditBox()
{
	//������ �������� ����������� �� ������������ ����� ������
	if ( nMaxLength != -1 && wszFullText.size() > nMaxLength )
		return false;

	if ( bTextScroll )
		return true;
//	NI_ASSERT_T( bTextScroll == false, "Error calling IsTextInsideEditBox()" );
	IGFXText *pGFXText = states[nCurrentState].pGfxText;
	IText *pText = pGFXText->GetText();
	pText->SetText( wszFullText.c_str() + nBeginText );
	pGFXText->SetText( pText );
	return pGFXText->GetWidth( -1 ) <= wndRect.Width() - vTextPos.x - 2;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
