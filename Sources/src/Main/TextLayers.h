#ifndef __TEXT_LAYERS_H__
#define __TEXT_LAYERS_H__
#pragma ONCE
// The storage side of NTextPrecedence: the gameplay mod's and the language mod's storages, which
// MainLoopCommands publishes as it mounts them, and the read of one text through the rule.
#include <string>

interface IDataStorage;

namespace NTextLayers
{
	// The storages mounted as MOD and LANGUAGE, or 0 when none is; each call forgets the translation
	// memory, which is built again from the language on the first text a mod has.
	void SetMod( IDataStorage *pMod );
	void SetLanguage( IDataStorage *pLanguage );
	// Releases both, before the storage module unloads (NMain::Finalize).
	void Clear();
	// The text of a key (a stream name, "textes\\...\\name.txt") as the rule picks it, without its
	// byte order mark. False when no mod and language are both mounted or the mod lacks the key:
	// the caller reads the storage as it always did.
	bool Read( const char *pszName, std::u16string *pText );
}
#endif // __TEXT_LAYERS_H__
