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

}

#endif // __PLATFORM_LEGACYTEXT_H__
