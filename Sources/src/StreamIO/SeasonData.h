#ifndef BLITZKRIEG_STREAMIO_SEASON_DATA_H
#define BLITZKRIEG_STREAMIO_SEASON_DATA_H

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <system_error>
#include "StreamIO.h"
#include "StructureSaver.h"
#include "../Platform/Paths.h"

// The winter ("1w") and Africa ("1a") textures a unit's folder lacks - 83 of
// the 242 unit mesh folders have no 1w and most no 1a, the original game's too -
// derived from the summer ones by the build (tools/zig/season_textures.zig)
// and staged beside Data as SeasonData\SeasonTextures.pak, under Data's
// relative names (the directory's loose files would be read too). Mounted
// over the base Data and below any mod, so a mod's own season texture still
// wins, and a unit whose season file really is missing falls back to its
// summer one (VisObjBuilder.cpp, GetSeasonedMeshTexture). The build never
// generates a file Data has. An installation without SeasonData (another
// game's Data, picked in the editor) mounts nothing. Header-only: the game
// and the editor bridge both mount it.
namespace NSeasonData
{
inline const char *StorageName() { return "SEASON_DATA"; }

// Mounts SeasonData over pStorage's base, replacing an earlier mount. Call it
// before any mod is mounted: a later mount goes on top.
inline bool Mount( IDataStorage *pStorage )
{
	if ( pStorage == 0 )
		return false;
	pStorage->RemoveStorage( StorageName() );
	std::error_code error;
	if ( !std::filesystem::is_directory( std::filesystem::path( NPlatform::Paths::SeasonDataRoot() ), error ) )
		return false;
	CPtr<IDataStorage> pSeasonData = OpenStorage( NPlatform::Paths::SeasonDataArchivePattern().c_str(), STREAM_ACCESS_READ, STORAGE_TYPE_COMMON );
	const bool bMounted = pSeasonData != 0 && pStorage->AddStorage( pSeasonData, StorageName() );
	if ( getenv( "BK_UI_TRACE" ) )
		fprintf( stderr, "BK_UI_TRACE: season data %s from \"%s\"\n", bMounted ? "mounted" : "NOT mounted", NPlatform::Paths::SeasonDataArchivePattern().c_str() );
	return bMounted;
}

inline bool Unmount( IDataStorage *pStorage )
{
	return pStorage != 0 && pStorage->RemoveStorage( StorageName() );
}
}

#endif
