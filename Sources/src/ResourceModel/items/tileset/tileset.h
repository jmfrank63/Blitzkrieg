#pragma once
// TileSet sub-editor - project extension .til, project XML root tag
// "TileSet_Composer_Project". MFC source: Sources/src/editor/TileTreeItem.{h,cpp}.
// Mirrors REGISTER_CLASS entries in Sources/src/editor/TreeItemFactory.cpp:143-156
// which also carry the E_CROSSET_* items that live alongside tilesets.

#include "../stats_item.h"
#include "../tree_item_types.h"

namespace NResourceModel
{

class CTileSetTreeRootItem : public CStatsItem
{
public:
	CTileSetTreeRootItem() : CStatsItem( ETIT_TILESET_ROOT_ITEM, "TileSet_Composer_Project" ) {}
};
class CTileSetCommonPropsItem   : public CStatsItem { public: CTileSetCommonPropsItem()   : CStatsItem( ETIT_TILESET_COMMON_PROPS_ITEM ) {} };
class CTileSetTerrainsItem      : public CStatsItem { public: CTileSetTerrainsItem()      : CStatsItem( ETIT_TILESET_TERRAINS_ITEM ) {} };
class CTileSetTerrainPropsItem  : public CStatsItem { public: CTileSetTerrainPropsItem()  : CStatsItem( ETIT_TILESET_TERRAIN_PROPS_ITEM ) {} };
class CTileSetTilePropsItem     : public CStatsItem { public: CTileSetTilePropsItem()     : CStatsItem( ETIT_TILESET_TILE_PROPS_ITEM ) {} };
class CCrossetsItem             : public CStatsItem { public: CCrossetsItem()             : CStatsItem( ETIT_CROSSETS_ITEM ) {} };
class CCrossetPropsItem         : public CStatsItem { public: CCrossetPropsItem()         : CStatsItem( ETIT_CROSSET_PROPS_ITEM ) {} };
class CCrossetTilesItem         : public CStatsItem { public: CCrossetTilesItem()         : CStatsItem( ETIT_CROSSET_TILES_ITEM ) {} };
class CCrossetTilePropsItem     : public CStatsItem { public: CCrossetTilePropsItem()     : CStatsItem( ETIT_CROSSET_TILE_PROPS_ITEM ) {} };
class CTileSetTilesItem         : public CStatsItem { public: CTileSetTilesItem()         : CStatsItem( ETIT_TILESET_TILES_ITEM ) {} };
class CTileSetASoundsItem       : public CStatsItem { public: CTileSetASoundsItem()       : CStatsItem( ETIT_TILESET_ASOUNDS_ITEM ) {} };
class CTileSetASoundPropsItem   : public CStatsItem { public: CTileSetASoundPropsItem()   : CStatsItem( ETIT_TILESET_ASOUND_PROPS_ITEM ) {} };
class CTileSetLSoundsItem       : public CStatsItem { public: CTileSetLSoundsItem()       : CStatsItem( ETIT_TILESET_LSOUNDS_ITEM ) {} };
class CTileSetLSoundPropsItem   : public CStatsItem { public: CTileSetLSoundPropsItem()   : CStatsItem( ETIT_TILESET_LSOUND_PROPS_ITEM ) {} };

}
