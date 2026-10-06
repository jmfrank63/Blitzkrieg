#pragma once
// TileSet (with crossets) sub-editor - project extension .til, project XML root tag
// "TileSet_Composer_Project". MFC source: Sources/src/editor/TileTreeItem.{h,cpp}.
// Each class is the MFC class of the same name without its UI: the
// constructor sets the same flags and calls InitDefaultValues, whose body is
// the MFC one line for line; key, mouse and export handlers stay with the
// editor. Classes MFC declares without an InitDefaultValues of their own keep
// the base's empty one.

#include "../stats_item.h"
#include "../tree_item_types.h"

namespace NResourceModel
{

class CTileSetTreeRootItem : public CStatsItem
{
public:
	CTileSetTreeRootItem() : CStatsItem( ETIT_TILESET_ROOT_ITEM, "TileSet_Composer_Project" ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CTileSetCommonPropsItem : public CStatsItem
{
public:
	CTileSetCommonPropsItem() : CStatsItem( ETIT_TILESET_COMMON_PROPS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CTileSetTerrainsItem : public CStatsItem
{
public:
	CTileSetTerrainsItem() : CStatsItem( ETIT_TILESET_TERRAINS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CTileSetTerrainPropsItem : public CStatsItem
{
public:
	CTileSetTerrainPropsItem() : CStatsItem( ETIT_TILESET_TERRAIN_PROPS_ITEM ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CTileSetTilesItem : public CStatsItem
{
public:
	CTileSetTilesItem() : CStatsItem( ETIT_TILESET_TILES_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CTileSetASoundsItem : public CStatsItem
{
public:
	CTileSetASoundsItem() : CStatsItem( ETIT_TILESET_ASOUNDS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CTileSetASoundPropsItem : public CStatsItem
{
public:
	CTileSetASoundPropsItem() : CStatsItem( ETIT_TILESET_ASOUND_PROPS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CTileSetLSoundsItem : public CStatsItem
{
public:
	CTileSetLSoundsItem() : CStatsItem( ETIT_TILESET_LSOUNDS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CTileSetLSoundPropsItem : public CStatsItem
{
public:
	CTileSetLSoundPropsItem() : CStatsItem( ETIT_TILESET_LSOUND_PROPS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CTileSetTilePropsItem : public CStatsItem
{
public:
	CTileSetTilePropsItem() : CStatsItem( ETIT_TILESET_TILE_PROPS_ITEM ) { InitDefaultValues(); }

	int nTileIndex = -1;	// the tile's place in the tileset, which the export assigns

protected:
	void ReadData( const NResourceXml::Node &node ) override;
	void WriteData( NResourceXml::Node &node ) const override;
	bool OwnsField( const std::string &name ) const override { return name == "TileIndex" || CStatsItem::OwnsField( name ); }
	void InitDefaultValues() override;
};

class CCrossetsItem : public CStatsItem
{
public:
	CCrossetsItem() : CStatsItem( ETIT_CROSSETS_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CCrossetPropsItem : public CStatsItem
{
public:
	CCrossetPropsItem() : CStatsItem( ETIT_CROSSET_PROPS_ITEM ) { bStaticElements = true; InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CCrossetTilesItem : public CStatsItem
{
public:
	CCrossetTilesItem() : CStatsItem( ETIT_CROSSET_TILES_ITEM ) { InitDefaultValues(); }

protected:
	void InitDefaultValues() override;
};

class CCrossetTilePropsItem : public CStatsItem
{
public:
	CCrossetTilePropsItem() : CStatsItem( ETIT_CROSSET_TILE_PROPS_ITEM ) { InitDefaultValues(); }

	int nCrossIndex = -1;	// the crosset tile's index, which the export assigns

protected:
	void ReadData( const NResourceXml::Node &node ) override;
	void WriteData( NResourceXml::Node &node ) const override;
	bool OwnsField( const std::string &name ) const override { return name == "CrossIndex" || CStatsItem::OwnsField( name ); }
	void InitDefaultValues() override;
};

// CTileSetFrame::InitFreeTerrainIndexes / InitFreeCrossetIndexes and the
// Get/RemoveFree*Index pools (TileSetFrm.cpp:332-400, 968-1011). MFC kept a
// list of the free indexes in the frame: the gaps below the highest index in
// use, then one open-ended "next" entry. Getting an index took the front of
// the list (the lowest gap, else the next one up) and removing a tile put its
// index back in order, so the pool always handed out the lowest index no tile
// uses. The port derives that from the tree, which is the one place the
// indexes live: nothing to keep in step through undo, redo and reload, and
// the same answer as the list.
//
// Gives every tile props item that still has index -1 (a project from before
// the indexes were stored) the running count of tiles, as the Init pass did;
// returns how many got one. Terrain tiles and crosset tiles count apart.
int AssignMissingTileIndexes( CTreeItem &root );
// The index a new CTileSetTilePropsItem / CCrossetTilePropsItem takes in the
// project whose root is given: the lowest non-negative index no tile has.
int GetFreeTerrainIndex( const CTreeItem &root );
int GetFreeCrossetIndex( const CTreeItem &root );

}
