#include "StdAfx.h"

#include "FontAtlas.h"

#include <cmath>
#include <algorithm>

#include "Shaper.h"

// Static: Dear ImGui links its own copy of the same rasteriser, and an editor
// that links both must not see two definitions of every stbtt_ function.
#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "../../../vendor/dcimgui/src-docking/imstb_truetype.h"

namespace FontAtlas
{
namespace
{
// FontGen's spacing between two glyph cells in a row, and here between two
// rows as well: the GPU path samples linearly with FontGen's half-texel UVs
// and no half-pixel vertex shift, so a glyph's last screen row reads half of
// the texel row under its cell. FontGen stacked rows without a gap, which GDI
// fonts survive by an empty top row; a face with ink in its first row drew
// the top of the glyph below as a line under the text (D-14's capture).
const int LEADING_PIXELS = 2;
// The largest atlas side FontGen's estimate tried (1 << 12).
const int MAX_TEXTURE_SIZE = 4096;
// Slant of the synthetic italic: about 11 degrees, close to GDI's.
const float ITALIC_SLANT = 0.2f;

// Bytes 0x80..0xFF of the code pages the game's languages use (Windows
// best-fit tables; undefined bytes map to U+0080..U+009F as
// MultiByteToWideChar maps them).
const WORD CP1250[128] =
{
	0x20AC, 0x0081, 0x201A, 0x0083, 0x201E, 0x2026, 0x2020, 0x2021,
	0x0088, 0x2030, 0x0160, 0x2039, 0x015A, 0x0164, 0x017D, 0x0179,
	0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
	0x0098, 0x2122, 0x0161, 0x203A, 0x015B, 0x0165, 0x017E, 0x017A,
	0x00A0, 0x02C7, 0x02D8, 0x0141, 0x00A4, 0x0104, 0x00A6, 0x00A7,
	0x00A8, 0x00A9, 0x015E, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x017B,
	0x00B0, 0x00B1, 0x02DB, 0x0142, 0x00B4, 0x00B5, 0x00B6, 0x00B7,
	0x00B8, 0x0105, 0x015F, 0x00BB, 0x013D, 0x02DD, 0x013E, 0x017C,
	0x0154, 0x00C1, 0x00C2, 0x0102, 0x00C4, 0x0139, 0x0106, 0x00C7,
	0x010C, 0x00C9, 0x0118, 0x00CB, 0x011A, 0x00CD, 0x00CE, 0x010E,
	0x0110, 0x0143, 0x0147, 0x00D3, 0x00D4, 0x0150, 0x00D6, 0x00D7,
	0x0158, 0x016E, 0x00DA, 0x0170, 0x00DC, 0x00DD, 0x0162, 0x00DF,
	0x0155, 0x00E1, 0x00E2, 0x0103, 0x00E4, 0x013A, 0x0107, 0x00E7,
	0x010D, 0x00E9, 0x0119, 0x00EB, 0x011B, 0x00ED, 0x00EE, 0x010F,
	0x0111, 0x0144, 0x0148, 0x00F3, 0x00F4, 0x0151, 0x00F6, 0x00F7,
	0x0159, 0x016F, 0x00FA, 0x0171, 0x00FC, 0x00FD, 0x0163, 0x02D9,
};
const WORD CP1251[128] =
{
	0x0402, 0x0403, 0x201A, 0x0453, 0x201E, 0x2026, 0x2020, 0x2021,
	0x20AC, 0x2030, 0x0409, 0x2039, 0x040A, 0x040C, 0x040B, 0x040F,
	0x0452, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
	0x0098, 0x2122, 0x0459, 0x203A, 0x045A, 0x045C, 0x045B, 0x045F,
	0x00A0, 0x040E, 0x045E, 0x0408, 0x00A4, 0x0490, 0x00A6, 0x00A7,
	0x0401, 0x00A9, 0x0404, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x0407,
	0x00B0, 0x00B1, 0x0406, 0x0456, 0x0491, 0x00B5, 0x00B6, 0x00B7,
	0x0451, 0x2116, 0x0454, 0x00BB, 0x0458, 0x0405, 0x0455, 0x0457,
	0x0410, 0x0411, 0x0412, 0x0413, 0x0414, 0x0415, 0x0416, 0x0417,
	0x0418, 0x0419, 0x041A, 0x041B, 0x041C, 0x041D, 0x041E, 0x041F,
	0x0420, 0x0421, 0x0422, 0x0423, 0x0424, 0x0425, 0x0426, 0x0427,
	0x0428, 0x0429, 0x042A, 0x042B, 0x042C, 0x042D, 0x042E, 0x042F,
	0x0430, 0x0431, 0x0432, 0x0433, 0x0434, 0x0435, 0x0436, 0x0437,
	0x0438, 0x0439, 0x043A, 0x043B, 0x043C, 0x043D, 0x043E, 0x043F,
	0x0440, 0x0441, 0x0442, 0x0443, 0x0444, 0x0445, 0x0446, 0x0447,
	0x0448, 0x0449, 0x044A, 0x044B, 0x044C, 0x044D, 0x044E, 0x044F,
};
const WORD CP1252[128] =
{
	0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
	0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008D, 0x017D, 0x008F,
	0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
	0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178,
	0x00A0, 0x00A1, 0x00A2, 0x00A3, 0x00A4, 0x00A5, 0x00A6, 0x00A7,
	0x00A8, 0x00A9, 0x00AA, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x00AF,
	0x00B0, 0x00B1, 0x00B2, 0x00B3, 0x00B4, 0x00B5, 0x00B6, 0x00B7,
	0x00B8, 0x00B9, 0x00BA, 0x00BB, 0x00BC, 0x00BD, 0x00BE, 0x00BF,
	0x00C0, 0x00C1, 0x00C2, 0x00C3, 0x00C4, 0x00C5, 0x00C6, 0x00C7,
	0x00C8, 0x00C9, 0x00CA, 0x00CB, 0x00CC, 0x00CD, 0x00CE, 0x00CF,
	0x00D0, 0x00D1, 0x00D2, 0x00D3, 0x00D4, 0x00D5, 0x00D6, 0x00D7,
	0x00D8, 0x00D9, 0x00DA, 0x00DB, 0x00DC, 0x00DD, 0x00DE, 0x00DF,
	0x00E0, 0x00E1, 0x00E2, 0x00E3, 0x00E4, 0x00E5, 0x00E6, 0x00E7,
	0x00E8, 0x00E9, 0x00EA, 0x00EB, 0x00EC, 0x00ED, 0x00EE, 0x00EF,
	0x00F0, 0x00F1, 0x00F2, 0x00F3, 0x00F4, 0x00F5, 0x00F6, 0x00F7,
	0x00F8, 0x00F9, 0x00FA, 0x00FB, 0x00FC, 0x00FD, 0x00FE, 0x00FF,
};

const WORD* UpperHalf( int nCodePage )
{
	switch ( nCodePage )
	{
		case 1250: return CP1250;
		case 1251: return CP1251;
		case 1252: return CP1252;
		default: return 0;
	}
}

int Round( float f ) { return int( floorf( f + 0.5f ) ); }

// One rendered character: GDI's ABC widths in pixels and its coverage
// bitmap, whose left column is the ink's (A) and whose top row is nTop pixels
// below the cell top.
struct SGlyph
{
	BYTE cAnsi;
	WORD wUnicode;
	int nA, nB, nC;
	int nSizeY, nTop;
	std::vector<BYTE> coverage;					// nB * nSizeY
	int nCluster;												// index into the request's clusters, -1 for a plain character
	SGlyph() : cAnsi( 0 ), wUnicode( 0 ), nA( 0 ), nB( 0 ), nC( 0 ), nSizeY( 0 ), nTop( 0 ), nCluster( -1 ) {  }
	int Cell() const { return nB + abs( nC ); }	// FontGen's nNextCharShift
};

// A coverage bitmap before the style is applied: its left column is x0 pixels
// right of the pen, its top row y0 pixels below the baseline.
struct SRaster
{
	int x0, y0, nSizeX, nSizeY;
	std::vector<BYTE> coverage;
	SRaster() : x0( 0 ), y0( 0 ), nSizeX( 0 ), nSizeY( 0 ) {  }
};

void RasterGlyph( const stbtt_fontinfo &font, float fScale, int nGlyph, SRaster *pRaster )
{
	int x1 = 0, y1 = 0;
	stbtt_GetGlyphBitmapBox( &font, nGlyph, fScale, fScale, &pRaster->x0, &pRaster->y0, &x1, &y1 );
	pRaster->nSizeX = x1 - pRaster->x0;
	pRaster->nSizeY = y1 - pRaster->y0;
	if ( pRaster->nSizeX > 0 && pRaster->nSizeY > 0 )
	{
		pRaster->coverage.resize( pRaster->nSizeX * pRaster->nSizeY );
		stbtt_MakeGlyphBitmap( &font, &pRaster->coverage[0], pRaster->nSizeX, pRaster->nSizeY, pRaster->nSizeX, fScale, fScale, nGlyph );
	}
}

// The glyphs of a shaped cluster in one bitmap, each at HarfBuzz's position
// scaled to pixels (the fraction of it kept by the subpixel raster, so the
// parts of a conjunct and the headline of a Devanagari cluster meet where the
// font put them). Returns the run's advance in pixels, unrounded.
float RasterCluster( const stbtt_fontinfo &font, float fScale, const SBkShaped &shaped, SRaster *pRaster )
{
	struct SPart { int nGlyph, x0, y0, x1, y1; float fShiftX, fShiftY; };
	std::vector<SPart> parts;
	float fPen = 0;
	int nMinX = 0, nMinY = 0, nMaxX = 0, nMaxY = 0;
	for ( size_t i = 0; i < shaped.nGlyphs; ++i )
	{
		const SBkShapedGlyph &glyph = shaped.pGlyphs[i];
		const float fX = fPen + glyph.nXOffset * fScale;
		const float fY = -glyph.nYOffset * fScale;
		fPen += glyph.nXAdvance * fScale;
		const float fIntX = floorf( fX ), fIntY = floorf( fY );
		SPart part;
		part.nGlyph = int( glyph.nGlyphId );
		part.fShiftX = fX - fIntX;
		part.fShiftY = fY - fIntY;
		stbtt_GetGlyphBitmapBoxSubpixel( &font, part.nGlyph, fScale, fScale, part.fShiftX, part.fShiftY, &part.x0, &part.y0, &part.x1, &part.y1 );
		part.x0 += int( fIntX ); part.x1 += int( fIntX );
		part.y0 += int( fIntY ); part.y1 += int( fIntY );
		if ( part.x1 <= part.x0 || part.y1 <= part.y0 )
			continue;
		if ( parts.empty() || part.x0 < nMinX ) nMinX = part.x0;
		if ( parts.empty() || part.y0 < nMinY ) nMinY = part.y0;
		if ( parts.empty() || part.x1 > nMaxX ) nMaxX = part.x1;
		if ( parts.empty() || part.y1 > nMaxY ) nMaxY = part.y1;
		parts.push_back( part );
	}
	if ( !parts.empty() )
	{
		pRaster->x0 = nMinX;
		pRaster->y0 = nMinY;
		pRaster->nSizeX = nMaxX - nMinX;
		pRaster->nSizeY = nMaxY - nMinY;
		pRaster->coverage.assign( size_t( pRaster->nSizeX ) * pRaster->nSizeY, 0 );
		std::vector<BYTE> one;
		for ( size_t i = 0; i < parts.size(); ++i )
		{
			const SPart &part = parts[i];
			const int nW = part.x1 - part.x0, nH = part.y1 - part.y0;
			one.assign( size_t( nW ) * nH, 0 );
			stbtt_MakeGlyphBitmapSubpixel( &font, &one[0], nW, nH, nW, fScale, fScale, part.fShiftX, part.fShiftY, part.nGlyph );
			// Where two parts overlap the stronger coverage wins, so a
			// headline drawn by two glyphs does not get a brighter seam.
			for ( int y = 0; y < nH; ++y )
				for ( int x = 0; x < nW; ++x )
				{
					BYTE &c = pRaster->coverage[size_t( part.y0 - nMinY + y ) * pRaster->nSizeX + part.x0 - nMinX + x];
					c = (std::max)( c, one[size_t( y ) * nW + x] );
				}
		}
	}
	return fPen;
}

// Applies the style to the raster and turns it into the character's ABC
// widths in pixels (nPixelAdvance before the simulated bold's extra pixel).
void FinishGlyph( int nAscent, const SFontRequest &request, bool bEmbolden, bool bSlant, int nPixelAdvance,
	SRaster *pRaster, SGlyph *pGlyph )
{
	int x0 = pRaster->x0, y0 = pRaster->y0, nSizeX = pRaster->nSizeX, nSizeY = pRaster->nSizeY;
	std::vector<BYTE> &coverage = pRaster->coverage;
	if ( nSizeX > 0 && nSizeY > 0 )
	{
		if ( !request.bAntialias )
			for ( size_t i = 0; i < coverage.size(); ++i )
				coverage[i] = coverage[i] >= 128 ? 255 : 0;
		if ( bSlant )
		{
			// Shear about the baseline: a row h pixels above it moves h * slant
			// to the right, as GDI's simulated italic does.
			std::vector<int> shifts( nSizeY );
			int nMinShift = 0, nMaxShift = 0;
			for ( int y = 0; y < nSizeY; ++y )
			{
				shifts[y] = Round( -( y0 + y + 0.5f ) * ITALIC_SLANT );
				if ( y == 0 || shifts[y] < nMinShift ) nMinShift = shifts[y];
				if ( y == 0 || shifts[y] > nMaxShift ) nMaxShift = shifts[y];
			}
			const int nSlantedX = nSizeX + nMaxShift - nMinShift;
			std::vector<BYTE> slanted( nSlantedX * nSizeY, 0 );
			for ( int y = 0; y < nSizeY; ++y )
				for ( int x = 0; x < nSizeX; ++x )
					slanted[y * nSlantedX + x + shifts[y] - nMinShift] = coverage[y * nSizeX + x];
			coverage.swap( slanted );
			x0 += nMinShift;
			nSizeX = nSlantedX;
		}
		if ( bEmbolden )
		{
			// GDI's simulated bold: the glyph smeared one pixel to the right,
			// and one pixel wider to advance over.
			std::vector<BYTE> bold( ( nSizeX + 1 ) * nSizeY, 0 );
			for ( int y = 0; y < nSizeY; ++y )
				for ( int x = 0; x <= nSizeX; ++x )
				{
					const BYTE cLeft = x > 0 ? coverage[y * nSizeX + x - 1] : 0;
					const BYTE cHere = x < nSizeX ? coverage[y * nSizeX + x] : 0;
					bold[y * ( nSizeX + 1 ) + x] = cLeft > cHere ? cLeft : cHere;
				}
			coverage.swap( bold );
			++nSizeX;
		}
	}
	else
	{
		// No ink (space, no-break space): all of the advance is pre/post-space.
		x0 = 0;
		nSizeX = 0;
		nSizeY = 0;
	}
	if ( bEmbolden )
		++nPixelAdvance;
	pGlyph->nA = x0;
	pGlyph->nB = nSizeX;
	pGlyph->nC = nPixelAdvance - x0 - nSizeX;
	pGlyph->nSizeY = nSizeY;
	pGlyph->nTop = nAscent + y0;
	pGlyph->coverage.swap( coverage );
}

void RenderGlyph( const stbtt_fontinfo &font, float fScale, int nAscent, const SFontRequest &request,
	bool bEmbolden, bool bSlant, SGlyph *pGlyph )
{
	const int nGlyph = stbtt_FindGlyphIndex( &font, pGlyph->wUnicode );	// 0, the .notdef box, when missing
	int nAdvance = 0, nLeftBearing = 0;
	stbtt_GetGlyphHMetrics( &font, nGlyph, &nAdvance, &nLeftBearing );
	SRaster raster;
	RasterGlyph( font, fScale, nGlyph, &raster );
	FinishGlyph( nAscent, request, bEmbolden, bSlant, Round( nAdvance * fScale ), &raster, pGlyph );
}

// FontGen's EstimateTextureSize: the smallest 2:1 or 1:1 power of two that
// holds every character at the average width, nHeight being the row pitch.
bool EstimateTextureSize( int nAveCharWidth, int nHeight, int nNumChars, int *pSizeX, int *pSizeY )
{
	const int nCellX = nAveCharWidth + LEADING_PIXELS;
	for ( int i = 6; i < 13; ++i )
	{
		if ( ( ( 1 << i ) / nCellX ) * ( ( 1 << ( i - 1 ) ) / nHeight ) >= nNumChars )
		{
			*pSizeX = 1 << i;
			*pSizeY = 1 << ( i - 1 );
			return true;
		}
		if ( ( ( 1 << i ) / nCellX ) * ( ( 1 << i ) / nHeight ) >= nNumChars )
		{
			*pSizeX = *pSizeY = 1 << i;
			return true;
		}
	}
	return false;
}

// FontGen's row packing: cells left to right, LEADING_PIXELS apart, a new row
// nHeight pixels further down (the row pitch) when the next cell does not fit. Writes each cell's left
// edge and row; false when the rows run out of texture.
bool Pack( const std::vector<SGlyph> &glyphs, int nHeight, int nSizeX, int nSizeY, std::vector<int> *pX, std::vector<int> *pRow )
{
	pX->resize( glyphs.size() );
	pRow->resize( glyphs.size() );
	int x = 0, nRow = 0;
	for ( size_t i = 0; i < glyphs.size(); ++i )
	{
		const int nCell = glyphs[i].Cell();
		if ( x + nCell + LEADING_PIXELS > nSizeX )
		{
			++nRow;
			x = 0;
			if ( nCell + LEADING_PIXELS > nSizeX )
				return false;
		}
		if ( ( nRow + 1 ) * nHeight > nSizeY )
			return false;
		x += LEADING_PIXELS;
		(*pX)[i] = x;
		(*pRow)[i] = nRow;
		x += nCell;
	}
	return true;
}

std::string FamilyName( const stbtt_fontinfo &font )
{
	int nLength = 0;
	// Windows Unicode English first (UTF-16BE), then Mac Roman.
	if ( const char *pszName = stbtt_GetFontNameString( &font, &nLength, STBTT_PLATFORM_ID_MICROSOFT, STBTT_MS_EID_UNICODE_BMP, STBTT_MS_LANG_ENGLISH, 1 ) )
	{
		std::string szName;
		for ( int i = 0; i + 1 < nLength; i += 2 )
		{
			const int nChar = ( BYTE( pszName[i] ) << 8 ) | BYTE( pszName[i + 1] );
			szName += nChar < 128 ? char( nChar ) : '?';
		}
		return szName;
	}
	if ( const char *pszName = stbtt_GetFontNameString( &font, &nLength, STBTT_PLATFORM_ID_MAC, STBTT_MAC_EID_ROMAN, STBTT_MAC_LANG_ENGLISH, 1 ) )
		return std::string( pszName, nLength );
	return std::string();
}
}

bool GetCharSet( int nCodePage, BYTE *pCharSet )
{
	switch ( nCodePage )
	{
		case 1250: *pCharSet = 238; return true;		// EASTEUROPE_CHARSET
		case 1251: *pCharSet = 204; return true;		// RUSSIAN_CHARSET
		case 1252: *pCharSet = 0; return true;			// ANSI_CHARSET
		default: return false;
	}
}

WORD ToUnicode( int nCodePage, BYTE c )
{
	const WORD *pUpper = UpperHalf( nCodePage );
	if ( pUpper == 0 )
		return 0;
	return c < 128 ? WORD( c ) : pUpper[c - 128];
}

int FindFace( const BYTE *pFontData, int nFontDataSize, const std::string &szName, bool bBold, bool bItalic )
{
	if ( pFontData == 0 || nFontDataSize < 12 || szName.empty() )
		return -1;
	stbtt_uint8 *pData = const_cast<stbtt_uint8*>( pFontData );
	stbtt_uint8 *pName = reinterpret_cast<stbtt_uint8*>( const_cast<char*>( szName.c_str() ) );
	const int nStyle = ( bBold ? STBTT_MACSTYLE_BOLD : 0 ) | ( bItalic ? STBTT_MACSTYLE_ITALIC : 0 );
	// The asked style first (STBTT_MACSTYLE_NONE means "regular"), then any
	// face of the family, which Build then emboldens or slants.
	const int styles[2] = { nStyle != 0 ? nStyle : STBTT_MACSTYLE_NONE, STBTT_MACSTYLE_DONTCARE };
	for ( int s = 0; s < 2; ++s )
		for ( int nIndex = 0; ; ++nIndex )
		{
			const int nOffset = stbtt_GetFontOffsetForIndex( pData, nIndex );
			if ( nOffset < 0 || nOffset >= nFontDataSize )
				break;
			if ( stbtt__matches( pData, nOffset, pName, styles[s] ) )
				return nIndex;
		}
	return -1;
}

bool Build( const SFontRequest &request, SFontAtlas *pAtlas, std::string *pszError )
{
	BYTE cCharSet = 0;
	if ( !GetCharSet( request.nCodePage, &cCharSet ) )
	{
		*pszError = NStr::Format( "code page %d is not supported (1250, 1251 and 1252 are)", request.nCodePage );
		return false;
	}
	const bool bUnicode = !request.codePoints.empty();
	if ( !bUnicode && !request.clusters.empty() )
	{
		*pszError = "cluster glyphs need a code point list (Unicode mode)";
		return false;
	}
	if ( request.clusters.size() > size_t( CBkClusterTable::LAST - CBkClusterTable::FIRST + 1 ) )
	{
		*pszError = NStr::Format( "%d clusters do not fit the %d Private Use Area slots", int( request.clusters.size() ), CBkClusterTable::LAST - CBkClusterTable::FIRST + 1 );
		return false;
	}
	if ( request.nHeight < 4 || request.nHeight > 256 )
	{
		*pszError = NStr::Format( "height %d is outside 4..256 pixels", request.nHeight );
		return false;
	}
	if ( request.nWeight < 0 || request.nWeight > 1000 )
	{
		*pszError = NStr::Format( "weight %d is outside 0..1000", request.nWeight );
		return false;
	}
	stbtt_fontinfo font;
	stbtt_uint8 *pData = const_cast<stbtt_uint8*>( request.pFontData );
	const int nOffset = pData != 0 && request.nFontDataSize >= 12 ? stbtt_GetFontOffsetForIndex( pData, request.nFaceIndex ) : -1;
	if ( nOffset < 0 || nOffset >= request.nFontDataSize || !stbtt_InitFont( &font, pData, nOffset ) )
	{
		*pszError = NStr::Format( "not a TrueType or OpenType font (or no face %d in it)", request.nFaceIndex );
		return false;
	}
	// macStyle of the face itself: a face that is already bold or italic is
	// not emboldened or slanted again.
	const stbtt_uint32 nHead = stbtt__find_table( pData, nOffset, "head" );
	const int nMacStyle = nHead != 0 ? ttUSHORT( pData + nHead + 44 ) : 0;
	const bool bEmbolden = request.nWeight >= 600 && ( nMacStyle & 1 ) == 0;
	const bool bSlant = request.bItalic && ( nMacStyle & 2 ) == 0;

	// GDI's positive height is the cell, ascent + descent, which is what
	// stbtt_ScaleForPixelHeight maps to the requested pixels.
	const float fScale = stbtt_ScaleForPixelHeight( &font, float( request.nHeight ) );
	int nAscentUnits = 0, nDescentUnits = 0, nLineGapUnits = 0;
	stbtt_GetFontVMetrics( &font, &nAscentUnits, &nDescentUnits, &nLineGapUnits );
	int nAscent = Round( nAscentUnits * fScale );
	if ( nAscent > request.nHeight ) nAscent = request.nHeight;
	if ( nAscent < 0 ) nAscent = 0;
	const int nEmPixels = Round( fScale / stbtt_ScaleForMappingEmToPixels( &font, 1.0f ) );

	std::vector<SGlyph> glyphs;
	int nSkipped = 0;
	if ( bUnicode )
	{
		// The request's code points, sorted and distinct, without the ones
		// the face cannot draw: the game draws wDefaultChar for those.
		std::vector<WORD> codes( request.codePoints );
		std::sort( codes.begin(), codes.end() );
		codes.erase( std::unique( codes.begin(), codes.end() ), codes.end() );
		glyphs.reserve( codes.size() + request.clusters.size() );
		for ( size_t i = 0; i < codes.size(); ++i )
		{
			if ( stbtt_FindGlyphIndex( &font, codes[i] ) == 0 )
			{
				++nSkipped;
				continue;
			}
			SGlyph glyph;
			glyph.wUnicode = codes[i];
			glyphs.push_back( glyph );
		}
		for ( size_t i = 0; i < request.clusters.size(); ++i )
		{
			SGlyph glyph;
			glyph.wUnicode = WORD( CBkClusterTable::FIRST + i );
			glyph.nCluster = int( i );
			glyphs.push_back( glyph );
		}
	}
	else
	{
		// FontGen's character list: 32..255 plus the default character, sorted.
		glyphs.reserve( LAST_CHAR - FIRST_CHAR + 2 );
		SGlyph glyph;
		glyph.cAnsi = DEFAULT_CHAR;
		glyph.wUnicode = ToUnicode( request.nCodePage, DEFAULT_CHAR );
		glyphs.push_back( glyph );
		for ( int c = FIRST_CHAR; c <= LAST_CHAR; ++c )
		{
			SGlyph glyph;
			glyph.cAnsi = BYTE( c );
			glyph.wUnicode = ToUnicode( request.nCodePage, BYTE( c ) );
			glyphs.push_back( glyph );
		}
	}
	if ( glyphs.empty() )
	{
		*pszError = "the font has none of the requested characters";
		return false;
	}
	int nMaxCharWidth = 0;
	int nAveCharWidth = 0;
	for ( size_t i = 0; i < glyphs.size(); ++i )
	{
		if ( glyphs[i].nCluster < 0 )
			RenderGlyph( font, fScale, nAscent, request, bEmbolden, bSlant, &glyphs[i] );
		else
		{
			std::u16string cluster = request.clusters[glyphs[i].nCluster];
			// A cluster that ends in a halant is a half form in its text (the next consonant
			// follows it); alone, its halant would draw as a visible mark. The zero-width joiner
			// asks for the half form, the glyph the shaped text has.
			if ( !cluster.empty() && cluster[cluster.size() - 1] == 0x094D )
				cluster += char16_t( 0x200D );
			char szShapeError[256] = "";
			SBkShaped *pShaped = cluster.empty() ? 0 : BkShapeClusters( request.pFontData, request.nFontDataSize,
				reinterpret_cast<const unsigned short*>( cluster.data() ), cluster.size(), request.szScript.c_str(),
				request.szLanguage.c_str(), szShapeError, sizeof( szShapeError ) );
			if ( pShaped == 0 )
			{
				*pszError = NStr::Format( "cluster %d (U+%04X) cannot be shaped: %s", glyphs[i].nCluster, int( glyphs[i].wUnicode ), szShapeError );
				return false;
			}
			SRaster raster;
			const float fAdvance = RasterCluster( font, fScale, *pShaped, &raster );
			BkShapeFree( pShaped );
			FinishGlyph( nAscent, request, bEmbolden, bSlant, Round( fAdvance ), &raster, &glyphs[i] );
		}
		const int nAdvance = glyphs[i].nA + glyphs[i].nB + glyphs[i].nC;
		if ( nAdvance > nMaxCharWidth )
			nMaxCharWidth = nAdvance;
		if ( glyphs[i].wUnicode == 'x' )
			nAveCharWidth = nAdvance;
	}
	if ( nAveCharWidth <= 0 && bUnicode )
	{
		// No 'x' (Devanagari, Han): the mean advance of the plain characters.
		int nSum = 0, nCount = 0;
		for ( size_t i = 0; i < glyphs.size(); ++i )
			if ( glyphs[i].nCluster < 0 )
			{
				nSum += glyphs[i].nA + glyphs[i].nB + glyphs[i].nC;
				++nCount;
			}
		nAveCharWidth = nCount > 0 ? nSum / nCount : 0;
	}
	if ( nAveCharWidth <= 0 )
		nAveCharWidth = nMaxCharWidth > 0 ? nMaxCharWidth : 1;

	// FontGen estimated the size, packed once and grew one step on overflow;
	// growing until everything fits gives the same size whenever FontGen's
	// result was complete.
	int nSizeX = 0, nSizeY = 0;
	const int nRowPitch = request.nHeight + LEADING_PIXELS;
	if ( !EstimateTextureSize( nAveCharWidth, nRowPitch, int( glyphs.size() ), &nSizeX, &nSizeY ) )
	{
		nSizeX = MAX_TEXTURE_SIZE;
		nSizeY = MAX_TEXTURE_SIZE;
	}
	std::vector<int> cellX, cellRow;
	while ( !Pack( glyphs, nRowPitch, nSizeX, nSizeY, &cellX, &cellRow ) )
	{
		if ( nSizeX == nSizeY )
			nSizeX <<= 1;
		else
			nSizeY = nSizeX;
		if ( nSizeX > MAX_TEXTURE_SIZE )
		{
			*pszError = NStr::Format( "%d glyphs of %d pixels do not fit a %dx%d atlas", int( glyphs.size() ), request.nHeight, MAX_TEXTURE_SIZE, MAX_TEXTURE_SIZE );
			return false;
		}
	}

	SFontFormat &format = pAtlas->format;
	format.szFaceName = request.szFaceName.empty() ? FamilyName( font ) : request.szFaceName;
	NStr::ToLower( format.szFaceName );
	format.metrics.nHeight = request.nHeight;
	format.metrics.nAscent = nAscent;
	format.metrics.nDescent = request.nHeight - nAscent;
	format.metrics.nInternalLeading = nEmPixels < request.nHeight ? request.nHeight - nEmPixels : 0;
	format.metrics.nExternalLeading = nLineGapUnits > 0 ? Round( nLineGapUnits * fScale ) : 0;
	format.metrics.nAveCharWidth = nAveCharWidth;
	format.metrics.nMaxCharWidth = nMaxCharWidth;
	format.metrics.cCharSet = cCharSet;
	if ( !bUnicode )
		format.metrics.wDefaultChar = ToUnicode( request.nCodePage, DEFAULT_CHAR );
	else
	{
		// '?' if the font has it, else the space, else the first character:
		// never a code page byte, the request has none.
		format.metrics.wDefaultChar = glyphs[0].wUnicode;
		for ( size_t i = 0; i < glyphs.size(); ++i )
			if ( glyphs[i].wUnicode == '?' ) { format.metrics.wDefaultChar = '?'; break; }
			else if ( glyphs[i].wUnicode == ' ' ) format.metrics.wDefaultChar = ' ';
	}
	format.chars.clear();
	format.kerns.clear();

	SFontImage &image = pAtlas->image;
	image.nSizeX = nSizeX;
	image.nSizeY = nSizeY;
	image.pixels.assign( size_t( nSizeX ) * nSizeY, 0 );
	for ( size_t i = 0; i < glyphs.size(); ++i )
	{
		const SGlyph &glyph = glyphs[i];
		const int x = cellX[i];
		const int nCellTop = cellRow[i] * nRowPitch;
		SFontFormat::SCharDesc &desc = format.chars[glyph.wUnicode];
		desc.fA = float( glyph.nA );
		desc.fB = float( glyph.nB );
		desc.fC = float( glyph.nC );
		desc.fWidth = float( glyph.nB + ( glyph.nC > 0 ? glyph.nC : 0 ) );
		// FontGen's texture rect, half-texel offset included: the GPU path and
		// the shipped fonts both expect it.
		desc.x1 = ( x + 0.5f ) / nSizeX;
		desc.y1 = ( nCellTop + 0.5f ) / nSizeY;
		desc.x2 = ( x + desc.fWidth + 0.5f ) / nSizeX;
		desc.y2 = ( nCellTop + request.nHeight + 0.5f ) / nSizeY;
		// The ink starts at the cell's left edge (FontGen drew at x - A) and
		// the baseline sits nAscent below the cell top; rows outside the cell
		// (accents above a tall capital at a small size) are cut, so no glyph
		// reaches into the next row's cells.
		for ( int y = 0; y < glyph.nSizeY; ++y )
		{
			const int nRowInCell = glyph.nTop + y;
			if ( nRowInCell < 0 || nRowInCell >= request.nHeight )
				continue;
			DWORD *pRow = &image.pixels[size_t( nCellTop + nRowInCell ) * nSizeX + x];
			for ( int g = 0; g < glyph.nB; ++g )
			{
				// FontGen's pixel: white, alpha = coverage, the grey repeated.
				const DWORD c = glyph.coverage[y * glyph.nB + g];
				pRow[g] = ( c << 24 ) | ( c << 16 ) | ( c << 8 ) | c;
			}
		}
	}
	{
		const SFontFormat::CCharacterMap::const_iterator space = format.chars.find( ToUnicode( request.nCodePage, ' ' ) );
		format.metrics.fSpaceWidth = space != format.chars.end() ? space->second.fWidth : float( nAveCharWidth / 2 );
	}

	// GetKerningPairs' list, read from the face's kern or GPOS table and
	// rounded to whole pixels as GDI rounds it; zero pairs are left out.
	// A cluster glyph is a whole shaped run, which carries its own spacing, and
	// the Han ranges are not kerned by their faces: leaving both out also keeps
	// a few thousand characters from costing millions of GPOS lookups.
	std::vector<WORD> kerned;
	for ( size_t i = 0; i < glyphs.size(); ++i )
	{
		const WORD w = glyphs[i].wUnicode;
		const bool bSkip = bUnicode && ( glyphs[i].nCluster >= 0 || ( w >= 0x2E80 && w < 0xA000 ) || ( w >= 0xAC00 && w < 0xD800 ) || w >= 0xF900 );
		if ( !bSkip )
			kerned.push_back( w );
	}
	for ( size_t i = 0; i < kerned.size(); ++i )
		for ( size_t j = 0; j < kerned.size(); ++j )
		{
			const int nKern = Round( stbtt_GetCodepointKernAdvance( &font, kerned[i], kerned[j] ) * fScale );
			if ( nKern != 0 )
				format.kerns[( DWORD( kerned[i] ) << 16 ) | kerned[j]] = float( nKern );
		}
	pAtlas->nSkipped = nSkipped;
	pAtlas->nClusters = int( request.clusters.size() );
	return true;
}
}
