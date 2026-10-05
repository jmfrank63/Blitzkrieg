#include "grid_projection.h"

#include <cmath>
#include <cstdlib>

namespace NResourceModel
{

// GridFrm.cpp: "do not change, measured for compatibility with old projects".
static const float fOX = -622;
static const float fOY = 296;
// Formats/fmtTerrain.h.
static const float fCellSizeY = 16;
static const float fCellSizeX = fCellSizeY * 2.0f;
static const float kPi = 3.14159265f;

// The MFC code spells these out per call; they are the same three numbers.
static float Alpha() { return std::asin( 1.0f / std::sqrt( 5.0f ) ); }
static float TileRealSize() { return std::sqrt( ( fCellSizeX / 2 ) * ( fCellSizeX / 2 ) + ( fCellSizeY / 2 ) * ( fCellSizeY / 2 ) ); }

void GridProjection::ScreenToTileF( int screenX, int screenY, float &tileX, float &tileY )
{
	float OM = std::sqrt( ( fOX - screenX ) * ( fOX - screenX ) + ( fOY - screenY ) * ( fOY - screenY ) );
	float alpha = Alpha();
	float beta = std::atan2( (float)screenY - fOY, (float)screenX - fOX );
	float fTemp = OM / std::sin( 2 * alpha );
	float OC = fTemp * std::sin( alpha + beta );
	float CM = fTemp * std::sin( alpha - beta );
	float size = TileRealSize();
	tileX = OC / size;
	tileY = CM / size;
}

void GridProjection::ScreenToTile( int screenX, int screenY, int &tileX, int &tileY )
{
	float fx, fy;
	ScreenToTileF( screenX, screenY, fx, fy );
	tileX = (int)fx;
	tileY = (int)fy;
}

STileCorners GridProjection::TileCorners( int tileX, int tileY )
{
	float alpha = Alpha();
	float size = TileRealSize();
	float c = std::cos( alpha ), s = std::sin( alpha );
	STileCorners r;
	r.c2.x = fOX + size * tileX * c + size * tileY * c;
	r.c2.y = fOY + size * tileX * s - size * tileY * s;
	r.c3.x = fOX + size * ( tileX + 1 ) * c + size * tileY * c;
	r.c3.y = fOY + size * ( tileX + 1 ) * s - size * tileY * s;
	r.c4.x = fOX + size * ( tileX + 1 ) * c + size * ( tileY + 1 ) * c;
	r.c4.y = fOY + size * ( tileX + 1 ) * s - size * ( tileY + 1 ) * s;
	r.c1.x = fOX + size * tileX * c + size * ( tileY + 1 ) * c;
	r.c1.y = fOY + size * tileX * s - size * ( tileY + 1 ) * s;
	return r;
}

SVec2 GridProjection::Pos3To2( const SVec3 &p ) const
{
	SVec2 r;
	r.x = camera_.m11 * p.x + camera_.m12 * p.y + camera_.m13 * p.z + camera_.m14;
	r.y = camera_.m21 * p.x + camera_.m22 * p.y + camera_.m23 * p.z + camera_.m24;
	return r;
}

SVec3 GridProjection::Pos2To3( const SVec2 &p ) const
{
	const SGroundCamera &m = camera_;
	float det = m.m11 * m.m22 - m.m12 * m.m21;
	SVec3 r;
	r.x = ( m.m12 * m.m24 - m.m12 * p.y - m.m14 * m.m22 + p.x * m.m22 ) / det;
	r.y = -( m.m11 * m.m24 - m.m11 * p.y - m.m14 * m.m21 + p.x * m.m21 ) / det;
	r.z = 0;
	return r;
}

SVec3 GridProjection::OriginOfGrid( const SVec3 &zeroPos3, int minTileX, int minTileY ) const
{
	SVec3 mostLeft3 = Pos2To3( TileCorners( minTileX, minTileY ).c2 );
	SVec3 r;
	r.x = zeroPos3.x - mostLeft3.x;
	r.y = zeroPos3.y - mostLeft3.y;
	return r;
}

void GridProjection::FirstTileOfGrid( const SVec3 &zeroPos3, const SVec3 &origin, int &tileX, int &tileY ) const
{
	SVec3 begin3;
	begin3.x = zeroPos3.x - origin.x;
	begin3.y = zeroPos3.y - origin.y;
	begin3.z = 0;
	SVec2 pos2 = Pos3To2( begin3 );
	ScreenToTile( (int)( pos2.x + fCellSizeX / 2 ), (int)pos2.y, tileX, tileY );
}

int GridProjection::NormalOfLine( const STransLine &line ) const
{
	SVec3 v1 = Pos2To3( line.p1 );
	SVec3 v2 = Pos2To3( line.p2 );
	float alpha = std::atan2( v2.y - v1.y, v2.x - v1.x );
	alpha += kPi / 16.0f;
	if ( alpha > kPi )
		alpha -= 2 * kPi;
	int nRes = (int)( (float)( kPi + alpha ) * 8 / kPi );
	return ( nRes + 8 ) % 16;
}

void GridProjection::DirTilesFromTransLines( const std::vector<STransLine> &lines, CListOfNormalTiles &dirTiles ) const
{
	for ( const STransLine &line : lines )
	{
		int nRes = NormalOfLine( line );
		float ftx1, fty1, ftx2, fty2;
		ScreenToTileF( (int)line.p1.x, (int)line.p1.y, ftx1, fty1 );
		ScreenToTileF( (int)line.p2.x, (int)line.p2.y, ftx2, fty2 );
		std::list<std::pair<int, int>> coords;
		FillBresenham( (int)ftx1, (int)fty1, (int)ftx2, (int)fty2, coords );
		for ( const auto &c : coords )
			SetTileInListOfNormalTiles( dirTiles, c.first, c.second, nRes );
	}
}

void FillBresenham( int x1, int y1, int x2, int y2, std::list<std::pair<int, int>> &tiles )
{
	int dx = std::abs( x2 - x1 );
	int dy = std::abs( y2 - y1 );
	int inc1, inc2, d, x, y, xend, yend, s;

	if ( dx > dy )
	{
		inc1 = dy * 2;
		inc2 = ( dy - dx ) * 2;
		d = 2 * dy - dx;
		if ( x2 > x1 )
		{
			x = x1; y = y1; xend = x2;
			s = y1 < y2 ? 1 : -1;
		}
		else
		{
			x = x2; y = y2; xend = x1;
			s = y2 < y1 ? 1 : -1;
		}
		tiles.push_back( { x, y } );
		while ( x < xend )
		{
			if ( d > 0 )
			{
				y += s;
				d += inc2;
				tiles.push_back( { x, y } );
			}
			else
				d += inc1;
			x++;
			tiles.push_back( { x, y } );
		}
	}
	else
	{
		inc1 = dx * 2;
		inc2 = ( dx - dy ) * 2;
		d = 2 * dx - dy;
		if ( y2 > y1 )
		{
			x = x1; y = y1; yend = y2;
			s = x1 < x2 ? 1 : -1;
		}
		else
		{
			x = x2; y = y2; yend = y1;
			s = x2 < x1 ? 1 : -1;
		}
		tiles.push_back( { x, y } );
		while ( y < yend )
		{
			if ( d > 0 )
			{
				x += s;
				d += inc2;
				tiles.push_back( { x, y } );
			}
			else
				d += inc1;
			y++;
			tiles.push_back( { x, y } );
		}
	}
}

namespace
{
struct Box
{
	int minX, maxX, minY, maxY;
	explicit Box( int x, int y ) : minX( x ), maxX( x ), minY( y ), maxY( y ) {}
	void Add( int x, int y )
	{
		if ( x < minX ) minX = x;
		if ( x > maxX ) maxX = x;
		if ( y < minY ) minY = y;
		if ( y > maxY ) maxY = y;
	}
};

STileGrid MakeGrid( const Box &box )
{
	STileGrid g;
	g.sizeX = box.maxX - box.minX + 1;
	g.sizeY = box.maxY - box.minY + 1;
	g.minTileX = box.minX;
	g.minTileY = box.minY;
	g.data.assign( (size_t)g.sizeX * g.sizeY, 0 );
	return g;
}
}

STileGrid TilesToGrid( const CListOfTiles &tiles )
{
	if ( tiles.empty() )
		return STileGrid();
	Box box( tiles.front().nTileX, tiles.front().nTileY );
	for ( const SAITile &t : tiles )
		box.Add( t.nTileX, t.nTileY );
	STileGrid g = MakeGrid( box );
	// MFC scans the list per cell and takes the first match; filling in reverse
	// order gives the same cell for a list that (wrongly) repeats a tile.
	for ( CListOfTiles::const_reverse_iterator it = tiles.rbegin(); it != tiles.rend(); ++it )
		g.data[(size_t)( it->nTileX - box.minX ) + (size_t)( it->nTileY - box.minY ) * g.sizeX] = (unsigned char)it->nVal;
	return g;
}

STileGrid TilesToVisGrid( const CListOfTiles &transparences, const CListOfNormalTiles &dirTiles )
{
	if ( transparences.empty() && dirTiles.empty() )
		return STileGrid();
	Box box = transparences.empty() ? Box( dirTiles.front().nTileX, dirTiles.front().nTileY )
	                                : Box( transparences.front().nTileX, transparences.front().nTileY );
	for ( const SAITile &t : transparences )
		box.Add( t.nTileX, t.nTileY );
	for ( const SAINormalTile &t : dirTiles )
		box.Add( t.nTileX, t.nTileY );
	STileGrid g = MakeGrid( box );
	for ( CListOfTiles::const_reverse_iterator it = transparences.rbegin(); it != transparences.rend(); ++it )
		g.data[(size_t)( it->nTileX - box.minX ) + (size_t)( it->nTileY - box.minY ) * g.sizeX] = (unsigned char)it->nVal;
	for ( CListOfNormalTiles::const_reverse_iterator it = dirTiles.rbegin(); it != dirTiles.rend(); ++it )
		g.data[(size_t)( it->nTileX - box.minX ) + (size_t)( it->nTileY - box.minY ) * g.sizeX] = (unsigned char)( ( it->nVal << 4 ) | 0x08 );
	return g;
}

void GridToTiles( const STileGrid &grid, int firstTileX, int firstTileY, CListOfTiles &tiles )
{
	for ( int y = 0; y < grid.sizeY; ++y )
		for ( int x = 0; x < grid.sizeX; ++x )
			if ( int v = grid.data[(size_t)x + (size_t)y * grid.sizeX] )
				SetTileInListOfTiles( tiles, firstTileX + x, firstTileY + y, v );
}

void VisGridToTiles( const STileGrid &grid, int firstTileX, int firstTileY, CListOfTiles &transparences, CListOfNormalTiles &dirTiles )
{
	for ( int y = 0; y < grid.sizeY; ++y )
		for ( int x = 0; x < grid.sizeX; ++x )
		{
			int v = grid.data[(size_t)x + (size_t)y * grid.sizeX];
			if ( v & 0x07 )
				SetTileInListOfTiles( transparences, firstTileX + x, firstTileY + y, v & 0x07 );
			else if ( v & 0x08 )
				SetTileInListOfNormalTiles( dirTiles, firstTileX + x, firstTileY + y, v >> 4 );
		}
}

}
