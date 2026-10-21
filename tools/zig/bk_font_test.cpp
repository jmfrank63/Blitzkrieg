// test-bk-font: the D-14 structural acceptance of FontAtlas and the proof
// that the game's readers get back what bk-font wrote (1.tfd, 1.tga, 1_h.dds). Byte parity with the
// GDI-made Data/Fonts is not a goal (different rasteriser); instead every font
// here - the generated ones and, as a check of the checker, the shipped ones -
// must have every requested character, self-consistent metrics, texture rects
// inside the texture, no two glyph cells overlapping and no ink outside a
// cell, and the generated ones must carry exactly the source face's kerning.
//
// Arguments: <module root> <fixture font> <bk-font output dir> [more fonts...]
// The language-mod fonts (M002 D058: Cyrillic, Han, Hindi PUA clusters from the
// vendored Noto faces) are judged from BK_FONT_TEST_UNICODE, the folder with
// the cyrillic, han30 and hindi directories the build's bk-font runs wrote.
// The output dir is what the build's bk-font run wrote for the fixture at 13
// pixels, code page 1252. Extra fonts get the structural and kerning checks,
// a way to exercise kerning with a face that has some (the committed fixture,
// ProggyClean, has none).
#include "StdAfx.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "data_only_startup.h"
#include "../../Sources/src/FontAtlas/FontAtlas.h"
#include "../../Sources/src/FontAtlas/Shaper.h"
#include "../../Sources/src/Image/Image.h"

// The test's own copy of the rasteriser, as the oracle for the kerning pairs.
#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "../../vendor/dcimgui/src-docking/imstb_truetype.h"

namespace fs = std::filesystem;

static int g_nFailures = 0;

static bool Check( bool bCondition, const std::string &szWhat )
{
	if ( !bCondition )
	{
		printf( "FAIL: %s\n", szWhat.c_str() );
		++g_nFailures;
	}
	return bCondition;
}

static bool Near( float a, float b ) { return fabsf( a - b ) < 1e-3f; }

static std::vector<BYTE> ReadBytes( const fs::path &path )
{
	std::ifstream in( path, std::ios::binary );
	return std::vector<BYTE>( std::istreambuf_iterator<char>( in ), std::istreambuf_iterator<char>() );
}

// A texture rect in pixels, the half-texel offset FontGen put into the UVs
// taken back out.
struct SCell
{
	WORD wChar;
	float fLeft, fTop, fRight, fBottom;
};

static SCell CellOf( WORD wChar, const SFontFormat::SCharDesc &desc, int nSizeX, int nSizeY )
{
	SCell cell;
	cell.wChar = wChar;
	cell.fLeft = desc.x1 * nSizeX - 0.5f;
	cell.fTop = desc.y1 * nSizeY - 0.5f;
	cell.fRight = desc.x2 * nSizeX - 0.5f;
	cell.fBottom = desc.y2 * nSizeY - 0.5f;
	return cell;
}

// The D-14 structure. pPixels may be null (the shipped fonts' textures are
// DDS); then only the description is checked.
// pCodes (Unicode mode) replaces "bytes 32..255 of the code page" as the
// characters that must be there, and drops the checks that assume Latin.
static void CheckStructure( const std::string &szWhat, const SFontFormat &format, int nSizeX, int nSizeY,
	const std::vector<DWORD> *pPixels, int nCodePage, const std::vector<WORD> *pCodes = 0 )
{
	const SFontFormat::SFontMetrics &m = format.metrics;
	// Every requested character, and the one drawn for the rest.
	int nMissing = 0;
	if ( pCodes != 0 )
	{
		for ( size_t i = 0; i < pCodes->size(); ++i )
			if ( format.chars.find( (*pCodes)[i] ) == format.chars.end() )
				++nMissing;
	}
	else
		for ( int c = FontAtlas::FIRST_CHAR; c <= FontAtlas::LAST_CHAR; ++c )
			if ( format.chars.find( FontAtlas::ToUnicode( nCodePage, BYTE( c ) ) ) == format.chars.end() )
				++nMissing;
	Check( nMissing == 0, szWhat + ": every character 32..255 of the code page is present (" + std::to_string( nMissing ) + " missing)" );
	Check( format.chars.find( m.wDefaultChar ) != format.chars.end(), szWhat + ": the default character is present" );
	BYTE cCharSet = 0;
	Check( pCodes != 0 || ( FontAtlas::GetCharSet( nCodePage, &cCharSet ) && cCharSet == m.cCharSet ), szWhat + ": the charset byte names the code page" );

	// Metrics against each other and against the characters.
	Check( m.nHeight > 0 && m.nAscent >= 0 && m.nDescent >= 0 && m.nAscent + m.nDescent == m.nHeight, szWhat + ": ascent + descent == height" );
	Check( m.nInternalLeading >= 0 && m.nInternalLeading <= m.nHeight && m.nExternalLeading >= 0, szWhat + ": leadings inside the cell" );
	Check( m.nAveCharWidth > 0 && m.nAveCharWidth <= m.nMaxCharWidth, szWhat + ": 0 < average width <= maximum width" );
	const std::unordered_map<WORD, SFontFormat::SCharDesc>::const_iterator space = format.chars.find( FontAtlas::ToUnicode( nCodePage, ' ' ) );
	Check( pCodes != 0 || ( space != format.chars.end() && Near( m.fSpaceWidth, space->second.fWidth ) ), szWhat + ": the space width is the space's width" );

	std::vector<SCell> cells;
	int nBadChars = 0;
	for ( SFontFormat::CCharacterMap::const_iterator it = format.chars.begin(); it != format.chars.end(); ++it )
	{
		const SFontFormat::SCharDesc &desc = it->second;
		const SCell cell = CellOf( it->first, desc, nSizeX, nSizeY );
		const bool bWidth = desc.fB >= 0 && Near( desc.fWidth, desc.fB + ( desc.fC > 0 ? desc.fC : 0 ) );
		const bool bAdvance = desc.fA + desc.fB + desc.fC <= m.nMaxCharWidth + 0.001f;
		// The rect is fWidth wide, one cell high, on whole pixels, inside the texture.
		const bool bRect = Near( cell.fRight - cell.fLeft, desc.fWidth ) && Near( cell.fBottom - cell.fTop, float( m.nHeight ) ) &&
			Near( cell.fLeft, floorf( cell.fLeft + 0.5f ) ) && Near( cell.fTop, floorf( cell.fTop + 0.5f ) ) &&
			cell.fLeft > -0.001f && cell.fTop > -0.001f && cell.fRight < nSizeX + 0.001f && cell.fBottom < nSizeY + 0.001f;
		if ( !bWidth || !bAdvance || !bRect )
		{
			if ( nBadChars < 5 )
				printf( "bk-font-test: %s: U+%04X a=%g b=%g c=%g width=%g rect %g,%g-%g,%g\n", szWhat.c_str(), it->first,
					desc.fA, desc.fB, desc.fC, desc.fWidth, cell.fLeft, cell.fTop, cell.fRight, cell.fBottom );
			++nBadChars;
		}
		if ( cell.fRight - cell.fLeft > 0.001f )
			cells.push_back( cell );
	}
	Check( nBadChars == 0, szWhat + ": every character's widths and texture rect are consistent (" + std::to_string( nBadChars ) + " are not)" );

	int nOverlaps = 0;
	for ( size_t i = 0; i < cells.size(); ++i )
		for ( size_t j = i + 1; j < cells.size(); ++j )
			if ( cells[i].fLeft < cells[j].fRight - 0.001f && cells[j].fLeft < cells[i].fRight - 0.001f &&
			     cells[i].fTop < cells[j].fBottom - 0.001f && cells[j].fTop < cells[i].fBottom - 0.001f )
			{
				if ( nOverlaps < 5 )
					printf( "bk-font-test: %s: U+%04X and U+%04X overlap\n", szWhat.c_str(), cells[i].wChar, cells[j].wChar );
				++nOverlaps;
			}
	Check( nOverlaps == 0, szWhat + ": no two glyph cells overlap (" + std::to_string( nOverlaps ) + " pairs do)" );

	if ( pPixels == 0 )
		return;
	Check( int( pPixels->size() ) == nSizeX * nSizeY, szWhat + ": the image is the described size" );
	// No ink outside the cells, and ink in the cell of every visible ASCII
	// character (a font that renders nothing would pass the rest).
	std::vector<BYTE> covered( size_t( nSizeX ) * nSizeY, 0 );
	for ( size_t i = 0; i < cells.size(); ++i )
		for ( int y = int( cells[i].fTop + 0.5f ); y < int( cells[i].fBottom + 0.5f ); ++y )
			for ( int x = int( cells[i].fLeft + 0.5f ); x < int( cells[i].fRight + 0.5f ); ++x )
				covered[size_t( y ) * nSizeX + x] = 1;
	int nStray = 0;
	for ( size_t i = 0; i < pPixels->size(); ++i )
		if ( (*pPixels)[i] != 0 && !covered[i] )
			++nStray;
	Check( nStray == 0, szWhat + ": no ink outside the glyph cells (" + std::to_string( nStray ) + " pixels)" );
	// The texel row under a cell and the column right of it stay empty: the
	// GPU path samples linearly with the half-texel UVs and no half-pixel
	// vertex shift, so a glyph's last screen row and column read half of
	// those texels. Cells stacked without a gap drew the top of the glyph
	// below as a line under the text (the D-14 in-game capture).
	int nBleeding = 0;
	for ( size_t i = 0; i < cells.size(); ++i )
	{
		const int nLeft = int( cells[i].fLeft + 0.5f ), nTop = int( cells[i].fTop + 0.5f );
		const int nRight = int( cells[i].fRight + 0.5f ), nBottom = int( cells[i].fBottom + 0.5f );
		int nInk = 0;
		for ( int x = nLeft; nBottom < nSizeY && x <= nRight && x < nSizeX; ++x )
			nInk += (*pPixels)[size_t( nBottom ) * nSizeX + x] != 0;
		for ( int y = nTop; nRight < nSizeX && y < nBottom; ++y )
			nInk += (*pPixels)[size_t( y ) * nSizeX + nRight] != 0;
		if ( nInk > 0 && nBleeding < 5 )
			printf( "bk-font-test: %s: U+%04X has %d inked texels under or right of its cell\n", szWhat.c_str(), cells[i].wChar, nInk );
		nBleeding += nInk > 0;
	}
	Check( nBleeding == 0, szWhat + ": no ink in the texel row under or the column right of a cell (" + std::to_string( nBleeding ) + " cells have some)" );
	if ( pCodes != 0 )
		return;
	int nBlank = 0;
	for ( WORD c = 33; c < 127; ++c )
	{
		const SCell cell = CellOf( c, format.chars.find( c )->second, nSizeX, nSizeY );
		bool bInk = false;
		for ( int y = int( cell.fTop + 0.5f ); y < int( cell.fBottom + 0.5f ) && !bInk; ++y )
			for ( int x = int( cell.fLeft + 0.5f ); x < int( cell.fRight + 0.5f ) && !bInk; ++x )
				bInk = (*pPixels)[size_t( y ) * nSizeX + x] != 0;
		if ( !bInk )
			++nBlank;
	}
	Check( nBlank == 0, szWhat + ": every visible ASCII character has ink (" + std::to_string( nBlank ) + " are blank)" );
}

// The atlas's kerning against the face's own table, read by a separate copy
// of the rasteriser: the same pairs, the same whole-pixel amounts, both ways.
static void CheckKerning( const std::string &szWhat, const FontAtlas::SFontAtlas &atlas, const std::vector<BYTE> &font, int nHeight, int nCodePage )
{
	stbtt_fontinfo info;
	if ( !Check( stbtt_InitFont( &info, &font[0], stbtt_GetFontOffsetForIndex( &font[0], 0 ) ) != 0, szWhat + ": the oracle reads the font" ) )
		return;
	const float fScale = stbtt_ScaleForPixelHeight( &info, float( nHeight ) );
	std::vector<WORD> chars;
	chars.push_back( FontAtlas::ToUnicode( nCodePage, FontAtlas::DEFAULT_CHAR ) );
	for ( int c = FontAtlas::FIRST_CHAR; c <= FontAtlas::LAST_CHAR; ++c )
		chars.push_back( FontAtlas::ToUnicode( nCodePage, BYTE( c ) ) );
	int nSource = 0, nWrong = 0;
	for ( size_t i = 0; i < chars.size(); ++i )
		for ( size_t j = 0; j < chars.size(); ++j )
		{
			const int nKern = int( floorf( stbtt_GetCodepointKernAdvance( &info, chars[i], chars[j] ) * fScale + 0.5f ) );
			const SFontFormat::CKernMap::const_iterator pos = atlas.format.kerns.find( ( DWORD( chars[i] ) << 16 ) | chars[j] );
			if ( nKern != 0 )
				++nSource;
			if ( nKern != 0 ? pos == atlas.format.kerns.end() || pos->second != float( nKern ) : pos != atlas.format.kerns.end() )
				++nWrong;
		}
	Check( nWrong == 0 && int( atlas.format.kerns.size() ) == nSource,
		szWhat + ": kerning matches the face (" + std::to_string( nSource ) + " pairs in the face, " +
		std::to_string( atlas.format.kerns.size() ) + " in the atlas, " + std::to_string( nWrong ) + " differ)" );
	printf( "bk-font-test: %s: %d kerning pairs\n", szWhat.c_str(), nSource );
}

static bool BuildFont( const std::vector<BYTE> &font, int nHeight, int nCodePage, FontAtlas::SFontAtlas *pAtlas,
	int nWeight = 400, bool bItalic = false, bool bAntialias = true )
{
	FontAtlas::SFontRequest request;
	request.pFontData = &font[0];
	request.nFontDataSize = int( font.size() );
	request.nHeight = nHeight;
	request.nCodePage = nCodePage;
	request.nWeight = nWeight;
	request.bItalic = bItalic;
	request.bAntialias = bAntialias;
	std::string szError;
	return Check( FontAtlas::Build( request, pAtlas, &szError ), "Build " + std::to_string( nHeight ) + "px: " + szError );
}

static bool SameDesc( const SFontFormat::SCharDesc &a, const SFontFormat::SCharDesc &b )
{
	return a.x1 == b.x1 && a.y1 == b.y1 && a.x2 == b.x2 && a.y2 == b.y2 && a.fA == b.fA && a.fB == b.fB && a.fC == b.fC && a.fWidth == b.fWidth;
}

static bool SameFormat( const SFontFormat &a, const SFontFormat &b )
{
	const SFontFormat::SFontMetrics &ma = a.metrics, &mb = b.metrics;
	if ( a.szFaceName != b.szFaceName || ma.nHeight != mb.nHeight || ma.nAscent != mb.nAscent || ma.nDescent != mb.nDescent ||
	     ma.nInternalLeading != mb.nInternalLeading || ma.nExternalLeading != mb.nExternalLeading || ma.nAveCharWidth != mb.nAveCharWidth ||
	     ma.nMaxCharWidth != mb.nMaxCharWidth || ma.fSpaceWidth != mb.fSpaceWidth || ma.cCharSet != mb.cCharSet || ma.wDefaultChar != mb.wDefaultChar ||
	     a.chars.size() != b.chars.size() || a.kerns.size() != b.kerns.size() )
		return false;
	for ( SFontFormat::CCharacterMap::const_iterator it = a.chars.begin(); it != a.chars.end(); ++it )
	{
		SFontFormat::CCharacterMap::const_iterator pos = b.chars.find( it->first );
		if ( pos == b.chars.end() || !SameDesc( it->second, pos->second ) )
			return false;
	}
	for ( SFontFormat::CKernMap::const_iterator it = a.kerns.begin(); it != a.kerns.end(); ++it )
	{
		SFontFormat::CKernMap::const_iterator pos = b.kerns.find( it->first );
		if ( pos == b.kerns.end() || pos->second != it->second )
			return false;
	}
	return true;
}

// What the game does with a font directory (FontGpu::Load): chunk 1 of
// 1.tfd through the structure saver, 1.tga through the image processor.
static bool ReadFontDirectory( const fs::path &dir, SFontFormat *pFormat, int *pSizeX, int *pSizeY, std::vector<DWORD> *pPixels )
{
	const std::string szDir = dir.generic_string() + "/";
	CPtr<IDataStorage> pStorage = OpenStorage( szDir.c_str(), STREAM_ACCESS_READ, STORAGE_TYPE_FILE );
	if ( !Check( pStorage != 0, "the engine opens " + szDir ) )
		return false;
	{
		CPtr<IDataStream> pStream = pStorage->OpenStream( "1.tfd", STREAM_ACCESS_READ );
		if ( !Check( pStream != 0, "the engine opens 1.tfd in " + szDir ) )
			return false;
		CPtr<IStructureSaver> pSaver = CreateStructureSaver( pStream, IStructureSaver::READ );
		CSaverAccessor saver = pSaver;
		saver.Add( 1, pFormat );
	}
	if ( pPixels == 0 )
		return true;
	CPtr<IDataStream> pStream = pStorage->OpenStream( "1.tga", STREAM_ACCESS_READ );
	if ( !Check( pStream != 0, "the engine opens 1.tga in " + szDir ) )
		return false;
	CPtr<IImage> pImage = GetImageProcessor()->LoadImage( pStream );
	if ( !Check( pImage != 0, "the engine's TGA reader loads 1.tga" ) )
		return false;
	*pSizeX = pImage->GetSizeX();
	*pSizeY = pImage->GetSizeY();
	const DWORD *pLFB = reinterpret_cast<const DWORD*>( pImage->GetLFB() );
	pPixels->assign( pLFB, pLFB + size_t( *pSizeX ) * *pSizeY );
	return true;
}

// The CLI's output for the fixture, read back by the engine, is the atlas the
// library builds in this process for the same request.
static void TestEngineReadsWhatBkFontWrote( const std::vector<BYTE> &font, const fs::path &out )
{
	FontAtlas::SFontAtlas expected;
	if ( !BuildFont( font, 13, 1252, &expected ) )
		return;
	SFontFormat format;
	int nSizeX = 0, nSizeY = 0;
	std::vector<DWORD> pixels;
	if ( !ReadFontDirectory( out, &format, &nSizeX, &nSizeY, &pixels ) )
		return;
	Check( SameFormat( format, expected.format ), "1.tfd read back by the engine is the built SFontFormat" );
	Check( nSizeX == expected.image.nSizeX && nSizeY == expected.image.nSizeY && pixels == expected.image.pixels,
		"1.tga read back by the engine is the built atlas" );
	CheckStructure( "bk-font output", format, nSizeX, nSizeY, &pixels, 1252 );
	// The texture the game draws with: 1_h.dds through the engine's DDS
	// reader, the same pixels.
	{
		const std::string szDir = out.generic_string() + "/";
		CPtr<IDataStorage> pStorage = OpenStorage( szDir.c_str(), STREAM_ACCESS_READ, STORAGE_TYPE_FILE );
		CPtr<IDataStream> pStream = pStorage != 0 ? pStorage->OpenStream( "1_h.dds", STREAM_ACCESS_READ ) : 0;
		CPtr<IDDSImage> pDDS = pStream != 0 ? GetImageProcessor()->LoadDDSImage( pStream ) : 0;
		CPtr<IImage> pImage = pDDS != 0 ? GetImageProcessor()->Decompress( pDDS ) : 0;
		if ( Check( pImage != 0, "the engine's DDS reader loads 1_h.dds" ) )
		{
			const DWORD *pLFB = reinterpret_cast<const DWORD*>( pImage->GetLFB() );
			Check( pImage->GetSizeX() == nSizeX && pImage->GetSizeY() == nSizeY &&
				std::vector<DWORD>( pLFB, pLFB + size_t( nSizeX ) * nSizeY ) == expected.image.pixels, "1_h.dds holds the built atlas" );
		}
	}
	printf( "bk-font-test: bk-font output: face \"%s\", %d characters, %dx%d, height %d, ascent %d, space %g\n",
		format.szFaceName.c_str(), int( format.chars.size() ), nSizeX, nSizeY, format.metrics.nHeight, format.metrics.nAscent, format.metrics.fSpaceWidth );
}

static void TestFixtureStructure( const std::vector<BYTE> &font )
{
	// FontGen's default height and ELK's FONTS_SIZE, in the three code pages.
	const int heights[] = { 8, 13, 16, 20, 24, 48 };
	const int codePages[] = { 1250, 1251, 1252 };
	for ( size_t h = 0; h < sizeof( heights ) / sizeof( heights[0] ); ++h )
		for ( size_t p = 0; p < sizeof( codePages ) / sizeof( codePages[0] ); ++p )
		{
			FontAtlas::SFontAtlas atlas;
			if ( !BuildFont( font, heights[h], codePages[p], &atlas ) )
				continue;
			const std::string szWhat = "ProggyClean " + std::to_string( heights[h] ) + "px cp" + std::to_string( codePages[p] );
			CheckStructure( szWhat, atlas.format, atlas.image.nSizeX, atlas.image.nSizeY, &atlas.image.pixels, codePages[p] );
			CheckKerning( szWhat, atlas, font, heights[h], codePages[p] );
			// ProggyClean is monospaced: one advance for every character.
			int nOdd = 0;
			for ( SFontFormat::CCharacterMap::const_iterator it = atlas.format.chars.begin(); it != atlas.format.chars.end(); ++it )
				if ( it->second.fA + it->second.fB + it->second.fC != float( atlas.format.metrics.nAveCharWidth ) )
					++nOdd;
			Check( nOdd == 0, szWhat + ": a monospaced face advances every character by the average width" );
		}
}

static void TestDeterministic( const std::vector<BYTE> &font )
{
	FontAtlas::SFontAtlas first, second;
	if ( BuildFont( font, 20, 1252, &first ) && BuildFont( font, 20, 1252, &second ) )
		Check( SameFormat( first.format, second.format ) && first.image.pixels == second.image.pixels, "two builds of one request are identical" );
}

static void TestStyles( const std::vector<BYTE> &font )
{
	FontAtlas::SFontAtlas regular, bold, italic, aliased;
	if ( !BuildFont( font, 20, 1252, &regular ) || !BuildFont( font, 20, 1252, &bold, 700 ) ||
	     !BuildFont( font, 20, 1252, &italic, 400, true ) || !BuildFont( font, 20, 1252, &aliased, 400, false, false ) )
		return;
	CheckStructure( "bold", bold.format, bold.image.nSizeX, bold.image.nSizeY, &bold.image.pixels, 1252 );
	CheckStructure( "italic", italic.format, italic.image.nSizeX, italic.image.nSizeY, &italic.image.pixels, 1252 );
	CheckStructure( "no-aa", aliased.format, aliased.image.nSizeX, aliased.image.nSizeY, &aliased.image.pixels, 1252 );
	// Simulated bold: one pixel more ink and one more advance where there is
	// ink; simulated italic: the same advance, the ink slanted.
	int nBold = 0, nItalic = 0;
	for ( SFontFormat::CCharacterMap::const_iterator it = regular.format.chars.begin(); it != regular.format.chars.end(); ++it )
	{
		const SFontFormat::SCharDesc &r = it->second, &b = bold.format.chars[it->first], &i = italic.format.chars[it->first];
		const float fInk = r.fB > 0 ? 1.0f : 0.0f;
		if ( b.fB != r.fB + fInk || b.fA + b.fB + b.fC != r.fA + r.fB + r.fC + 1 )
			++nBold;
		if ( i.fA + i.fB + i.fC != r.fA + r.fB + r.fC )
			++nItalic;
	}
	Check( nBold == 0, "bold adds one pixel of ink and of advance (" + std::to_string( nBold ) + " characters differ)" );
	Check( nItalic == 0, "italic keeps every advance (" + std::to_string( nItalic ) + " characters differ)" );
	const SFontFormat::SCharDesc &l = italic.format.chars['l'], &lr = regular.format.chars['l'];
	Check( l.fB > lr.fB, "italic slants a tall glyph wider" );
	int nGrey = 0;
	for ( size_t i = 0; i < aliased.image.pixels.size(); ++i )
		if ( aliased.image.pixels[i] != 0 && aliased.image.pixels[i] != 0xFFFFFFFF )
			++nGrey;
	Check( nGrey == 0, "--no-aa leaves no grey pixel (" + std::to_string( nGrey ) + ")" );
}

static void TestRefusals( const std::vector<BYTE> &font )
{
	FontAtlas::SFontRequest request;
	FontAtlas::SFontAtlas atlas;
	std::string szError;
	const BYTE garbage[64] = { 'n', 'o', 't', ' ', 'a', ' ', 'f', 'o', 'n', 't' };
	request.pFontData = garbage;
	request.nFontDataSize = sizeof( garbage );
	Check( !FontAtlas::Build( request, &atlas, &szError ) && !szError.empty(), "a file that is not a font is refused" );
	request.pFontData = &font[0];
	request.nFontDataSize = int( font.size() );
	request.nCodePage = 437;
	szError.clear();
	Check( !FontAtlas::Build( request, &atlas, &szError ) && !szError.empty(), "an unsupported code page is refused" );
	request.nCodePage = 1252;
	request.nHeight = 3;
	szError.clear();
	Check( !FontAtlas::Build( request, &atlas, &szError ) && !szError.empty(), "a height below 4 is refused" );
	request.nHeight = 20;
	request.nFaceIndex = 1;
	szError.clear();
	Check( !FontAtlas::Build( request, &atlas, &szError ) && !szError.empty(), "a face index past a plain .ttf is refused" );
	Check( FontAtlas::FindFace( &font[0], int( font.size() ), "ProggyCleanTT", false, false ) == 0, "FindFace finds the fixture's family" );
	Check( FontAtlas::FindFace( &font[0], int( font.size() ), "No Such Face", false, false ) == -1, "FindFace misses another family" );
}

// The checker against the fonts FontGen made with GDI: if they fail, the
// rule is wrong, not the generator.
static void TestShippedFontsPassTheChecker()
{
	const char *sizes[] = { "tiny", "small", "medium", "large" };
	for ( size_t i = 0; i < sizeof( sizes ) / sizeof( sizes[0] ); ++i )
	{
		const fs::path dir = fs::path( "Data" ) / "Fonts" / sizes[i];
		const std::vector<BYTE> dds = ReadBytes( dir / "1_c.dds" );
		if ( !Check( dds.size() > 128 && memcmp( &dds[0], "DDS ", 4 ) == 0, "shipped " + dir.generic_string() + "/1_c.dds is a DDS" ) )
			continue;
		const int nSizeY = int( dds[12] | ( dds[13] << 8 ) | ( dds[14] << 16 ) | ( dds[15] << 24 ) );
		const int nSizeX = int( dds[16] | ( dds[17] << 8 ) | ( dds[18] << 16 ) | ( dds[19] << 24 ) );
		SFontFormat format;
		if ( !ReadFontDirectory( dir, &format, 0, 0, 0 ) )
			continue;
		const int nCodePage = format.metrics.cCharSet == 204 ? 1251 : format.metrics.cCharSet == 238 ? 1250 : 1252;
		printf( "bk-font-test: shipped %s: face \"%s\", %d characters, %dx%d, height %d, charset %d, default U+%04X, %d kerning pairs\n",
			sizes[i], format.szFaceName.c_str(), int( format.chars.size() ), nSizeX, nSizeY, format.metrics.nHeight,
			int( format.metrics.cCharSet ), format.metrics.wDefaultChar, int( format.kerns.size() ) );
		CheckStructure( std::string( "shipped " ) + sizes[i], format, nSizeX, nSizeY, 0, nCodePage );
	}
}


// ---- Unicode mode: the language-mod fonts (M002 D058) ----------------------

static std::vector<BYTE> VendorFont( const char *pszName )
{
	const std::vector<BYTE> data = ReadBytes( fs::path( "vendor" ) / "fonts" / pszName );
	Check( data.size() > 1024, std::string( "vendor/fonts/" ) + pszName + " reads" );
	return data;
}

static bool CellHasInk( const SFontFormat &format, WORD wChar, int nSizeX, int nSizeY, const std::vector<DWORD> &pixels )
{
	const SFontFormat::CCharacterMap::const_iterator it = format.chars.find( wChar );
	if ( it == format.chars.end() )
		return false;
	const SCell cell = CellOf( wChar, it->second, nSizeX, nSizeY );
	for ( int y = int( cell.fTop + 0.5f ); y < int( cell.fBottom + 0.5f ); ++y )
		for ( int x = int( cell.fLeft + 0.5f ); x < int( cell.fRight + 0.5f ); ++x )
			if ( ( pixels[size_t( y ) * nSizeX + x] >> 24 ) != 0 )
				return true;
	return false;
}

static bool ReadUnicodeFont( const fs::path &dir, SFontFormat *pFormat, int *pSizeX, int *pSizeY, std::vector<DWORD> *pPixels )
{
	return Check( fs::is_directory( dir ), "bk-font wrote " + dir.generic_string() ) && ReadFontDirectory( dir, pFormat, pSizeX, pSizeY, pPixels );
}

// --set cyrillic from NotoSans: every U+0410..U+044F is a character with ink.
static void TestCyrillic( const fs::path &dir )
{
	SFontFormat format;
	int nSizeX = 0, nSizeY = 0;
	std::vector<DWORD> pixels;
	if ( !ReadUnicodeFont( dir, &format, &nSizeX, &nSizeY, &pixels ) )
		return;
	std::vector<WORD> codes;
	for ( WORD c = 32; c < 127; ++c ) codes.push_back( c );
	for ( WORD c = 0x0410; c <= 0x044F; ++c ) codes.push_back( c );
	CheckStructure( "cyrillic", format, nSizeX, nSizeY, &pixels, 1251, &codes );
	int nBlank = 0;
	for ( WORD c = 0x0410; c <= 0x044F; ++c )
		nBlank += !CellHasInk( format, c, nSizeX, nSizeY, pixels );
	Check( nBlank == 0, "cyrillic: every U+0410..U+044F reads back with ink (" + std::to_string( nBlank ) + " blank)" );
	Check( format.metrics.wDefaultChar == '?', "cyrillic: the default character is '?'" );
	printf( "bk-font-test: cyrillic: %d characters, %dx%d\n", int( format.chars.size() ), nSizeX, nSizeY );
}

// --chars of 30 Han characters from NotoSansSC.
static void TestHan( const fs::path &dir )
{
	SFontFormat format;
	int nSizeX = 0, nSizeY = 0;
	std::vector<DWORD> pixels;
	if ( !ReadUnicodeFont( dir, &format, &nSizeX, &nSizeY, &pixels ) )
		return;
	const std::vector<BYTE> hanText = ReadBytes( "tools/zig/fixtures/fonts/han30.txt" );
	const std::u16string han = BkUtf8ToUtf16( std::string( hanText.begin(), hanText.end() ) );
	std::vector<WORD> codes;
	for ( WORD c = 32; c < 127; ++c ) codes.push_back( c );
	int nHan = 0, nBlank = 0;
	for ( size_t i = 0; i < han.size(); ++i )
		if ( han[i] >= 0x4E00 )
		{
			codes.push_back( han[i] );
			++nHan;
			nBlank += !CellHasInk( format, han[i], nSizeX, nSizeY, pixels );
		}
	Check( nHan == 30, "han: the fixture holds 30 Han characters (" + std::to_string( nHan ) + ")" );
	CheckStructure( "han", format, nSizeX, nSizeY, &pixels, 1252, &codes );
	Check( nBlank == 0, "han: every character reads back with ink (" + std::to_string( nBlank ) + " blank)" );
	Check( nSizeX <= 4096 && nSizeY <= 4096, "han: the atlas fits" );
	printf( "bk-font-test: han: %d characters, %dx%d\n", int( format.chars.size() ), nSizeX, nSizeY );
}

// The cluster glyph drawn again straight from stb at the shaper's glyph ids
// and positions, as the oracle for FontAtlas.
struct SOracle
{
	int x0, y0, nW, nH, nAdvance;
	std::vector<BYTE> coverage;
};

static bool OracleCluster( const std::vector<BYTE> &font, int nHeight, const std::u16string &cluster, SOracle *pOracle )
{
	stbtt_fontinfo info;
	if ( !stbtt_InitFont( &info, &font[0], stbtt_GetFontOffsetForIndex( &font[0], 0 ) ) )
		return false;
	const float fScale = stbtt_ScaleForPixelHeight( &info, float( nHeight ) );
	char szError[128] = "";
	SBkShaped *pShaped = BkShapeClusters( &font[0], font.size(), reinterpret_cast<const unsigned short*>( cluster.data() ), cluster.size(), "Deva", "hi", szError, sizeof( szError ) );
	if ( !Check( pShaped != 0, std::string( "the oracle shapes the cluster: " ) + szError ) )
		return false;
	struct SPart { int g, x0, y0, x1, y1; float sx, sy; };
	std::vector<SPart> parts;
	float fPen = 0;
	int nMinX = 0, nMinY = 0, nMaxX = 0, nMaxY = 0;
	for ( size_t i = 0; i < pShaped->nGlyphs; ++i )
	{
		const SBkShapedGlyph &g = pShaped->pGlyphs[i];
		const float fX = fPen + g.nXOffset * fScale, fY = -g.nYOffset * fScale;
		fPen += g.nXAdvance * fScale;
		SPart p;
		p.g = int( g.nGlyphId );
		p.sx = fX - floorf( fX );
		p.sy = fY - floorf( fY );
		stbtt_GetGlyphBitmapBoxSubpixel( &info, p.g, fScale, fScale, p.sx, p.sy, &p.x0, &p.y0, &p.x1, &p.y1 );
		p.x0 += int( floorf( fX ) ); p.x1 += int( floorf( fX ) );
		p.y0 += int( floorf( fY ) ); p.y1 += int( floorf( fY ) );
		if ( p.x1 <= p.x0 || p.y1 <= p.y0 )
			continue;
		if ( parts.empty() || p.x0 < nMinX ) nMinX = p.x0;
		if ( parts.empty() || p.y0 < nMinY ) nMinY = p.y0;
		if ( parts.empty() || p.x1 > nMaxX ) nMaxX = p.x1;
		if ( parts.empty() || p.y1 > nMaxY ) nMaxY = p.y1;
		parts.push_back( p );
	}
	BkShapeFree( pShaped );
	pOracle->x0 = nMinX; pOracle->y0 = nMinY;
	pOracle->nW = nMaxX - nMinX; pOracle->nH = nMaxY - nMinY;
	pOracle->nAdvance = int( floorf( fPen + 0.5f ) );
	pOracle->coverage.assign( size_t( pOracle->nW ) * pOracle->nH, 0 );
	for ( size_t i = 0; i < parts.size(); ++i )
	{
		const SPart &p = parts[i];
		std::vector<BYTE> one( size_t( p.x1 - p.x0 ) * ( p.y1 - p.y0 ) );
		stbtt_MakeGlyphBitmapSubpixel( &info, &one[0], p.x1 - p.x0, p.y1 - p.y0, p.x1 - p.x0, fScale, fScale, p.sx, p.sy, p.g );
		for ( int y = 0; y < p.y1 - p.y0; ++y )
			for ( int x = 0; x < p.x1 - p.x0; ++x )
			{
				BYTE &c = pOracle->coverage[size_t( p.y0 - nMinY + y ) * pOracle->nW + p.x0 - nMinX + x];
				if ( one[size_t( y ) * ( p.x1 - p.x0 ) + x] > c ) c = one[size_t( y ) * ( p.x1 - p.x0 ) + x];
			}
	}
	return true;
}

// The longest run of columns without ink (alpha < 128) between the first and
// the last inked column of the word's strongest row, the headline.
static int HeadlineGap( const std::vector<BYTE> &alpha, int nWidth, int nHeight, int *pRow, int *pInked )
{
	int nBest = 0;
	*pRow = 0;
	for ( int y = 0; y < nHeight; ++y )
	{
		int nCount = 0;
		for ( int x = 0; x < nWidth; ++x )
			nCount += alpha[size_t( y ) * nWidth + x] >= 128;
		if ( nCount > nBest ) { nBest = nCount; *pRow = y; }
	}
	*pInked = nBest;
	int nFirst = -1, nLast = -1;
	for ( int x = 0; x < nWidth; ++x )
		if ( alpha[size_t( *pRow ) * nWidth + x] >= 128 )
		{
			if ( nFirst < 0 ) nFirst = x;
			nLast = x;
		}
	int nGap = 0, nWidest = 0;
	for ( int x = nFirst; nFirst >= 0 && x <= nLast; ++x )
	{
		nGap = alpha[size_t( *pRow ) * nWidth + x] >= 128 ? 0 : nGap + 1;
		if ( nGap > nWidest ) nWidest = nGap;
	}
	return nWidest;
}

// --clusters with ki, k-ssa, sha-raa, kaa and kii from NotoSansDevanagari: PUA
// glyphs equal to stb's raster of the shaper's glyphs, no kerning for them,
// and a word set from the cells at their advances keeps one headline (D058's
// fallback trigger: a gap wider than a pixel means the plan must change).
static void TestHindi( const fs::path &dir, const std::vector<BYTE> &font, int nHeight )
{
	SFontFormat format;
	int nSizeX = 0, nSizeY = 0;
	std::vector<DWORD> pixels;
	if ( !ReadUnicodeFont( dir, &format, &nSizeX, &nSizeY, &pixels ) )
		return;
	CBkClusterTable table;
	std::string szError;
	const std::vector<BYTE> text = ReadBytes( "tools/zig/fixtures/fonts/hindi-clusters.txt" );
	if ( !Check( table.Load( std::string( text.begin(), text.end() ), &szError ), "hindi: the cluster fixture loads: " + szError ) )
		return;
	Check( table.Size() == 5, "hindi: the fixture holds five clusters" );
	int nWrongKerns = 0;
	for ( SFontFormat::CKernMap::const_iterator it = format.kerns.begin(); it != format.kerns.end(); ++it )
		nWrongKerns += ( it->first >> 16 ) >= CBkClusterTable::FIRST || ( it->first & 0xFFFF ) >= CBkClusterTable::FIRST;
	Check( nWrongKerns == 0, "hindi: no kerning pair involves a cluster glyph (" + std::to_string( nWrongKerns ) + ")" );

	std::vector<int> advances;
	std::vector<int> cellX, cellY, inkLeft, inkWidth;
	for ( size_t i = 0; i < table.Size(); ++i )
	{
		const WORD w = WORD( CBkClusterTable::FIRST + i );
		const SFontFormat::CCharacterMap::const_iterator it = format.chars.find( w );
		if ( !Check( it != format.chars.end(), "hindi: cluster U+" + std::to_string( w ) + " is a character" ) )
			return;
		const SFontFormat::SCharDesc &desc = it->second;
		const SCell cell = CellOf( w, desc, nSizeX, nSizeY );
		SOracle oracle;
		if ( !OracleCluster( font, nHeight, table.Entries()[i], &oracle ) )
			return;
		const std::string szWhat = "hindi cluster " + BkUtf16ToUtf8( table.Entries()[i] );
		Check( CellHasInk( format, w, nSizeX, nSizeY, pixels ), szWhat + " has ink" );
		Check( desc.fA == float( oracle.x0 ) && desc.fB == float( oracle.nW ) && desc.fA + desc.fB + desc.fC == float( oracle.nAdvance ),
			szWhat + ": A/B/C are the shaped run's (a=" + std::to_string( desc.fA ) + " b=" + std::to_string( desc.fB ) + " c=" + std::to_string( desc.fC ) + ")" );
		int nWrong = 0;
		const int nCellX = int( cell.fLeft + 0.5f ), nCellY = int( cell.fTop + 0.5f );
		for ( int y = 0; y < oracle.nH; ++y )
		{
			const int nRow = format.metrics.nAscent + oracle.y0 + y;
			if ( nRow < 0 || nRow >= nHeight )
				continue;
			for ( int x = 0; x < oracle.nW; ++x )
				if ( BYTE( pixels[size_t( nCellY + nRow ) * nSizeX + nCellX + x] >> 24 ) != oracle.coverage[size_t( y ) * oracle.nW + x] )
					++nWrong;
		}
		Check( nWrong == 0, szWhat + ": the cell's ink is stb's raster of the shaped glyphs (" + std::to_string( nWrong ) + " pixels differ)" );
		inkLeft.push_back( int( desc.fA ) );
		inkWidth.push_back( int( desc.fB ) );
		advances.push_back( int( desc.fA + desc.fB + desc.fC ) );
		cellX.push_back( nCellX );
		cellY.push_back( nCellY );
	}

	// A word from full-bar cells (ki, kaa, kii), side by side at their
	// advances: the headline is one stroke. Conjuncts like k-ssa and sha-raa
	// carry the bar over their stem only, so they are judged at the junction
	// where they hand over to a cell that starts with a bar.
	const int bars[] = { 0, 3, 4, 0, 3, 4 };
	const int nBars = int( sizeof( bars ) / sizeof( bars[0] ) );
	int nWordWidth = 0;
	for ( int i = 0; i < nBars; ++i )
		nWordWidth += advances[bars[i]];
	const int nStride = nWordWidth + 8;
	std::vector<BYTE> word( size_t( nStride ) * nHeight, 0 );
	int nPen = 0;
	for ( int k = 0; k < nBars; ++k )
	{
		const int i = bars[k];
		for ( int y = 0; y < nHeight; ++y )
			for ( int x = 0; x < inkWidth[i]; ++x )
			{
				const int nX = nPen + inkLeft[i] + x;
				if ( nX >= 0 && nX < nStride )
				{
					BYTE &c = word[size_t( y ) * nStride + nX];
					const BYTE a = BYTE( pixels[size_t( cellY[i] + y ) * nSizeX + cellX[i] + x] >> 24 );
					if ( a > c ) c = a;
				}
			}
		nPen += advances[i];
	}
	int nRow = 0, nInked = 0;
	const int nGap = HeadlineGap( word, nStride, nHeight, &nRow, &nInked );
	if ( getenv( "BK_FONT_TEST_DUMP" ) )
		for ( int y = 0; y < nHeight; ++y )
		{
			printf( "  y%02d ", y );
			for ( int x = 0; x < nStride; ++x )
			{
				const int c = word[size_t( y ) * nStride + x];
				putchar( c >= 192 ? '#' : c >= 128 ? '+' : c >= 32 ? '.' : ' ' );
			}
			putchar( '\n' );
		}
	printf( "bk-font-test: hindi: %d px word, headline row %d with %d inked columns, widest gap %d px\n", nWordWidth, nRow, nInked, nGap );
	Check( nInked >= nWordWidth * 3 / 4, "hindi: the word has a headline (" + std::to_string( nInked ) + " inked columns of " + std::to_string( nWordWidth ) + ")" );
	Check( nGap <= 1, "hindi: the headline of cells at their advances has no gap wider than 1 px (" + std::to_string( nGap ) + ") - D058 fallback trigger" );

	// k-ssa and sha-raa end in a stem with the bar on top; the cell after them
	// starts with a bar. The bar's last column of the one and first column of
	// the other, on the word's headline row, are at most one pixel apart.
	const int junctions[][2] = { { 1, 0 }, { 2, 3 }, { 1, 4 }, { 2, 0 } };
	for ( size_t j = 0; j < sizeof( junctions ) / sizeof( junctions[0] ); ++j )
	{
		const int l = junctions[j][0], r = junctions[j][1];
		int nLastLeft = -1, nFirstRight = -1;
		for ( int x = 0; x < inkWidth[l]; ++x )
			if ( ( pixels[size_t( cellY[l] + nRow ) * nSizeX + cellX[l] + x] >> 24 ) >= 128 ) nLastLeft = x;
		for ( int x = inkWidth[r] - 1; x >= 0; --x )
			if ( ( pixels[size_t( cellY[r] + nRow ) * nSizeX + cellX[r] + x] >> 24 ) >= 128 ) nFirstRight = x;
		const std::string szJunction = "hindi junction " + BkUtf16ToUtf8( table.Entries()[l] ) + "|" + BkUtf16ToUtf8( table.Entries()[r] );
		if ( !Check( nLastLeft >= 0 && nFirstRight >= 0, szJunction + ": both cells have headline ink on row " + std::to_string( nRow ) ) )
			continue;
		const int nJunctionGap = ( advances[l] + inkLeft[r] + nFirstRight ) - ( inkLeft[l] + nLastLeft + 1 );
		Check( nJunctionGap <= 1, szJunction + ": the bars are " + std::to_string( nJunctionGap ) + " px apart (at most 1)" );
	}
}

// The same headline measure at the sizes the game's windows use: a word of
// full-bar cells (U+E000 ki, U+E003 kaa, U+E004 kii of the fixture table)
// at the cells' integer advances, built in process.
static void TestHindiSizes( const std::vector<BYTE> &font )
{
	const std::vector<BYTE> text = ReadBytes( "tools/zig/fixtures/fonts/hindi-clusters.txt" );
	CBkClusterTable table;
	std::string szError;
	if ( !Check( table.Load( std::string( text.begin(), text.end() ), &szError ), "hindi sizes: the cluster fixture loads" ) )
		return;
	const int heights[] = { 12, 14, 16, 18, 20, 24, 28, 32, 40, 48 };
	for ( size_t h = 0; h < sizeof( heights ) / sizeof( heights[0] ); ++h )
	{
		FontAtlas::SFontRequest request;
		request.pFontData = &font[0];
		request.nFontDataSize = int( font.size() );
		request.nHeight = heights[h];
		request.codePoints.push_back( ' ' );
		request.clusters = table.Entries();
		FontAtlas::SFontAtlas atlas;
		if ( !Check( FontAtlas::Build( request, &atlas, &szError ), "hindi sizes: Build " + std::to_string( heights[h] ) + "px: " + szError ) )
			continue;
		const int bars[] = { 0, 3, 4, 0, 3, 4 };
		int nWidth = 8;
		for ( int i = 0; i < 6; ++i )
			nWidth += int( atlas.format.chars[WORD( 0xE000 + bars[i] )].fA + atlas.format.chars[WORD( 0xE000 + bars[i] )].fB + atlas.format.chars[WORD( 0xE000 + bars[i] )].fC );
		std::vector<BYTE> word( size_t( nWidth ) * heights[h], 0 );
		int nPen = 0;
		for ( int i = 0; i < 6; ++i )
		{
			const SFontFormat::SCharDesc &desc = atlas.format.chars[WORD( 0xE000 + bars[i] )];
			const SCell cell = CellOf( WORD( 0xE000 + bars[i] ), desc, atlas.image.nSizeX, atlas.image.nSizeY );
			for ( int y = 0; y < heights[h]; ++y )
				for ( int x = 0; x < int( desc.fB ); ++x )
				{
					const int nX = nPen + int( desc.fA ) + x;
					BYTE &c = word[size_t( y ) * nWidth + nX];
					const BYTE a = BYTE( atlas.image.pixels[size_t( int( cell.fTop + 0.5f ) + y ) * atlas.image.nSizeX + int( cell.fLeft + 0.5f ) + x] >> 24 );
					if ( a > c ) c = a;
				}
			nPen += int( desc.fA + desc.fB + desc.fC );
		}
		int nRow = 0, nInked = 0;
		const int nGap = HeadlineGap( word, nWidth, heights[h], &nRow, &nInked );
		printf( "bk-font-test: hindi %dpx: %d px word, headline row %d, widest gap %d px\n", heights[h], nPen, nRow, nGap );
		Check( nGap <= 1, "hindi " + std::to_string( heights[h] ) + "px: the headline has no gap wider than 1 px (" + std::to_string( nGap ) + ") - D058 fallback trigger" );
	}
}

// A set too big for 4096x4096 fails with the size and the glyph count.
static void TestTooLarge( const std::vector<BYTE> &font )
{
	FontAtlas::SFontRequest request;
	request.pFontData = &font[0];
	request.nFontDataSize = int( font.size() );
	request.nHeight = 160;
	for ( WORD c = 32; c < 0x3000; ++c )
		request.codePoints.push_back( c );
	FontAtlas::SFontAtlas atlas;
	std::string szError;
	Check( !FontAtlas::Build( request, &atlas, &szError ), "a set too large for the atlas is refused" );
	Check( szError.find( "4096x4096" ) != std::string::npos && szError.find( "glyphs" ) != std::string::npos,
		"the refusal names the size and the glyph count: " + szError );
	// Characters the face lacks are skipped and counted, not drawn as boxes.
	request.codePoints.clear();
	request.nHeight = 16;
	request.codePoints.push_back( 'A' );
	request.codePoints.push_back( 0x4E2D );
	szError.clear();
	if ( Check( FontAtlas::Build( request, &atlas, &szError ), "a small Unicode set builds: " + szError ) )
		Check( atlas.nSkipped == 1 && atlas.format.chars.size() == 1, "NotoSans has no Han glyph: skipped, not boxed" );
}

// --fallback-ttf: the Devanagari face has no Latin letter, so 'A' comes from Noto Sans, drawn with ink
// and counted; without the fallback it is skipped. KA stays the Devanagari face's own.
static void TestFallback( const std::vector<BYTE> &devanagari, const std::vector<BYTE> &latin )
{
	FontAtlas::SFontRequest request;
	request.pFontData = &devanagari[0];
	request.nFontDataSize = int( devanagari.size() );
	request.nHeight = 20;
	request.codePoints.push_back( 'A' );
	request.codePoints.push_back( 0x0915 );
	FontAtlas::SFontAtlas alone;
	std::string szError;
	if ( Check( FontAtlas::Build( request, &alone, &szError ), "the Devanagari face alone builds: " + szError ) )
		Check( alone.nSkipped == 1 && alone.format.chars.count( 'A' ) == 0, "without a fallback the Devanagari face skips 'A'" );
	request.pFallbackData = &latin[0];
	request.nFallbackDataSize = int( latin.size() );
	FontAtlas::SFontAtlas merged;
	if ( !Check( FontAtlas::Build( request, &merged, &szError ), "the Devanagari face with the Noto Sans fallback builds: " + szError ) )
		return;
	Check( merged.nSkipped == 0 && merged.nFallback == 1, "the fallback draws 'A' and only 'A'" );
	Check( CellHasInk( merged.format, 'A', merged.image.nSizeX, merged.image.nSizeY, merged.image.pixels ), "the fallback's 'A' has ink" );
	Check( CellHasInk( merged.format, 0x0915, merged.image.nSizeX, merged.image.nSizeY, merged.image.pixels ), "KA has ink" );
	const std::vector<BYTE> broken( 64, 0 );
	request.pFallbackData = &broken[0];
	request.nFallbackDataSize = int( broken.size() );
	Check( !FontAtlas::Build( request, &merged, &szError ) && szError.find( "fallback" ) != std::string::npos, "a fallback that is not a font is refused: " + szError );
}

int main( int argc, char **argv )
{
	if ( argc < 4 )
	{
		printf( "usage: %s <module root> <fixture font> <bk-font output dir> [more fonts...]\n", argv[0] );
		return 2;
	}
	if ( !NDataOnly::StartStreamIO( argv[1] ) || !NDataOnly::StartImage( argv[1] ) )
		return 1;
	const std::vector<BYTE> font = ReadBytes( argv[2] );
	if ( !Check( font.size() > 1024, std::string( "the fixture font reads: " ) + argv[2] ) )
		return 1;
	TestEngineReadsWhatBkFontWrote( font, argv[3] );
	TestFixtureStructure( font );
	TestDeterministic( font );
	TestStyles( font );
	TestRefusals( font );
	TestShippedFontsPassTheChecker();
	if ( const char *pszUnicode = getenv( "BK_FONT_TEST_UNICODE" ) )
	{
		const fs::path root = pszUnicode;
		TestCyrillic( root / "cyrillic" );
		TestHan( root / "han30" );
		TestHindi( root / "hindi", VendorFont( "NotoSansDevanagari-Regular.ttf" ), 24 );
		TestHindiSizes( VendorFont( "NotoSansDevanagari-Regular.ttf" ) );
		TestTooLarge( VendorFont( "NotoSans-Regular.ttf" ) );
		TestFallback( VendorFont( "NotoSansDevanagari-Regular.ttf" ), VendorFont( "NotoSans-Regular.ttf" ) );
	}
	else
		Check( false, "BK_FONT_TEST_UNICODE names the folder of the language-mod fonts" );
	for ( int i = 4; i < argc; ++i )
	{
		const std::vector<BYTE> extra = ReadBytes( argv[i] );
		FontAtlas::SFontAtlas atlas;
		if ( !Check( !extra.empty(), std::string( "reads " ) + argv[i] ) || !BuildFont( extra, 16, 1252, &atlas ) )
			continue;
		CheckStructure( argv[i], atlas.format, atlas.image.nSizeX, atlas.image.nSizeY, &atlas.image.pixels, 1252 );
		CheckKerning( argv[i], atlas, extra, 16, 1252 );
	}
	NDataOnly::Stop();
	printf( g_nFailures == 0 ? "bk-font-test: PASS\n" : "bk-font-test: FAIL (%d)\n", g_nFailures );
	return g_nFailures == 0 ? 0 : 1;
}
