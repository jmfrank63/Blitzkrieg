#ifndef __PLATFORM_LEGACYTEXT_H__
#define __PLATFORM_LEGACYTEXT_H__

#include <string>

#include "LegacyTypes.h"

// The engine's module interfaces carry text as UTF-16 in `const WORD*`, which
// matches wchar_t only on Windows. Where wchar_t is 32 bits every UTF-16 unit
// pair would be read as a single character, so the boundary needs a real
// conversion rather than a reinterpret_cast. These helpers are that boundary.
namespace NPlatform
{

// Every 8-bit string of the engine is UTF-8 (XML, configs, paths, mod and profile names); every
// wide string holds UTF-16 code units, whatever the size of wchar_t, which is what the IText and UI
// interfaces take. The 2003 code converted through the Windows ANSI code page instead: on Linux and
// macOS that went through the C locale, which the Game never sets, so any non-ASCII byte gave an empty
// string (the blank "Español" of the Language setting); on Windows it gave mojibake. These two do the
// conversion themselves, the same on every platform. A malformed sequence becomes U+FFFD.
template <class TWideString>
inline void Utf8FromUtf16( std::string *pRes, const TWideString &szSrc )
{
	pRes->clear();
	pRes->reserve( szSrc.length() );
	for ( size_t i = 0; i < szSrc.length(); ++i )
	{
		// A 32-bit wchar_t may also hold a whole code point; it passes as it is.
		unsigned long nCode = static_cast<unsigned long>( szSrc[i] );
		if ( nCode >= 0xd800 && nCode < 0xdc00 && i + 1 < szSrc.length() )
		{
			const unsigned long nLow = static_cast<unsigned long>( szSrc[i + 1] ) & 0xffff;
			if ( nLow >= 0xdc00 && nLow < 0xe000 )
			{
				nCode = 0x10000 + ( ( nCode - 0xd800 ) << 10 ) + ( nLow - 0xdc00 );
				++i;
			}
		}
		if ( ( nCode >= 0xd800 && nCode < 0xe000 ) || nCode > 0x10ffff )
			nCode = 0xfffd;
		if ( nCode < 0x80 )
			*pRes += char( nCode );
		else if ( nCode < 0x800 )
		{
			*pRes += char( 0xc0 | ( nCode >> 6 ) );
			*pRes += char( 0x80 | ( nCode & 0x3f ) );
		}
		else if ( nCode < 0x10000 )
		{
			*pRes += char( 0xe0 | ( nCode >> 12 ) );
			*pRes += char( 0x80 | ( ( nCode >> 6 ) & 0x3f ) );
			*pRes += char( 0x80 | ( nCode & 0x3f ) );
		}
		else
		{
			*pRes += char( 0xf0 | ( nCode >> 18 ) );
			*pRes += char( 0x80 | ( ( nCode >> 12 ) & 0x3f ) );
			*pRes += char( 0x80 | ( ( nCode >> 6 ) & 0x3f ) );
			*pRes += char( 0x80 | ( nCode & 0x3f ) );
		}
	}
}

template <class TChar>
inline void Utf16FromUtf8( std::basic_string<TChar, std::char_traits<TChar>, std::allocator<TChar> > *pRes, const std::string &szSrc )
{
	pRes->clear();
	pRes->reserve( szSrc.length() );
	const unsigned char *p = reinterpret_cast<const unsigned char*>( szSrc.data() );
	const unsigned char *pEnd = p + szSrc.length();
	while ( p < pEnd )
	{
		unsigned long nCode = *p++;
		int nMore = 0;
		unsigned long nMin = 0;
		if ( nCode >= 0xf0 && nCode < 0xf5 ) { nCode &= 0x07; nMore = 3; nMin = 0x10000; }
		else if ( nCode >= 0xe0 && nCode < 0xf0 ) { nCode &= 0x0f; nMore = 2; nMin = 0x800; }
		else if ( nCode >= 0xc2 && nCode < 0xe0 ) { nCode &= 0x1f; nMore = 1; nMin = 0x80; }
		else if ( nCode >= 0x80 ) nCode = 0xfffd;
		for ( ; nMore > 0; --nMore )
		{
			if ( p >= pEnd || ( *p & 0xc0 ) != 0x80 ) { nCode = 0xfffd; nMin = 0; break; }
			nCode = ( nCode << 6 ) | ( *p++ & 0x3f );
		}
		if ( nCode < nMin || nCode > 0x10ffff || ( nCode >= 0xd800 && nCode < 0xe000 ) )
			nCode = 0xfffd;
		if ( nCode >= 0x10000 )
		{
			nCode -= 0x10000;
			pRes->push_back( static_cast<TChar>( 0xd800 + ( nCode >> 10 ) ) );
			pRes->push_back( static_cast<TChar>( 0xdc00 + ( nCode & 0x3ff ) ) );
		}
		else
			pRes->push_back( static_cast<TChar>( nCode ) );
	}
}


// UTF-16 (as the modules exchange it) to the host's wide string.
inline std::wstring WideFromWordString( const WORD *pszText )
{
	std::wstring text;
	if ( pszText == 0 ) return text;
	while ( *pszText != 0 )
	{
		text += static_cast<wchar_t>( *pszText );
		++pszText;
	}
	return text;
}

// Host wide string back to UTF-16 storage. Returning a u16string rather than a
// pointer is deliberate: the caller has to own the buffer, because there is no
// 16-bit copy of a 32-bit wstring to point at.
inline std::u16string WordStringFromWide( const std::wstring &text )
{
	std::u16string result;
	result.reserve( text.size() );
	for ( std::wstring::const_iterator it = text.begin(); it != text.end(); ++it )
		result += static_cast<char16_t>( *it );
	return result;
}

inline std::u16string WordStringFromWide( const wchar_t *text )
{
	std::u16string result;
	if ( text == 0 ) return result;
	while ( *text != 0 )
	{
		result += static_cast<char16_t>( *text );
		++text;
	}
	return result;
}

// u16string holds exactly the UTF-16 the interfaces expect, so this stays a
// plain 16-bit to 16-bit view with no reinterpretation of character width.
inline const WORD *WordStringData( const std::u16string &text )
{
	return reinterpret_cast<const WORD *>( text.c_str() );
}

// Fills a fixed wchar_t buffer from UTF-16. The status structs the engine passes
// between modules declare wchar_t arrays and were filled with a memcpy over a
// hardcoded two bytes per character, which is wchar_t's size only on Windows.
// Where wchar_t is 32 bits that packed two UTF-16 units into every element, and
// the readers -- which narrow each element back to 16 bits -- then showed every
// second character of the unit name in the status bar. The copy is also bounded,
// which the memcpy was not.
inline void CopyWordStringToWide( wchar_t *pDest, std::size_t nCapacity, const WORD *pszText )
{
	if ( pDest == 0 || nCapacity == 0 ) return;
	std::size_t i = 0;
	if ( pszText != 0 )
		for ( ; i + 1 < nCapacity && pszText[i] != 0; ++i )
			pDest[i] = static_cast<wchar_t>( pszText[i] );
	pDest[i] = 0;
}

inline void CopyWideToWide( wchar_t *pDest, std::size_t nCapacity, const wchar_t *pszText )
{
	if ( pDest == 0 || nCapacity == 0 ) return;
	std::size_t i = 0;
	if ( pszText != 0 )
		for ( ; i + 1 < nCapacity && pszText[i] != 0; ++i )
			pDest[i] = pszText[i];
	pDest[i] = 0;
}

}

#endif // __PLATFORM_LEGACYTEXT_H__
