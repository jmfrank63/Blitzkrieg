
#include "StdAfx.h"
#include "WindowEditLine.h"

#include "../GFX/GFX.H"
#include "../Main/TextSystem.h"
#include "../Scene/Scene.h"
#include "IUIInternal.h"
#include "UIScreen.h"

const int CURSOR_ANIMATION_TIME = 800;
IMPLEMENT_CLONABLE(CWindowEditLine)
BASIC_REGISTER_CLASS(CWindowEditLine);
void CWindowEditLine::RegisteMessageSinks()
{
}
void CWindowEditLine::UnRegisteMessageSinks()
{
}
void CWindowEditLine::OnMouseMove( const CVec2 &_vPos, const int nButton )
{
	if ( m_nBeginDragSel != -1 ) // selection is in progress
	{	
		if ( nButton & MSTATE_BUTTON1 )
		{
			nCursorPos = GetSelection( _vPos.x );
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
	}
}
void CWindowEditLine::RemoveFocus()
{
	GetParent()->ProcessMessage( SBUIMessage("MC_TEXT_MODE","", false) );
	m_nBeginDragSel = m_nBeginSel = m_nEndSel = -1;
	
	bFocused = false;
	GetScreen()->RegisterToSegment( this, false );
	UnRegisteMessageSinks();
}
void CWindowEditLine::OnButtonDown( const CVec2 &_vPos, const int nButton )
{
	if ( nButton & MSTATE_BUTTON1 )
	{
		if ( IsInside( _vPos ) )
		{
			nCursorPos = GetSelection( _vPos.x );
			m_nBeginDragSel = m_nBeginSel = m_nEndSel = nCursorPos + nBeginText;
			GetParent()->SetFocused( this, true );
			GetParent()->ProcessMessage( SBUIMessage("MC_TEXT_MODE","", true) );
			GetScreen()->RegisterToSegment( this, true );
			RegisteMessageSinks();
			bFocused = true;
		}
	}
}
int CWindowEditLine::GetSelection( const int nX )
{
	int nCur = 0, nPrev = 0;
	int i = 0;
	
	CTRect<float> editRect;
	FillWindowRectEditLine( &editRect );
	
	for ( ; i <= wszFullText.size(); i++ )
	{
		nCur = pGfxText->GetWidth( i ) + editRect.left;
		if ( nCur > nX )
		{
			if ( nX - nPrev < nCur - nX && i > 0 )			//ближе к левой букве чем к правой
				i--;
			break;
		}
		nPrev = nCur;
	}

	if ( nCur <= nX && i > 0 )		//нажато правее края текста
		i--;
	NI_ASSERT_T( i >= 0 && i <= wszFullText.size(), "Error in CWindowEditLine::GetSelection()" );
	return i;
}
void CWindowEditLine::SetCursor( const int nPos )
{
	if ( nPos < 0 )
		nCursorPos = wszFullText.length();
	else
		nCursorPos = nPos; 
}
bool CWindowEditLine::DeleteSelection()
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
bool CWindowEditLine::IsValidSymbol( const wchar_t chr )const
{
	if ( bGameSpySymbols )
	{
		static const std::wstring szValidSymbols = L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789[]\\`_^{|}-";
		static const int nLen = szValidSymbols.size();
		for ( int i = 0; i < nLen; i++ )
		{
			if ( chr == szValidSymbols[i] )
				return true;
		}
		return false;
	}
	
	if ( bFileNameSymbols )
	{
		static const std::wstring szValidSymbols = L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789[]`_^{}-!@#$%^&()+=~";
		static const int nLen = szValidSymbols.size();
		for ( int i = 0; i < nLen; i++ )
		{
			if ( chr == szValidSymbols[i] )
				return true;
		}
		return false;
	}

	if ( bNumericMode )
	{
		static const std::wstring szValidSymbols = L"0123456789";
		static const int nLen = szValidSymbols.size();
		for ( int i = 0; i < nLen; i++ )
		{
			if ( chr == szValidSymbols[i] )
				return true;
		}
		return false;
	}

	if ( bLocalPlayerNameMode )
	{
		static const std::wstring szInValidSymbols = L"&'\"<>";
		static const int nLen = szInValidSymbols.size();
		for ( int i = 0; i < nLen; i++ )
		{
			if ( chr == szInValidSymbols[i] )
				return false;
		}
	}
	
	return true;
}
void CWindowEditLine::OnReturn( const struct SGameMessage &msg )
{
	GetScreen()->RunStateCommandSequience( szOnReturn, this, true );
}
void CWindowEditLine::OnTab( const struct SGameMessage &msg )
{
	const std::wstring wszOldText = wszFullText;
	const int nOldCursorPos = nCursorPos;

	if ( !bNumericMode && !bGameSpySymbols && !bFileNameSymbols )
	{
		DeleteSelection();
		wszFullText.insert( nBeginText + nCursorPos, 4, VK_SPACE );
		nCursorPos += 4;
		if ( !IsTextInsideEditLine() )
		{
			wszFullText = wszOldText;
			nCursorPos = nOldCursorPos;
		}
		EnsureCursorVisible();
	}
}
void CWindowEditLine::OnBack( const struct SGameMessage &msg )
{
	if ( !DeleteSelection() && nBeginText + nCursorPos > 0 )
	{
		wszFullText.erase( nBeginText+nCursorPos-1, 1 );
		nCursorPos--;
		EnsureCursorVisible();
	}
}
void CWindowEditLine::OnDelete( const struct SGameMessage &msg )
{
	if ( !DeleteSelection() && nBeginText + nCursorPos < wszFullText.size() )
	{
		wszFullText.erase( nBeginText+nCursorPos, 1 );
		EnsureCursorVisible();
	}
}
void CWindowEditLine::OnLeft( const struct SGameMessage &msg )
{
	m_nBeginSel = m_nEndSel = -1;
	if ( nBeginText+nCursorPos == 0 )
		return;
	nCursorPos--;
	EnsureCursorVisible();
}
void CWindowEditLine::OnCtrlLeft( const struct SGameMessage &msg )
{
	m_nBeginSel = m_nEndSel = -1;
	if ( nBeginText+nCursorPos == 0 )
		return;

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
	EnsureCursorVisible();
}
void CWindowEditLine::OnRight( const struct SGameMessage &msg )
{
	m_nBeginSel = m_nEndSel = -1;
	if ( nBeginText+nCursorPos == wszFullText.size() )
		return;
	nCursorPos++;
	EnsureCursorVisible();
}
void CWindowEditLine::OnCtrlRight( const struct SGameMessage &msg )
{
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
	EnsureCursorVisible();
}
void CWindowEditLine::OnHome( const struct SGameMessage &msg )
{
	m_nBeginSel = m_nEndSel = -1;
	nBeginText = 0;
	nCursorPos = 0;
	EnsureCursorVisible();
}
void CWindowEditLine::OnEnd( const struct SGameMessage &msg )
{
	m_nBeginSel = m_nEndSel = -1;
	nCursorPos = wszFullText.size() - nBeginText;
	EnsureCursorVisible();
}
void CWindowEditLine::OnEscape( const struct SGameMessage &msg )
{
	m_nBeginSel = m_nEndSel = -1;
	SetFocus( false );		//сбрасываю фокус и заодно выключаю text mode 
	GetScreen()->RunStateCommandSequience( szOnEscape, this, true );
}
void CWindowEditLine::OnChar( const wchar_t chr )
{
	if ( !IsVisible() )
		return ;
	
	NotifyTextChanged();

	const std::wstring wszOldText = wszFullText;
	const int nOldCursorPos = nCursorPos;
	if ( IsValidSymbol( chr ) )
	{
		DeleteSelection();
		wszFullText.insert( Min( int(wszFullText.size()), nBeginText+nCursorPos), 1, chr );
		nCursorPos++;
		if ( !IsTextInsideEditLine() )
		{
			wszFullText = wszOldText;
			nCursorPos = nOldCursorPos;
		}
		EnsureCursorVisible();
	}
}
int CWindowEditLine::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.AddTypedSuper( static_cast<CWindow*>(this) );
	saver.Add( "Focus", &bFocused );
	saver.Add( "SelColor", &dwSelColor );
	saver.Add( "TextScroll", &bTextScroll );
	saver.Add( "NumericMode", &bNumericMode );
	saver.Add( "MaxLength", &nMaxLength );
	saver.Add( "GameSpySymbols", &bGameSpySymbols );
	saver.Add( "FileNameSymbols", &bFileNameSymbols );
	saver.Add( "LocalPlayerName", &bLocalPlayerNameMode );
	saver.Add( "FontName", &szFontName );
	saver.Add( "Color", &dwColor );
	saver.Add( "LeftSpace", &nLeftSpace );
	saver.Add( "RightSpace", &nRightSpace );
	saver.Add( "YOffset", &nYOffset );

	if ( saver.IsReading() )
	{
		CreateText();
	}
	return 0;
}
void CWindowEditLine::Init()
{
	CWindow::Init();
	if ( bFocused )
		GetParent()->SetFocused( this, true );

	CreateText();
}
void CWindowEditLine::CreateText()
{
	if ( !pGfxText )
	{
		pGfxText = CreateObject<IGFXText>( GFX_TEXT );
		IText *pText = CreateObject<IText>( TEXT_STRING );
		pGfxText->SetText( pText );
		IGFXFont *pFont = GetSingleton<IFontManager>()->GetFont( szFontName.c_str() );
		pGfxText->SetFont( pFont );	
		pGfxText->EnableRedLine( false );
		pGfxText->SetRedLine( 0 );
	}
}
int CWindowEditLine::operator&( IStructureSaver &ss )
{
	NI_ASSERT_T( FALSE, "NEED IMPLEMENT" );
	return 0;
}
void CWindowEditLine::Visit( interface ISceneVisitor *pVisitor )
{
	CWindow::Visit( pVisitor );

	if ( !IsVisible() )
		return;

	CTRect<float> wndRect;
	FillWindowRectEditLine( &wndRect );

	if ( m_nBeginSel != -1 && m_nBeginSel != m_nEndSel )
	{
		int nBegin = 0;
		if ( m_nBeginSel - nBeginText >= -1 )
			nBegin = pGfxText->GetWidth( m_nBeginSel - nBeginText );
		int nEnd = pGfxText->GetWidth( m_nEndSel - nBeginText );
		
		SGFXRect2 rc;
		rc.rect.left = wndRect.left + nBegin;
		rc.rect.right = wndRect.left + nEnd;
		if ( rc.rect.right > wndRect.right - 1 )
			rc.rect.right = wndRect.right - 1;
		int nH = pGfxText->GetLineSpace();
		rc.rect.top = (int) ( wndRect.Height() - nH ) / 2;
		rc.rect.top += wndRect.top;
		rc.rect.bottom = rc.rect.top + nH;
		rc.maps.x1 = rc.maps.y1 = rc.maps.x2 = rc.maps.y2 = 0;
		
		float fTemp;
		fTemp = wndRect.y1 - rc.rect.y1;
		if ( fTemp > 0 )
		{
			rc.rect.y1 = wndRect.y1;
		}

		fTemp = rc.rect.y2 - wndRect.y2;
		if ( fTemp > 0 )
		{
			rc.rect.y2 = wndRect.y2;
		}
		rc.color = dwSelColor;
		rc.fZ = 0;
		pVisitor->VisitUIRects( 0, 3, &rc, 1 );
	}
	
	{
		CTRect<float> textRect( wndRect );
		const int nH = pGfxText->GetLineSpace();
		textRect.top = wndRect.top + ( wndRect.Height() - nH ) / 2;
		textRect.bottom = textRect.top + nH;
		
		pVisitor->VisitUIText( pGfxText, textRect, 0, dwColor, FNT_FORMAT_LEFT );
	}
	if ( bFocused && bShowCursor )
	{
		int nWidth = pGfxText->GetWidth( nCursorPos );
		SGFXRect2 rc;
		rc.rect.left = wndRect.left + nWidth - 1;
		rc.rect.right = rc.rect.left + 2;
		if ( rc.rect.left < wndRect.right - 1 )
		{
			int nH = pGfxText->GetLineSpace();
			rc.rect.top = (int) ( wndRect.Height() - nH ) / 2;
			rc.rect.top += wndRect.top;
			rc.rect.bottom = rc.rect.top + nH;
			rc.maps.x1 = rc.maps.y1 = rc.maps.x2 = rc.maps.y2 = 0;
			
			float fTemp = wndRect.y1 - rc.rect.y1;
			if ( fTemp > 0 )
				rc.rect.y1 = wndRect.y1;
			
			fTemp = rc.rect.y2 - wndRect.y2;
			if ( fTemp > 0 )
				rc.rect.y2 = wndRect.y2;

			rc.color = dwColor;
			rc.fZ = 0;
			pVisitor->VisitUIRects( 0, 3, &rc, 1 );
		}
	}
}
void CWindowEditLine::Segment( const NTimer::STime timeDiff )
{
	timeSegment += timeDiff;
	if ( timeSegment > CURSOR_ANIMATION_TIME )
	{
		timeSegment = 0;
		bShowCursor = !bShowCursor;
	}
}
void CWindowEditLine::NotifyTextChanged()
{
	/*
	SUIMessage msg;
	msg.nMessageCode = UI_NOTIFY_EDIT_BOX_TEXT_CHANGED;
	msg.nFirst = GetWindowID();
	msg.nSecond = 0;
	GetParent()->ProcessMessage( msg );
	*/
}
void CWindowEditLine::SetText( const wchar_t *pszText )
{
	wszFullText = pszText;
	nBeginText = 0;
	nCursorPos = 0;
	m_nBeginDragSel = m_nBeginSel = m_nEndSel = -1;
}
void CWindowEditLine::EnsureCursorVisible()
{
	CTRect<float> wndRect;
	FillWindowRectEditLine( &wndRect );
	IText *pText = pGfxText->GetText();
	pText->SetText( reinterpret_cast<const WORD*>( wszFullText.c_str() + nBeginText ) );
	pGfxText->SetText( pText );

	if ( nCursorPos <= 0 && nBeginText > 0 )
	{
		NI_ASSERT_T( bTextScroll, "Edit box error: nCursorPos < 0 and bTextScroll == true" );
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
		pText->SetText( reinterpret_cast<const WORD*>( wszFullText.c_str() + nBeginText ) );
		pGfxText->SetText( pText );
	}
	else if ( pGfxText->GetWidth( nCursorPos ) > wndRect.Width() - 2 )
	{
		while ( pGfxText->GetWidth( nCursorPos ) > wndRect.Width() - 2 )		//2 is the width of cursor
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
			pText->SetText( reinterpret_cast<const WORD*>( wszFullText.c_str() + nBeginText ) );
			pGfxText->SetText( pText );
		}
	}
}
bool CWindowEditLine::IsTextInsideEditLine()
{
	CTRect<float> wndRect;
	FillWindowRectEditLine( &wndRect );
	if ( nMaxLength != -1 && wszFullText.size() > nMaxLength )
		return false;

	if ( bTextScroll )
		return true;

	IText *pText = pGfxText->GetText();
	pText->SetText( reinterpret_cast<const WORD*>( wszFullText.c_str() + nBeginText ) );
	pGfxText->SetText( pText );
	return pGfxText->GetWidth( -1 ) <= wndRect.Width() - 2;
}
void CWindowEditLine::SetSelection( const int nBegin, const int nEnd )
{
	m_nBeginSel = nBegin; 
	m_nEndSel = nEnd;
}
void CWindowEditLine::FillWindowRectEditLine( CTRect<float> *pRect )
{
	FillWindowRect( pRect );
	pRect->left += nLeftSpace;
	pRect->right -= nRightSpace;
	pRect->top += nYOffset;
}



















