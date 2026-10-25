// NMods (StreamIO/ModsPaths.h) on a game folder holding both "mods" and
// "Mods", as a Linux install does once a player copies a mod into a "Mods"
// beside the installs' language mods: both folders are mods folders, the
// installs' first, and a mod is found by name in whichever holds it.
#include "StreamIO/ModsPaths.h"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static int nFailures = 0;
#define CHECK( x ) do { if ( !(x) ) { std::fprintf( stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #x ); ++nFailures; } } while ( 0 )

static void SetModsRoot( const char *pszValue )
{
#if defined(_WIN32)
	_putenv_s( "BK_MODS_ROOT", pszValue ? pszValue : "" );
#else
	if ( pszValue ) setenv( "BK_MODS_ROOT", pszValue, 1 ); else unsetenv( "BK_MODS_ROOT" );
#endif
}

static void MakeMod( const fs::path &folder )
{
	fs::create_directories( folder / "data" );
	std::ofstream( folder / "data" / "texts.pak" ) << "PK";
}

int main()
{
	const fs::path root = fs::temp_directory_path() / "blitzkrieg-mods-paths-test";
	fs::remove_all( root );
	const fs::path game = root / "game";
	fs::create_directories( game );
	// The engine's base directory ends with a separator.
	const std::string szBase = game.string() + "/";
	SetModsRoot( 0 );

	// No mods folder: Root's, as before.
	{
		const std::vector<std::string> roots = NMods::Roots( szBase );
		CHECK( roots.size() == 1 && roots[0] == szBase + "mods\\" );
		CHECK( NMods::Find( szBase, "ru" ) == szBase + "mods\\ru" );
	}

	MakeMod( game / "mods" / "ru" );
	MakeMod( game / "mods" / "Common" );
	MakeMod( game / "Mods" / "AchtungPanzer2" );
	MakeMod( game / "Mods" / "common" );
	fs::create_directories( game / "modsextra" / "x" );
	const bool bCaseSensitive = !fs::exists( game / "MODS" );

	if ( bCaseSensitive )
	{
		// Both spellings are mods folders, the installs' "mods" first. Taking the first
		// spelling found ("Mods" sorts before "mods") hid the language mods.
		const std::vector<std::string> roots = NMods::Roots( szBase );
		CHECK( roots.size() == 2 );
		CHECK( roots.size() == 2 && roots[0] == szBase + "mods\\" && roots[1] == szBase + "Mods\\" );
		// A mod is found in whichever folder holds it, in any case: CICChangeMOD lower-cases.
		CHECK( NMods::Find( szBase, "achtungpanzer2" ) == szBase + "Mods\\AchtungPanzer2" );
		CHECK( NMods::Find( szBase, "achtungpanzer2\\" ) == szBase + "Mods\\AchtungPanzer2" );
		CHECK( NMods::Find( szBase, "RU" ) == szBase + "mods\\ru" );
		// A name in both folders is the first folder's.
		CHECK( NMods::Find( szBase, "common" ) == szBase + "mods\\Common" );
		CHECK( NMods::Find( szBase, "missing" ) == szBase + "mods\\missing" );
	}
	else
	{
		// One folder on a case-insensitive file system (Windows, a default macOS volume).
		const std::vector<std::string> roots = NMods::Roots( szBase );
		CHECK( roots.size() == 1 );
		// Find answers an engine path (backslash separators); the host file system wants slashes.
		std::string szFound = NMods::Find( szBase, "achtungpanzer2" );
		for ( size_t i = 0; i < szFound.size(); ++i )
			if ( szFound[i] == '\\' )
				szFound[i] = '/';
		CHECK( fs::exists( szFound ) );
	}

	// BK_MODS_ROOT is the one mods folder, whatever the game folder holds.
	const std::string szOverride = ( root / "scratch-mods" ).string();
	MakeMod( root / "scratch-mods" / "Fixture" );
	SetModsRoot( ( szOverride + "/" ).c_str() );
	{
		const std::vector<std::string> roots = NMods::Roots( szBase );
		CHECK( roots.size() == 1 && roots[0] == szOverride + "\\" );
		CHECK( NMods::Find( szBase, "ru" ) == szOverride + "\\ru" );
#if !defined(_WIN32)
		CHECK( NMods::Find( szBase, "fixture" ) == szOverride + "\\Fixture" );
#endif
	}
	SetModsRoot( 0 );

	fs::remove_all( root );
	return nFailures == 0 ? 0 : 1;
}
