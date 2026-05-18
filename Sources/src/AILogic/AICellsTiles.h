#ifndef _AI_CELLS_TILES_H__
#define _AI_CELLS_TILES_H__

#pragma ONCE
#include "AIInternalConsts.h"
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*				  Tiles and Cells enumeration/converting handling					*
//*******************************************************************
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
namespace AICellsTiles
{
	inline const SVector GetTile( const float x, const float y )
	{ 
		SVector res;
		if ( x < 0 ) 
			res.x = 0;
		else 
			res.x = x/SAIConsts::TILE_SIZE;
		if ( y < 0 ) 
			res.y = 0;
		else 
			res.y = y/SAIConsts::TILE_SIZE;

		return res;
	}

	inline const SVector GetTile( const SVector &point )
	{
		SVector res;
		if ( point.x < 0 ) 
			res.x = 0;
		else 
			res.x = point.x / SAIConsts::TILE_SIZE;
		if ( point.y < 0 ) 
			res.y = 0;
		else 
			res.y = point.y / SAIConsts::TILE_SIZE;

		return res;
	}

	inline const SVector GetTile( const CVec2 &point )
	{
		return GetTile( point.x, point.y );
	}

	// get center of the tile in point's coordinates by the point's coordinates
	inline const CVec2 GetCenterOfTile( const float x, const float y )
	{
		return CVec2( GetTile(x, y).x * SAIConsts::TILE_SIZE + SAIConsts::TILE_SIZE/2, GetTile(x, y).y * SAIConsts::TILE_SIZE + SAIConsts::TILE_SIZE/2 );
	}
	// get center of the tile in point's coordinates by the point's coordinates
	inline const CVec2 GetCenterOfTile( const CVec2& point )
	{
		return GetCenterOfTile( point.x, point.y );
	}

	// point coordinates by AI tile coordinates
	inline const CVec2 GetPointByTile( const int x, const int y )
	{
		return CVec2( x * SAIConsts::TILE_SIZE + SAIConsts::TILE_SIZE/2, y * SAIConsts::TILE_SIZE + SAIConsts::TILE_SIZE/2 );
	}

	inline const CVec2 GetPointByTile( const SVector &point )
	{
		return GetPointByTile( point.x, point.y );
	}
	
	// cell coordinates by the point coordinates
	inline const SVector GetCell( const int x, const int y )
	{ 
		SVector res;
		if ( x < 0 )
			res.x = 0;
		else
			res.x = x / SConsts::CELL_SIZE;
		if ( y < 0 )
			res.y = 0;
		else
			res.y = y / SConsts::CELL_SIZE;

		return res;
	}
	// big cell coordinates by the point coordinates
	inline const SVector GetBigCell( const int x, const int y )
	{ 
		SVector res;
		if ( x < 0 )
			res.x = 0;
		else
			res.x = x / SConsts::BIG_CELL_SIZE;
		if ( y < 0 )
			res.y = 0;
		else
			res.y = y / SConsts::BIG_CELL_SIZE;
		
		return res;
	}

	inline const SVector GetBigCell( const SVector &bigCell )
	{
		return GetBigCell( bigCell.x, bigCell.y );
	}

	// for general cells
	inline const SVector GetGeneralCell( const SVector &vPos )
	{
		return SVector( vPos.x / SConsts::GENERAL_CELL_SIZE,
									vPos.y / SConsts::GENERAL_CELL_SIZE );
	}
	inline const CVec2 GetCenterOfGeneralCell( const SVector &vPos )
	{
		return 		CVec2( vPos.x * SConsts::GENERAL_CELL_SIZE + SConsts::GENERAL_CELL_SIZE / 2,
												vPos.y * SConsts::GENERAL_CELL_SIZE + SConsts::GENERAL_CELL_SIZE / 2 );
	}
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // _AI_CELLS_TILES_H__
