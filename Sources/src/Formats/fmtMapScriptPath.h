#ifndef __FMT_MAP_SCRIPT_PATH_H__
#define __FMT_MAP_SCRIPT_PATH_H__
#include <string>

// How a map names its Lua script (SLoadMapInfo::szScriptFile) and where that
// name is looked up (the user's ruling of 2026-10-03: saves sync through the
// cloud to other computers, so a map file never carries a path of this one).
//
// THE BASE IS THE MAP'S OWN FOLDER. The game has only ever loaded
// "<the folder of the map it loaded>/<the last component of szScriptFile>.lua"
// (GameTT/iMissionInternal.cpp, Main/GameCreation.cpp), and the editor copies
// the script beside the map on the same rule, so the path of a script that
// lies beside its map, relative to that map, is its bare name. That is what a
// map stores: relative, '/' as the only separator it could ever hold, never a
// drive, never a root, never a backslash the editor wrote.
//
//  * stored   - what the editor writes into a map file (generation, Save,
//               Save As, the Script dialog): ToStored. A relative value is kept
//               as it is, so a map that is opened and saved unchanged stays
//               byte for byte (the shipped maps hold "maps\Name", which is not
//               absolute); an ABSOLUTE value - the old CreateRandomMap stored the
//               output path - is cut to its last component.
//  * expanded - what a loader asks the storage for: BesideMap is the map's
//               folder (in the spelling of the map's own name) and the last
//               component, split on either separator; ExpandOnLoad leaves a
//               legacy value (a backslash in it, or absolute) as it was, for the
//               consumers that used it raw (the check sums).
//
// Header only and std only: the game, the generator, the map file module and
// the editor bridge all include it.
namespace NMapScriptPath
{

// A path that names a place on one machine: a drive ("C:\x", "C:/x"), a root
// ("/x", "\x") or a share ("\\host\x").
inline bool IsAbsolute( const std::string &szPath )
{
	if ( szPath.empty() )
		return false;
	if ( szPath[0] == '/' || szPath[0] == '\\' )
		return true;
	return szPath.size() >= 2 && szPath[1] == ':' && ( ( szPath[0] >= 'A' && szPath[0] <= 'Z' ) || ( szPath[0] >= 'a' && szPath[0] <= 'z' ) );
}

// What follows the last '\' or '/' (the whole text when there is none).
inline std::string LastComponent( const std::string &szPath )
{
	const std::string::size_type nCut = szPath.find_last_of( "\\/" );
	return nCut == std::string::npos ? szPath : szPath.substr( nCut + 1 );
}

// The form this editor writes: no drive, no root, no backslash.
inline bool IsRelative( const std::string &szPath )
{
	return !szPath.empty() && !IsAbsolute( szPath ) && szPath.find( '\\' ) == std::string::npos;
}

// What a map file stores for the script value szScriptFile: an absolute path is
// cut to its last component (the script lies beside the map, the game looks
// only at that name), anything else - empty, a bare name, "scripts/name", the
// shipped maps' "maps\Name" - is returned as it is.
inline std::string ToStored( const std::string &szScriptFile )
{
	return IsAbsolute( szScriptFile ) ? LastComponent( szScriptFile ) : szScriptFile;
}

// The map's own folder in the spelling the map's name uses, with its trailing
// separator ("" for a name with none).
inline std::string FolderOf( const std::string &szMapName )
{
	const std::string::size_type nCut = szMapName.find_last_of( "\\/" );
	return nCut == std::string::npos ? std::string() : szMapName.substr( 0, nCut + 1 );
}

// Where the game looks for the script a map stores: the map's folder and the
// last component of the stored value. Empty stays empty. szMapName is the
// storage name of the map ("maps\Allies\Ardennes\BattleOfBulge", with or
// without an extension).
inline std::string BesideMap( const std::string &szStored, const std::string &szMapName )
{
	if ( szStored.empty() )
		return szStored;
	return FolderOf( szMapName ) + LastComponent( szStored );
}

// The expansion of a RELATIVE stored value (what this editor writes) to the
// storage name beside the map; a legacy value is returned untouched, so the
// consumers that read it raw keep their behaviour on every map older than the
// ruling.
inline std::string ExpandOnLoad( const std::string &szStored, const std::string &szMapName )
{
	return IsRelative( szStored ) ? BesideMap( szStored, szMapName ) : szStored;
}

} // namespace NMapScriptPath

#endif // __FMT_MAP_SCRIPT_PATH_H__
