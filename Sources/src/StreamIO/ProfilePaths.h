#ifndef BLITZKRIEG_STREAMIO_PROFILE_PATHS_H
#define BLITZKRIEG_STREAMIO_PROFILE_PATHS_H

#include <string>
#include <cstring>
#include "Globals.h"

// Player profiles. Each profile owns every piece of per-player data - saves,
// replays, screenshots and config.cfg - under <game>/profiles/<name>/. The
// active profile's name is carried by the "Profile.Name" global; GameMain
// bootstraps it (command line -profile=... beats profiles/active.cfg beats
// "Player") before the config is read, and the player-profile dialog switches
// it at runtime. The helpers are header-only because the callers span
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

// Profile names come from the player-name edit box; whatever cannot appear
// in a directory name is dropped. An empty result falls back to "Player".
inline std::string Sanitize( const std::string &szName )
{
	std::string szResult;
	for ( std::string::size_type i = 0; i < szName.size(); ++i )
	{
		const unsigned char c = szName[i];
		if ( c < 32 || strchr( "/\\:*?\"<>|", c ) != 0 )
			continue;
		szResult += char( c );
	}
	// Leading/trailing spaces and trailing dots are invalid on Windows and a
	// nuisance everywhere ("saves/ Mission Start Auto.sav" taught us that).
	while ( !szResult.empty() && szResult[0] == ' ' )
		szResult.erase( 0, 1 );
	while ( !szResult.empty() && (szResult[szResult.size() - 1] == ' ' || szResult[szResult.size() - 1] == '.') )
		szResult.erase( szResult.size() - 1 );
	if ( szResult.empty() )
		szResult = "Player";
	return szResult;
}
}

#endif
