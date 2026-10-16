// test-bk-font: the D-14 structural acceptance of FontAtlas and the proof
// that the game's readers get back what bk-font wrote (1.tfd, 1.tga, 1_h.dds). Byte parity with the
// GDI-made Data/Fonts is not a goal (different rasteriser); instead every font
// here - the generated ones and, as a check of the checker, the shipped ones -
// must have every requested character, self-consistent metrics, texture rects
// inside the texture, no two glyph cells overlapping and no ink outside a
// cell, and the generated ones must carry exactly the source face's kerning.
//
// Arguments: <module root> <fixture font> <bk-font output dir> [more fonts...]
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
static void CheckStructure( const std::string &szWhat, const SFontFormat &format, int nSizeX, int nSizeY,
	const std::vector<DWORD> *pPixels, int nCodePage )
{
	const SFontFormat::SFontMetrics &m = format.metrics;
	// Every requested character, and the one drawn for the rest.
	int nMissing = 0;
	for ( int c = FontAtlas::FIRST_CHAR; c <= FontAtlas::LAST_CHAR; ++c )
		if ( format.chars.find( FontAtlas::ToUnicode( nCodePage, BYTE( c ) ) ) == format.chars.end() )
			++nMissing;
	Check( nMissing == 0, szWhat + ": every character 32..255 of the code page is present (" + std::to_string( nMissing ) + " missing)" );
	Check( format.chars.find( m.wDefaultChar ) != format.chars.end(), szWhat + ": the default character is present" );
	BYTE cCharSet = 0;
	Check( FontAtlas::GetCharSet( nCodePage, &cCharSet ) && cCharSet == m.cCharSet, szWhat + ": the charset byte names the code page" );

	// Metrics against each other and against the characters.
	Check( m.nHeight > 0 && m.nAscent >= 0 && m.nDescent >= 0 && m.nAscent + m.nDescent == m.nHeight, szWhat + ": ascent + descent == height" );
	Check( m.nInternalLeading >= 0 && m.nInternalLeading <= m.nHeight && m.nExternalLeading >= 0, szWhat + ": leadings inside the cell" );
	Check( m.nAveCharWidth > 0 && m.nAveCharWidth <= m.nMaxCharWidth, szWhat + ": 0 < average width <= maximum width" );
	const std::unordered_map<WORD, SFontFormat::SCharDesc>::const_iterator space = format.chars.find( FontAtlas::ToUnicode( nCodePage, ' ' ) );
	Check( space != format.chars.end() && Near( m.fSpaceWidth, space->second.fWidth ), szWhat + ": the space width is the space's width" );

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
