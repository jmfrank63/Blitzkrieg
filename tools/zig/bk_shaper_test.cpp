// test-bk-shaper: the shaper C ABI and the PUA cluster table of the language
// mods (M002 D058) on the tracked Noto faces. It proves the shaping facts the
// rest of the slice builds on: Devanagari reordering and conjuncts come out as
// single clusters with the right glyphs, a text encodes deterministically into
// PUA characters (and back through the saved table), Latin and Cyrillic stay
// plain, and a full table is an error that names the cluster. It also checks
// vendor/fonts/SHA256SUMS against the tracked files, in this test's own SHA-256.
//
// Argument: the vendor/fonts folder.
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "../../Sources/src/FontAtlas/Shaper.h"

// The C ABI the language-mods step used to rewrite texts into PUA clusters with (M002 D058). The
// language mods ship plain Unicode since M004 S05; the cluster table stays what bk-font --clusters
// reads, so the shaper facts are still proved here.
struct SLmShaper
{
	std::string font;
	std::string script, language;
	CBkClusterTable table;
};

extern "C"
{

SLmShaper *LmShaperCreate( const unsigned char *pFont, size_t nFontSize, const char *szScript, const char *szLanguage )
{
	SLmShaper *pShaper = new SLmShaper;
	pShaper->font.assign( (const char *)pFont, nFontSize );
	pShaper->script = szScript;
	pShaper->language = szLanguage;
	return pShaper;
}

void LmShaperDestroy( SLmShaper *pShaper )
{
	delete pShaper;
}

// Rewrites pText[0, nLen) into pOut (room for nLen units) and returns the
// units written, or -1 with szError filled. The result is never longer than
// the text: a cluster becomes one character.
long LmShaperEncode( SLmShaper *pShaper, const unsigned short *pText, size_t nLen, unsigned short *pOut, char *szError, size_t nErrorSize )
{
	if ( nLen == 0 )
		return 0;
	SBkShaped *pShaped = BkShapeClusters( (const unsigned char *)pShaper->font.data(), pShaper->font.size(), pText, nLen,
		pShaper->script.c_str(), pShaper->language.c_str(), szError, nErrorSize );
	if ( pShaped == 0 )
		return -1;
	std::u16string result;
	std::string szWhy;
	const bool bOk = pShaper->table.Encode( pText, *pShaped, &result, &szWhy );
	BkShapeFree( pShaped );
	if ( !bOk )
	{
		if ( szError && nErrorSize > 0 )
		{
			strncpy( szError, szWhy.c_str(), nErrorSize - 1 );
			szError[nErrorSize - 1] = 0;
		}
		return -1;
	}
	memcpy( pOut, result.data(), result.size() * sizeof( unsigned short ) );
	return (long)result.size();
}

size_t LmShaperClusters( const SLmShaper *pShaper )
{
	return pShaper->table.Size();
}

// The table as bk-font --clusters reads it. Returns its size; copies it when
// it fits nCapacity.
size_t LmShaperSerialize( const SLmShaper *pShaper, char *pOut, size_t nCapacity )
{
	const std::string sz = pShaper->table.Serialize();
	if ( sz.size() <= nCapacity )
		memcpy( pOut, sz.data(), sz.size() );
	return sz.size();
}

}

static int g_nFailures = 0;

// What the test measured on the way is context for a failure, not news on a
// pass: it waits here and prints just before the next FAIL line, or at the
// end of a failing run.
static std::string g_szNotes;

static void Note( const char *pszFormat, ... )
{
	char szLine[4096];
	va_list args;
	va_start( args, pszFormat );
	vsnprintf( szLine, sizeof( szLine ), pszFormat, args );
	va_end( args );
	g_szNotes += szLine;
}

static void FlushNotes()
{
	fputs( g_szNotes.c_str(), stderr );
	g_szNotes.clear();
}
#define CHECK( cond ) do { if ( !( cond ) ) { FlushNotes(); fprintf( stderr, "bk-shaper-test: FAILED %s (line %d)\n", #cond, __LINE__ ); ++g_nFailures; } } while ( 0 )

static std::vector<unsigned char> ReadFile( const std::string &szPath )
{
	std::ifstream file( szPath.c_str(), std::ios::binary );
	return std::vector<unsigned char>( ( std::istreambuf_iterator<char>( file ) ), std::istreambuf_iterator<char>() );
}

// FIPS 180-4 SHA-256, enough to check the vendored files.
static std::string Sha256Hex( const std::vector<unsigned char> &data )
{
	static const uint32_t K[64] = {
		0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
		0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
		0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
		0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
		0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
		0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
		0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
		0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2 };
	uint32_t h[8] = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };
	std::vector<unsigned char> msg( data );
	msg.push_back( 0x80 );
	while ( msg.size() % 64 != 56 )
		msg.push_back( 0 );
	const uint64_t nBits = (uint64_t)data.size() * 8;
	for ( int i = 7; i >= 0; --i )
		msg.push_back( (unsigned char)( nBits >> ( i * 8 ) ) );
#define ROTR( x, n ) ( ( ( x ) >> ( n ) ) | ( ( x ) << ( 32 - ( n ) ) ) )
	for ( size_t nBlock = 0; nBlock < msg.size(); nBlock += 64 )
	{
		uint32_t w[64];
		for ( int i = 0; i < 16; ++i )
			w[i] = ( (uint32_t)msg[nBlock + i * 4] << 24 ) | ( (uint32_t)msg[nBlock + i * 4 + 1] << 16 ) | ( (uint32_t)msg[nBlock + i * 4 + 2] << 8 ) | msg[nBlock + i * 4 + 3];
		for ( int i = 16; i < 64; ++i )
		{
			const uint32_t s0 = ROTR( w[i - 15], 7 ) ^ ROTR( w[i - 15], 18 ) ^ ( w[i - 15] >> 3 );
			const uint32_t s1 = ROTR( w[i - 2], 17 ) ^ ROTR( w[i - 2], 19 ) ^ ( w[i - 2] >> 10 );
			w[i] = w[i - 16] + s0 + w[i - 7] + s1;
		}
		uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
		for ( int i = 0; i < 64; ++i )
		{
			const uint32_t t1 = hh + ( ROTR( e, 6 ) ^ ROTR( e, 11 ) ^ ROTR( e, 25 ) ) + ( ( e & f ) ^ ( ~e & g ) ) + K[i] + w[i];
			const uint32_t t2 = ( ROTR( a, 2 ) ^ ROTR( a, 13 ) ^ ROTR( a, 22 ) ) + ( ( a & b ) ^ ( a & c ) ^ ( b & c ) );
			hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
		}
		h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
	}
#undef ROTR
	std::string res;
	for ( int i = 0; i < 8; ++i )
	{
		char buf[16];
		snprintf( buf, sizeof( buf ), "%08x", h[i] );
		res += buf;
	}
	return res;
}

static void CheckChecksums( const std::string &szDir )
{
	const std::vector<unsigned char> sums = ReadFile( szDir + "/SHA256SUMS" );
	CHECK( !sums.empty() );
	const std::string szSums( sums.begin(), sums.end() );
	int nChecked = 0;
	for ( size_t nPos = 0; nPos < szSums.size(); )
	{
		size_t nEnd = szSums.find( '\n', nPos );
		if ( nEnd == std::string::npos )
			nEnd = szSums.size();
		std::string szLine = szSums.substr( nPos, nEnd - nPos );
		nPos = nEnd + 1;
		while ( !szLine.empty() && ( szLine[szLine.size() - 1] == '\r' || szLine[szLine.size() - 1] == ' ' ) )
			szLine.erase( szLine.size() - 1 );
		if ( szLine.size() < 67 )
			continue;
		const std::string szHash = szLine.substr( 0, 64 ), szName = szLine.substr( 66 );
		const std::vector<unsigned char> data = ReadFile( szDir + "/" + szName );
		const bool bOk = !data.empty() && Sha256Hex( data ) == szHash;
		if ( !bOk )
			fprintf( stderr, "bk-shaper-test: SHA256SUMS mismatch for %s\n", szName.c_str() );
		CHECK( bOk );
		++nChecked;
	}
	CHECK( nChecked == 7 );
	Note( "bk-shaper-test: SHA256SUMS lists %d files, all match\n", nChecked );
}

static SBkShaped *Shape( const std::vector<unsigned char> &font, const std::u16string &text, const char *szScript, const char *szLanguage )
{
	char szError[256];
	SBkShaped *pShaped = BkShapeClusters( &font[0], font.size(), (const unsigned short *)text.data(), text.size(), szScript, szLanguage, szError, sizeof( szError ) );
	if ( !pShaped )
		fprintf( stderr, "bk-shaper-test: shaping failed: %s\n", szError );
	CHECK( pShaped != 0 );
	return pShaped;
}

struct SShapedGuard
{
	SBkShaped *p;
	explicit SShapedGuard( SBkShaped *pShaped ) : p( pShaped ) {  }
	~SShapedGuard() { BkShapeFree( p ); }
};

static void CheckDevanagari( const std::vector<unsigned char> &font )
{
	// कि: the matra ि is drawn before the consonant but stays in its cluster.
	SShapedGuard ka( Shape( font, u"क", "Deva", "hi" ) );
	SShapedGuard ki( Shape( font, u"कि", "Deva", "hi" ) );
	if ( !ka.p || !ki.p )
		return;
	CHECK( ka.p->nGlyphs == 1 );
	CHECK( ki.p->nClusters == 1 );
	CHECK( ki.p->pClusters[0].nStart == 0 && ki.p->pClusters[0].nEnd == 2 );
	CHECK( ki.p->nGlyphs >= 2 );
	CHECK( ki.p->pGlyphs[0].nGlyphId != ka.p->pGlyphs[0].nGlyphId );
	CHECK( ki.p->pGlyphs[ki.p->nGlyphs - 1].nGlyphId == ka.p->pGlyphs[0].nGlyphId );
	// The matra is first: its pen position is left of the consonant's.
	CHECK( ki.p->pGlyphs[0].nXAdvance + ki.p->pGlyphs[0].nXOffset > 0 || ki.p->pGlyphs[ki.p->nGlyphs - 1].nXOffset != ki.p->pGlyphs[0].nXOffset );
	Note( "bk-shaper-test: ki: %zu cluster, %zu glyphs, first glyph %u, consonant glyph %u (units/em %u)\n", ki.p->nClusters, ki.p->nGlyphs,
		ki.p->pGlyphs[0].nGlyphId, ka.p->pGlyphs[0].nGlyphId, ki.p->nUnitsPerEm );

	const char16_t *conjuncts[4] = { u"क्ष", u"त्र", u"ज्ञ", u"श्र" };
	const char16_t *halants[4] = { u"क्‌ष", u"त्‌र", u"ज्‌ञ", u"श्‌र" };
	const char *names[4] = { "kssa", "tra", "jnya", "shra" };
	for ( int i = 0; i < 4; ++i )
	{
		SShapedGuard conjunct( Shape( font, conjuncts[i], "Deva", "hi" ) );
		SShapedGuard spelled( Shape( font, halants[i], "Deva", "hi" ) );
		if ( !conjunct.p || !spelled.p )
			continue;
		CHECK( conjunct.p->nClusters == 1 );
		CHECK( conjunct.p->nGlyphs < spelled.p->nGlyphs );
		Note( "bk-shaper-test: %s: %zu cluster of %zu glyphs, halant spelling %zu glyphs\n", names[i], conjunct.p->nClusters, conjunct.p->nGlyphs, spelled.p->nGlyphs );
	}
}

static std::string EncodeSentence( const std::vector<unsigned char> &font, const std::u16string &text, CBkClusterTable *pTable, std::u16string *pEncoded )
{
	SShapedGuard shaped( Shape( font, text, "Deva", "hi" ) );
	std::string szError;
	if ( !shaped.p )
		return "no shape";
	CHECK( pTable->Encode( (const unsigned short *)text.data(), *shaped.p, pEncoded, &szError ) );
	Note( "bk-shaper-test: %zu code units -> %zu clusters -> %zu table entries\n", text.size(), shaped.p->nClusters, pTable->Size() );
	return szError;
}

static void CheckTable( const std::vector<unsigned char> &font )
{
	// "Nayi khel shuru karen" and "Bahar niklen" (new game, start / exit), with conjuncts and matras.
	const std::u16string text = u"नया खेल शुरू करें क्षत्र विक्रम";
	CBkClusterTable first, second;
	std::u16string encodedFirst, encodedSecond;
	EncodeSentence( font, text, &first, &encodedFirst );
	EncodeSentence( font, text, &second, &encodedSecond );
	CHECK( first.Size() > 0 );
	CHECK( encodedFirst == encodedSecond );
	CHECK( first.Serialize() == second.Serialize() );
	CHECK( encodedFirst.size() < text.size() );
	const std::string szTable = first.Serialize();
	Note( "bk-shaper-test: cluster table\n%s", szTable.c_str() );
	for ( size_t i = 0; i < encodedFirst.size(); ++i )
		CHECK( encodedFirst[i] == u' ' || encodedFirst[i] >= 0x0900 );
	// Space is not shaped away: one cluster per word break.
	CHECK( encodedFirst.find( u' ' ) != std::u16string::npos );
	// Save, load, save: byte-identical, and every PUA character resolves back to its cluster.
	CBkClusterTable loaded;
	std::string szError;
	CHECK( loaded.Load( szTable, &szError ) );
	CHECK( loaded.Serialize() == szTable );
	CHECK( loaded.Size() == first.Size() );
	for ( size_t i = 0; i < encodedFirst.size(); ++i )
		if ( encodedFirst[i] >= CBkClusterTable::FIRST && encodedFirst[i] <= CBkClusterTable::LAST )
			CHECK( loaded.Lookup( encodedFirst[i] ) && first.Lookup( encodedFirst[i] ) && *loaded.Lookup( encodedFirst[i] ) == *first.Lookup( encodedFirst[i] ) );
	CHECK( szTable.find( "\r\n" ) != std::string::npos && szTable.find( "U+E000\t" ) == 0 );
	// A malformed table is refused.
	CBkClusterTable bad;
	CHECK( !bad.Load( "U+E001\tabc\r\n", &szError ) );
	CHECK( !szError.empty() );
}

static void CheckFull( const std::vector<unsigned char> &font )
{
	// Two slots, three different clusters: the third fails and names itself.
	const std::u16string text = u"कि खि गि";
	CBkClusterTable small( 2 );
	SShapedGuard shaped( Shape( font, text, "Deva", "hi" ) );
	if ( !shaped.p )
		return;
	std::u16string encoded;
	std::string szError;
	CHECK( !small.Encode( (const unsigned short *)text.data(), *shaped.p, &encoded, &szError ) );
	CHECK( szError.find( "PUA full" ) != std::string::npos );
	Note( "bk-shaper-test: full table error: %s\n", szError.c_str() );
	// The real limit: 6400 slots U+E000..U+F8FF, the next cluster is refused.
	CBkClusterTable big;
	bool bAllAdded = true;
	for ( unsigned i = 0; i < 6400; ++i )
	{
		std::u16string cluster;
		cluster += (char16_t)( 0x4E00 + i / 80 );
		cluster += (char16_t)( 0x4E00 + i % 80 );
		bAllAdded = bAllAdded && big.Add( cluster, &szError ) == CBkClusterTable::FIRST + i;
	}
	CHECK( bAllAdded );
	CHECK( big.Size() == 6400 );
	CHECK( big.Add( u"ककक", &szError ) == 0 );
	CHECK( szError.find( "PUA full" ) != std::string::npos );
	CHECK( big.Add( u"一一", &szError ) == CBkClusterTable::FIRST );
}

static void CheckPlain( const std::vector<unsigned char> &font )
{
	const std::u16string texts[2] = { u"Hello, World. Quit game?", u"Новая игра, Выход" };
	for ( int i = 0; i < 2; ++i )
	{
		SShapedGuard shaped( Shape( font, texts[i], 0, 0 ) );
		if ( !shaped.p )
			continue;
		CBkClusterTable table;
		std::u16string encoded;
		std::string szError;
		CHECK( shaped.p->nClusters == texts[i].size() );
		CHECK( shaped.p->nGlyphs == texts[i].size() );
		CHECK( table.Encode( (const unsigned short *)texts[i].data(), *shaped.p, &encoded, &szError ) );
		CHECK( table.Size() == 0 );
		CHECK( encoded == texts[i] );
		Note( "bk-shaper-test: plain text %d: %zu clusters, %zu PUA entries\n", i, shaped.p->nClusters, table.Size() );
	}
}

static void CheckErrors( const std::vector<unsigned char> &font )
{
	char szError[256];
	const char16_t text[2] = { u'a', u'b' };
	const unsigned char junk[64] = { 0 };
	CHECK( BkShapeClusters( junk, sizeof( junk ), (const unsigned short *)text, 2, 0, 0, szError, sizeof( szError ) ) == 0 );
	CHECK( strstr( szError, "face load failed" ) != 0 );
	CHECK( BkShapeClusters( &font[0], font.size(), (const unsigned short *)text, 0, 0, 0, szError, sizeof( szError ) ) == 0 );
	CHECK( strstr( szError, "no text" ) != 0 );
	Note( "bk-shaper-test: error text: %s\n", szError );
}

// Right-to-left text: HarfBuzz returns descending cluster values, so a range
// taken from the next glyph's cluster underflows. Script and language stay null
// so HarfBuzz guesses the direction. The faces may lack the glyphs (notdef),
// which still yields one cluster per code point.
static void CheckRightToLeft( const std::vector<unsigned char> &font )
{
	const char16_t *texts[3] = { u"\u05E9\u05DC\u05D5\u05DD \u05E2\u05D5\u05DC\u05DD", u"\u0645\u0631\u062D\u0628\u0627 \u0628\u0627\u0644\u0639\u0627\u0644\u0645", u"abc \u05D0\u05D1\u05D2 def" };
	for ( int t = 0; t < 3; ++t )
	{
		const std::u16string text = texts[t];
		SShapedGuard shaped( Shape( font, text, 0, 0 ) );
		if ( !shaped.p )
			continue;
		std::vector<int> covered( text.size(), 0 );
		bool bRanges = true;
		for ( size_t i = 0; i < shaped.p->nClusters; ++i )
		{
			const SBkShapedCluster &c = shaped.p->pClusters[i];
			Note( "bk-shaper-test: rtl %d cluster %zu: [%zu, %zu) glyphs %zu+%zu\n", t, i, c.nStart, c.nEnd, c.nGlyphStart, c.nGlyphCount );
			if ( !( c.nStart < c.nEnd && c.nEnd <= text.size() ) )
			{
				fprintf( stderr, "bk-shaper-test: rtl text %d cluster %zu has bad range [%zu, %zu) of %zu\n", t, i, c.nStart, c.nEnd, text.size() );
				bRanges = false;
				continue;
			}
			for ( size_t u = c.nStart; u < c.nEnd; ++u )
				++covered[u];
		}
		CHECK( bRanges );
		bool bTiled = bRanges;
		for ( size_t u = 0; u < covered.size(); ++u )
			bTiled = bTiled && covered[u] == 1;
		CHECK( bTiled );
		if ( !bRanges || !bTiled )
			continue;	// Encode would read outside the text
		CBkClusterTable table;
		std::u16string encoded;
		std::string szError;
		CHECK( table.Encode( (const unsigned short *)text.data(), *shaped.p, &encoded, &szError ) );
		// Every cluster decodes back to its own part of the text.
		size_t nUnits = 0;
		for ( size_t i = 0; i < shaped.p->nClusters; ++i )
			nUnits += shaped.p->pClusters[i].nEnd - shaped.p->pClusters[i].nStart;
		CHECK( nUnits == text.size() );
	}
	// A cluster that is empty or runs backwards is an error, not an underflow.
	SBkShapedCluster bad[2] = { { 3, 3, 0, 1 }, { 5, 2, 0, 1 } };
	const std::u16string text = u"abcdefgh";
	for ( int i = 0; i < 2; ++i )
	{
		SBkShaped fake = { &bad[i], 1, 0, 0, 1000, 0 };
		CBkClusterTable table;
		std::u16string encoded;
		std::string szError;
		CHECK( !table.Encode( (const unsigned short *)text.data(), fake, &encoded, &szError ) );
		CHECK( !szError.empty() );
		Note( "bk-shaper-test: bad cluster %d: %s\n", i, szError.c_str() );
	}
}

// LmShaperEncode with a full cluster table: the message goes to the caller's
// buffer only when there is one, truncated and terminated.
static void CheckEncodeErrorBuffer( const std::vector<unsigned char> &font )
{
	SLmShaper *pShaper = LmShaperCreate( &font[0], font.size(), "Deva", "hi" );
	for ( unsigned i = 0; i < 6400; ++i )
	{
		std::u16string cluster;
		cluster += (char16_t)( 0x4E00 + i / 80 );
		cluster += (char16_t)( 0x4E00 + i % 80 );
		CHECK( pShaper->table.Add( cluster, 0 ) != 0 );
	}
	const std::u16string text = u"\u0915\u093F";
	unsigned short out[8] = { 0 };

	// Size 0: the sentinel buffer stays untouched, with and without a pointer.
	char sentinel[16];
	memset( sentinel, 0x5A, sizeof( sentinel ) );
	CHECK( LmShaperEncode( pShaper, (const unsigned short *)text.data(), text.size(), out, sentinel, 0 ) == -1 );
	bool bUntouched = true;
	for ( size_t i = 0; i < sizeof( sentinel ); ++i )
		bUntouched = bUntouched && sentinel[i] == 0x5A;
	CHECK( bUntouched );
	CHECK( LmShaperEncode( pShaper, (const unsigned short *)text.data(), text.size(), out, 0, 0 ) == -1 );

	// A small buffer: truncated, terminated, nothing written past it.
	char small[24];
	memset( small, 0x5A, sizeof( small ) );
	CHECK( LmShaperEncode( pShaper, (const unsigned short *)text.data(), text.size(), out, small, 10 ) == -1 );
	CHECK( small[9] == 0 && strlen( small ) == 9 && strncmp( small, "shaper: P", 9 ) == 0 );
	bool bTail = true;
	for ( size_t i = 10; i < sizeof( small ); ++i )
		bTail = bTail && small[i] == 0x5A;
	CHECK( bTail );

	// A roomy buffer gets the whole message.
	char roomy[256];
	CHECK( LmShaperEncode( pShaper, (const unsigned short *)text.data(), text.size(), out, roomy, sizeof( roomy ) ) == -1 );
	CHECK( strstr( roomy, "PUA full" ) != 0 );
	Note( "bk-shaper-test: encode error with size 0 left the buffer alone, size 10 gave '%s', size 256 gave '%s'\n", small, roomy );
	LmShaperDestroy( pShaper );
}

int main( int argc, char **argv )
{
	if ( argc < 2 )
	{
		fprintf( stderr, "usage: bk-shaper-test <vendor/fonts>\n" );
		return 2;
	}
	const std::string szDir = argv[1];
	CheckChecksums( szDir );
	const std::vector<unsigned char> latin = ReadFile( szDir + "/NotoSans-Regular.ttf" );
	const std::vector<unsigned char> deva = ReadFile( szDir + "/NotoSansDevanagari-Regular.ttf" );
	CHECK( !latin.empty() && !deva.empty() );
	if ( latin.empty() || deva.empty() )
		return 1;
	CheckDevanagari( deva );
	CheckTable( deva );
	CheckFull( deva );
	CheckPlain( latin );
	CheckErrors( latin );
	CheckRightToLeft( latin );
	CheckEncodeErrorBuffer( deva );
	if ( g_nFailures )
	{
		FlushNotes();
		fprintf( stderr, "bk-shaper-test: %d checks failed\n", g_nFailures );
		return 1;
	}
	return 0;
}
