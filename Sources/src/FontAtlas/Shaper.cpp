#include "Shaper.h"

#include <hb.h>
#include <stdio.h>
#include <string.h>

#include <set>

namespace
{
struct SShapedImpl
{
	std::vector<SBkShapedCluster> clusters;
	std::vector<SBkShapedGlyph> glyphs;
};

void SetError( char *szError, size_t nErrorSize, const char *szFormat, size_t nA, size_t nB )
{
	if ( szError && nErrorSize > 0 )
		snprintf( szError, nErrorSize, szFormat, nA, nB );
}

std::string CodePointRange( const std::u16string &sz )
{
	char buf[64];
	snprintf( buf, sizeof( buf ), "U+%04X..", sz.empty() ? 0u : (unsigned)sz[0] );
	return buf;
}
}

SBkShaped *BkShapeClusters( const unsigned char *pFont, size_t nFontSize, const unsigned short *pUtf16, size_t nLen,
	const char *szScript, const char *szLanguage, char *szError, size_t nErrorSize )
{
	if ( nErrorSize > 0 && szError )
		szError[0] = 0;
	if ( !pFont || nFontSize == 0 || nFontSize > 0x7fffffff )
	{
		SetError( szError, nErrorSize, "shaper: face load failed (font data of %zu bytes, %zu)", nFontSize, 0 );
		return 0;
	}
	if ( !pUtf16 || nLen == 0 || nLen > 0x7fffffff )
	{
		SetError( szError, nErrorSize, "shaper: no text to shape (%zu code units, %zu)", nLen, 0 );
		return 0;
	}
	hb_blob_t *pBlob = hb_blob_create( (const char *)pFont, (unsigned)nFontSize, HB_MEMORY_MODE_READONLY, 0, 0 );
	hb_face_t *pFace = hb_face_create( pBlob, 0 );
	hb_blob_destroy( pBlob );
	const unsigned nUnitsPerEm = hb_face_get_upem( pFace );
	if ( hb_face_get_glyph_count( pFace ) == 0 )
	{
		hb_face_destroy( pFace );
		SetError( szError, nErrorSize, "shaper: face load failed (no glyphs in %zu bytes, %zu)", nFontSize, 0 );
		return 0;
	}
	hb_font_t *pHbFont = hb_font_create( pFace );
	hb_font_set_scale( pHbFont, nUnitsPerEm, nUnitsPerEm );
	hb_buffer_t *pBuffer = hb_buffer_create();
	hb_buffer_add_utf16( pBuffer, pUtf16, (int)nLen, 0, (int)nLen );
	hb_buffer_guess_segment_properties( pBuffer );
	if ( szScript && *szScript )
		hb_buffer_set_script( pBuffer, hb_script_from_string( szScript, -1 ) );
	if ( szLanguage && *szLanguage )
		hb_buffer_set_language( pBuffer, hb_language_from_string( szLanguage, -1 ) );
	hb_shape( pHbFont, pBuffer, 0, 0 );

	unsigned nGlyphs = 0;
	const hb_glyph_info_t *pInfo = hb_buffer_get_glyph_infos( pBuffer, &nGlyphs );
	const hb_glyph_position_t *pPos = hb_buffer_get_glyph_positions( pBuffer, &nGlyphs );
	SShapedImpl *pImpl = new SShapedImpl;
	for ( unsigned i = 0; i < nGlyphs; ++i )
	{
		SBkShapedGlyph glyph = { pInfo[i].codepoint, pPos[i].x_advance, pPos[i].y_advance, pPos[i].x_offset, pPos[i].y_offset };
		pImpl->glyphs.push_back( glyph );
		// Glyphs with the same cluster value form one cluster; HarfBuzz's default
		// (monotone graphemes) keeps them adjacent. The glyphs stay in visual
		// order, so for right to left text the cluster values descend.
		if ( i == 0 || pInfo[i].cluster != pInfo[i - 1].cluster )
		{
			SBkShapedCluster cluster = { pInfo[i].cluster, nLen, i, 0 };
			pImpl->clusters.push_back( cluster );
		}
		pImpl->clusters.back().nGlyphCount++;
	}
	// A cluster ends where the next larger cluster starts (the text's end for
	// the last one), whichever way the glyphs run.
	std::set<size_t> starts;
	for ( size_t i = 0; i < pImpl->clusters.size(); ++i )
		starts.insert( pImpl->clusters[i].nStart );
	for ( size_t i = 0; i < pImpl->clusters.size(); ++i )
	{
		const std::set<size_t>::const_iterator next = starts.upper_bound( pImpl->clusters[i].nStart );
		pImpl->clusters[i].nEnd = next == starts.end() ? nLen : *next;
	}
	hb_buffer_destroy( pBuffer );
	hb_font_destroy( pHbFont );
	hb_face_destroy( pFace );

	SBkShaped *pShaped = new SBkShaped;
	pShaped->pClusters = pImpl->clusters.empty() ? 0 : &pImpl->clusters[0];
	pShaped->nClusters = pImpl->clusters.size();
	pShaped->pGlyphs = pImpl->glyphs.empty() ? 0 : &pImpl->glyphs[0];
	pShaped->nGlyphs = pImpl->glyphs.size();
	pShaped->nUnitsPerEm = nUnitsPerEm;
	pShaped->pImpl = pImpl;
	return pShaped;
}

void BkShapeFree( SBkShaped *pShaped )
{
	if ( !pShaped )
		return;
	delete (SShapedImpl *)pShaped->pImpl;
	delete pShaped;
}

std::string BkUtf16ToUtf8( const std::u16string &sz )
{
	std::string res;
	for ( size_t i = 0; i < sz.size(); ++i )
	{
		unsigned c = sz[i];
		if ( c >= 0xD800 && c < 0xDC00 && i + 1 < sz.size() && sz[i + 1] >= 0xDC00 && sz[i + 1] < 0xE000 )
			c = 0x10000 + ( ( c - 0xD800 ) << 10 ) + ( sz[++i] - 0xDC00 );
		if ( c < 0x80 )
			res += (char)c;
		else if ( c < 0x800 )
		{
			res += (char)( 0xC0 | ( c >> 6 ) );
			res += (char)( 0x80 | ( c & 0x3F ) );
		}
		else if ( c < 0x10000 )
		{
			res += (char)( 0xE0 | ( c >> 12 ) );
			res += (char)( 0x80 | ( ( c >> 6 ) & 0x3F ) );
			res += (char)( 0x80 | ( c & 0x3F ) );
		}
		else
		{
			res += (char)( 0xF0 | ( c >> 18 ) );
			res += (char)( 0x80 | ( ( c >> 12 ) & 0x3F ) );
			res += (char)( 0x80 | ( ( c >> 6 ) & 0x3F ) );
			res += (char)( 0x80 | ( c & 0x3F ) );
		}
	}
	return res;
}

std::u16string BkUtf8ToUtf16( const std::string &sz )
{
	std::u16string res;
	for ( size_t i = 0; i < sz.size(); )
	{
		const unsigned char c = (unsigned char)sz[i];
		unsigned nExtra = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
		unsigned cp = nExtra == 3 ? c & 0x07 : nExtra == 2 ? c & 0x0F : nExtra == 1 ? c & 0x1F : c;
		++i;
		for ( ; nExtra > 0 && i < sz.size(); --nExtra, ++i )
			cp = ( cp << 6 ) | ( (unsigned char)sz[i] & 0x3F );
		if ( cp >= 0x10000 )
		{
			cp -= 0x10000;
			res += (char16_t)( 0xD800 + ( cp >> 10 ) );
			res += (char16_t)( 0xDC00 + ( cp & 0x3FF ) );
		}
		else
			res += (char16_t)cp;
	}
	return res;
}

unsigned short CBkClusterTable::Add( const std::u16string &cluster, std::string *pszError )
{
	std::map<std::u16string, unsigned short>::const_iterator it = index.find( cluster );
	if ( it != index.end() )
		return it->second;
	if ( entries.size() >= nMax || FIRST + entries.size() > LAST )
	{
		if ( pszError )
		{
			char buf[96];
			snprintf( buf, sizeof( buf ), "shaper: PUA full after %zu clusters, no slot for cluster at ", entries.size() );
			*pszError = buf + CodePointRange( cluster );
		}
		return 0;
	}
	const unsigned short wch = (unsigned short)( FIRST + entries.size() );
	entries.push_back( cluster );
	index[cluster] = wch;
	return wch;
}

const std::u16string *CBkClusterTable::Lookup( unsigned short wch ) const
{
	if ( wch < FIRST || wch - FIRST >= entries.size() )
		return 0;
	return &entries[wch - FIRST];
}

bool CBkClusterTable::Encode( const unsigned short *pText, const SBkShaped &shaped, std::u16string *pResult, std::string *pszError )
{
	pResult->clear();
	for ( size_t i = 0; i < shaped.nClusters; ++i )
	{
		const SBkShapedCluster &cluster = shaped.pClusters[i];
		// Encode has no text length: BkShapeClusters guarantees nStart < nEnd <= nLen,
		// so only the lower bound can be checked here.
		if ( cluster.nEnd <= cluster.nStart )
		{
			if ( pszError )
			{
				char buf[96];
				snprintf( buf, sizeof( buf ), "shaper: cluster %zu has the empty range [%zu, %zu)", i, cluster.nStart, cluster.nEnd );
				*pszError = buf;
			}
			return false;
		}
		const std::u16string src( (const char16_t *)pText + cluster.nStart, cluster.nEnd - cluster.nStart );
		const bool bSingle = src.size() == 1 || ( src.size() == 2 && src[0] >= 0xD800 && src[0] < 0xDC00 );
		if ( bSingle )
		{
			*pResult += src;
			continue;
		}
		const unsigned short wch = Add( src, pszError );
		if ( wch == 0 )
			return false;
		*pResult += (char16_t)wch;
	}
	return true;
}

std::string CBkClusterTable::Serialize() const
{
	std::string res;
	for ( size_t i = 0; i < entries.size(); ++i )
	{
		char buf[16];
		snprintf( buf, sizeof( buf ), "U+%04X\t", (unsigned)( FIRST + i ) );
		res += buf;
		res += BkUtf16ToUtf8( entries[i] );
		res += "\r\n";
	}
	return res;
}

bool CBkClusterTable::Load( const std::string &szText, std::string *pszError )
{
	entries.clear();
	index.clear();
	size_t nPos = 0, nLine = 0;
	while ( nPos < szText.size() )
	{
		size_t nEnd = szText.find( '\n', nPos );
		if ( nEnd == std::string::npos )
			nEnd = szText.size();
		std::string szLine = szText.substr( nPos, nEnd - nPos );
		nPos = nEnd + 1;
		++nLine;
		if ( !szLine.empty() && szLine[szLine.size() - 1] == '\r' )
			szLine.erase( szLine.size() - 1 );
		if ( szLine.empty() )
			continue;
		unsigned nSlot = 0;
		if ( szLine.size() < 8 || sscanf( szLine.c_str(), "U+%4X", &nSlot ) != 1 || szLine[6] != '\t' || nSlot != FIRST + entries.size() )
		{
			if ( pszError )
			{
				char buf[96];
				snprintf( buf, sizeof( buf ), "shaper: cluster table line %zu is malformed or out of order", nLine );
				*pszError = buf;
			}
			return false;
		}
		const std::u16string cluster = BkUtf8ToUtf16( szLine.substr( 7 ) );
		if ( index.count( cluster ) )
		{
			if ( pszError )
			{
				char buf[96];
				snprintf( buf, sizeof( buf ), "shaper: cluster table line %zu repeats a cluster", nLine );
				*pszError = buf;
			}
			return false;
		}
		index[cluster] = (unsigned short)( FIRST + entries.size() );
		entries.push_back( cluster );
	}
	return true;
}
