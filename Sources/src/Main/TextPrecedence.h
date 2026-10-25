#ifndef __TEXT_PRECEDENCE_H__
#define __TEXT_PRECEDENCE_H__
#pragma ONCE
// Which text a key shows when a gameplay mod and a language mod are both loaded.
//
// The language is mounted above the mod, so it wins for every key it has; a text only the mod has
// (its own trees, houses and vehicles, about 2,300 texts in Achtung Panzer 2) stayed English, though
// many of them are the base game's English word for word under a new key ("Birch Tree", "Bush").
//
// The language used to win for the texts the mod changed too. The rule, for a key the mod has:
// - the mod's text is the base game's English of the key, after normalising: not a change of the
//   mod, the language's translation of the key;
// - the mod's text is empty: the mod removed the text, it stays empty;
// - the language's translation memory (the base game's English of every key the language
//   translates) has the mod's text: that translation, the mod's own words translated;
// - the mod's text is a light rewording of the base game's English of the key (Similarity at least
//   kRewordedSimilarity: "paused" for "Pause", "German Reich" for "Germany", "German officer
//   (Walther P38)" for "German Officer - Walther P38 Sidearm"): the language's translation of the
//   key, which names the same thing where the mod's English would leave a Russian game English;
// - otherwise the mod's own text: the mod changed what the key says, and the translation of the
//   base game's text would say something else. Achtung Panzer 2 turns the switch-mod message box
//   into "Grand Campaign" (0.17 against "SWITCH MOD") / "To start the campaign of this mod, go to
//   Custom Game" (0.30), marks demo maps "zzz BK DEMO MAP (DO NOT RUN)" (0.43 to 0.54) and writes its
//   own credits and help page (0.30, 0.37).
// The threshold is measured on Achtung Panzer 2's 109 shared keys whose English it changed: no
// change of meaning scores above 0.54 (the Kursk demo map's warning), 18 rewordings score 0.6 or
// more. Most of its infantry names (0.45 to 0.60: "German rifleman (Karabiner 98k)" 0.53) and
// "Factory" for "Factory Warehouse" (0.58) fall between and stay English.
// Normalising drops a byte order mark, folds every run of white space to one space, trims both
// ends and folds the case of Latin and Cyrillic letters: the case is the mod's styling of a menu
// ("multiplayer" for "MULTIPLAYER"), not other words.
//
// Only std types: the rule is tested on its own (test-text-precedence), the storage behind it is
// TextLayers.cpp's.
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace NTextPrecedence
{
	std::u16string Normalize( const std::u16string &szText );
	// How alike two texts are, 0 to 1: twice their longest common subsequence of code units over their
	// summed lengths, on the normalised texts (two empty texts are alike).
	double Similarity( const std::u16string &szA, const std::u16string &szB );
	const double kRewordedSimilarity = 0.6;

	// The base game's English and the language's translation of every key the language translates,
	// indexed by the normalised English.
	class CMemory
	{
		struct STranslation { std::string szKey; std::u16string szText; };
		std::map<std::u16string, std::vector<STranslation>> entries;
	public:
		void Add( const std::string &szKey, const std::u16string &szEnglish, const std::u16string &szTranslation );
		void Clear() { entries.clear(); }
		bool IsEmpty() const { return entries.empty(); }
		// The translation of an English text. When several keys share the English and translate it
		// differently, the translation most of them use wins (translations that normalise the same
		// count as one, the first key's spelling is used); a tie has no translation, the English stays.
		bool Find( const std::u16string &szEnglish, std::u16string *pTranslation ) const;
	};

	// Reads one layer's text of a key (no byte order mark); false when the layer has no such text.
	typedef std::function<bool( const std::string &szKey, std::u16string *pText )> FReader;

	enum ESource
	{
		SOURCE_STORAGE,			// the mod does not have the key: the storage's own order decides
		SOURCE_LANGUAGE,		// the mod's text is the base game's English: the language's text of the key
		SOURCE_MEMORY,			// the language's translation of the mod's English, from any key
		SOURCE_MOD,				// the mod's own text
		SOURCE_REWORDED,		// the mod lightly reworded the key's English: the language's text of the key
	};

	ESource Resolve( const std::string &szKey, const FReader &base, const FReader &mod, const FReader &language,
		const CMemory &memory, std::u16string *pText );
}
#endif // __TEXT_PRECEDENCE_H__
