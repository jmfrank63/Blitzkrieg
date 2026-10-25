// NTextPrecedence, the rule that picks a key's text when a gameplay mod and a language mod are both
// loaded (Sources/src/Main/TextPrecedence.h). Each layer is a map of key to text; each case names
// the source and the text Resolve must pick. The data imitates Achtung Panzer 2 under lang-ru: a
// tree it ships under the base game's key and English, a menu caption it only lower-cased, a word
// it reworded ("paused"), a caption it turned into another one ("Grand Campaign"), a caption it emptied, its own mission text, a bush under a key of its own
// whose English the base game has, and one of its own vehicles the base game has no English for.
//
// Each mismatch prints one line; the test prints nothing when it passes.
#include <cstdio>
#include <map>
#include <string>

#include "../../Sources/src/Main/TextPrecedence.h"

namespace {

int failures = 0;

std::string Narrow( const std::u16string &text )
{
	std::string out;
	for ( char16_t c : text ) out += c < 0x80 ? char( c ) : '?';
	return out;
}

typedef std::map<std::string, std::u16string> CLayer;

NTextPrecedence::FReader Reader( const CLayer &layer )
{
	return [&layer]( const std::string &key, std::u16string *text ) {
		const CLayer::const_iterator pos = layer.find( key );
		if ( pos == layer.end() ) return false;
		*text = pos->second;
		return true;
	};
}

void Expect( bool condition, const char *what )
{
	if ( condition ) return;
	std::printf( "text-precedence: %s\n", what );
	++failures;
}

}

int main()
{
	// Normalising: a byte order mark, white space runs, both ends and ASCII case.
	Expect( NTextPrecedence::Normalize( u"\xfeff  Birch\r\n Tree \r\n" ) == u"birch tree", "white space and the byte order mark normalise away" );
	Expect( NTextPrecedence::Normalize( u"MULTIPLAYER" ) == NTextPrecedence::Normalize( u"multiplayer" ), "ASCII case normalises away" );
	Expect( NTextPrecedence::Normalize( u"\x0411\x0415\x0420\x0401\x0417\x0410 \x00c4" ) == u"\x0431\x0435\x0440\x0451\x0437\x0430 \x00e4", "Cyrillic and Latin-1 case normalises away" );
	Expect( NTextPrecedence::Normalize( u"\x00d7" ) == u"\x00d7", "the multiplication sign is not a letter" );
	Expect( NTextPrecedence::Normalize( u"Exit Game" ) != NTextPrecedence::Normalize( u"Exit Blitzkrieg" ), "other words stay other words" );

	// Similarity: 2 * LCS / (|a| + |b|) on the normalised texts, against values measured on AP2's keys.
	const struct { const char16_t *a, *b; double expected; } similar[] = {
		{ u"paused", u"Pause", 10.0 / 11.0 },
		{ u"German Reich", u"Germany", 12.0 / 19.0 },
		{ u"Grand Campaign", u"SWITCH MOD", 4.0 / 24.0 },
		{ u"", u"", 1.0 },
		{ u"abc", u"", 0.0 },
	};
	for ( const auto &one : similar )
	{
		const double measured = NTextPrecedence::Similarity( one.a, one.b );
		if ( measured > one.expected + 1e-9 || measured < one.expected - 1e-9 )
		{
			std::printf( "text-precedence: similarity of \"%s\" and \"%s\" is %f, expected %f\n", Narrow( one.a ).c_str(), Narrow( one.b ).c_str(), measured, one.expected );
			++failures;
		}
	}
	Expect( NTextPrecedence::Similarity( u"German Reich", u"Germany" ) >= NTextPrecedence::kRewordedSimilarity, "German Reich is a rewording of Germany" );
	Expect( NTextPrecedence::Similarity( u"zzz BK DEMO MAP (DO NOT RUN)\r\nNot One Step Back", u"Not One Step Back" ) < NTextPrecedence::kRewordedSimilarity, "a demo map's warning is not a rewording" );

	const CLayer base = {
		{ "objects\\flora\\birch01\\name.txt", u"Birch Tree" },
		{ "objects\\flora\\bush01\\name.txt", u"Bush" },
		{ "objects\\flora\\bush02\\name.txt", u"Bush\r\n" },
		{ "textes\\mainmenu\\multiplayer.txt", u"MULTIPLAYER" },
		{ "textes\\credits.txt", u"Nival Interactive" },
		{ "textes\\exit.txt", u"EXIT BLITZKRIEG?" },
		{ "textes\\exitgame.txt", u"Exit Game?" },
		{ "textes\\units.txt", u"Units" },
		{ "textes\\units2.txt", u"UNITS" },
		{ "textes\\squad.txt", u"Squad" },
		{ "textes\\squad2.txt", u"Squad" },
		{ "textes\\notranslation.txt", u"Hut" },
		{ "textes\\pause.txt", u"Pause" },
		{ "textes\\switch.txt", u"SWITCH MOD" },
		{ "textes\\caption.txt", u"ESRB Notice:" },
	};
	const CLayer language = {
		{ "objects\\flora\\birch01\\name.txt", u"\x0411\x0435\x0440\x0451\x0437\x0430" },
		{ "objects\\flora\\bush01\\name.txt", u"\x041a\x0443\x0441\x0442" },
		{ "objects\\flora\\bush02\\name.txt", u"\x041a\x0443\x0441\x0442" },
		{ "textes\\mainmenu\\multiplayer.txt", u"\x0421\x0415\x0422\x042c" },
		{ "textes\\credits.txt", u"\x041d\x0438\x0432\x0430\x043b" },
		{ "textes\\exit.txt", u"\x0412\x042b\x0425\x041e\x0414?" },
		{ "textes\\exitgame.txt", u"\x0412\x044b\x0445\x043e\x0434?" },
		{ "textes\\units.txt", u"\x041e\x0442\x0440\x044f\x0434\x044b" },
		{ "textes\\units2.txt", u"\x041e\x0422\x0420\x042f\x0414\x042b" },
		{ "textes\\squad.txt", u"\x0413\x0440\x0443\x043f\x043f\x0430" },
		{ "textes\\squad2.txt", u"\x0412\x0437\x0432\x043e\x0434" },
		{ "textes\\languageonly.txt", u"\x042f" },
		{ "textes\\pause.txt", u"\x041f\x0430\x0443\x0437\x0430" },
		{ "textes\\switch.txt", u"\x0421\x041c\x0415\x041d\x0410" },
		{ "textes\\caption.txt", u"\x0412\x043d\x0438\x043c\x0430\x043d\x0438\x0435" },
	};
	const CLayer mod = {
		{ "objects\\flora\\birch01\\name.txt", u"Birch Tree" },
		{ "textes\\mainmenu\\multiplayer.txt", u"multiplayer" },
		{ "textes\\credits.txt", u"ACHTUNG PANZER 2 created by GordonCZ" },
		{ "textes\\exit.txt", u"EXIT GAME?" },
		{ "objects\\flora\\bush04e\\name.txt", u"  Bush " },
		{ "units\\technics\\g_vt_opel_blitz_43\\name.txt", u"Opel Blitz" },
		{ "textes\\mod_units.txt", u"units" },
		{ "textes\\mod_squad.txt", u"Squad" },
		{ "textes\\empty.txt", u"" },
		{ "textes\\pause.txt", u"paused" },
		{ "textes\\switch.txt", u"Grand Campaign" },
		{ "textes\\caption.txt", u"\r\n" },
		{ "scenarios\\wizna\\1.txt", u"Hold the bridgehead at Wizna" },
	};
	NTextPrecedence::CMemory memory;
	for ( const auto &entry : language )
	{
		const CLayer::const_iterator english = base.find( entry.first );
		if ( english != base.end() ) memory.Add( entry.first, english->second, entry.second );
	}

	const struct { const char *key; NTextPrecedence::ESource source; std::u16string text; const char *why; } cases[] = {
		{ "objects\\flora\\birch01\\name.txt", NTextPrecedence::SOURCE_LANGUAGE, u"\x0411\x0435\x0440\x0451\x0437\x0430", "the mod's unchanged English shows the translation of its key" },
		{ "textes\\mainmenu\\multiplayer.txt", NTextPrecedence::SOURCE_LANGUAGE, u"\x0421\x0415\x0422\x042c", "a caption the mod only lower-cased shows the translation" },
		{ "textes\\credits.txt", NTextPrecedence::SOURCE_MOD, u"ACHTUNG PANZER 2 created by GordonCZ", "the mod's rewritten text stays its own" },
		{ "textes\\pause.txt", NTextPrecedence::SOURCE_REWORDED, u"\x041f\x0430\x0443\x0437\x0430", "a word the mod lightly reworded shows the translation of the key" },
		{ "textes\\switch.txt", NTextPrecedence::SOURCE_MOD, u"Grand Campaign", "a caption the mod gave another meaning stays its own" },
		{ "textes\\caption.txt", NTextPrecedence::SOURCE_MOD, u"\r\n", "a text the mod emptied stays empty" },
		{ "scenarios\\wizna\\1.txt", NTextPrecedence::SOURCE_MOD, u"Hold the bridgehead at Wizna", "the mod's own text nothing translates stays its own" },
		{ "textes\\exit.txt", NTextPrecedence::SOURCE_MEMORY, u"\x0412\x044b\x0445\x043e\x0434?", "a rewritten text the base game has under another key shows that translation" },
		{ "objects\\flora\\bush04e\\name.txt", NTextPrecedence::SOURCE_MEMORY, u"\x041a\x0443\x0441\x0442", "a key only the mod has shows the translation of the same English" },
		{ "units\\technics\\g_vt_opel_blitz_43\\name.txt", NTextPrecedence::SOURCE_MOD, u"Opel Blitz", "English the base game does not have stays the mod's" },
		{ "textes\\mod_units.txt", NTextPrecedence::SOURCE_MEMORY, u"\x041e\x0442\x0440\x044f\x0434\x044b", "translations differing only in case count as one, the first key's spelling wins" },
		{ "textes\\mod_squad.txt", NTextPrecedence::SOURCE_MOD, u"Squad", "a tie between two different translations keeps the English" },
		{ "textes\\empty.txt", NTextPrecedence::SOURCE_MOD, u"", "an empty mod text stays empty" },
		{ "textes\\languageonly.txt", NTextPrecedence::SOURCE_STORAGE, u"", "a key the mod does not have is the storage's" },
		{ "objects\\flora\\bush01\\name.txt", NTextPrecedence::SOURCE_STORAGE, u"", "a base key the mod does not ship is the storage's" },
	};
	for ( const auto &one : cases )
	{
		std::u16string text;
		const NTextPrecedence::ESource source = NTextPrecedence::Resolve( one.key, Reader( base ), Reader( mod ), Reader( language ), memory, &text );
		if ( source == one.source && ( source == NTextPrecedence::SOURCE_STORAGE || text == one.text ) ) continue;
		std::printf( "text-precedence: %s: %s: source %d text \"%s\", expected source %d text \"%s\"\n", one.key, one.why, int( source ), Narrow( text ).c_str(), int( one.source ), Narrow( one.text ).c_str() );
		++failures;
	}

	// The same English twice with the same translation is one vote, not a tie.
	NTextPrecedence::CMemory twice;
	twice.Add( "a", u"Bush", u"\x041a\x0443\x0441\x0442" );
	twice.Add( "b", u"Bush", u"\x041a\x0443\x0441\x0442" );
	std::u16string found;
	Expect( twice.Find( u"bush", &found ) && found == u"\x041a\x0443\x0441\x0442", "the same translation under two keys is found" );
	Expect( !twice.Find( u"Birch", &found ), "English the memory lacks is not found" );
	// A majority wins over a single different translation.
	twice.Add( "c", u"Bush", u"\x041a\x0443\x0441\x0442\x0438\x043a" );
	Expect( twice.Find( u"Bush", &found ) && found == u"\x041a\x0443\x0441\x0442", "the translation most keys use wins" );
	return failures == 0 ? 0 : 1;
}
