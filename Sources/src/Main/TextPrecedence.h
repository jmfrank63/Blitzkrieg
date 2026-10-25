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
// - otherwise the mod's own text. The mod changed the key's English, and the translation of the
//   base game's text may say something else: Achtung Panzer 2 turns the switch-mod message box into
//   "Grand Campaign" / "To start the campaign of this mod, go to Custom Game", whose translation
//   asked whether to change the mod. Its rewordings ("paused" for "Pause") stay English with it.
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
	};

	ESource Resolve( const std::string &szKey, const FReader &base, const FReader &mod, const FReader &language,
		const CMemory &memory, std::u16string *pText );
}
#endif // __TEXT_PRECEDENCE_H__
