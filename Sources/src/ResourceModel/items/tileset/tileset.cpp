#include "tileset.h"

#include "../../mfc_value.h"

#include "../../editor_env.h"

namespace NResourceModel
{

void CTileSetTreeRootItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;

	defaultChilds.clear();

	SChildItem child;

	child.nChildItemType = ETIT_TILESET_COMMON_PROPS_ITEM;
	child.szDefaultName = "Basic Info";
	child.szDisplayName = "Basic Info";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_TILESET_TERRAINS_ITEM;
	child.szDefaultName = "Terrains";
	child.szDisplayName = "Terrains";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_CROSSETS_ITEM;
	child.szDefaultName = "Crossets";
	child.szDisplayName = "Crossets";
	defaultChilds.push_back( child );
}

void CTileSetCommonPropsItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_STR;
	prop.szDefaultName = "Name";
	prop.szDisplayName = "Name";
	prop.value = "Unknown Tile Set";
	defaultValues.push_back( prop );

	values = defaultValues;
}

void CTileSetTerrainsItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_BROWSEDIR;
	prop.szDefaultName = "Tiles directory";
	prop.szDisplayName = "Tiles directory";
	prop.value = "";
	defaultValues.push_back( prop );

	values = defaultValues;
}

void CTileSetTerrainPropsItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_STR;
	prop.szDefaultName = "Name";
	prop.szDisplayName = "Name";
	prop.value = "Unknown terrain";
	defaultValues.push_back( prop );

	prop.nId = 2;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Crosset number";
	prop.szDisplayName = "Crosset number";
	prop.value = 1;
	defaultValues.push_back( prop );

	prop.nId = 3;
	prop.nDomenType = DT_DEC;
	prop.szDefaultName = "Mask priority";
	prop.szDisplayName = "Mask priority";
	prop.value = 3;
	defaultValues.push_back( prop );

	prop.nId = 4;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Passability coefficient";
	prop.szDisplayName = "Passability coefficient";
	prop.value = 1.0f;
	defaultValues.push_back( prop );

	prop.nId = 5;
	prop.nDomenType = DT_BOOL;
	prop.szDefaultName = "Passability for infantry";
	prop.szDisplayName = "Passability for infantry";
	prop.value = true;
	defaultValues.push_back( prop );

	prop.nId = 6;
	prop.nDomenType = DT_BOOL;
	prop.szDefaultName = "Passability for wheels";
	prop.szDisplayName = "Passability for wheels";
	prop.value = true;
	defaultValues.push_back( prop );

	prop.nId = 7;
	prop.nDomenType = DT_BOOL;
	prop.szDefaultName = "Passability for halftracks";
	prop.szDisplayName = "Passability for halftracks";
	prop.value = true;
	defaultValues.push_back( prop );

	prop.nId = 8;
	prop.nDomenType = DT_BOOL;
	prop.szDefaultName = "Passability for tracks";
	prop.szDisplayName = "Passability for tracks";
	prop.value = true;
	defaultValues.push_back( prop );

	prop.nId = 9;
	prop.nDomenType = DT_BOOL;
	prop.szDefaultName = "Has microtexture?";
	prop.szDisplayName = "Has microtexture?";
	prop.value = true;
	defaultValues.push_back( prop );

	prop.nId = 10;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Sound volume";
	prop.szDisplayName = "Sound volume";
	prop.value = 1.0f;
	defaultValues.push_back( prop );

	prop.nId = 11;
	prop.nDomenType = DT_SOUND_REF;
	prop.szDefaultName = "Sound";
	prop.szDisplayName = "Sound";
	prop.value = "";
	defaultValues.push_back( prop );

	prop.nId = 12;
	prop.nDomenType = DT_SOUND_REF;
	prop.szDefaultName = "Looped sound";
	prop.szDisplayName = "Looped sound";
	prop.value = "";
	defaultValues.push_back( prop );

	prop.nId = 13;
	prop.nDomenType = DT_BOOL;
	prop.szDefaultName = "Can build entrenchment?";
	prop.szDisplayName = "Can build entrenchment?";
	prop.value = true;
	defaultValues.push_back( prop );

	prop.nId = 14;
	prop.nDomenType = DT_BOOL;
	prop.szDefaultName = "Is water?";
	prop.szDisplayName = "Is water?";
	prop.value = false;
	defaultValues.push_back( prop );

	prop.nId = 15;
	prop.nDomenType = DT_BOOL;
	prop.szDefaultName = "Leave tracks flag";
	prop.szDisplayName = "Leave tracks flag";
	prop.value = false;
	defaultValues.push_back( prop );

	prop.nId = 16;
	prop.nDomenType = DT_BOOL;
	prop.szDefaultName = "Dust flag";
	prop.szDisplayName = "Dust flag";
	prop.value = false;
	defaultValues.push_back( prop );

	values = defaultValues;

	SChildItem child;

	child.nChildItemType = ETIT_TILESET_TILES_ITEM;
	child.szDefaultName = "Tiles";
	child.szDisplayName = "Tiles";
	defaultChilds.push_back( child );
}

void CTileSetTilesItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;
}

void CTileSetASoundsItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;
}

void CTileSetASoundPropsItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Sound";
	prop.szDisplayName = "Sound";
	prop.value = "";
	prop.szStrings.push_back( GetEditorDataDir() + "sounds\\" );
	prop.szStrings.push_back( szSoundFilter );
	defaultValues.push_back( prop );
	prop.szStrings.clear();

	prop.nId = 2;
	prop.nDomenType = DT_BOOL;
	prop.szDefaultName = "Peaceful flag";
	prop.szDisplayName = "Peaceful flag";
	prop.value = true;
	defaultValues.push_back( prop );

	prop.nId = 3;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Probability";
	prop.szDisplayName = "Probability";
	prop.value = 15.0f;
	defaultValues.push_back( prop );

	values = defaultValues;
}

void CTileSetLSoundsItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;
}

void CTileSetLSoundPropsItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_BROWSE;
	prop.szDefaultName = "Sound";
	prop.szDisplayName = "Sound";
	prop.value = "";
	prop.szStrings.push_back( GetEditorDataDir() + "sounds\\" );
	prop.szStrings.push_back( szSoundFilter );
	defaultValues.push_back( prop );
	prop.szStrings.clear();

	prop.nId = 2;
	prop.nDomenType = DT_BOOL;
	prop.szDefaultName = "Peaceful flag";
	prop.szDisplayName = "Peaceful flag";
	prop.value = true;
	defaultValues.push_back( prop );

	values = defaultValues;
}

void CTileSetTilePropsItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Probability";
	prop.szDisplayName = "Probability";
	prop.value = 25.0f;
	defaultValues.push_back( prop );

	prop.nId = 2;
	prop.nDomenType = DT_COMBO;
	prop.szDefaultName = "Flipped state";
	prop.szDisplayName = "Flipped state";
	prop.value = "normal and flipped";
	prop.szStrings.push_back( "normal and flipped" );
	prop.szStrings.push_back( "normal" );
	prop.szStrings.push_back( "flipped" );
	defaultValues.push_back( prop );

	values = defaultValues;
}

void CCrossetsItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_BROWSEDIR;
	prop.szDefaultName = "Crossets directory";
	prop.szDisplayName = "Crossets directory";
	prop.value = "";
	defaultValues.push_back( prop );

	values = defaultValues;
}

void CCrossetPropsItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_STR;
	prop.szDefaultName = "Name";
	prop.szDisplayName = "Name";
	prop.value = "Unknown crosset";
	defaultValues.push_back( prop );

	values = defaultValues;

	defaultChilds.clear();
	SChildItem child;

	child.nChildItemType = ETIT_CROSSET_TILES_ITEM;
	child.szDefaultName = "a";
	child.szDisplayName = "a";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_CROSSET_TILES_ITEM;
	child.szDefaultName = "b";
	child.szDisplayName = "b";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_CROSSET_TILES_ITEM;
	child.szDefaultName = "c";
	child.szDisplayName = "c";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_CROSSET_TILES_ITEM;
	child.szDefaultName = "d";
	child.szDisplayName = "d";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_CROSSET_TILES_ITEM;
	child.szDefaultName = "e";
	child.szDisplayName = "e";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_CROSSET_TILES_ITEM;
	child.szDefaultName = "f";
	child.szDisplayName = "f";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_CROSSET_TILES_ITEM;
	child.szDefaultName = "a'";
	child.szDisplayName = "a'";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_CROSSET_TILES_ITEM;
	child.szDefaultName = "b'";
	child.szDisplayName = "b'";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_CROSSET_TILES_ITEM;
	child.szDefaultName = "c'";
	child.szDisplayName = "c'";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_CROSSET_TILES_ITEM;
	child.szDefaultName = "d'";
	child.szDisplayName = "d'";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_CROSSET_TILES_ITEM;
	child.szDefaultName = "e'";
	child.szDisplayName = "e'";
	defaultChilds.push_back( child );

	child.nChildItemType = ETIT_CROSSET_TILES_ITEM;
	child.szDefaultName = "f'";
	child.szDisplayName = "f'";
	defaultChilds.push_back( child );
}

void CCrossetTilesItem::InitDefaultValues()
{
	defaultValues.clear();
	values = defaultValues;
}

void CCrossetTilePropsItem::InitDefaultValues()
{
	defaultValues.clear();
	SProp prop;

	prop.nId = 1;
	prop.nDomenType = DT_FLOAT;
	prop.szDefaultName = "Probability";
	prop.szDisplayName = "Probability";
	prop.value = 25.0f;
	defaultValues.push_back( prop );

	values = defaultValues;
}

// CTileSetTilePropsItem::operator&: the base fields, then TileIndex.
void CTileSetTilePropsItem::ReadData( const NResourceXml::Node &node )
{
	CStatsItem::ReadData( node );
	ReadInt( node, "TileIndex", nTileIndex );
}

void CTileSetTilePropsItem::WriteData( NResourceXml::Node &node ) const
{
	CStatsItem::WriteData( node );
	SetAttr( node, "TileIndex", MfcInt( nTileIndex ) );
}

// CCrossetTilePropsItem::operator&: the base fields, then CrossIndex.
void CCrossetTilePropsItem::ReadData( const NResourceXml::Node &node )
{
	CStatsItem::ReadData( node );
	ReadInt( node, "CrossIndex", nCrossIndex );
}

void CCrossetTilePropsItem::WriteData( NResourceXml::Node &node ) const
{
	CStatsItem::WriteData( node );
	SetAttr( node, "CrossIndex", MfcInt( nCrossIndex ) );
}

}
