#include "ai_tiles.h"

#include <cstdlib>

#include "../mfc_value.h"

namespace NResourceModel
{

void ReadTiles( const NResourceXml::Node &list, CListOfTiles &tiles )
{
	tiles.clear();
	for ( const auto &entry : list.children )
	{
		if ( entry.kind != NResourceXml::Node::Element || entry.name != "item" )
			continue;
		SAITile tile;
		if ( const std::string *v = FindAttr( entry, "x" ) )
			tile.nTileX = (int)std::strtol( v->c_str(), nullptr, 0 );
		if ( const std::string *v = FindAttr( entry, "y" ) )
			tile.nTileY = (int)std::strtol( v->c_str(), nullptr, 0 );
		if ( const std::string *v = FindAttr( entry, "val" ) )
			tile.nVal = (int)std::strtol( v->c_str(), nullptr, 0 );
		tiles.push_back( tile );
	}
}

NResourceXml::Node TilesElement( const std::string &name, const CListOfTiles &tiles )
{
	NResourceXml::Node list;
	list.kind = NResourceXml::Node::Element;
	list.name = name;
	for ( const SAITile &tile : tiles )
	{
		NResourceXml::Node entry;
		entry.kind = NResourceXml::Node::Element;
		entry.name = "item";
		SetAttr( entry, "x", MfcInt( tile.nTileX ) );
		SetAttr( entry, "y", MfcInt( tile.nTileY ) );
		SetAttr( entry, "val", MfcInt( tile.nVal ) );
		list.children.push_back( std::move( entry ) );
	}
	return list;
}

void SetTileInListOfTiles( CListOfTiles &tiles, int nTileX, int nTileY, int nVal )
{
	for ( CListOfTiles::iterator it = tiles.begin(); it != tiles.end(); ++it )
	{
		if ( it->nTileX == nTileX && it->nTileY == nTileY )
		{
			if ( nVal == 0 )
				tiles.erase( it );
			else
				it->nVal = nVal;
			return;
		}
	}
	if ( nVal == 0 )
		return;
	SAITile tile;
	tile.nTileX = nTileX;
	tile.nTileY = nTileY;
	tile.nVal = nVal;
	tiles.push_back( tile );
}

void SetTileInListOfNormalTiles( CListOfNormalTiles &tiles, int nTileX, int nTileY, int nVal )
{
	for ( SAINormalTile &tile : tiles )
	{
		if ( tile.nTileX == nTileX && tile.nTileY == nTileY )
		{
			tile.nVal = nVal;
			return;
		}
	}
	SAINormalTile tile;
	tile.nTileX = nTileX;
	tile.nTileY = nTileY;
	tile.nVal = nVal;
	tiles.push_back( tile );
}

void DeleteTileInListOfNormalTiles( CListOfNormalTiles &tiles, int nTileX, int nTileY )
{
	for ( CListOfNormalTiles::iterator it = tiles.begin(); it != tiles.end(); ++it )
	{
		if ( it->nTileX == nTileX && it->nTileY == nTileY )
		{
			tiles.erase( it );
			return;
		}
	}
}

}
