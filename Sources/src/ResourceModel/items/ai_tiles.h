#pragma once
// SAITile and CListOfTiles from Sources/src/editor/GridFrm.h: the AI passability
// tiles the Fence and Bridge editors draw on their grid and store in the
// project (CFencePropsItem, CBridgePartsItem). The port keeps the data and
// drops pVertices, the tile's quad in the editor view.
//
// SAITile::operator& (GridFrm.cpp) writes x, y and val as attributes; a list
// is DTHelper's container, one <item> per tile. MFC's LoadMyData runs
// SetTileInListOfTiles over a read list to rebuild the quads, which would
// also drop a tile with val 0; MFC never stores one, so the port keeps the
// list as read.

#include <list>
#include <string>

#include "../xml.h"

namespace NResourceModel
{

struct SAITile
{
	int nTileX = 0;
	int nTileY = 0;
	int nVal = 0;
};
using CListOfTiles = std::list<SAITile>;

// SAINormalTile from GridFrm.h: a one-way transparency tile. nVal is the line
// normal direction 0..15 (sixteenths of a half turn, see GridProjection).
// MFC's operator& writes x, y and val like SAITile, so the XML shape is the same.
struct SAINormalTile
{
	int nTileX = 0;
	int nTileY = 0;
	int nVal = 0;
};
using CListOfNormalTiles = std::list<SAINormalTile>;

// CGridFrame::SetTileInListOfTiles without the quad: sets the tile's value, and
// a value of 0 erases it, so a list never holds a 0 tile after an edit.
void SetTileInListOfTiles( CListOfTiles &tiles, int nTileX, int nTileY, int nVal );
// CGridFrame::SetTileInListOfNormalTiles: updates the tile if present, else appends.
void SetTileInListOfNormalTiles( CListOfNormalTiles &tiles, int nTileX, int nTileY, int nVal );
// CGridFrame::DeleteTileInListOfNormalTiles.
void DeleteTileInListOfNormalTiles( CListOfNormalTiles &tiles, int nTileX, int nTileY );

void ReadTiles( const NResourceXml::Node &list, CListOfTiles &tiles );
NResourceXml::Node TilesElement( const std::string &name, const CListOfTiles &tiles );

}
