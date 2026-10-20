#ifndef __BK_SHAPER_H__
#define __BK_SHAPER_H__
#pragma once
// The text shaper of the language mods (M002 D058): HarfBuzz on a font file in
// memory, reporting clusters, and the table that gives every cluster the game
// cannot draw glyph by glyph a Private Use Area code point. The game draws one
// glyph per character, so a Hindi text is stored as a string of PUA characters
// and the atlas holds one glyph (the whole shaped cluster) for each of them.
//
// Standalone: no engine headers, so ELK, bk-font and the tests share it.
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

// One glyph of a shaped run, in font units (BkShaped::nUnitsPerEm per em).
struct SBkShapedGlyph
{
	uint32_t nGlyphId;
	int32_t nXAdvance, nYAdvance;
	int32_t nXOffset, nYOffset;
};

// The glyphs [nGlyphStart, nGlyphStart + nGlyphCount) draw the UTF-16 code
// units [nStart, nEnd) of the text, with nStart < nEnd <= the text length.
// Clusters come in visual (left to right) order, which for right to left text
// means descending ranges; a reordered matra stays inside the cluster of its
// consonant.
struct SBkShapedCluster
{
	size_t nStart, nEnd;
	size_t nGlyphStart, nGlyphCount;
};

struct SBkShaped
{
	const SBkShapedCluster *pClusters;
	size_t nClusters;
	const SBkShapedGlyph *pGlyphs;
	size_t nGlyphs;
	uint32_t nUnitsPerEm;
	void *pImpl;								// owned by BkShapeFree
};

// Shapes pUtf16[0, nLen) with the face of the font file pFont (.ttf/.otf, read
// only during the call; the result does not point into it). szScript is an
// ISO 15924 tag such as "Deva" and szLanguage a BCP 47 tag such as "hi"; null
// lets HarfBuzz guess either one. Returns null and fills szError (nErrorSize
// bytes, may be null) when the face cannot be loaded or the text is empty.
SBkShaped *BkShapeClusters( const unsigned char *pFont, size_t nFontSize, const unsigned short *pUtf16, size_t nLen,
	const char *szScript, const char *szLanguage, char *szError, size_t nErrorSize );
void BkShapeFree( SBkShaped *pShaped );

#ifdef __cplusplus
}

#include <string>
#include <map>
#include <vector>

// Maps cluster strings (UTF-16) to PUA characters U+E000.. in order of first
// appearance. Only clusters of more than one code point get a slot: a single
// code point stays itself (Encode), so Latin and Cyrillic text is unchanged.
class CBkClusterTable
{
public:
	static const unsigned short FIRST = 0xE000;
	static const unsigned short LAST = 0xF8FF;		// the last BMP PUA code point; 6400 slots

	// nMaxEntries is only lowered by tests, to reach the "table full" error.
	explicit CBkClusterTable( size_t nMaxEntries = LAST - FIRST + 1 ) : nMax( nMaxEntries ) {  }
	// The PUA character of the cluster, added when new. Returns 0 and sets
	// *pszError when the table is full (the message names the cluster).
	unsigned short Add( const std::u16string &cluster, std::string *pszError );
	// The cluster behind a PUA character; null when the table has none.
	const std::u16string *Lookup( unsigned short wch ) const;
	size_t Size() const { return entries.size(); }
	// pText with every multi-code-point cluster of pShaped replaced by its PUA
	// character; false (and *pszError) when the table is full or a cluster range
	// is empty. The upper bound (nEnd <= text length) is the shaper's guarantee.
	bool Encode( const unsigned short *pText, const SBkShaped &shaped, std::u16string *pResult, std::string *pszError );
	// "U+E000<TAB>cluster" lines, UTF-8, CRLF, in order of first appearance.
	std::string Serialize() const;
	// Replaces the table with Serialize()'s output; false (and *pszError) for a
	// malformed line, a slot out of order or a duplicate cluster.
	bool Load( const std::string &szText, std::string *pszError );
	const std::vector<std::u16string> &Entries() const { return entries; }

private:
	size_t nMax;
	std::vector<std::u16string> entries;							// entries[i] is U+E000 + i
	std::map<std::u16string, unsigned short> index;
};

std::string BkUtf16ToUtf8( const std::u16string &sz );
std::u16string BkUtf8ToUtf16( const std::string &sz );

#endif // __cplusplus
#endif // __BK_SHAPER_H__
