#ifndef BLITZKRIEG_STREAMIO_MODS_PATHS_H
#define BLITZKRIEG_STREAMIO_MODS_PATHS_H

#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>
#if !defined(_WIN32)
#include <filesystem>
#include <system_error>
#endif

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

// Whether two names are equal ignoring ASCII case, the way Windows compares
// the folder names a mod or a language is chosen by.
inline bool SameName( const std::string &szLeft, const std::string &szRight )
{
	if ( szLeft.size() != szRight.size() )
		return false;
	for ( size_t i = 0; i < szLeft.size(); ++i )
	{
		const char cLeft = szLeft[i] >= 'A' && szLeft[i] <= 'Z' ? char( szLeft[i] - 'A' + 'a' ) : szLeft[i];
		const char cRight = szRight[i] >= 'A' && szRight[i] <= 'Z' ? char( szRight[i] - 'A' + 'a' ) : szRight[i];
		if ( cLeft != cRight )
			return false;
	}
	return true;
}

#if !defined(_WIN32)
// The subfolders of `szFolder` (engine spelling, either separator), sorted.
inline std::vector<std::string> SubFolders( const std::string &szFolder )
{
	std::string szHost = szFolder;
	std::replace( szHost.begin(), szHost.end(), '\\', '/' );
	std::vector<std::string> names;
	std::error_code error;
	for ( std::filesystem::directory_iterator it( szHost.empty() ? std::string( "." ) : szHost, error ), end; !error && it != end; it.increment( error ) )
	{
		std::error_code entryError;
		if ( it->is_directory( entryError ) && !entryError )
			names.push_back( it->path().filename().string() );
	}
	std::sort( names.begin(), names.end() );
	return names;
}
#endif

// Every folder the mods live in, each with a trailing backslash. A player who
// copies a mod into a "Mods" beside the installs' "mods" on a case-sensitive
// file system has two folders that are one on Windows: both are listed, the
// installs' "mods" first, and a mod is found in whichever holds it. Taking only
// the first spelling found hid the nine language mods behind a Mods/ holding
// AchtungPanzer2, and Options > Language offered only Default. BK_MODS_ROOT,
// when set, is the only folder; with no mods folder on disk, Root's.
inline std::vector<std::string> Roots( const std::string &szBase )
{
	std::vector<std::string> roots;
#if !defined(_WIN32)
	if ( Override().empty() )
	{
		const std::vector<std::string> names = SubFolders( szBase );
		for ( size_t i = 0; i < names.size(); ++i )
		{
			if ( !SameName( names[i], "mods" ) )
				continue;
			if ( names[i] == "mods" )
				roots.insert( roots.begin(), szBase + names[i] + "\\" );
			else
				roots.push_back( szBase + names[i] + "\\" );
		}
	}
#endif
	if ( roots.empty() )
		roots.push_back( Root( szBase ) );
	return roots;
}

// The folder of the mod named `szName` (any case, a trailing separator
// allowed), in the first mods folder that holds it, spelled as on disk and
// with no trailing separator: CICChangeMOD lower-cases the name it is given.
// Where no mods folder holds it, Root's spelling, which then opens nothing.
inline std::string Find( const std::string &szBase, const std::string &szName )
{
	std::string szWanted = szName;
	while ( !szWanted.empty() && (szWanted[szWanted.size() - 1] == '/' || szWanted[szWanted.size() - 1] == '\\') )
		szWanted.erase( szWanted.size() - 1 );
#if !defined(_WIN32)
	const std::vector<std::string> roots = Roots( szBase );
	for ( size_t r = 0; r < roots.size(); ++r )
	{
		const std::vector<std::string> names = SubFolders( roots[r] );
		for ( size_t i = 0; i < names.size(); ++i )
			if ( names[i] == szWanted )
				return roots[r] + names[i];
		for ( size_t i = 0; i < names.size(); ++i )
			if ( SameName( names[i], szWanted ) )
				return roots[r] + names[i];
	}
#endif
	return Root( szBase ) + szWanted;
}
}

#endif
