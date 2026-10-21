#ifndef __FONT_ATLAS_H__
#define __FONT_ATLAS_H__
#pragma once
// The font generator FontGen.exe and ELK's CFontGen used to be: a TrueType face
// rendered into one glyph atlas plus the SFontFormat that describes it, with no
// HDC, no dialog and no globals, so bk-font and ELK's font page share it
// (Phase 8, D-12..D-14). Glyphs are rasterised with the vendored
// imstb_truetype.h; the layout is FontGen's, so the game draws the result the
// way it draws the shipped fonts.
//
// Include after the module's StdAfx.h: fmtFont.h needs the engine's basic
// types and IStructureSaver.
#include "../Formats/fmtFont.h"

namespace FontAtlas
{
// FontGen's character range: every byte 32..255 of the chosen code page.
const int FIRST_CHAR = 32;
const int LAST_CHAR = 255;
// The character drawn for anything the font lacks. FontGen took GDI's
// tmDefaultChar, which is 0x1F for TrueType faces: the face's .notdef box.
const BYTE DEFAULT_CHAR = 0x1F;

struct SFontRequest
{
	const BYTE *pFontData;								// a TrueType/OpenType file (or .ttc) in memory; read only during Build
	int nFontDataSize;
	int nFaceIndex;												// the face inside a collection; 0 for a plain .ttf
	std::string szFaceName;								// stored lower-cased; empty takes the face's family name
	int nHeight;													// cell height in pixels, ascent + descent (FontGen's -h, GDI's positive height)
	int nWeight;													// 100..900; 600 and more emboldens a face that is not bold itself
	bool bItalic;													// slants a face that is not italic itself
	bool bAntialias;											// false keeps every pixel fully on or off
	int nCodePage;												// 1250, 1251 or 1252: the charset byte, and the characters when codePoints is empty
	// Unicode mode (language mods): when not empty these BMP code points are
	// the font's characters instead of bytes 32..255 of the code page. A code
	// point the face has no glyph for is left out (counted in
	// SFontAtlas::nSkipped), so the game draws wDefaultChar for it.
	std::vector<WORD> codePoints;
	// Cluster glyphs: clusters[i] (UTF-16) is drawn as one glyph, shaped with
	// szScript/szLanguage, and stored under the Private Use Area character
	// U+E000 + i (CBkClusterTable's numbering). Only with codePoints.
	std::vector<std::u16string> clusters;
	std::string szScript;									// ISO 15924 tag for the shaper, "Deva" for Hindi
	std::string szLanguage;								// BCP 47 tag, "hi"
	// A second face for the code points the first has no glyph for (Unicode
	// mode only): scaled to the same cell height and drawn on the first face's
	// baseline. The Devanagari face holds no Latin letters, and Hindi texts name
	// units and maps in them. Null for none; read only during Build.
	const BYTE *pFallbackData;
	int nFallbackDataSize;
	SFontRequest() : pFontData( 0 ), nFontDataSize( 0 ), nFaceIndex( 0 ), nHeight( 20 ), nWeight( 400 ),
		bItalic( false ), bAntialias( true ), nCodePage( 1252 ), szScript( "Deva" ), szLanguage( "hi" ),
		pFallbackData( 0 ), nFallbackDataSize( 0 ) {  }
};

struct SFontImage
{
	int nSizeX, nSizeY;										// powers of two, as FontGen sized them
	std::vector<DWORD> pixels;						// nSizeX * nSizeY, top row first; white with alpha = coverage
	SFontImage() : nSizeX( 0 ), nSizeY( 0 ) {  }
};

struct SFontAtlas
{
	SFontFormat format;										// what the game reads from 1.tfd
	SFontImage image;											// what it reads from 1.tga (IImageProcessor::CreateImage's layout)
	int nSkipped;													// requested code points neither the face nor the fallback has a glyph for
	int nFallback;												// code points drawn from the fallback face
	int nClusters;												// cluster glyphs in the atlas
	SFontAtlas() : nSkipped( 0 ), nFallback( 0 ), nClusters( 0 ) {  }
};

// The GDI charset byte FontGen stored in SFontMetrics::cCharSet for a code
// page; false for a code page the library does not know.
bool GetCharSet( int nCodePage, BYTE *pCharSet );
// The Unicode character byte c of the code page stands for (undefined bytes map
// to U+0080..U+009F as Windows maps them); 0 for an unknown code page.
WORD ToUnicode( int nCodePage, BYTE c );
// The index of the face in pFontData whose family name is szName, preferring
// the bold/italic style asked for; -1 when no face has that name.
int FindFace( const BYTE *pFontData, int nFontDataSize, const std::string &szName, bool bBold, bool bItalic );
// Renders the request into pAtlas. False with the reason in *pszError when the
// font cannot be read, the request is out of range or the atlas would be
// larger than 4096x4096 (the message names the size and the glyph count).
bool Build( const SFontRequest &request, SFontAtlas *pAtlas, std::string *pszError );
// A font that keeps every character of base (its format and its atlas of
// nBaseSizeX x nBaseSizeY pixels) as it is, at the same atlas pixels with the
// same widths, metrics, default character and kerning, and adds the characters
// of added (built at base's height for the characters base lacks) below them,
// moved to base's baseline. A character both have stays base's. False with the
// reason when the heights differ or the atlas would be larger than 4096x4096.
bool AddToBase( const SFontFormat &base, int nBaseSizeX, int nBaseSizeY, const std::vector<DWORD> &basePixels,
	const SFontAtlas &added, SFontAtlas *pAtlas, std::string *pszError );
}

#endif // __FONT_ATLAS_H__
