#ifndef __TEXTBREAK_H__
#define __TEXTBREAK_H__
#pragma once
// Line break opportunities for text without spaces. The shipped formatter only
// breaks at ASCII spaces, which leaves a Chinese or Japanese label as one
// unbreakable word. A break is also allowed between two characters when either
// one is Han, kana or CJK punctuation, except where kinsoku forbids it.
// Private-use code points (the Hindi cluster glyphs) are never break
// opportunities: a cluster word has to stay whole.
inline bool IsTextBreakHan( const wchar_t c )
{
	return ( c >= 0x3400 && c <= 0x4DBF ) || ( c >= 0x4E00 && c <= 0x9FFF ) || ( c >= 0xF900 && c <= 0xFAFF );
}
inline bool IsTextBreakKana( const wchar_t c ) { return c >= 0x3040 && c <= 0x30FF; }
inline bool IsTextBreakCjkPunct( const wchar_t c ) { return ( c >= 0x3000 && c <= 0x303F ) || ( c >= 0xFF00 && c <= 0xFFEF ); }
inline bool IsTextBreakCjk( const wchar_t c ) { return IsTextBreakHan( c ) || IsTextBreakKana( c ) || IsTextBreakCjkPunct( c ); }
// Characters that must not start a line: closing brackets, stops, the prolonged
// sound mark, iteration marks, the two-dot and three-dot leaders, the katakana double hyphen, the
// fullwidth wave dash and the small kana.
inline bool IsTextBreakNoStart( const wchar_t c )
{
	switch ( c )
	{
		case 0x3001: case 0x3002: case 0x3005: case 0x3009: case 0x300B: case 0x300D: case 0x300F: case 0x3011: case 0x3015:
		case 0x301C: case 0x30FB: case 0x30FC: case 0x30FD: case 0x30FE: case 0x309B: case 0x309C: case 0x309D: case 0x309E:
		case 0x2025: case 0x2026: case 0x30A0: case 0xFF5E:
			case 0xFF01: case 0xFF09: case 0xFF0C: case 0xFF0E: case 0xFF1A: case 0xFF1B: case 0xFF1F: case 0xFF3D: case 0xFF5D:
			return true;
		default:
			break;
	}
	// Small hiragana and katakana: the a/i/u/e/o row, tsu, ya/yu/yo, wa, ka, ke.
	if ( c >= 0x3041 && c <= 0x3049 ) return ( c & 1 ) != 0;
	if ( c == 0x3063 || c == 0x3083 || c == 0x3085 || c == 0x3087 || c == 0x308E ) return true;
	if ( c >= 0x30A1 && c <= 0x30A9 ) return ( c & 1 ) != 0;
	return c == 0x30C3 || c == 0x30E3 || c == 0x30E5 || c == 0x30E7 || c == 0x30EE || c == 0x30F5 || c == 0x30F6;
}
// Characters that must not end a line: opening brackets.
inline bool IsTextBreakNoEnd( const wchar_t c )
{
	switch ( c )
	{
		case 0x3008: case 0x300A: case 0x300C: case 0x300E: case 0x3010: case 0x3014:
		case 0xFF08: case 0xFF3B: case 0xFF5B:
			return true;
		default:
			return false;
	}
}
// True when the line may break between cPrev and cNext, with no space there.
inline bool CanBreakTextBetween( const wchar_t cPrev, const wchar_t cNext )
{
	if ( ( cPrev >= 0xE000 && cPrev <= 0xF8FF ) || ( cNext >= 0xE000 && cNext <= 0xF8FF ) )
		return false;
	if ( !IsTextBreakCjk( cPrev ) && !IsTextBreakCjk( cNext ) )
		return false;
	return !IsTextBreakNoStart( cNext ) && !IsTextBreakNoEnd( cPrev );
}
#endif // __TEXTBREAK_H__
