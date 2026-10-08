#pragma once
// The AI-tile grid math of the MFC editors, ported without a window: tile <->
// screen (CGridFrame::ComputeGameTileCoordinates / GetGameTileCoordinates,
// GridFrm.cpp), tile list <-> cropped grid plus origin (ObjectFrm.cpp
// SaveRPGStats / LoadRPGStats) and the one-way transparency lines
// (ObjectFrm.cpp UpdateNormalForSelectedLine, MyFillBrezenham). The Object,
// Fence, Building and Bridge editors all use it.
//
// The tile <-> screen part is closed form: the grid origin is fixed at
// (-622, 296) and a tile step is (16, 8) along X and (16, -8) along Y, because
// fCellSizeX/Y are 32/16 (Formats/fmtTerrain.h). The screen <-> world part is
// not constant: CScene::GetPos2 is the camera's 4x4 transform (matTransform)
// and GetPos3 inverts it on the z=0 plane (SceneInternal.cpp), so the camera
// is an explicit input, SGroundCamera, never a baked-in guess. Callers fill it
// from the transform of the camera the editor view really uses.

#include <list>
#include <utility>
#include <vector>

#include "items/ai_tiles.h"

namespace NResourceModel
{

// The affine part of CScene's matTransform (row r of the 4x4 is mR1..mR4).
// Pos3To2 is x' = m11 x + m12 y + m13 z + m14, y' = m21 x + m22 y + m23 z + m24;
// Pos2To3 solves that for z = 0 exactly as CScene::GetPos3 does with bOnZero.
struct SGroundCamera
{
	float m11 = 1, m12 = 0, m13 = 0, m14 = 0;
	float m21 = 0, m22 = 1, m23 = 0, m24 = 0;
};

struct SVec2 { float x = 0, y = 0; };
struct SVec3 { float x = 0, y = 0, z = 0; };

struct STileCorners
{
	// MFC's numbering: 2 is the tile's leftmost corner (the one origin maths
	// use), 4 the opposite one, 1 and 3 the other two.
	SVec2 c1, c2, c3, c4;
};

// A one-way transparency line in screen coordinates (CObjectFrame::STransLine).
struct STransLine
{
	SVec2 p1, p2;
};

// A tile value grid cropped to the bounding box of its tiles, row-major like
// the MFC passability / visibility arrays: cell (x, y) is data[x + y*sizeX]
// and stands for tile (minTileX + x, minTileY + y).
struct STileGrid
{
	int sizeX = 0, sizeY = 0;
	int minTileX = 0, minTileY = 0;
	std::vector<unsigned char> data;
	bool empty() const { return sizeX == 0 || sizeY == 0; }
};

// The camera the grid constants imply: world (0, 0) at the grid origin
// (-622, 296) and one world cell (fWorldCellSize) a tile step of (16, 8)
// along x and (16, -8) along y. The exporters use it only when the host gives
// no engine camera (SExportContext::groundCamera).
SGroundCamera DefaultEditorCamera();

// The camera the shipped MFC editors had while they batch-exported: the scene's
// default placement (SetDefaultCamera: distance 700, pitch -120 degrees, yaw 45
// degrees) over the editor's 800 x 600 game window (GAME_SIZE_X/Y, Specific.h),
// an orthographic projection of one world unit per pixel (MainFrm.cpp), with the
// anchor a frame sets in ShowFrameWindows snapped as CCamera::Update snaps it.
// The ground-plane part of the scene's matTransform is then closed form; the
// golden comparison feeds it to the exporters as the engine camera.
SGroundCamera MfcEditorCamera( const SVec3 &anchor );

class GridProjection
{
public:
	explicit GridProjection( const SGroundCamera &camera = SGroundCamera() ) : camera_( camera ) {}

	// ComputeGameTileCoordinates: fractional tile coordinates of a screen point.
	static void ScreenToTileF( int screenX, int screenY, float &tileX, float &tileY );
	// The int the MFC callers store: a plain C++ float-to-int cast, so it truncates toward zero.
	static void ScreenToTile( int screenX, int screenY, int &tileX, int &tileY );
	// GetGameTileCoordinates.
	static STileCorners TileCorners( int tileX, int tileY );

	// CScene::GetPos2 / GetPos3 (z = 0) through the camera.
	SVec2 Pos3To2( const SVec3 &pos ) const;
	SVec3 Pos2To3( const SVec2 &pos ) const;

	// The rule SaveRPGStats uses for vOrigin and vVisOrigin: the sprite's zero
	// position in world space minus the world position of the grid's leftmost
	// corner (corner 2 of its minimum tile).
	SVec3 OriginOfGrid( const SVec3 &zeroPos3, int minTileX, int minTileY ) const;
	// LoadRPGStats' inverse: the tile that holds the grid's first cell, from
	// zeroPos3 - origin, through GetPos2, +fCellSizeX/2 in x and the same int
	// truncation MFC's POINT does.
	void FirstTileOfGrid( const SVec3 &zeroPos3, const SVec3 &origin, int &tileX, int &tileY ) const;

	// Each one-way line's normal direction 0..15 and the tiles its Bresenham
	// run covers, written into dirTiles like the editor's mouse-up handler.
	int NormalOfLine( const STransLine &line ) const;
	void DirTilesFromTransLines( const std::vector<STransLine> &lines, CListOfNormalTiles &dirTiles ) const;

private:
	SGroundCamera camera_;
};

// MyFillBrezenham, ObjectFrm.cpp: integer Bresenham, including its habit of
// emitting the corner tile of every diagonal step.
void FillBresenham( int x1, int y1, int x2, int y2, std::list<std::pair<int, int>> &tiles );

// The passability grid of SaveRPGStats: the bounding box of the locked tiles,
// each cell the tile's value or 0. An empty list gives an empty grid.
STileGrid TilesToGrid( const CListOfTiles &tiles );
// The visibility grid: the box covers transparency and one-way tiles; a cell
// holds the transparency value (0..7), overwritten by (dir << 4) | 0x08 where a
// one-way tile sits.
STileGrid TilesToVisGrid( const CListOfTiles &transparences, const CListOfNormalTiles &dirTiles );
// LoadRPGStats: the grid back into a locked-tile list, first tile at (firstTileX, firstTileY).
void GridToTiles( const STileGrid &grid, int firstTileX, int firstTileY, CListOfTiles &tiles );
// LoadRPGStats for visibility: bits 0..2 become a transparency tile, else bit 3
// makes a one-way tile with direction value >> 4.
void VisGridToTiles( const STileGrid &grid, int firstTileX, int firstTileY, CListOfTiles &transparences, CListOfNormalTiles &dirTiles );

}
