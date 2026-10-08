// The tileset exporter: CTileSetTreeRootItem::ComposeTiles
// (Sources/src/editor/TileTreeItem.cpp:41-415), line for line. Each tile
// picture is multiplied by the tile mask and laid into the atlas at the
// place its index says, the engine's STilesetDesc and SCrossetDesc are filled
// from the tree and written through their own operator&, and the atlases are
// compressed by the engine's image processor.
//
// Where MFC would over-read, this stops short and says so: the mask rectangle
// is the mask's size (64 x 32), which MFC applied to a picture of any size.
// A smaller picture (the fixture's 16 x 16 art) is processed over its own
// size, a larger one over the mask's, and the result is the same for the
// 64 x 32 art the game's tilesets use.
#include "StdAfx.h"

#include <algorithm>
#include <filesystem>
#include <set>

#include "tileset_export.h"
#include "tileset.h"

#include "../stats_export.h"
#include "../../image_export.h"
#include "../../factory.h"
#include "../tree_item_types.h"
#include "../../../Formats/fmtTerrain.h"
#include "../../../Main/RPGStats.h"
// Builders.h declares vertex builders on the GFX buffers; only the inline
// tile-map helpers are used here, so the names are enough.
struct IGFXVertices;
struct IGFXIndices;
#include "../../../RandomMapGen/Builders.h"

namespace NResourceModel
{

namespace
{

using namespace NStatsExport;
namespace fs = std::filesystem;

const char kTileSetAddDir[] = "terrain\\sets\\";

int NextPow2( int nValue )
{
	int nResult = 1;
	while ( nResult < nValue )
		nResult <<= 1;
	return nResult;
}

// The tiles directory value as the frame stores it, made a full folder as
// ComposeTiles did: a related path is taken from the project folder.
std::string SourceDirectory( const SExportContext &context, const std::string &szDirName )
{
	std::string szDir = szDirName;
	std::replace( szDir.begin(), szDir.end(), '/', '\\' );
	if ( IsRelatedPath( szDir ) )
	{
		std::string szProjectDir = ProjectDirectory( context );
		std::replace( szProjectDir.begin(), szProjectDir.end(), '/', '\\' );
		szDir = MakeFullPath( szProjectDir, szDir );
	}
	if ( !szDir.empty() && szDir.back() != '\\' )
		szDir += '\\';
	return szDir;
}

// The mask: editor\terrain\tilemask.tga of the editor data folder, else of
// the export root's data folder, with the case of the folders on disk.
fs::path MaskFile( const SExportContext &context )
{
	std::error_code ec;
	for ( const std::string &szRoot : { context.szEditorDataDir, context.szDataRoot } )
	{
		if ( szRoot.empty() )
			continue;
		const fs::path file = FoldedChild( FoldedChild( FoldedChild( fs::path( szRoot ), "editor" ), "terrain" ), "tilemask.tga" );
		if ( fs::is_regular_file( file, ec ) )
			return file;
	}
	return fs::path();
}

// FillTileMaps( nSizeX, nSizeY, tileMaps, true ) of editor/common.cpp: the
// engine maps of every tile of an atlas, row pair by row pair, each primary
// and secondary tile with its flipped twin after it.
void FillTileMaps( int nSizeX, int nSizeY, std::vector<STileMapsDesc> &tileMaps )
{
	CVec2 maps[4];
	auto addPrimary = [&]( int nColumn )
	{
		for ( int k = 0; k < 4; k++ )
			for ( const bool bFlip : { false, true } )
			{
				GetPrimaryMaps( k, nColumn, bFlip, maps, float( nSizeX ), float( nSizeY ) );
				tileMaps.push_back( STileMapsDesc( maps[0], maps[1], maps[2], maps[3] ) );
			}
	};
	int nNumColumn = 0;
	for ( int i = 0; i < nSizeY - 32; i += 32 )
	{
		addPrimary( nNumColumn );
		for ( int k = 0; k < 3; k++ )
			for ( const bool bFlip : { false, true } )
			{
				GetSecondaryMaps( k, nNumColumn, bFlip, maps, float( nSizeX ), float( nSizeY ) );
				tileMaps.push_back( STileMapsDesc( maps[0], maps[1], maps[2], maps[3] ) );
			}
		nNumColumn++;
	}
	addPrimary( nNumColumn );
}

// MFC's getters of the tile props item.
enum { E_NORMAL, E_FLIPPED, E_BOTH };

int FlippedState( const CTreeItem &tile, std::string &szError )
{
	const std::string szVal = ValueStr( tile, 1 );
	if ( szVal == "normal and flipped" || szVal == "Normal and flipped" )
		return E_BOTH;
	if ( szVal == "normal" || szVal == "Normal" )
		return E_NORMAL;
	if ( szVal == "flipped" || szVal == "Flipped" )
		return E_FLIPPED;
	szError = "unknown flipped state \"" + szVal + "\" of tile " + tile.GetDisplayName();
	return -1;
}

// The picture of one tile, modulated by the mask over rc, or null when the
// file is not there or not a picture.
CPtr<IImage> LoadTile( const std::string &szDir, const std::string &szName, IImage *pMask, RECT &rc )
{
	SExportOutcome scratch;
	CPtr<IImage> pTile = NImageExport::LoadPicture( FoldedFile( szDir + szName + ".tga" ).string(), scratch );
	if ( pTile == 0 )
		return 0;
	rc.left = 0;
	rc.top = 0;
	rc.right = (std::min)( pMask->GetSizeX(), pTile->GetSizeX() );
	rc.bottom = (std::min)( pMask->GetSizeY(), pTile->GetSizeY() );
	pTile->ModulateColorFrom( pMask, &rc, 0, 0 );
	return pTile;
}

void AddFlipped( STerrTypeDesc &terrType, int nFlipped, int nTileIndex, float fProbTo )
{
	SMainTileDesc main;
	main.fProbFrom = 0;
	main.fProbTo = fProbTo;
	if ( nFlipped == E_NORMAL || nFlipped == E_BOTH )
	{
		main.nIndex = nTileIndex * 2;
		terrType.tiles.push_back( main );
	}
	if ( nFlipped == E_FLIPPED || nFlipped == E_BOTH )
	{
		main.nIndex = nTileIndex * 2 + 1;
		terrType.tiles.push_back( main );
	}
}

}

int TileSetAtlasHeight( int nMaxIndex )
{
	return NextPow2( ( nMaxIndex / 7 ) * 32 + 16 );
}

int CrossetAtlasHeight( int nCrossCount )
{
	return NextPow2( ( ( nCrossCount + 6 ) / 7 ) * 32 + 16 );
}

void TileSetAtlasPosition( int nIndex, int &nPosX, int &nPosY )
{
	const int nMod7 = nIndex % 7;
	if ( nMod7 < 4 )
	{
		nPosX = nMod7 * 64;
		nPosY = ( nIndex / 7 ) * 32;
	}
	else
	{
		nPosX = ( nMod7 - 4 ) * 64 + 32;
		nPosY = ( nIndex / 7 ) * 32 + 16;
	}
}

void CrossetAtlasPosition( int nCrossIndex, int &nPosX, int &nPosY )
{
	const int nMod7 = ( nCrossIndex / 2 ) % 7;
	if ( nMod7 < 4 )
	{
		nPosX = nMod7 * 64;
		nPosY = ( nCrossIndex / 14 ) * 32;
	}
	else
	{
		nPosX = ( nMod7 - 4 ) * 64 + 32;
		nPosY = ( nCrossIndex / 14 ) * 32 + 16;
	}
}

bool ExportTileSet( const Project &project, const SExportContext &context, SExportOutcome &outcome )
{
	const std::unique_ptr<Project> pProject = PreparedCopy( project, ETIT_TILESET_ROOT_ITEM, "tileset", outcome );
	if ( !pProject )
		return false;
	// CTileSetFrame::InitFreeTerrainIndexes ran after every load: a tile
	// without a stored index gets its place before the export reads them.
	AssignMissingTileIndexes( *pProject->root );
	const CTreeItem &root = *pProject->root;
	const CTreeItem *pCommon = RequireChild( root, ETIT_TILESET_COMMON_PROPS_ITEM, 0, "Basic Info", outcome );
	const CTreeItem *pTerrains = pCommon != nullptr ? RequireChild( root, ETIT_TILESET_TERRAINS_ITEM, 0, "Terrains", outcome ) : nullptr;
	const CTreeItem *pCrossets = pTerrains != nullptr ? RequireChild( root, ETIT_CROSSETS_ITEM, 0, "Crossets", outcome ) : nullptr;
	if ( pCrossets == nullptr )
		return false;

	const std::string szTerrainsName = ValueStr( *pTerrains, 0 );
	const std::string szTilesDir = SourceDirectory( context, szTerrainsName );

	// The highest tile index decides the atlas height.
	int nMaxIndex = -1;
	for ( const auto &pTerrain : pTerrains->GetChildren() )
		if ( const CTreeItem *pTiles = ChildItem( *pTerrain, ETIT_TILESET_TILES_ITEM ) )
			for ( const auto &pTile : pTiles->GetChildren() )
				if ( pTile->GetItemType() == ETIT_TILESET_TILE_PROPS_ITEM )
					nMaxIndex = (std::max)( nMaxIndex, static_cast<const CTileSetTilePropsItem *>( pTile.get() )->nTileIndex );
	const int nSizeY = TileSetAtlasHeight( nMaxIndex );

	IImageProcessor *pImageProcessor = GetImageProcessor();
	CPtr<IImage> pTileSetImage = pImageProcessor->CreateImage( 64 * 4, nSizeY );
	pTileSetImage->Set( 0 );
	const fs::path maskFile = MaskFile( context );
	if ( maskFile.empty() )
	{
		outcome.szError = "Error: Cannot open terrain mask file: editor\\terrain\\tilemask.tga (looked in " +
		                  ( context.szEditorDataDir.empty() ? context.szDataRoot : context.szEditorDataDir ) + ")";
		return false;
	}
	CPtr<IImage> pMaskImage = NImageExport::LoadPicture( maskFile.string(), outcome );
	if ( pMaskImage == 0 )
		return false;

	std::vector<std::string> invalidFiles;
	STilesetDesc tileSetDesc;
	tileSetDesc.szName = ValueStr( *pCommon, 0 );
	for ( const auto &pTerrain : pTerrains->GetChildren() )
	{
		if ( pTerrain->GetItemType() != ETIT_TILESET_TERRAIN_PROPS_ITEM )
			continue;
		const CTreeItem &props = *pTerrain;
		STerrTypeDesc terrType;
		terrType.szName = ValueStr( props, 0 );
		terrType.nCrosset = ValueInt( props, 1 );
		terrType.nPriority = ValueInt( props, 2 );
		terrType.fPassability = ValueFloat( props, 3 );
		terrType.bMicroTexture = ValueBool( props, 8 );
		terrType.fSoundVolume = ValueFloat( props, 9 );
		terrType.szSound = ValueStr( props, 10 );
		terrType.szLoopedSound = ValueStr( props, 11 );
		terrType.bCanEntrench = ValueBool( props, 12 );
		if ( ValueBool( props, 14 ) )
			terrType.cSoilParams |= STerrTypeDesc::ESP_TRACE;
		if ( ValueBool( props, 15 ) )
			terrType.cSoilParams |= STerrTypeDesc::ESP_DUST;

		terrType.dwAIClasses = 0;
		if ( ValueBool( props, 4 ) )
			terrType.dwAIClasses |= AI_CLASS_HUMAN;
		if ( ValueBool( props, 5 ) )
			terrType.dwAIClasses |= AI_CLASS_WHEEL;
		if ( ValueBool( props, 6 ) )
			terrType.dwAIClasses |= AI_CLASS_HALFTRACK;
		if ( ValueBool( props, 7 ) )
			terrType.dwAIClasses |= AI_CLASS_TRACK;
		terrType.dwAIClasses = ~terrType.dwAIClasses;
		if ( ValueBool( props, 13 ) )
			terrType.dwAIClasses |= 0x80000000;
		else
			terrType.dwAIClasses &= 0x7fffffff;

		// The terrain sounds lists were commented out of ComposeTiles.
		const CTreeItem *pTiles = ChildItem( props, ETIT_TILESET_TILES_ITEM );
		if ( pTiles == nullptr )
		{
			outcome.szError = "the terrain \"" + terrType.szName + "\" has no Tiles item";
			return false;
		}
		for ( const auto &pTileItem : pTiles->GetChildren() )
		{
			if ( pTileItem->GetItemType() != ETIT_TILESET_TILE_PROPS_ITEM )
				continue;
			const CTileSetTilePropsItem &tile = static_cast<const CTileSetTilePropsItem &>( *pTileItem );
			RECT rc;
			CPtr<IImage> pCurrent = LoadTile( szTilesDir, tile.GetDisplayName(), pMaskImage, rc );
			if ( pCurrent == 0 )
			{
				invalidFiles.push_back( szTerrainsName + tile.GetDisplayName() + ".tga" );
				continue;
			}
			int nPosX, nPosY;
			TileSetAtlasPosition( tile.nTileIndex, nPosX, nPosY );
			pTileSetImage->CopyFromAB( pCurrent, &rc, nPosX, nPosY );
			const int nFlipped = FlippedState( tile, outcome.szError );
			if ( nFlipped < 0 )
				return false;
			AddFlipped( terrType, nFlipped, tile.nTileIndex, ValueFloat( tile, 0 ) );
		}
		tileSetDesc.terrtypes.push_back( terrType );
	}
	for ( const std::string &szFile : invalidFiles )
		outcome.warnings.push_back( "tileset: cannot open " + szFile + "; its tile is left out and the export goes on" );

	const std::string szFile = StatsFileName( project, context, kTileSetAddDir, false );
	std::string szBase = szFile;
	const std::string::size_type nDot = szBase.find_last_of( '.' );
	if ( nDot != std::string::npos && nDot > szBase.find_last_of( '\\' ) + 1 )
		szBase.resize( nDot );
	const NImageExport::SGamma gamma = NImageExport::ReadGammaConfig( ProjectDirectory( context ) );

	FillTileMaps( 256, nSizeY, tileSetDesc.tilemaps );
	if ( !context.bStatsOnly && !NImageExport::SaveCompressedTexture( context, pTileSetImage, szBase, gamma, GFXPF_DXT1, GFXPF_ARGB0565, outcome ) )
		return false;
	if ( !WriteStats( context, szFile, [&]( IDataTree *pDT )
	{
		CTreeAccessor tree = pDT;
		tree.Add( "tileset", &tileSetDesc );
	}, outcome ) )
		return false;
	outcome.szObjectName = szBase;

	int nCrossCount = 0;
	for ( const auto &pCrosset : pCrossets->GetChildren() )
		nCrossCount += int( pCrosset->GetChildren().size() );
	if ( nCrossCount == 0 )
		return true;

	// What is counted above is each crosset's children, the twelve a..f'
	// groups, not the pictures in them: MFC sized the atlas by that.
	const std::string szCrossDir = SourceDirectory( context, ValueStr( *pCrossets, 0 ) );
	const int nCrossSizeY = CrossetAtlasHeight( nCrossCount );
	CPtr<IImage> pCrossSetImage = pImageProcessor->CreateImage( 64 * 4, nCrossSizeY );
	pCrossSetImage->Set( 0 );

	invalidFiles.clear();
	SCrossetDesc crossSetDesc;
	for ( const auto &pCrosset : pCrossets->GetChildren() )
	{
		if ( pCrosset->GetItemType() != ETIT_CROSSET_PROPS_ITEM )
			continue;
		SCrossDesc crossDesc;
		crossDesc.szName = ValueStr( *pCrosset, 0 );
		for ( const auto &pCrossTiles : pCrosset->GetChildren() )
		{
			if ( pCrossTiles->GetItemType() != ETIT_CROSSET_TILES_ITEM )
				continue;
			SCrossTileTypeDesc crossTileDesc;
			crossTileDesc.szName = pCrossTiles->GetDisplayName();
			for ( const auto &pTileItem : pCrossTiles->GetChildren() )
			{
				if ( pTileItem->GetItemType() != ETIT_CROSSET_TILE_PROPS_ITEM )
					continue;
				const CCrossetTilePropsItem &tile = static_cast<const CCrossetTilePropsItem &>( *pTileItem );
				RECT rc;
				CPtr<IImage> pCurrent = LoadTile( szCrossDir, tile.GetDisplayName(), pMaskImage, rc );
				if ( pCurrent == 0 )
				{
					invalidFiles.push_back( ValueStr( *pCrossets, 0 ) + tile.GetDisplayName() + ".tga" );
					break;
				}
				int nPosX, nPosY;
				CrossetAtlasPosition( tile.nCrossIndex, nPosX, nPosY );
				pCrossSetImage->CopyFromAB( pCurrent, &rc, nPosX, nPosY );
				SMainTileDesc main;
				main.fProbFrom = 0;
				main.fProbTo = ValueFloat( tile, 0 );
				main.nIndex = tile.nCrossIndex;
				crossTileDesc.tiles.push_back( main );
			}
			crossDesc.tiles.push_back( crossTileDesc );
		}
		crossSetDesc.crosses.push_back( crossDesc );
	}
	for ( const std::string &szMissing : invalidFiles )
		outcome.warnings.push_back( "crosset: cannot open " + szMissing + "; the rest of its group is left out and the export goes on" );

	// The whiteout pass: every pixel's colour set to white, alpha kept. MFC
	// indexed the rows with a stride of 256, which is the atlas width.
	{
		SColor *p = pCrossSetImage->GetLFB();
		const int nX = pCrossSetImage->GetSizeX();
		const int nY = pCrossSetImage->GetSizeY();
		for ( int y = 0; y < nY; y++ )
			for ( int x = 0; x < nX; x++ )
				p[y * 256 + x].r = p[y * 256 + x].g = p[y * 256 + x].b = 255;
	}

	FillTileMaps( 256, nCrossSizeY, crossSetDesc.tilemaps );
	const std::string szCrossBase = DirectoryOf( szFile ) + "crosset";
	if ( !context.bStatsOnly && !NImageExport::SaveCompressedTexture( context, pCrossSetImage, szCrossBase, gamma, GFXPF_DXT5, GFXPF_ARGB4444, outcome ) )
		return false;
	return WriteStats( context, DirectoryOf( szFile ) + "crosset.xml", [&]( IDataTree *pDT )
	{
		CTreeAccessor tree = pDT;
		tree.Add( "crosset", &crossSetDesc );
	}, outcome );
}

}
