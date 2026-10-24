#ifndef BLITZKRIEG_STREAMIO_PROFILE_PATHS_H
#define BLITZKRIEG_STREAMIO_PROFILE_PATHS_H

#include <cstdlib>
#include <string>
#include "Globals.h"
#include "ProfileNames.h"

// Player profiles. Each profile owns every piece of per-player data - saves,
// replays, screenshots and config.cfg - under <game>/profiles/<name>/. The
// active profile's name is carried by the "Profile.Name" global; GameMain
// bootstraps it (command line -profile=... beats profiles/active.cfg beats
// "Player") before the config is read, and the player-profile dialog switches
// it at runtime. The names themselves and the profiles/ directory are
// ProfileNames.h. The helpers are header-only because the callers span
// several dylibs.
namespace NProfile
{
// Directory segment to splice between the game root and a per-player leaf
// ("saves\\", "screenshots\\", "config.cfg"): "profiles\\<name>\\", or an
// empty string if no profile is active (pre-profile layout).
inline std::string Segment()
{
	const std::string szName = GetGlobalVar( "Profile.Name", "" );
	if ( szName.empty() )
		return std::string();
	return "profiles\\" + szName + "\\";
}

// What to splice a per-player leaf onto for the profile `szName`:
// `szBase` + "profiles\\<name>\\", or the same profile under BK_PROFILES_ROOT
// (an absolute path, so the base is not used). `szBase` alone for no profile.
inline std::string PrefixFor( const std::string &szBase, const std::string &szName )
{
	if ( szName.empty() )
		return szBase;
	const char *pszRoot = std::getenv( "BK_PROFILES_ROOT" );
	if ( pszRoot == 0 || pszRoot[0] == 0 )
		return szBase + "profiles\\" + szName + "\\";
	return Dir() + "\\" + szName + "\\";
}

// The same for the active profile.
inline std::string Prefix( const std::string &szBase )
{
	return PrefixFor( szBase, GetGlobalVar( "Profile.Name", "" ) );
}
}

#endif
