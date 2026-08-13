#ifndef BLITZKRIEG_STREAMIO_PROFILE_PATHS_H
#define BLITZKRIEG_STREAMIO_PROFILE_PATHS_H

#include <string>
#include <cstring>
#include <vector>
#include <filesystem>
#include <algorithm>
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
// Only printable ASCII survives: the edit box hands the name through a
// wchar->char truncation, so anything beyond ASCII arrives as mojibake
// bytes - and those form invalid UTF-8 that APFS refuses in a path, which
// made every profile write (config, saves, autosaves) fail silently once
// such a name was written to active.cfg.
inline std::string Sanitize( const std::string &szName )
{
	std::string szResult;
	for ( std::string::size_type i = 0; i < szName.size(); ++i )
	{
		const unsigned char c = szName[i];
		if ( c < 32 || c > 126 || strchr( "/\\:*?\"<>|", c ) != 0 )
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

// Profile names are printable ASCII (Sanitize guarantees it) and live on
// case-insensitive filesystems (APFS, NTFS), where "player" and "Player"
// are one directory - so name comparisons must be case-insensitive too,
// and plain ASCII lowering is exact, no locale involved.
inline char NameLower( char c )
{
	return c >= 'A' && c <= 'Z' ? char( c - 'A' + 'a' ) : c;
}

inline bool NameLess( const std::string &a, const std::string &b )
{
	const std::string::size_type nCommon = a.size() < b.size() ? a.size() : b.size();
	for ( std::string::size_type i = 0; i < nCommon; ++i )
	{
		const char ca = NameLower( a[i] ), cb = NameLower( b[i] );
		if ( ca != cb )
			return ca < cb;
	}
	return a.size() < b.size();
}

inline bool NameEquals( const std::string &a, const std::string &b )
{
	return !NameLess( a, b ) && !NameLess( b, a );
}

// The filesystem is the profile registry: a profile is any directory
// directly under profiles/ whose name survives Sanitize unchanged. A
// directory the user made by hand with a name Sanitize would alter is
// neither listed nor ever touched. A missing profiles/ directory is an
// empty list, not an error.
inline std::vector<std::string> List()
{
	std::vector<std::string> names;
	std::error_code ec;
	std::filesystem::directory_iterator it( "profiles", ec );
	const std::filesystem::directory_iterator end;
	for ( ; !ec && it != end; it.increment( ec ) )
	{
		std::error_code ecEntry;
		if ( !it->is_directory( ecEntry ) || ecEntry )
			continue;
		const std::string szName = it->path().filename().string();
		if ( Sanitize( szName ) != szName )
			continue;
		names.push_back( szName );
	}
	std::sort( names.begin(), names.end(), NameLess );
	return names;
}

// Renames profiles/<from> to profiles/<to>. Refuses when <to> already
// exists as another profile - one directory cannot be two profiles. On
// failure returns false with the filesystem's message in *pError (which
// may be null).
inline bool Rename( const std::string &from, const std::string &to, std::string *pError )
{
	const std::string szFrom = Sanitize( from );
	const std::string szTo = Sanitize( to );
	if ( szFrom == szTo )
		return true;
	std::error_code ec;
	if ( NameEquals( szFrom, szTo ) )
	{
		// Only the case differs, and the filesystem considers that the same
		// directory: a direct rename is a silent no-op on APFS. Hopping
		// through a temporary name forces a real change both times.
		const std::string szTemp = "profiles/" + szTo + ".tmp-rename";
		std::filesystem::rename( "profiles/" + szFrom, szTemp, ec );
		if ( !ec )
			std::filesystem::rename( szTemp, "profiles/" + szTo, ec );
	}
	else
	{
		const std::vector<std::string> names = List();
		for ( std::vector<std::string>::size_type i = 0; i < names.size(); ++i )
		{
			if ( NameEquals( names[i], szTo ) )
			{
				if ( pError != 0 )
					*pError = "profile \"" + szTo + "\" already exists";
				return false;
			}
		}
		std::filesystem::rename( "profiles/" + szFrom, "profiles/" + szTo, ec );
	}
	if ( ec )
	{
		if ( pError != 0 )
			*pError = ec.message();
		return false;
	}
	return true;
}

// Removes profiles/<name> and everything under it. On failure returns
// false with the filesystem's message in *pError (which may be null).
inline bool Delete( const std::string &name, std::string *pError )
{
	std::error_code ec;
	std::filesystem::remove_all( "profiles/" + Sanitize( name ), ec );
	if ( ec )
	{
		if ( pError != 0 )
			*pError = ec.message();
		return false;
	}
	return true;
}
}

#endif
