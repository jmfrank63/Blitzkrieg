#ifndef BLITZKRIEG_STREAMIO_MODS_PATHS_H
#define BLITZKRIEG_STREAMIO_MODS_PATHS_H

#include <cstdlib>
#include <string>

// The folder the mods live in: <game>/mods/, one folder per mod (the original
// layout, which -mod=Name, Load Mod and Options > Language read). Header-only
// because the callers span several dylibs and the editor bridge.
namespace NMods
{
// The folder BK_MODS_ROOT names, with no trailing separator, or an empty
// string when it is not set. Tests set it so the mods they stage, export and
// play never land in an installation's mods folder, which is the player's: the
// leftovers of the editor tiers once pushed the Game's Detach All MODs row down
// its Load Mod list. Not a player setting.
inline std::string Override()
{
	const char *pszRoot = std::getenv( "BK_MODS_ROOT" );
	if ( pszRoot == 0 || pszRoot[0] == 0 )
		return std::string();
	std::string szRoot = pszRoot;
	while ( szRoot.size() > 1 && (szRoot[szRoot.size() - 1] == '/' || szRoot[szRoot.size() - 1] == '\\') )
		szRoot.erase( szRoot.size() - 1 );
	return szRoot;
}

// The mods folder with a trailing backslash, the engine's separator: the
// override, or `szBase` + "mods\\".
inline std::string Root( const std::string &szBase )
{
	const std::string szOverride = Override();
	if ( !szOverride.empty() )
		return szOverride + "\\";
	return szBase + "mods\\";
}
}

#endif
