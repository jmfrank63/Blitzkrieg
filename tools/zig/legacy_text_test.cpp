// NPlatform's UTF-8 <-> UTF-16 conversion (Platform/LegacyText.h), which NStr::ToUnicode and
// NStr::ToAscii and the options bridge use on every platform: every 8-bit string is UTF-8, every wide
// string holds UTF-16 units. The language names are the ones whose blank Language setting found the
// old code page conversion (2026-10-22).
#include "Platform/LegacyText.h"
#include <cassert>
#include <string>

static std::u16string Units( const std::string &szUtf8 )
{
	std::u16string units;
	NPlatform::Utf16FromUtf8( &units, szUtf8 );
	return units;
}

static std::string Utf8( const std::u16string &units )
{
	std::string text;
	NPlatform::Utf8FromUtf16( &text, units );
	return text;
}

int main()
{
	assert( Units( "Default" ) == u"Default" );
	assert( Units( "Espa\xc3\xb1ol" ) == u"Español" );
	assert( Units( "Portugu\xc3\xaas (Brasil)" ) == u"Português (Brasil)" );
	assert( Units( "\xd0\xa0\xd1\x83\xd1\x81\xd1\x81\xd0\xba\xd0\xb8\xd0\xb9" ) == u"Русский" );
	assert( Units( "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e" ) == u"日本語" );
	assert( Units( "\xe0\xa4\xb9\xe0\xa4\xbf\xe0\xa4\xa8\xe0\xa5\x8d\xe0\xa4\xa6\xe0\xa5\x80" ) == u"हिन्दी" );
	// Above U+FFFF: a surrogate pair, and back to the same four bytes.
	assert( Units( "\xf0\x9f\x98\x80" ) == u"\U0001F600" );
	assert( Units( "\xf0\x9f\x98\x80" ).size() == 2 );
	assert( Utf8( u"\U0001F600" ) == "\xf0\x9f\x98\x80" );
	// Every name survives the round trip byte for byte.
	const char *names[] = { "Deutsch", "Espa\xc3\xb1ol", "Fran\xc3\xa7" "ais", "\xe7\xae\x80\xe4\xbd\x93\xe4\xb8\xad\xe6\x96\x87", "Bahasa Indonesia" };
	for ( const char *name : names )
		assert( Utf8( Units( name ) ) == name );
	// Malformed input becomes U+FFFD, never an empty or shortened string: a lone continuation byte,
	// a truncated sequence, an overlong encoding, an encoded surrogate and a cp1251 byte.
	assert( Units( "a\x80z" ) == u"a�z" );
	assert( Units( "a\xe6\x97" ) == u"a�" );
	assert( Units( "\xc0\xaf" ) == u"��" );
	assert( Units( "\xed\xa0\x80" ) == u"�" );
	assert( Units( "\xc0" ) == u"�" );
	assert( Units( "\xd0" ) == u"�" );
	// A lone surrogate in a wide string encodes as U+FFFD; a 32-bit wchar_t holding a whole code point
	// is taken as it is.
	assert( Utf8( std::u16string( 1, char16_t( 0xd800 ) ) ) == "\xef\xbf\xbd" );
	std::string fromWide;
	NPlatform::Utf8FromUtf16( &fromWide, std::wstring( 1, wchar_t( sizeof( wchar_t ) == 4 ? 0x1F600 : 0x263A ) ) );
	assert( fromWide == ( sizeof( wchar_t ) == 4 ? "\xf0\x9f\x98\x80" : "\xe2\x98\xba" ) );
	return 0;
}
