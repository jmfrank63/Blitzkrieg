// Line breaking for text without spaces. The font is fixed-width: every glyph
// advances 10, so a box of width 10 * n holds n characters and the expected
// lines can be read off the string.
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "../../Sources/src/GFXGPU/TextWrap.h"

struct SFixedFont
{
	float Advance( WORD, WORD ) const { return 10.0f; }
	float RunWidth( const WORD *, int count ) const { return 10.0f * count; }
};

static int nFailures = 0;

static std::string Utf8( const std::wstring &w )
{
	std::string s;
	for ( size_t i = 0; i < w.size(); ++i )
	{
		const unsigned c = w[i];
		if ( c < 0x80 ) s += char( c );
		else if ( c < 0x800 ) { s += char( 0xC0 | ( c >> 6 ) ); s += char( 0x80 | ( c & 0x3F ) ); }
		else { s += char( 0xE0 | ( c >> 12 ) ); s += char( 0x80 | ( ( c >> 6 ) & 0x3F ) ); s += char( 0x80 | ( c & 0x3F ) ); }
	}
	return s;
}

// The wrapped lines of text, in a box nChars glyphs wide, joined with '|'.
static std::wstring Wrap( const std::wstring &text, int nChars )
{
	std::vector<WORD> units( text.begin(), text.end() );
	units.push_back( 0 );
	SFixedFont font;
	std::vector<std::pair<size_t, size_t> > lines;
	WrapTextLinesWith( &font, &units[0], 1.0f, 10.0f * nChars, lines );
	std::wstring result;
	for ( size_t i = 0; i < lines.size(); ++i )
	{
		if ( i ) result += L'|';
		result += text.substr( lines[i].first, lines[i].second - lines[i].first );
	}
	return result;
}

static void Expect( const wchar_t *pszText, int nChars, const wchar_t *pszExpected, const char *pszWhat )
{
	const std::wstring got = Wrap( pszText, nChars );
	if ( got == pszExpected )
		return;
	std::printf( "FAIL: %s\n  expected %s\n  got      %s\n", pszWhat, Utf8( pszExpected ).c_str(), Utf8( got ).c_str() );
	++nFailures;
}

int main()
{
	// Han and kana break between characters.
	Expect( L"一二三四五六", 4, L"一二三四|五六", "Han wraps between characters" );
	Expect( L"あいうえおか", 3, L"あいう|えおか", "kana wrap like Han" );

	// Kinsoku: the fifth character would start the line, so the fourth goes down with it.
	Expect( L"一二三四。五", 4, L"一二三|四。五", "an ideographic full stop never starts a line" );
	Expect( L"一二三四、五", 4, L"一二三|四、五", "a comma never starts a line" );
	Expect( L"一二三四，五", 4, L"一二三|四，五", "a fullwidth comma never starts a line" );
	Expect( L"一二三四！五", 4, L"一二三|四！五", "a fullwidth exclamation mark never starts a line" );
	Expect( L"一二三四？五", 4, L"一二三|四？五", "a fullwidth question mark never starts a line" );
	Expect( L"一二三四）五", 4, L"一二三|四）五", "a closing parenthesis never starts a line" );
	Expect( L"一二三四」五", 4, L"一二三|四」五", "a closing corner bracket never starts a line" );
	Expect( L"一二三四』五", 4, L"一二三|四』五", "a closing double corner bracket never starts a line" );
	Expect( L"あいうーあ", 3, L"あい|うーあ", "the prolonged sound mark never starts a line" );
	Expect( L"あいうゃあ", 3, L"あい|うゃあ", "a small kana never starts a line" );
	Expect( L"アイウィア", 3, L"アイ|ウィア", "a small katakana never starts a line" );
	Expect( L"一二三四\u2025五", 4, L"一二三|四\u2025五", "a two-dot leader never starts a line" );
	Expect( L"一二三四\u2026五", 4, L"一二三|四\u2026五", "an ellipsis never starts a line" );
	Expect( L"一二三四\u30A0五", 4, L"一二三|四\u30A0五", "the katakana double hyphen never starts a line" );
	Expect( L"一二三四\uFF5E五", 4, L"一二三|四\uFF5E五", "the fullwidth wave dash never starts a line" );
	// The opening brackets never end a line.
	Expect( L"一二三（四五", 4, L"一二三|（四五", "an opening parenthesis never ends a line" );
	Expect( L"一二三「四五", 4, L"一二三|「四五", "an opening corner bracket never ends a line" );
	Expect( L"一二三『四五", 4, L"一二三|『四五", "an opening double corner bracket never ends a line" );

	// Mixed text breaks at the space and between Han.
	Expect( L"ab \u4E00\u4E8C\u4E09\u56DB", 5, L"ab \u4E00\u4E8C|\u4E09\u56DB", "mixed text fills the line, then breaks between Han" );
	Expect( L"ab \u4E00\u4E8C\u4E09\u56DB", 3, L"ab|\u4E00\u4E8C\u4E09|\u56DB", "a space break leaves no trailing space" );
	Expect( L"一二三 ab 四五", 6, L"一二三 ab|四五", "Han then Latin, then Han after a space" );
	Expect( L"ab一二三", 3, L"ab一|二三", "Latin runs into Han with a break at the script change" );

	// Latin and Cyrillic wrap as before: at the last space, or at the overflowing character.
	Expect( L"aaa bbb ccc", 7, L"aaa bbb|ccc", "Latin breaks at the last space" );
	Expect( L"aaaaaaaaaa", 4, L"aaaa|aaaa|aa", "a long Latin word breaks where it overflows" );
	Expect( L"привет мир", 7, L"привет|мир", "Cyrillic breaks at the space" );
	Expect( L"приветмир", 4, L"прив|етми|р", "a long Cyrillic word breaks where it overflows" );
	Expect( L"one\ntwo", 9, L"one|two", "an explicit newline still breaks" );

	// Private-use code points (Hindi clusters) are never break opportunities.
	Expect( L" ", 5, L"|", "a cluster word stays whole before the space" );
	for ( wchar_t cPua = 0xE000; cPua <= 0xF8FF; cPua += 0x7F )
		if ( CanBreakTextBetween( cPua, cPua + 1 ) || CanBreakTextBetween( 0x4E00, cPua ) || CanBreakTextBetween( cPua, 0x4E00 ) )
		{
			std::printf( "FAIL: U+%04X is a break opportunity\n", unsigned( cPua ) );
			++nFailures;
			break;
		}

	return nFailures == 0 ? 0 : 1;
}
