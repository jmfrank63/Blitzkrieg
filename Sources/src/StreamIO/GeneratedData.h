#ifndef BLITZKRIEG_STREAMIO_GENERATED_DATA_H
#define BLITZKRIEG_STREAMIO_GENERATED_DATA_H

#include <filesystem>
#include <string>
#include "Globals.h"
#include "StreamIO.h"
#include "StructureSaver.h"
#include "ProfilePaths.h"

// Data the game generates at runtime under names that belong to Data: a
// template mission's random map (maps\templatemaps\...: .bzm, .lua, .seed and
// pictures), the mission file rewritten with the generated objectives, and the
// briefing picture. The original wrote all of it into the installation's Data
// directory - failing on a protected installation, overwriting the shipped
// template mission and a briefing picture several missions share, and letting
// two players' campaigns overwrite each other's maps. It now goes to
// <user cache>\generated\<profile>\<mod or base>\, under the same relative
// names, and that directory is mounted over the data storage, above any mod, so
// the game finds it by those names. A save regenerates it from the seed it
// stores, so it is a cache. Header-only: the callers span several dylibs.
namespace NGeneratedData
{
inline std::string ModKey( const std::string &szMOD )
{
	std::string szKey;
	for ( std::string::size_type i = 0; i < szMOD.size(); ++i )
	{
		const unsigned char c = szMOD[i];
		if ( c == '\\' || c == '/' )
			continue;
		szKey += ( ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' ) || ( c >= '0' && c <= '9' ) || c == '-' || c == '_' || c == '.' ) ? char( c ) : '_';
	}
	return ( szKey.empty() || szKey == "." || szKey == ".." ) ? std::string( "base" ) : szKey;
}

// The root for the active profile and the given mod folder, with backslashes
// and a trailing one, as the storage layer takes paths.
inline std::string Root( const std::string &szMOD )
{
	const std::string szProfile = GetGlobalVar( "Profile.Name", "" );
	std::string szRoot = ( std::filesystem::path( NProfile::GeneratedDirectory( szProfile.empty() ? std::string( "default" ) : szProfile ) ) / ModKey( szMOD ) ).string();
	for ( std::string::size_type i = 0; i < szRoot.size(); ++i )
	{
		if ( szRoot[i] == '/' )
			szRoot[i] = '\\';
	}
	return szRoot + "\\";
}

// Creates the directories a file is about to be written into.
inline void CreateParentDirectories( const std::string &szFilePath )
{
	std::string szHostPath = szFilePath;
#if !defined(_WIN32)
	for ( std::string::size_type i = 0; i < szHostPath.size(); ++i )
	{
		if ( szHostPath[i] == '\\' )
			szHostPath[i] = '/';
	}
#endif
	std::error_code error;
	std::filesystem::create_directories( std::filesystem::path( szHostPath ).parent_path(), error );
}

// Whether the profile's generated data holds this data name - asked of the
// generated directory alone, not of Data or the mod beneath it.
inline bool Exists( const std::string &szMOD, const std::string &szName )
{
	CPtr<IDataStorage> pGenerated = OpenStorage( ( Root( szMOD ) + "*.pak" ).c_str(), STREAM_ACCESS_READ, STORAGE_TYPE_COMMON );
	return pGenerated != 0 && pGenerated->IsStreamExist( szName.c_str() );
}

// (Re)mounts the generated directory over the data storage. Called at startup
// and whenever the mod or the profile changes; a later mount goes on top, so
// the mod is mounted first.
inline void Mount( const std::string &szMOD )
{
	IDataStorage *pStorage = GetSingleton<IDataStorage>();
	if ( pStorage == 0 )
		return;
	pStorage->RemoveStorage( "GENERATED" );
	const std::string szRoot = Root( szMOD );
	CreateParentDirectories( szRoot + "placeholder" );
	if ( CPtr<IDataStorage> pGenerated = OpenStorage( ( szRoot + "*.pak" ).c_str(), STREAM_ACCESS_READ, STORAGE_TYPE_COMMON ) )
		pStorage->AddStorage( pGenerated, "GENERATED" );
	if ( getenv( "BK_UI_TRACE" ) )
		fprintf( stderr, "BK_UI_TRACE: generated data mounted from \"%s\"\n", szRoot.c_str() );
}
}

#endif
