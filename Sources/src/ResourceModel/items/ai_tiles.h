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

void ReadTiles( const NResourceXml::Node &list, CListOfTiles &tiles );
NResourceXml::Node TilesElement( const std::string &name, const CListOfTiles &tiles );

}
