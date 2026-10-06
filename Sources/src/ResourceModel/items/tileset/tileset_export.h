#pragma once
// The tileset exporter: CTileSetTreeRootItem::ComposeTiles and
// CTileSetFrame::ExportFrameData (Sources/src/editor/TileTreeItem.cpp:41-415,
// TileSetFrm.cpp:434). ExportTileSet is declared in stats_export.h with the
// other exporters; the geometry of the atlases is here because the tests
// measure the pictures at the places the engine's tile maps point to.
//
// The export writes the tileset (<name>.xml, root "tileset", with
// <name>_c/_l/_h.dds beside it) and, when the project has crosset tiles,
// crosset.xml (root "crosset") with crosset_c/_l/_h.dds in the same folder.
// There is no import: CTileSetFrame::LoadRPGStats only rebuilds the index
// pools, so MFC never read a tileset back from game data.

namespace NResourceModel
{

inline const char *TileSetImportRefusal()
{
	return "importing .til from game data is refused: no reverse path in MFC (CTileSetFrame::LoadRPGStats only rebuilds index pools)";
}

// The height of the tileset atlas (256 wide) for the highest tile index in
// use: ( nMaxIndex / 7 ) rows of 32 and one half row of 16, rounded up to a
// power of two. A project without tiles has index -1 and the same formula.
int TileSetAtlasHeight( int nMaxIndex );
// The height of the crosset atlas for its tile count: ( n + 6 ) / 7 rows.
int CrossetAtlasHeight( int nCrossCount );
// Where tile nIndex of the tileset atlas sits: seven tiles to a row pair,
// four on the whole-row line and three shifted half a tile right and down.
void TileSetAtlasPosition( int nIndex, int &nPosX, int &nPosY );
// The same for a crosset tile; the crosset index counts two per tile
// (normal and flipped) like the tileset's engine index, so it halves first.
void CrossetAtlasPosition( int nCrossIndex, int &nPosX, int &nPosY );

}
