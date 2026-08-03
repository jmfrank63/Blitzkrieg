
#include "StdAfx.h"
#include "WindowTextView.h"
#include "..\GFX\GFX.h"
#include "..\Scene\Scene.h"
#include "..\Main\TextSystem.h"
IMPLEMENT_CLONABLE(CWindowTextView)
int CWindowTextView::operator&( IDataTree &ss )
{
	CTreeAccessor saver = &ss;
	saver.AddTypedSuper( static_cast<CWindow*>(this));
	saver.Add( "Color", &dwColor );
/*	EGFXFontFormat
	FNT_FORMAT_LEFT					= 0x00000001,	// format to left
	FNT_FORMAT_RIGHT				= 0x00000002,	// format to right
	FNT_FORMAT_CENTER				= 0x00000004,	// format to center
	FNT_FORMAT_JUSTIFY			= 0x00000008,	// format to width (justify)
	FNT_FORMAT_SINGLE_LINE	= 0x00010000,	// single line with clipping
*/
	saver.Add( "Format", &format );

	saver.Add( "TextKey", &szKey );
	if ( saver.IsReading() )
	{
		pGfxText = CreateObject<IGFXText>( GFX_TEXT );

		if ( !szKey.empty() )
		{
			IText *pText = GetSingleton<ITextManager>()->GetDialog( szKey.c_str() );
			if ( pText )
				pGfxText->SetText( pText );
		}
	}

	saver.Add( "FontName", &szFontName );
	if ( saver.IsReading() )
	{
		IGFXFont *pFont = GetSingleton<IFontManager>()->GetFont( szFontName.c_str() );
		pGfxText->SetFont( pFont );	
	}

	saver.Add( "RedLine", &nRedLineSpace );
	if ( saver.IsReading() )
	{
		pGfxText->EnableRedLine( nRedLineSpace != 0 );
		pGfxText->SetRedLine( nRedLineSpace );
	}

	if ( saver.IsReading() )
		InitHeight();

	return 0;
}
bool CWindowTextView::InitHeight()
{
	int nWidth, nHeight;
	GetPlacement( 0, 0, &nWidth, &nHeight );
	pGfxText->SetWidth( nWidth );
	const int nTmp = pGfxText->GetNumLines() * pGfxText->GetLineSpace();
	const bool bRet = nTmp != nHeight;
	if ( bRet )
		SetPlacement( 0, 0, 0, nTmp, EWPF_SIZE_Y );
	return bRet;
}
CWindowTextView::CWindowTextView( int TEST )
{
	dwColor = 0xff000000;
	format = FNT_FORMAT_LEFT;
	szKey = "Textes\\nextchapterquestion.txt";
	szFontName = "fonts\\medium";
	Init( 0 );
}
int CWindowTextView::operator&( IStructureSaver &ss )
{
	NI_ASSERT_T( FALSE, "NEED IMPLEMENT" );
	return 0;
}

void CWindowTextView::Visit( interface ISceneVisitor *pVisitor )
{
	CWindow::Visit( pVisitor );

	CTRect<float> textRC;
	FillWindowRect( &textRC );
	pVisitor->VisitUIText( pGfxText, textRC, 0, dwColor, format );
}
bool CWindowTextView::SetText( const std::wstring &szText )
{
	CTRect<float> textRC;
	FillWindowRect( &textRC );

	pGfxText->GetText()->SetText( (const WORD*)szText.c_str() );
	pGfxText->SetWidth( textRC.Width() );
	return InitHeight();
}