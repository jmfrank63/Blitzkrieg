#include "StdAfx.h"

#include "UIColorTextScroll.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int STDCALL CUIColorTextScroll::CColorTextEntry::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &nHeight );
	saver.Add( 2, &nY );
	saver.Add( 3, &dwCaptionColor );
	saver.Add( 4, &dwEntryColor );
	saver.Add( 5, &szCaptionString );
	saver.Add( 6, &entryStartX );
	saver.Add( 7, &entryStrings );
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CVisibleString CUIColorTextScroll::CColorTextEntry::CreateString( const std::wstring &szSource, const int nWidth, const DWORD dwColor, const int nRedLine )
{
	CVisibleString result;
	result.first = CreateObject<ITextDialog>( TEXT_STRING );
	result.first->SetText( szSource.c_str() );
	
	result.second = CreateObject<IGFXText>( GFX_TEXT );
	result.second->SetText( result.first );
	result.second->SetColor( dwColor );
	result.second->SetFont( GetSingleton<IFontManager>()->GetFont( "fonts\\medium" ) );
	result.second->SetWidth( nWidth );
	if ( !nRedLine )
		result.second->EnableRedLine( false );
	else
	{
		result.second->EnableRedLine( true );
		result.second->SetRedLine( nRedLine );
	}
	return result;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CUIColorTextScroll::CColorTextEntry::Get1LineHeight() const
{
	if ( entry.second )
		return entry.second->GetLineSpace();
	if ( caption.second )
		return caption.second->GetLineSpace();
	return 1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CUIColorTextScroll::CColorTextEntry::CColorTextEntry( const wchar_t *pszCaptionText, const DWORD _dwCaptionColor,
										 const wchar_t *pszEntryText, const DWORD _dwEntryColor,
										 const int _nY, const int nWidth )
: nY( _nY ), 
	dwCaptionColor( _dwCaptionColor ),
	dwEntryColor( _dwEntryColor ), 
	entryStartX( 0 ), nHeight( 0 )
{
	// determine caption size
	if ( pszCaptionText )
	{
		szCaptionString = pszCaptionText;
		caption = CreateString( szCaptionString, nWidth, dwCaptionColor, 0 );
		entryStartX = caption.second->GetWidth() + 20;
	}

	if ( pszEntryText )
	{
		entryStrings.push_back( pszEntryText );
		entry = CreateString( entryStrings[0], nWidth, dwEntryColor, entryStartX );
	}

	//determine total item height 
	if ( entry.second )
		nHeight = entry.second->GetNumLines() * entry.second->GetLineSpace();
	else if ( caption.second )
		nHeight = caption.second->GetLineSpace();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIColorTextScroll::CColorTextEntry::Visit( interface ISceneVisitor *pVisitor, const CTRect<float> &border, const int nYOffset )
{
	if ( caption.second )
		pVisitor->VisitUIText( caption.second, border, nY + nYOffset, dwCaptionColor, FNT_FORMAT_SINGLE_LINE|FNT_FORMAT_LEFT );
	
	if ( entry.second )
		pVisitor->VisitUIText( entry.second, border, nY + nYOffset, dwEntryColor, FNT_FORMAT_LEFT );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CUIColorTextScroll::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.AddTypedSuper( 1, static_cast<CUIScrollTextBox*>(this) );
	saver.Add( 2,  &colors );
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CUIColorTextScroll::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.AddTypedSuper( static_cast<CUIScrollTextBox*>(this) );
	saver.Add( "Colors",  &colors );
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIColorTextScroll::Reposition( const CTRect<float> &rcParent )
{
	RepositionScrollbar();
	CMultipleWindow::Reposition( rcParent );
	UpdateScrollBar( 0, 0 );
	
	// ToDo if needed
	// reposition entrys
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIColorTextScroll::SetWindowText( int nState, const WORD *pszText ) 
{ 
	textEntrys.clear(); 
	AppendMessage( pszText, 0, IUIColorTextScroll::E_COLOR_DEFAULT );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIColorTextScroll::Draw( IGFX *pGFX )
{
	NI_ASSERT_T( false, "wrong call" );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIColorTextScroll::AppendMessage( const WORD *pszCaption, const WORD *pszMessage,
																			const enum IUIColorTextScroll::EColorEntrys color )
{
	const int nColorIndex = int(color);
	NI_ASSERT_T( nColorIndex < colors.size(), NStr::Format("wrong color index %d", nColorIndex) );

	CTRect<float> rect;
	GetBorderRect( &rect );

	CColorTextEntry * pNewEntry = new CColorTextEntry( reinterpret_cast<const wchar_t*>(pszCaption), colors[nColorIndex].first,
																						 reinterpret_cast<const wchar_t*>(pszMessage), colors[nColorIndex].second,
																						 nCurrentYSize, rect.Width() );
	nCurrentYSize += pNewEntry->GetSizeY();
	textEntrys.push_back( pNewEntry );
																						 
	const int nSize = Max( nCurrentYSize - rect.Height(), 0.0f );

	//const int nLineHeigth = states[nState].pGfxText->GetLineSpace();
	
	UpdateScrollBar( nSize, nSize );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUIColorTextScroll::Visit( interface ISceneVisitor *pVisitor )
{
	CUIScrollTextBox::Visit( pVisitor );
	
	CTRect<float> borderRect;
	GetBorderRect( &borderRect );


	// draw visible text entrys
	// to do: draw only visible
	for ( CTextEntrys::iterator it = textEntrys.begin(); it != textEntrys.end(); ++it )
	{
		(*it)->Visit( pVisitor, borderRect, GetY() );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
