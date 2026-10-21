// test-bk-text, C++ side: the bk_text.h contract through the BkText library. Every function is
// called the way S02/S03 will call it: shaping with a too-small buffer first and then exactly,
// measure against the shaped advances, wrap against measure, the atlas against the glyphs shaped,
// the missing-glyph callback, bad arguments, and a clean teardown (the run has BK_MEM_REPORT on, so
// a leaked block fails it).
//
// Argument: the vendor/fonts folder.
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "bk_text.h"

static int g_nFailures = 0;
#define CHECK( cond ) do { if ( !( cond ) ) { fprintf( stderr, "bk-text-abi-test: FAILED %s (line %d)\n", #cond, __LINE__ ); ++g_nFailures; } } while ( 0 )
#define CHECK_OK( call ) do { BkTextResult r_ = ( call ); if ( r_ != BK_TEXT_OK ) { fprintf( stderr, "bk-text-abi-test: FAILED %s returned %d (line %d)\n", #call, (int)r_, __LINE__ ); ++g_nFailures; } } while ( 0 )

static std::vector<uint16_t> Utf16( const char *sz )
{
	std::vector<uint16_t> out;
	const unsigned char *p = (const unsigned char *)sz;
	while ( *p )
	{
		uint32_t cp;
		if ( *p < 0x80 ) { cp = *p++; }
		else if ( *p < 0xE0 ) { cp = ( ( p[0] & 0x1Fu ) << 6 ) | ( p[1] & 0x3Fu ); p += 2; }
		else if ( *p < 0xF0 ) { cp = ( ( p[0] & 0x0Fu ) << 12 ) | ( ( p[1] & 0x3Fu ) << 6 ) | ( p[2] & 0x3Fu ); p += 3; }
		else { cp = ( ( p[0] & 0x07u ) << 18 ) | ( ( p[1] & 0x3Fu ) << 12 ) | ( ( p[2] & 0x3Fu ) << 6 ) | ( p[3] & 0x3Fu ); p += 4; }
		if ( cp >= 0x10000 ) { cp -= 0x10000; out.push_back( (uint16_t)( 0xD800 + ( cp >> 10 ) ) ); out.push_back( (uint16_t)( 0xDC00 + ( cp & 0x3FF ) ) ); }
		else out.push_back( (uint16_t)cp );
	}
	return out;
}

struct SMissing { int count = 0; uint32_t codepoint = 0; uint32_t index = 0; std::string line; };

static void OnMissing( void *user, uint32_t codepoint, uint32_t index, const char *line )
{
	SMissing *m = (SMissing *)user;
	++m->count;
	m->codepoint = codepoint;
	m->index = index;
	m->line = line;
}

static void TestBadArguments( const char *szFonts )
{
	BkTextContext *ctx = nullptr;
	BkTextFont *font = nullptr;
	CHECK( bk_text_context_create( szFonts, "en", nullptr ) == BK_TEXT_INVALID_ARGUMENT );
	CHECK( bk_text_context_create( nullptr, "en", &ctx ) == BK_TEXT_INVALID_ARGUMENT );
	CHECK( bk_text_context_create( szFonts, nullptr, &ctx ) == BK_TEXT_INVALID_ARGUMENT );
	CHECK( ctx == nullptr );
	CHECK( bk_text_context_create( "no/such/fonts/folder", "en", &ctx ) == BK_TEXT_NOT_FOUND );
	CHECK( ctx == nullptr );
	bk_text_context_destroy( nullptr );
	bk_text_font_close( nullptr );

	CHECK( bk_text_context_set_missing_glyph_callback( nullptr, nullptr, nullptr ) == BK_TEXT_INVALID_ARGUMENT );
	CHECK( bk_text_font_open( nullptr, BK_TEXT_SIZE_MEDIUM, 20.0f, &font ) == BK_TEXT_INVALID_ARGUMENT );
	CHECK_OK( bk_text_context_create( szFonts, "en", &ctx ) );
	CHECK( bk_text_font_open( ctx, 4, 20.0f, &font ) == BK_TEXT_INVALID_ARGUMENT );
	CHECK( bk_text_font_open( ctx, BK_TEXT_SIZE_MEDIUM, 0.0f, &font ) == BK_TEXT_INVALID_ARGUMENT );
	CHECK( bk_text_font_open( ctx, BK_TEXT_SIZE_MEDIUM, 20.0f, nullptr ) == BK_TEXT_INVALID_ARGUMENT );
	CHECK( font == nullptr );
	CHECK_OK( bk_text_font_open( ctx, BK_TEXT_SIZE_MEDIUM, 20.0f, &font ) );

	const uint16_t aText[] = { 'A', 'B' };
	uint32_t nCount = 0;
	BkTextGlyph aGlyphs[4];
	BkTextMetrics metrics;
	BkTextLine aLines[4];
	BkTextAtlasGlyph glyph;
	CHECK( bk_text_shape_utf16( nullptr, aText, 2, aGlyphs, 4, &nCount ) == BK_TEXT_INVALID_ARGUMENT );
	CHECK( bk_text_shape_utf16( font, nullptr, 2, aGlyphs, 4, &nCount ) == BK_TEXT_INVALID_ARGUMENT );
	CHECK( bk_text_shape_utf16( font, aText, 2, aGlyphs, 4, nullptr ) == BK_TEXT_INVALID_ARGUMENT );
	CHECK( bk_text_shape_utf16( font, aText, 2, nullptr, 4, &nCount ) == BK_TEXT_INVALID_ARGUMENT );
	CHECK( bk_text_shape_utf16( font, nullptr, 0, nullptr, 0, &nCount ) == BK_TEXT_OK && nCount == 0 );
	CHECK( bk_text_measure_utf16( nullptr, aText, 2, &metrics ) == BK_TEXT_INVALID_ARGUMENT );
	CHECK( bk_text_measure_utf16( font, aText, 2, nullptr ) == BK_TEXT_INVALID_ARGUMENT );
	CHECK( bk_text_wrap_utf16( nullptr, aText, 2, 100.0f, aLines, 4, &nCount ) == BK_TEXT_INVALID_ARGUMENT );
	CHECK( bk_text_wrap_utf16( font, aText, 2, 100.0f, aLines, 4, nullptr ) == BK_TEXT_INVALID_ARGUMENT );
	CHECK( bk_text_atlas_glyph( nullptr, 0, &glyph ) == BK_TEXT_INVALID_ARGUMENT );
	CHECK( bk_text_atlas_glyph( font, 0xFF000000u, &glyph ) == BK_TEXT_INVALID_ARGUMENT );
	CHECK( bk_text_atlas_info( nullptr, nullptr, nullptr, nullptr ) == BK_TEXT_INVALID_ARGUMENT );
	CHECK( bk_text_atlas_begin_frame( nullptr, 1 ) == BK_TEXT_INVALID_ARGUMENT );
	CHECK( bk_text_atlas_take_dirty( nullptr, nullptr, 0, &nCount ) == BK_TEXT_INVALID_ARGUMENT );
	CHECK( bk_text_atlas_pixels( nullptr, nullptr, nullptr ) == BK_TEXT_INVALID_ARGUMENT );
	CHECK( bk_text_stats( nullptr, nullptr ) == BK_TEXT_INVALID_ARGUMENT );

	bk_text_font_close( font );
	bk_text_context_destroy( ctx );
}

static bool Covered( const BkTextRect &r, const std::vector<BkTextRect> &dirty )
{
	for ( const BkTextRect &d : dirty )
		if ( r.x >= d.x && r.y >= d.y && r.x + r.w <= d.x + d.w && r.y + r.h <= d.y + d.h ) return true;
	return false;
}

// One language end to end on one line of its script.
static void TestLanguage( const char *szFonts, const char *szLang, const char *szLine )
{
	BkTextContext *ctx = nullptr;
	BkTextFont *font = nullptr;
	CHECK_OK( bk_text_context_create( szFonts, szLang, &ctx ) );
	CHECK_OK( bk_text_font_open( ctx, BK_TEXT_SIZE_MEDIUM, 20.0f, &font ) );
	if ( !ctx || !font ) return;

	const std::vector<uint16_t> text = Utf16( szLine );
	uint32_t nCount = 0;
	BkTextGlyph one;
	CHECK( bk_text_shape_utf16( font, text.data(), (uint32_t)text.size(), &one, 1, &nCount ) == BK_TEXT_BUFFER_TOO_SMALL );
	CHECK( nCount > 1 );
	std::vector<BkTextGlyph> glyphs( nCount );
	uint32_t nNeeded = nCount;
	CHECK_OK( bk_text_shape_utf16( font, text.data(), (uint32_t)text.size(), glyphs.data(), nNeeded, &nCount ) );
	CHECK( nCount == nNeeded );

	// measure equals the sum of the shaped advances.
	BkTextMetrics m;
	CHECK_OK( bk_text_measure_utf16( font, text.data(), (uint32_t)text.size(), &m ) );
	float fSum = 0.0f;
	for ( const BkTextGlyph &g : glyphs ) fSum += g.x_advance;
	CHECK( m.advance == fSum );
	CHECK( m.width == ceilf( fSum ) );
	CHECK( m.ascent > 0.0f && m.descent > 0.0f && m.line_space >= m.ascent + m.descent - 1.0f );

	// The atlas holds every shaped glyph in bounds and the dirty rects cover them.
	uint32_t w = 0, h = 0, gen = 0;
	CHECK_OK( bk_text_atlas_info( ctx, &w, &h, &gen ) );
	CHECK( w > 0 && h > 0 );
	CHECK_OK( bk_text_atlas_begin_frame( ctx, 1 ) );
	std::vector<BkTextAtlasGlyph> entries( glyphs.size() );
	int nInk = 0;
	for ( size_t i = 0; i < glyphs.size(); ++i )
	{
		CHECK_OK( bk_text_atlas_glyph( font, glyphs[i].glyph_key, &entries[i] ) );
		const BkTextAtlasGlyph &e = entries[i];
		CHECK( e.rect.x + e.rect.w <= w && e.rect.y + e.rect.h <= h );
		CHECK( e.generation == gen );
		if ( e.rect.w ) ++nInk;
	}
	CHECK( nInk > 0 );
	uint32_t nDirty = 0;
	CHECK( bk_text_atlas_take_dirty( ctx, nullptr, 0, &nDirty ) == BK_TEXT_BUFFER_TOO_SMALL );
	CHECK( nDirty > 0 );
	std::vector<BkTextRect> dirty( nDirty );
	uint32_t nNeededDirty = nDirty;
	CHECK_OK( bk_text_atlas_take_dirty( ctx, dirty.data(), nNeededDirty, &nDirty ) );
	CHECK( nDirty == nNeededDirty );
	for ( const BkTextAtlasGlyph &e : entries )
		if ( e.rect.w ) CHECK( Covered( e.rect, dirty ) );
	CHECK_OK( bk_text_atlas_take_dirty( ctx, nullptr, 0, &nDirty ) );
	CHECK( nDirty == 0 );
	const uint8_t *pixels = nullptr;
	uint32_t pitch = 0;
	CHECK_OK( bk_text_atlas_pixels( ctx, &pixels, &pitch ) );
	CHECK( pixels && pitch == w );
	bool bInk = false;
	for ( const BkTextAtlasGlyph &e : entries )
		for ( uint32_t y = 0; y < e.rect.h && !bInk; ++y )
			for ( uint32_t x = 0; x < e.rect.w; ++x )
				if ( pixels[(size_t)( e.rect.y + y ) * pitch + e.rect.x + x] ) { bInk = true; break; }
	CHECK( bInk );

	BkTextStats stats;
	CHECK_OK( bk_text_stats( ctx, &stats ) );
	CHECK( stats.atlas_glyph_count > 0 && stats.atlas_used_area > 0 && stats.atlas_eviction_count == 0 );
	CHECK( stats.shape_cache_misses >= 1 && stats.shape_cache_hits >= 1 && stats.shape_cache_entries == 1 );
	CHECK( stats.missing_glyph_count == 0 );

	bk_text_font_close( font );
	bk_text_context_destroy( ctx );
}

static void TestWrap( const char *szFonts )
{
	BkTextContext *ctx = nullptr;
	BkTextFont *font = nullptr;
	CHECK_OK( bk_text_context_create( szFonts, "en", &ctx ) );
	CHECK_OK( bk_text_font_open( ctx, BK_TEXT_SIZE_MEDIUM, 20.0f, &font ) );
	if ( !ctx || !font ) return;
	const std::vector<uint16_t> text = Utf16( "Panzer divisions advance across the river. Infantry holds the village." );
	uint32_t nCount = 0;
	BkTextLine one;
	CHECK( bk_text_wrap_utf16( font, text.data(), (uint32_t)text.size(), 100.0f, &one, 1, &nCount ) == BK_TEXT_BUFFER_TOO_SMALL );
	CHECK( nCount > 1 );
	std::vector<BkTextLine> lines( nCount );
	const uint32_t nNeeded = nCount;
	CHECK_OK( bk_text_wrap_utf16( font, text.data(), (uint32_t)text.size(), 100.0f, lines.data(), nNeeded, &nCount ) );
	CHECK( nCount == nNeeded );
	uint32_t nPrevEnd = 0;
	for ( const BkTextLine &l : lines )
	{
		CHECK( l.start >= nPrevEnd && l.end > l.start && l.end <= text.size() );
		nPrevEnd = l.end;
		BkTextMetrics m;
		CHECK_OK( bk_text_measure_utf16( font, text.data() + l.start, l.end - l.start, &m ) );
		CHECK( m.width <= 100.0f );
	}
	CHECK( nPrevEnd == text.size() );
	bk_text_font_close( font );
	bk_text_context_destroy( ctx );
}

static void TestMissingGlyph( const char *szFonts )
{
	BkTextContext *ctx = nullptr;
	BkTextFont *font = nullptr;
	CHECK_OK( bk_text_context_create( szFonts, "en", &ctx ) );
	SMissing missing;
	CHECK_OK( bk_text_context_set_missing_glyph_callback( ctx, OnMissing, &missing ) );
	CHECK_OK( bk_text_font_open( ctx, BK_TEXT_SIZE_MEDIUM, 20.0f, &font ) );
	if ( !ctx || !font ) return;
	// U+10FFFD, a noncharacter in the private use plane, as a surrogate pair after an A.
	const uint16_t aText[] = { 'A', 0xDBFF, 0xDFFD, 'B', 0xDBFF, 0xDFFD };
	uint32_t nCount = 0;
	BkTextGlyph aGlyphs[8];
	CHECK_OK( bk_text_shape_utf16( font, aText, 6, aGlyphs, 8, &nCount ) );
	CHECK_OK( bk_text_shape_utf16( font, aText, 3, aGlyphs, 8, &nCount ) );
	CHECK( missing.count == 1 );
	CHECK( missing.codepoint == 0x10FFFD && missing.index == 1 );
	CHECK( missing.line == "BK_GLYPH_TRACE: no glyph U+10FFFD in font \"bk_text/en/medium\"" );
	BkTextStats stats;
	CHECK_OK( bk_text_stats( ctx, &stats ) );
	CHECK( stats.missing_glyph_count == 1 );
	// Removing the callback is accepted.
	CHECK_OK( bk_text_context_set_missing_glyph_callback( ctx, nullptr, nullptr ) );
	bk_text_font_close( font );
	bk_text_context_destroy( ctx );
}

int main( int argc, char **argv )
{
	if ( argc < 2 )
	{
		fprintf( stderr, "usage: bk-text-abi-test <vendor/fonts>\n" );
		return 2;
	}
	const char *szFonts = argv[1];
	TestBadArguments( szFonts );
	TestLanguage( szFonts, "en", "Panzer IV" );
	TestLanguage( szFonts, "ja", "\xE6\x88\xA6\xE8\xBB\x8A" );
	TestLanguage( szFonts, "hi", "\xE0\xA4\x9F\xE0\xA5\x88\xE0\xA4\x82\xE0\xA4\x95" );
	TestWrap( szFonts );
	TestMissingGlyph( szFonts );
	if ( g_nFailures ) fprintf( stderr, "bk-text-abi-test: %d check(s) failed\n", g_nFailures );
	else printf( "bk-text-abi-test: ok\n" );
	return g_nFailures ? 1 : 0;
}
