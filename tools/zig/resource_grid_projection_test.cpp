// S09 T01 test for Sources/src/ResourceModel/grid_projection.* and the tile list
// helpers of items/ai_tiles.*. Headless: no window, no engine. Expected values
// are computed by hand from the MFC formulas in GridFrm.cpp: with fOX=-622,
// fOY=296 and cells of 32x16 the leftmost corner of tile (tx, ty) is
// (-622 + 16(tx+ty), 296 + 8(tx-ty)). Each failing check prints expected and actual.

#include <cmath>
#include <cstdio>
#include <set>
#include <tuple>

#include "../../Sources/src/ResourceModel/grid_projection.h"

using namespace NResourceModel;

static int g_fail = 0;
#define CHECK( c ) do { if ( !( c ) ) { std::fprintf( stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c ); ++g_fail; } } while ( 0 )
static void CheckNear( const char *what, float expected, float actual, float tol = 1e-2f )
{
	if ( std::fabs( expected - actual ) > tol )
	{
		std::fprintf( stderr, "FAIL %s expected %f actual %f\n", what, expected, actual );
		++g_fail;
	}
}
static void CheckInt( const char *what, int expected, int actual )
{
	if ( expected != actual )
	{
		std::fprintf( stderr, "FAIL %s expected %d actual %d\n", what, expected, actual );
		++g_fail;
	}
}

static std::set<std::tuple<int, int, int>> AsSet( const CListOfTiles &l )
{
	std::set<std::tuple<int, int, int>> s;
	for ( const SAITile &t : l ) s.insert( { t.nTileX, t.nTileY, t.nVal } );
	return s;
}
static std::set<std::tuple<int, int, int>> AsSet( const CListOfNormalTiles &l )
{
	std::set<std::tuple<int, int, int>> s;
	for ( const SAINormalTile &t : l ) s.insert( { t.nTileX, t.nTileY, t.nVal } );
	return s;
}

int main()
{
	// Three tile / screen pairs by hand.
	const int hand[3][4] = { { 0, 0, -622, 296 }, { 3, 1, -558, 312 }, { 10, 4, -398, 344 } };
	for ( const auto &h : hand )
	{
		STileCorners c = GridProjection::TileCorners( h[0], h[1] );
		CheckNear( "corner2.x", (float)h[2], c.c2.x );
		CheckNear( "corner2.y", (float)h[3], c.c2.y );
		CheckNear( "corner4.x", (float)h[2] + 32, c.c4.x );
		CheckNear( "corner4.y", (float)h[3], c.c4.y );
		CheckNear( "corner3.x", (float)h[2] + 16, c.c3.x );
		CheckNear( "corner3.y", (float)h[3] + 8, c.c3.y );
		CheckNear( "corner1.x", (float)h[2] + 16, c.c1.x );
		CheckNear( "corner1.y", (float)h[3] - 8, c.c1.y );
		float fx, fy;
		GridProjection::ScreenToTileF( h[2] + 16, h[3], fx, fy );
		CheckNear( "centre tileX", h[0] + 0.5f, fx, 0.05f );
		CheckNear( "centre tileY", h[1] + 0.5f, fy, 0.05f );
	}

	// tile -> corners -> tile over the whole 60x60 grid.
	int bad = 0;
	for ( int ty = 0; ty < 60; ++ty )
		for ( int tx = 0; tx < 60; ++tx )
		{
			STileCorners c = GridProjection::TileCorners( tx, ty );
			int rx, ry;
			GridProjection::ScreenToTile( (int)std::lround( ( c.c2.x + c.c4.x ) / 2 ), (int)std::lround( ( c.c2.y + c.c4.y ) / 2 ), rx, ry );
			if ( rx != tx || ry != ty ) ++bad;
		}
	CheckInt( "60x60 round-trip mismatches", 0, bad );

	// Camera: Pos2To3 inverts Pos3To2 on the ground plane.
	SGroundCamera cam;
	cam.m11 = 2; cam.m12 = 0.5f; cam.m14 = 10; cam.m21 = -0.25f; cam.m22 = 1.5f; cam.m24 = -3;
	GridProjection proj( cam );
	SVec3 w; w.x = 7; w.y = -4;
	SVec3 back = proj.Pos2To3( proj.Pos3To2( w ) );
	CheckNear( "camera round trip x", 7, back.x, 1e-3f );
	CheckNear( "camera round trip y", -4, back.y, 1e-3f );

	// Bounding box + origin on a hand case with a non-zero origin (identity camera).
	GridProjection ident;
	CListOfTiles locked = { { 5, 2, 1 }, { 7, 3, 2 }, { 6, 2, 1 } };
	STileGrid g = TilesToGrid( locked );
	CheckInt( "grid sizeX", 3, g.sizeX );
	CheckInt( "grid sizeY", 2, g.sizeY );
	CheckInt( "grid minX", 5, g.minTileX );
	CheckInt( "grid minY", 2, g.minTileY );
	CHECK( g.data == std::vector<unsigned char>( { 1, 1, 0, 0, 0, 2 } ) );
	CListOfTiles again;
	GridToTiles( g, g.minTileX, g.minTileY, again );
	CHECK( AsSet( again ) == AsSet( locked ) );
	CHECK( TilesToGrid( CListOfTiles() ).empty() );

	SVec3 zero; zero.x = 50; zero.y = 60;
	SVec3 origin = ident.OriginOfGrid( zero, 3, 1 );  // corner2 of tile (3,1) is (-558, 312)
	CheckNear( "origin.x", 50 + 558, origin.x );
	CheckNear( "origin.y", 60 - 312, origin.y );
	int ftx, fty;
	ident.FirstTileOfGrid( zero, origin, ftx, fty );
	CheckInt( "first tile x", 3, ftx );
	CheckInt( "first tile y", 1, fty );

	// Visibility grid: transparency plus one-way tiles, one-way wins a shared cell.
	CListOfTiles trans = { { 4, 4, 3 }, { 6, 4, 7 } };
	CListOfNormalTiles dirs = { { 5, 4, 8 }, { 6, 4, 2 } };
	STileGrid vg = TilesToVisGrid( trans, dirs );
	CheckInt( "vis sizeX", 3, vg.sizeX );
	CheckInt( "vis sizeY", 1, vg.sizeY );
	CHECK( vg.data == std::vector<unsigned char>( { 3, (unsigned char)( ( 8 << 4 ) | 8 ), (unsigned char)( ( 2 << 4 ) | 8 ) } ) );
	CListOfTiles t2;
	CListOfNormalTiles d2;
	VisGridToTiles( vg, vg.minTileX, vg.minTileY, t2, d2 );
	CHECK( AsSet( t2 ) == AsSet( CListOfTiles( { { 4, 4, 3 } } ) ) );
	CHECK( AsSet( d2 ) == AsSet( dirs ) );
	CHECK( TilesToVisGrid( CListOfTiles(), CListOfNormalTiles() ).empty() );

	// List edit semantics.
	CListOfTiles l;
	SetTileInListOfTiles( l, 1, 1, 3 );
	SetTileInListOfTiles( l, 1, 1, 5 );
	CHECK( l.size() == 1 && l.front().nVal == 5 );
	SetTileInListOfTiles( l, 1, 1, 0 );
	CHECK( l.empty() );
	SetTileInListOfTiles( l, 2, 2, 0 );
	CHECK( l.empty() );
	CListOfNormalTiles n;
	SetTileInListOfNormalTiles( n, 1, 1, 0 );  // normal tiles keep value 0 (direction 0)
	CHECK( n.size() == 1 );
	DeleteTileInListOfNormalTiles( n, 1, 1 );
	CHECK( n.empty() );

	// dirTiles from a diagonal line: tile (3,3) centre to tile (0,0) centre,
	// i.e. screen (-510,296) to (-606,296). The world direction is -x, so the
	// normal is 8; Bresenham on the equal-delta pair walks 7 tiles.
	std::vector<STransLine> lines( 1 );
	lines[0].p1.x = -510; lines[0].p1.y = 296;
	lines[0].p2.x = -606; lines[0].p2.y = 296;
	CheckInt( "normal of line", 8, ident.NormalOfLine( lines[0] ) );
	CListOfNormalTiles dir;
	ident.DirTilesFromTransLines( lines, dir );
	CListOfNormalTiles want = { { 0, 0, 8 }, { 1, 0, 8 }, { 1, 1, 8 }, { 2, 1, 8 }, { 2, 2, 8 }, { 3, 2, 8 }, { 3, 3, 8 } };
	CHECK( AsSet( dir ) == AsSet( want ) );
	std::list<std::pair<int, int>> bres;
	FillBresenham( 0, 0, 5, 1, bres );
	CHECK( bres.size() == 7 && bres.front() == std::make_pair( 0, 0 ) && bres.back() == std::make_pair( 5, 1 ) );

	std::fprintf( stderr, g_fail ? "resource-grid-projection FAILED (%d)\n" : "resource-grid-projection OK\n", g_fail );
	return g_fail ? 1 : 0;
}
