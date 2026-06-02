#include "StdAfx.h"

#include "Text.h"

#include "..\Formats\fmtFont.h"
#include "GFXTextVisitors.h"
CGFXText::CGFXText()
: bPreFormatted( false ), fWidth( 0 ), bRedLine( true ), fRedLineSize( 0 )
{
	dwDefColor = GetGlobalVar( "Scene.Colors.Summer.Text.Default.Color", int(0xffd8bd3e) );
}
void CGFXText::SetupRedLine()
{
	if ( bRedLine )
	{
		if ( pFont && (fRedLineSize == 0) )
			fRedLineSize = 2.0f * pFont->GetFormat().metrics.nAveCharWidth;
	}
	else
		fRedLineSize = 0;
}
int CGFXText::GetNumLines() const 
{ 
	PreFormat(); 
	return pft.lines.size(); 
}
int CGFXText::GetLineSpace() const 
{ 
	return pFont != 0 ? pFont->GetFormat().GetLineSpace() : -1;
}
int CGFXText::GetWidth( int nNumCharacters ) const
{
	if ( pText == 0 ) 
		return 1;
	const wchar_t *pszStringBegin = (const wchar_t*)(const void*)pText->GetString();
	if ( pszStringBegin == 0 ) 
		return 1;
	const int nStrLen = GetLength( pszStringBegin );
	if ( nStrLen == 0 ) 
		return 1;
	nNumCharacters = nNumCharacters < 0 ? nStrLen : Min( nNumCharacters, nStrLen );
	return VisitText( pszStringBegin, pszStringBegin + nNumCharacters, 0, 0, CTextWidthVisitor() );
}
inline const bool IsEOL( const wchar_t chr ) { return (chr == '\n') || (chr == '\r'); }
inline const bool IsSPC( const wchar_t chr ) { return chr == L' '; }
inline const bool IsEOF( const wchar_t chr ) { return chr == L'\0'; }
inline const bool IsSpecial( const wchar_t *pszCurr, const wchar_t *pszEnd ) 
{
	return ( pszCurr >= pszEnd ) || IsEOL( *pszCurr ) || IsSPC( *pszCurr ) || IsEOF( *pszCurr );
}
void CGFXText::PreFormat() const
{
	if ( pText && pText->IsChanged() ) 
	{
		bPreFormatted = false;
		bPreFormattedLine = false;
		pText->ResetChanged();
	}
	if ( bPreFormatted )
		return;
	pft.Clear();
	if ( pText == 0 ) 
		return;
	const wchar_t *pszStringBegin = (const wchar_t*)(const void*)pText->GetString();
	const int nStrLen =  NStr::GetStrLen( pszStringBegin );
	const wchar_t *pszStringEnd = pszStringBegin + nStrLen;
	pft.fWidth = fWidth;
  pft.lines.clear();
	if ( nStrLen == 0 ) 
	{
		bPreFormatted = true;
		bPreFormattedLine = false;
		return;
	}
	NI_ASSERT_T( fWidth > 0, "Can't format string to zero width!!!" );
	const SFontFormat &format = pFont->GetFormat();
  pft.lines.reserve( MINT( float( format.metrics.nAveCharWidth * nStrLen ) / fWidth * 1.5f ) + 1 );
  const wchar_t *pszLine = pszStringBegin;
	const wchar_t *pszWordBegin = pszStringBegin;
	wchar_t wLastChar = 0;
  pft.lines.resize( pft.lines.size() + 1 );
	SPreFormattedText::SLine *pLine = &( pft.lines.back() );
	pLine->properties |= SPreFormattedText::SLine::PROP_FIRST_LINE;
  float fLineWidth = GetRedLine(), fLineWordWidth = GetRedLine();	// current line width and whole-words line width
	int nNumWordSpaces = 0;	// number of spaces, before added word
	while ( 1 ) 
	{
		const wchar_t c = *pszLine;
		if ( IsSpecial(pszLine, pszStringEnd) )
		{
			if ( pszLine > pszWordBegin ) 
			{
        pLine->words.push_back( SPreFormattedText::SLine::SWord(pszWordBegin, pszLine, nNumWordSpaces) );
	      ++pLine->nNumWords;
        fLineWordWidth = fLineWidth;
        if ( pLine->nNumWords == 1 )		
          pLine->fPreSpace = format.GetChar(*pszWordBegin).fA;
				if ( pLine->IsFirstLine() || (pLine->nNumWords > 1) ) 
					pLine->nNumSpaces += nNumWordSpaces;
				nNumWordSpaces = 0;
			}
			if ( IsEOF(c) )					// end-of-file. finish line and exit from the loop
			{
				pLine->properties |= SPreFormattedText::SLine::PROP_LAST_LINE;
        pLine->fWidth = fLineWordWidth - pLine->fPreSpace;
				if ( !pLine->IsFirstLine() && !pLine->words.empty() ) 
					pLine->words.front().nNumPreSpaces = 0;
				break;
			} 
			else if ( IsEOL(c) )		// end-of-line. finish line and begin next line
			{
				pLine->properties |= SPreFormattedText::SLine::PROP_LAST_LINE;
        pLine->fWidth = fLineWordWidth - pLine->fPreSpace;
				if ( !pLine->IsFirstLine() && !pLine->words.empty() ) 
					pLine->words.front().nNumPreSpaces = 0;
        fLineWidth = fLineWordWidth = GetRedLine();
        pft.lines.resize( pft.lines.size() + 1 );
        pLine = &( pft.lines.back() );
				pLine->properties |= SPreFormattedText::SLine::PROP_FIRST_LINE;
				nNumWordSpaces = 0;
        if ( c == 13 )
          ++pszLine;
			}
			else if ( IsSPC(c) )		// white-space
				++nNumWordSpaces;
      pszWordBegin = pszLine + 1;
      wLastChar = 0;
		}
		else											// simply symbol
		{
      const SFontFormat::SCharDesc &character = format.GetChar( c );
      fLineWidth += format.GetKern( wLastChar, c );
			fLineWidth += character.fA + character.fB + character.fC;
      const float fTotalLineWidth = fLineWidth + (pLine->nNumSpaces + nNumWordSpaces) * format.metrics.fSpaceWidth - pLine->fPreSpace;
      if ( fTotalLineWidth > fWidth )   // if current width have exceeded allowed width - 'EOL'
      {
        if ( pLine->words.empty() )
        {
          if ( pszLine - pszWordBegin > 0 )
          {
            const SFontFormat::SCharDesc &character = format.GetChar( *pszWordBegin );
            pLine->words.push_back( SPreFormattedText::SLine::SWord(pszWordBegin, pszLine, nNumWordSpaces) );
            pLine->nNumWords++;
            fLineWordWidth = fLineWidth;
            pLine->fPreSpace = character.fA;
						if ( pLine->IsFirstLine() ) 
							pLine->nNumSpaces += nNumWordSpaces;
						nNumWordSpaces = 0;
						pszWordBegin = pszLine;
          }
        }
        pLine->fWidth = fLineWordWidth - pLine->fPreSpace;
				if ( !pLine->IsFirstLine() && !pLine->words.empty() ) 
					pLine->words.front().nNumPreSpaces = 0;
        fLineWidth = fLineWordWidth = 0;
        pft.lines.resize( pft.lines.size() + 1 );
        pLine = &( pft.lines.back() );
        pszLine = pszWordBegin - 1;
				nNumWordSpaces = 0;
      }
      wLastChar = c;
		}
    ++pszLine;
	}
	bPreFormatted = true;
	bPreFormattedLine = false;
}
void CGFXText::PreFormatLine() const
{
	if ( pText && pText->IsChanged() ) 
	{
		bPreFormatted = false;
		bPreFormattedLine = false;
		pText->ResetChanged();
	}
	if ( bPreFormattedLine )
		return;
	pft.Clear();
	if ( pText == 0 ) 
		return;
	const wchar_t *pszStringBegin = (const wchar_t*)(const void*)pText->GetString();
	const int nStrLen =  NStr::GetStrLen( pszStringBegin );
	const wchar_t *pszStringEnd = pszStringBegin + nStrLen;
	pft.fWidth = fWidth;
  pft.lines.clear();
	if ( nStrLen == 0 ) 
	{
		bPreFormattedLine = true;
		bPreFormatted = false;
		return;
	}
	const SFontFormat &format = pFont->GetFormat();
  const wchar_t *pszLine = pszStringBegin;
	const wchar_t *pszWordBegin = pszStringBegin;
	wchar_t wLastChar = 0;
  pft.lines.resize( 1 );
	SPreFormattedText::SLine *pLine = &( pft.lines.back() );
  float fLineWidth = 0, fLineWordWidth = 0;	// current line width and whole-words line width
	int nNumWordSpaces = 0;	// number of spaces, before added word
	while ( 1 ) 
	{
		const wchar_t c = *pszLine;
		if ( IsSpecial(pszLine, pszStringEnd) )
		{
			if ( pszLine > pszWordBegin ) 
			{
        pLine->words.push_back( SPreFormattedText::SLine::SWord(pszWordBegin, pszLine, nNumWordSpaces) );
	      ++pLine->nNumWords;
        fLineWordWidth = fLineWidth;
        if ( pLine->nNumWords == 1 )		
          pLine->fPreSpace = format.GetChar(*pszWordBegin).fA;
				if ( pLine->nNumWords > 1 ) 
					pLine->nNumSpaces += nNumWordSpaces;
				nNumWordSpaces = 0;
			}
			if ( IsEOF(c) )					// end-of-file. finish line and exit from the loop
			{
        pLine->fWidth = fLineWordWidth - pLine->fPreSpace;
				if ( !pLine->words.empty() ) 
					pLine->words.front().nNumPreSpaces = 0;
				break;
			} 
			else if ( IsSPC(c) )		// white-space
				++nNumWordSpaces;
			else
			{
				NI_ASSERT_SLOW_T( IsEOL(c), "EOL characters doesn't supported in the single-line mode!!!" );
			}
      pszWordBegin = pszLine + 1;
      wLastChar = 0;
		}
		else											// simply symbol
		{
      const SFontFormat::SCharDesc &character = format.GetChar( c );
      fLineWidth += format.GetKern( wLastChar, c );
			fLineWidth += character.fA + character.fB + character.fC;
      wLastChar = c;
		}
    ++pszLine;
	}
	bPreFormatted = false;
	bPreFormattedLine = true;
}
void EstimateNumVerticesAndIndices( const SPreFormattedText::SLine &line, int &nNumVertices, int &nNumIndices )
{
  for ( std::list<SPreFormattedText::SLine::SWord>::const_iterator pos = line.words.begin(); pos != line.words.end(); ++pos )
  {
    nNumVertices += ( pos->pszEnd - pos->pszBegin ) * 4;
    nNumIndices  += ( pos->pszEnd - pos->pszBegin ) * 6;
  }
}
float CGFXText::FillGeometryDataNoClip( const wchar_t *pszStringBegin, const wchar_t *pszStringEnd, 
																				float sx, const float sy, const CTRect<float> &rcClipRect, 
																				const DWORD dwColor, const DWORD dwSpecular, 
																				std::vector<SGFXLVertex> &vertices, std::vector<WORD> &indices ) const
{
	if ( pszStringBegin == pszStringEnd )
		return sx;                       // can't render zero-length string
	const int nStrLen = pszStringEnd - pszStringBegin;
	vertices.reserve( vertices.size() + nStrLen*4 );
  indices.reserve( indices.size() + nStrLen*6 );
	CTextNoClipVisitor visitor( vertices, indices, pFont->GetFormat().metrics.nHeight, dwColor, dwSpecular );
	return VisitText( pszStringBegin, pszStringEnd, sx, sy, visitor );
}
float CGFXText::FillGeometryDataClip( const wchar_t *pszStringBegin, const wchar_t *pszStringEnd, 
																		  float sx, const float sy, const CTRect<float> &rcClipRect, 
																			const DWORD dwColor, const DWORD dwSpecular, 
																			std::vector<SGFXLVertex> &vertices, std::vector<WORD> &indices ) const
{
	if ( pszStringBegin == pszStringEnd )
		return sx;                       // can't render zero-length string
	const int nStrLen = pszStringEnd - pszStringBegin;
	vertices.reserve( vertices.size() + nStrLen*4 );
  indices.reserve( indices.size() + nStrLen*6 );
	CTextClipVisitor visitor( vertices, indices, rcClipRect, pFont->GetFormat().metrics.nHeight, dwColor, dwSpecular );
	return VisitText( pszStringBegin, pszStringEnd, sx, sy, visitor );
}
const DWORD FNT_CLIP_TOP = 1;
const DWORD FNT_CLIP_BOTTOM = 2;
inline int Width( const RECT &rect ) { return rect.right - rect.left; }
bool CGFXText::FillGeometryData( DWORD dwFlags, const RECT &rect, float sy, 
																 DWORD dwColor, const DWORD dwSpecular,
																 std::vector<SGFXLVertex> &vertices, std::vector<WORD> &indices ) const
{
	dwColor = dwColor == 0 ? dwDefColor : dwColor;
	if ( dwFlags & FNT_FORMAT_SINGLE_LINE ) 
	{
		PreFormatLine();
		if ( pft.lines.empty() ) 
			return true;
		const float sx = rect.left;
		if ( dwFlags & FNT_FORMAT_LEFT ) 
	    FillGeometryDataLeft( pft.lines.front(), sx, sy, rect, dwColor, dwSpecular, FNT_CLIP_TOP | FNT_CLIP_BOTTOM, vertices, indices );
		else if ( dwFlags & FNT_FORMAT_RIGHT ) 
	    FillGeometryDataRight( pft.lines.front(), sx, sy, rect, dwColor, dwSpecular, FNT_CLIP_TOP | FNT_CLIP_BOTTOM, vertices, indices );
		else
		{
			NI_ASSERT_SLOW_TF( dwFlags & (FNT_FORMAT_CENTER | FNT_FORMAT_JUSTIFY), "Center and justify alignment doesn't supportd for single-line text mode", return false );
			return false;
		}
		return true;
	}
	PreFormat();
	if ( pft.lines.empty() ) 
		return true;
	NI_ASSERT_SLOW_TF( Width(rect) == pft.GetWidth(), "Can't fill text geometry data - incorrect width", return false );
  const std::vector<SPreFormattedText::SLine> &lines = pft.lines;
	const SFontFormat &format = pFont->GetFormat();
  int nY = MINT( sy );
  int nLineSpace = format.GetLineSpace();
  int nFirstLine = 0, nLastLine = lines.size();
  bool bClipFirstLine = false, bClipLastLine = false;
  if ( (nY >= rect.bottom) || (nY + int(lines.size())*nLineSpace <= rect.top) )
    return true;
  for ( int i = nFirstLine; i < nLastLine; ++i )
  {
    if ( (nY > rect.top) || ((nY <= rect.top) && (nY + nLineSpace > rect.top)) )
    {
      nFirstLine = i;
      if ( (nY < rect.top) && (nY + nLineSpace > rect.top) )
        bClipFirstLine = true;
      break;
    }
    nY += nLineSpace;
  }
  for ( int i = nFirstLine; i < nLastLine; ++i )
  {
    if ( (nY > rect.bottom) || ((nY < rect.bottom) && (nY + nLineSpace >= rect.bottom)) )
    {
      nLastLine = i + 1;
      if ( (nY < rect.bottom) && (nY + nLineSpace > rect.bottom) )
        bClipLastLine = true;
      break;
    }
    nY += nLineSpace;
  }
  int nTotalNumVertices = 0;
  int nTotalNumIndices = 0;
  for ( int i = nFirstLine; i < nLastLine; ++i )
  {
    int nNumVertices = 0;
    int nNumIndices = 0;
    EstimateNumVerticesAndIndices( lines[i], nNumVertices, nNumIndices );
    if ( ((i == nFirstLine) && bClipFirstLine) || ((i == nLastLine - 1) && bClipLastLine) )
    {
      nNumVertices += nNumVertices / 3;
      nNumIndices += nNumIndices >> 1;
    }
    nTotalNumVertices += nNumVertices;
    nTotalNumIndices += nNumIndices;
  }
  vertices.reserve( vertices.size() + nTotalNumVertices );
  indices.reserve( indices.size() + nTotalNumIndices );
  float sx = rect.left;
  sy += nFirstLine * nLineSpace;
  DWORD dwClipFlags = 0;
  if ( dwFlags & FNT_FORMAT_LEFT )      // left
  {
    for ( int i = nFirstLine; i < nLastLine; ++i )
    {
      if ( (i == nFirstLine) && bClipFirstLine )
        dwClipFlags = FNT_CLIP_TOP;
      else if ( (i == nLastLine - 1) && bClipLastLine )
        dwClipFlags = FNT_CLIP_BOTTOM;
      else
        dwClipFlags = 0;
      FillGeometryDataLeft( lines[i], sx, sy, rect, dwColor, dwSpecular, dwClipFlags, vertices, indices );
      sy += format.GetLineSpace();
    }
  }
  else if ( dwFlags & FNT_FORMAT_RIGHT ) // right
  {
    for ( int i = nFirstLine; i < nLastLine; ++i )
    {
      if ( (i == nFirstLine) && bClipFirstLine )
        dwClipFlags = FNT_CLIP_TOP;
      else if ( (i == nLastLine - 1) && bClipLastLine )
        dwClipFlags = FNT_CLIP_BOTTOM;
      else
        dwClipFlags = 0;
      FillGeometryDataRight( lines[i], sx, sy, rect, dwColor, dwSpecular, dwClipFlags, vertices, indices );
      sy += format.GetLineSpace();
    }
  }
  else if ( dwFlags & FNT_FORMAT_CENTER ) // center
  {
    for ( int i = nFirstLine; i < nLastLine; ++i )
    {
      if ( (i == nFirstLine) && bClipFirstLine )
        dwClipFlags = FNT_CLIP_TOP;
      else if ( (i == nLastLine - 1) && bClipLastLine )
        dwClipFlags = FNT_CLIP_BOTTOM;
      else
        dwClipFlags = 0;
      FillGeometryDataCenter( lines[i], sx, sy, rect, dwColor, dwSpecular, dwClipFlags, vertices, indices );
      sy += format.GetLineSpace();
    }
  }
  else if ( dwFlags & FNT_FORMAT_JUSTIFY ) // justify
  {
    for ( int i = nFirstLine; i < nLastLine; ++i )
    {
      if ( (i == nFirstLine) && bClipFirstLine )
        dwClipFlags = FNT_CLIP_TOP;
      else if ( (i == nLastLine - 1) && bClipLastLine )
        dwClipFlags = FNT_CLIP_BOTTOM;
      else
        dwClipFlags = 0;
      if ( lines[i].IsLastLine() )
        FillGeometryDataLeft( lines[i], sx, sy, rect, dwColor, dwSpecular, dwClipFlags, vertices, indices );
      else
        FillGeometryDataJustify( lines[i], sx, sy, rect, dwColor, dwSpecular, dwClipFlags, vertices, indices );
      sy += format.GetLineSpace();
    }
  }
  else                                  // unknown formatting flag
		NI_ASSERT_SLOW_TF( false, "Unknown formatting flag", return false );
  return true;
}
bool CGFXText::FillGeometryDataLeft( const SPreFormattedText::SLine &line, float sx, const float sy, const RECT &rect, 
																		 const DWORD dwColor, const DWORD dwSpecular, DWORD dwClipFlags, 
																		 std::vector<SGFXLVertex> &vertices, std::vector<WORD> &indices ) const
{
  if ( line.words.empty() )
    return true;
	const SFontFormat &format = pFont->GetFormat();
  const float fWidth = Width( rect );
  sx -= line.fPreSpace;
  if ( line.IsFirstLine() )
    sx += GetRedLine();
	CTRect<float> rcClipRect = rect;
  if ( line.nNumWords == 1 )						// do not need spacing
	{
		sx += line.words.back().nNumPreSpaces * format.metrics.fSpaceWidth;
		if ( dwClipFlags == 0 )
			FillGeometryDataNoClip( line.words.back().pszBegin, line.words.back().pszEnd, sx, sy, rcClipRect, dwColor, dwSpecular, vertices, indices );
		else
			FillGeometryDataClip( line.words.back().pszBegin, line.words.back().pszEnd, sx, sy, rcClipRect, dwColor, dwSpecular, vertices, indices );
	}
  else																	// add spaces before each word
  {
    for ( std::list<SPreFormattedText::SLine::SWord>::const_iterator it = line.words.begin(); it != line.words.end(); ++it )
    {
			sx += it->nNumPreSpaces * format.metrics.fSpaceWidth;
			if ( dwClipFlags == 0 )
				sx = FillGeometryDataNoClip( it->pszBegin, it->pszEnd, sx, sy, rcClipRect, dwColor, dwSpecular, vertices, indices );
			else
				sx = FillGeometryDataClip( it->pszBegin, it->pszEnd, sx, sy, rcClipRect, dwColor, dwSpecular, vertices, indices );
    }
  }

  return true;
}
bool CGFXText::FillGeometryDataRight( const SPreFormattedText::SLine &line, float sx, const float sy, const RECT &rect, 
																			const DWORD dwColor, const DWORD dwSpecular, DWORD dwClipFlags, 
																			std::vector<SGFXLVertex> &vertices, std::vector<WORD> &indices ) const
{
  if ( line.words.empty() )
    return true;
	const SFontFormat &format = pFont->GetFormat();
  const float fWidth = Width( rect );
  sx -= line.fPreSpace;
  if ( line.IsFirstLine() )
    sx += GetRedLine();
	CTRect<float> rcClipRect = rect;
  if ( line.nNumWords == 1 )           // do not need spacing
  {
    sx += fWidth - line.fWidth;
		if ( dwClipFlags == 0 )
			FillGeometryDataNoClip( line.words.back().pszBegin, line.words.back().pszEnd, sx, sy, rcClipRect, dwColor, dwSpecular, vertices, indices );
		else
			FillGeometryDataClip( line.words.back().pszBegin, line.words.back().pszEnd, sx, sy, rcClipRect, dwColor, dwSpecular, vertices, indices );
  }
  else                                  // add spaces before each word 
  {
    sx += ( fWidth - line.fWidth ) - ( line.nNumSpaces * format.metrics.fSpaceWidth );
    for ( std::list<SPreFormattedText::SLine::SWord>::const_iterator it = line.words.begin(); it != line.words.end(); ++it )
    {
      sx += it->nNumPreSpaces * format.metrics.fSpaceWidth;
			if ( dwClipFlags == 0 )
				sx = FillGeometryDataNoClip( it->pszBegin, it->pszEnd, sx, sy, rcClipRect, dwColor, dwSpecular, vertices, indices );
			else
				sx = FillGeometryDataClip( it->pszBegin, it->pszEnd, sx, sy, rcClipRect, dwColor, dwSpecular, vertices, indices );
    }
  }

  return true;
}
bool CGFXText::FillGeometryDataCenter( const SPreFormattedText::SLine &line, float sx, const float sy, const RECT &rect, 
																			 const DWORD dwColor, const DWORD dwSpecular, DWORD dwClipFlags, 
																			 std::vector<SGFXLVertex> &vertices, std::vector<WORD> &indices ) const
{
  if ( line.words.empty() )
    return true;
	const SFontFormat &format = pFont->GetFormat();
  const float fWidth = Width( rect );
  sx -= line.fPreSpace;
  if ( line.IsFirstLine() )
    sx += GetRedLine();
	CTRect<float> rcClipRect = rect;
  if ( line.nNumWords == 1 )						// do not need spacing
  {
    sx += float( floor( ( fWidth - line.fWidth ) * 0.5 ) ) + line.words.back().nNumPreSpaces * format.metrics.fSpaceWidth;
		if ( dwClipFlags == 0 )
			FillGeometryDataNoClip( line.words.back().pszBegin, line.words.back().pszEnd, sx, sy, rcClipRect, dwColor, dwSpecular, vertices, indices );
		else
			FillGeometryDataClip( line.words.back().pszBegin, line.words.back().pszEnd, sx, sy, rcClipRect, dwColor, dwSpecular, vertices, indices );
  }
  else                                  // add spaces before each word
  {
    sx += float( floor( ( ( fWidth - line.fWidth ) - (line.nNumSpaces * format.metrics.fSpaceWidth) ) * 0.5 ) );
    for ( std::list<SPreFormattedText::SLine::SWord>::const_iterator it = line.words.begin(); it != line.words.end(); ++it )
    {
      sx += it->nNumPreSpaces * format.metrics.fSpaceWidth;
			if ( dwClipFlags == 0 )
				sx = FillGeometryDataNoClip( it->pszBegin, it->pszEnd, sx, sy, rcClipRect, dwColor, dwSpecular, vertices, indices );
			else
				sx = FillGeometryDataClip( it->pszBegin, it->pszEnd, sx, sy, rcClipRect, dwColor, dwSpecular, vertices, indices );
    }
  }

  return true;
}
bool CGFXText::FillGeometryDataJustify( const SPreFormattedText::SLine &line, float sx, const float sy, const RECT &rect, 
																				const DWORD dwColor, const DWORD dwSpecular, DWORD dwClipFlags, 
																				std::vector<SGFXLVertex> &vertices, std::vector<WORD> &indices ) const
{
  if ( line.words.empty() )
    return true;
	const SFontFormat &format = pFont->GetFormat();
  const float fWidth = Width( rect );
  sx -= line.fPreSpace;
  if ( line.IsFirstLine() )
    sx += GetRedLine();
	CTRect<float> rcClipRect = rect;
  if ( line.nNumWords == 1 )           // do not need spacing
	{
		sx += line.words.back().nNumPreSpaces * format.metrics.fSpaceWidth;
		if ( dwClipFlags == 0 )
			FillGeometryDataNoClip( line.words.back().pszBegin, line.words.back().pszEnd, sx, sy, rcClipRect, dwColor, dwSpecular, vertices, indices );
		else
			FillGeometryDataClip( line.words.back().pszBegin, line.words.back().pszEnd, sx, sy, rcClipRect, dwColor, dwSpecular, vertices, indices );
	}
  else                                  // add spaces after each word
  {
    float fTotalSpace = 0, fLastTotalSpace = 0;
    const float fSpaceSize = ( fWidth - line.fWidth ) / float( line.nNumSpaces );
    for ( std::list<SPreFormattedText::SLine::SWord>::const_iterator it = line.words.begin(); it != line.words.end(); ++it )
    {
      fTotalSpace += it->nNumPreSpaces * fSpaceSize;
      const float fSpace = float( int(fTotalSpace) ) - fLastTotalSpace;
      sx += fSpace;
      fLastTotalSpace += fSpace;
			if ( dwClipFlags == 0 )
				sx = FillGeometryDataNoClip( it->pszBegin, it->pszEnd, sx, sy, rcClipRect, dwColor, dwSpecular, vertices, indices );
			else
				sx = FillGeometryDataClip( it->pszBegin, it->pszEnd, sx, sy, rcClipRect, dwColor, dwSpecular, vertices, indices );
    }
  }

  return true;
}
int CGFXText::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	saver.Add( 1, &pText );
	saver.Add( 2, &fWidth );
	saver.Add( 3, &dwDefColor );
	saver.Add( 4, &pFont );
	saver.Add( 5, &bRedLine );
	saver.Add( 7, &fRedLineSize );
	if ( saver.IsReading() )
	{
		pft.Clear();
		bPreFormatted = false;
	}
	return 0;
}
