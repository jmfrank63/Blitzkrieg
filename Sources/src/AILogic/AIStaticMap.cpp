#include "stdafx.h"

#include "AIStaticMap.h"
#include "RectTiles.h"
#include "Units.h"
#include "UnitsIterators2.h"
#include "UnitsIterators.h"
#include "AIUnit.h"
#include "Diplomacy.h"
#include "StaticObjects.h"
#include "StaticObject.h"
#include "StaticObjectsIters.h"

#include "..\Formats\fmtMap.h"
#include "..\Formats\fmtTerrain.h"
#include "..\Formats\fmtTerrain.h"

#include "..\RandomMapGen\VA_Types.h"

#include "..\Scene\Scene.h"
#include "..\Scene\Terrain.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CStaticMap theStaticMap;
extern CStaticObjects theStatObjs;
extern CUnits units;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*												  CStaticMap															*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
std::vector<SVector> CStaticMap::oneWayDirs;
std::vector<BYTE> CStaticMap::classToIndex;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const bool CStaticMap::CanDigEntrenchment( const int x, const int y ) const
{
	return !entrenchPossibility.GetData( x, y );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const void CStaticMap::AddUndigableTiles( const CTilesSet &tiles )
{
	for ( CTilesSet::const_iterator it = tiles.begin(); it != tiles.end(); ++it )
		entrenchPossibility.SetData( it->x, it->y );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStaticMap::LoadPassabilities( const struct STerrainInfo &terrainInfo )
{
	passTypes.SetSizes( nSizeX / 2 + 1, nSizeY / 2 + 1 );
	passTypes.SetZero();

	STilesetDesc tilesetDesc;
	CPtr<IDataStream> pStream = GetSingleton<IDataStorage>()->OpenStream( NStr::Format("%s.xml", terrainInfo.szTilesetDesc.c_str()), STREAM_ACCESS_READ );
	CTreeAccessor tree = CreateDataTreeSaver( pStream, IDataTree::READ );
	tree.Add( "tileset", &tilesetDesc );

	std::vector<bool> tileEntrenchPossibility;
	tileEntrenchPossibility.resize( tilesetDesc.terrtypes.size(), 0 );
	passClasses.resize( tilesetDesc.terrtypes.size(), 0 );
	passabilities.resize( tilesetDesc.terrtypes.size(), 0 );

	std::vector<BYTE> soilParams;
	soilParams.resize( tilesetDesc.terrtypes.size(), 0 );
	for ( int i = 0; i < tilesetDesc.terrtypes.size(); ++i )
	{
		for ( int j = 0; j < tilesetDesc.terrtypes[i].tiles.size(); ++j )
		{
			const int &nIndex = tilesetDesc.terrtypes[i].tiles[j].nIndex;
			if ( terrSubTypes.size() <= nIndex )
				terrSubTypes.resize( nIndex + 1 );
			
			terrSubTypes[nIndex] = i;
		}

		passabilities[i] = tilesetDesc.terrtypes[i].fPassability;
		passClasses[i] = tilesetDesc.terrtypes[i].dwAIClasses;
		soilParams[i] = tilesetDesc.terrtypes[i].cSoilParams;
		tileEntrenchPossibility[i] = tilesetDesc.terrtypes[i].bCanEntrench;
	}

	for ( int y = 0; y < terrainInfo.tiles.GetSizeY(); ++y )
	{
		for ( int x = 0; x < terrainInfo.tiles.GetSizeX(); ++x )
		{
			const int rightX = terrainInfo.X2World( x );
			const int rightY = terrainInfo.Y2World( y );

			const BYTE cSoilType = soilParams[ terrSubTypes[terrainInfo.tiles[y][x].tile] ];
			soil[rightY * 2][rightX * 2 ] = soil[rightY * 2][rightX * 2 + 1] = 
			soil[rightY * 2 + 1][rightX * 2	] = soil[rightY * 2 + 1][rightX * 2 + 1] = cSoilType;

			// инициализировать возможность строительства окопов
			if ( !tileEntrenchPossibility[terrSubTypes[terrainInfo.tiles[y][x].tile]] )
			{
				entrenchPossibility.SetData( rightX * 2,			rightY * 2 );
				entrenchPossibility.SetData( rightX * 2 + 1,	rightY * 2 );
				entrenchPossibility.SetData( rightX * 2,			rightY * 2 + 1 );
				entrenchPossibility.SetData( rightX	* 2 + 1,  rightY * 2 + 1 );
			}
			passTypes[rightY][rightX] = terrSubTypes[terrainInfo.tiles[y][x].tile];

			if ( passabilities[ terrSubTypes[terrainInfo.tiles[y][x].tile] ] == 0.0f || ( passClasses[ terrSubTypes[terrainInfo.tiles[y][x].tile] ] & ~0x80000000 ) != 0 )
			{
				BYTE aiClass;
				if ( passabilities[ terrSubTypes[terrainInfo.tiles[y][x].tile] ] == 0.0f )
					aiClass = AI_CLASS_ANY;
				else
					aiClass = passClasses[ terrSubTypes[terrainInfo.tiles[y][x].tile] ] & ~0x80000000;

				LockTile( rightX * 2,			rightY * 2		, aiClass );
				LockTile( rightX * 2,			rightY * 2 + 1, aiClass );
				LockTile( rightX * 2 + 1, rightY * 2		, aiClass );
				LockTile( rightX * 2 + 1, rightY * 2 + 1, aiClass );
			}

			// инициализировать типы terrain для воронок
			if ( passClasses[ terrSubTypes[terrainInfo.tiles[y][x].tile] ] & 0x80000000 )
			{				
				int nX = Clamp( rightX * 2, 0, terrainTypes.GetSizeX() - 1 );
				int nY = Clamp( rightY * 2, 0, terrainTypes.GetSizeY() - 1 );
				terrainTypes.SetData( nX, nY, ETT_RIVER_TERRAIN );

				nX = Clamp( rightX * 2, 0, terrainTypes.GetSizeX() - 1 );
				nY = Clamp( rightY * 2 + 1, 0, terrainTypes.GetSizeY() - 1 );
				terrainTypes.SetData( nX, nY, ETT_RIVER_TERRAIN );

				nX = Clamp( rightX * 2 + 1, 0, terrainTypes.GetSizeX() - 1 );
				nY = Clamp( rightY * 2, 0, terrainTypes.GetSizeY() - 1 );
				terrainTypes.SetData( nX, nY, ETT_RIVER_TERRAIN );

				nX = Clamp( rightX * 2 + 1, 0, terrainTypes.GetSizeX() - 1 );
				nY = Clamp( rightY * 2 + 1, 0, terrainTypes.GetSizeY() - 1 );
				terrainTypes.SetData( nX, nY, ETT_RIVER_TERRAIN );
			}
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const ETerrainTypes CStaticMap::GetTerrainType( const int nX, const int nY ) const
{
	if ( !IsTileInside( nX, nY ) )
		return ETT_EARTH_TERRAIN;
	else
		return ETerrainTypes( terrainTypes.GetData( nX, nY ) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStaticMap::AddRiverTiles( const CTilesSet &tiles )
{
	for ( CTilesSet::const_iterator iter = tiles.begin(); iter != tiles.end(); ++iter )
	{
		const SVector &tile = *iter;
		if ( IsTileInside(tile) )
			terrainTypes.SetData( tile.x, tile.y, ETT_RIVER_TERRAIN );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStaticMap::AddEarthSeaTiles( const CTilesSet &tiles )
{
	for ( CTilesSet::const_iterator iter = tiles.begin(); iter != tiles.end(); ++iter )
	{
		if ( GetTerrainType( iter->x, iter->y ) != ETT_RIVER_TERRAIN )
			terrainTypes.SetData( iter->x, iter->y, ETT_EARTH_SEA_TERRAIN );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void GetRiverTiles( const SVectorStripeObject &vectorStripeObject, const int j, const float fCoeff, CTilesSet *pTiles )
{
	const CVec2 vCenter1( vectorStripeObject.points[j].vPos.x, vectorStripeObject.points[j].vPos.y );
	const CVec2 vCenter2( vectorStripeObject.points[j+1].vPos.x, vectorStripeObject.points[j+1].vPos.y );
	
	CVec2 vNorm1( vectorStripeObject.points[j].vNorm.x, vectorStripeObject.points[j].vNorm.y );
	CVec2 vNorm2( vectorStripeObject.points[j+1].vNorm.x, vectorStripeObject.points[j+1].vNorm.y );

	CVec2 v[4];
	v[0] = vCenter1 + vNorm1 * vectorStripeObject.points[j].fWidth * fCoeff;
	v[1] = vCenter1 - vNorm1 * vectorStripeObject.points[j].fWidth * fCoeff;
	v[2] = vCenter2 - vNorm2 * vectorStripeObject.points[j+1].fWidth * fCoeff;
	v[3] = vCenter2 + vNorm2 * vectorStripeObject.points[j+1].fWidth * fCoeff;

	Vis2AI( &v[0] );
	Vis2AI( &v[1] );
	Vis2AI( &v[2] );
	Vis2AI( &v[3] );

//	NI_ASSERT_T( fabs( v[2] - v[1] ) != 0, "Intersecting river's normals" );
//	NI_ASSERT_T( fabs( v[3] - v[0] ) != 0, "Intersecting river's normals" );

	const float fDist12 = fabs( v[2] - v[1] );
	if ( fDist12 < 2.5 * (float)SConsts::TILE_SIZE )
	{
		if ( fabs( fDist12 ) < FP_EPSILON )
		{
			v[2] = v[1];
		}
		else
		{
			v[2] = v[1] + ( v[2] - v[1] ) * ( 2.5 * (float)SConsts::TILE_SIZE / fDist12 );
		}
	}
	const float fDist03 = fabs( v[3] - v[0] );
	if ( fDist03 < 2.5 * (float)SConsts::TILE_SIZE )
	{
		if ( fabs( fDist03 ) < FP_EPSILON )
		{
			v[3] = v[0];
		}
		else
		{
			v[3] = v[0] + ( v[3] - v[0] ) * ( 2.5 * (float)SConsts::TILE_SIZE / fDist03 );
		}
	}
	

	// отсортировать точки против часовой стрелки
	const CVec2 vCenter = ( v[0] + v[1] + v[2] + v[3] ) / 4.0f;
	for ( int i = 0; i < 3; ++i )
	{
		for ( int j = i + 1; j < 4; ++j )
		{
			const WORD wDirI = GetDirectionByVector( v[i] - vCenter );
			const WORD wDirJ = GetDirectionByVector( v[j] - vCenter );

			if ( wDirI > wDirJ )
				std::swap( v[i], v[j] );
		}
	}

	GetTilesCoveredByQuadrangle( v[0], v[1], v[2], v[3], pTiles );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStaticMap::Load3DRoads( const STerrainInfo &terrainInfo )
{
	const int nTerrTypes = passabilities.size();
	passabilities.resize( nTerrTypes + terrainInfo.roads3.size() );
	
	for ( int i = 0; i < terrainInfo.roads3.size(); ++i )
	{
		int nPassIndex = nTerrTypes + i - 1;
		while ( nPassIndex >= 0 && passabilities[nPassIndex] != terrainInfo.roads3[i].fPassability )
			--nPassIndex;

		if ( nPassIndex < 0 )
		{
			nPassIndex = nTerrTypes + i;
			passabilities[nPassIndex] = terrainInfo.roads3[i].fPassability;
		}

		NI_ASSERT_T( nPassIndex < 256, "To many different passabilities ( more than 256 )" );

		const BYTE aiClass = 
			( passabilities[nPassIndex] == 0.0f ) ? AI_CLASS_ANY : ( terrainInfo.roads3[i].dwAIClasses & ~0x80000000 );
		const BYTE cSoilParams = terrainInfo.roads3[i].cSoilParams;

		for ( int j = 0; j < terrainInfo.roads3[i].points.size() - 1; ++j )
		{
			CTilesSet tiles;			
			GetRiverTiles( terrainInfo.roads3[i], j, 1.0f, &tiles );
			for ( CTilesSet::iterator iter = tiles.begin(); iter != tiles.end(); ++iter )
			{
				const SVector tile = *iter;
				if ( IsTileInside( tile ) )
				{
					entrenchPossibility.SetData( tile.x, tile.y );

					// влияние на скорость юнитов
					passTypes[tile.y / 2][tile.x / 2] = nPassIndex;

					// проходимость
					for ( int i = 1; i < 16; i *= 2 )
						UnlockTile( tile.x, tile.y, i );
					UnlockTile( tile.x, tile.y, AI_CLASS_ANY );

					LockTile( tile.x, tile.y, aiClass );

					soil[tile.y][tile.x] = cSoilParams;
				}
			}
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStaticMap::UpdateRiverPassability( const SVectorStripeObject &river, bool bAdd, bool bUpdate )
{
	for ( int j = 0; j < river.points.size() - 1; ++j )
	{
		// залокать тайлы реки
		CTilesSet tiles;			
		GetRiverTiles( river, j, 1.0f, &tiles );

		int downX = 2 * SConsts::MAX_MAP_SIZE; 
		int downY = 2 * SConsts::MAX_MAP_SIZE; 
		int upX = -10;
		int upY = -10;

		for ( CTilesSet::iterator iter = tiles.begin(); iter != tiles.end(); ++iter )
		{
			const SVector tile = *iter;
			if ( IsTileInside( tile ) )
			{
				bAdd ? LockTile( tile, AI_CLASS_ANY ) : UnlockTile( tile, AI_CLASS_ANY );

				downX = Min( downX, tile.x - SConsts::MAX_UNIT_TILE_RADIUS - 1 );
				downY = Min( downY, tile.y - SConsts::MAX_UNIT_TILE_RADIUS - 1 );
				upX = Max( upX, tile.x + SConsts::MAX_UNIT_TILE_RADIUS + 1 );
				upY = Max( upY, tile.y + SConsts::MAX_UNIT_TILE_RADIUS + 1 );
			}
		}

		downX = Max( 0, downX );
		downY = Max( 0, downY );
		upX = Min( nSizeX - 1, upX );
		upY = Min( nSizeY - 1, upY );

		if ( bUpdate )
		{
			MemMode();

			SetMode( ELM_STATIC );

				bAdd ? 
					UpdateMaxesForAddedTiles( downX, upX, downY, upY, classToIndex[AI_CLASS_ANY] ) :
					UpdateMaxesForRemovedTiles( downX, upX, downY, upY, classToIndex[AI_CLASS_ANY] );

			SetMode( ELM_ALL );
			bAdd ?
				UpdateMaxesForAddedTiles( downX, upX, downY, upY, classToIndex[AI_CLASS_ANY] ) :
				UpdateMaxesForRemovedTiles( downX, upX, downY, upY, classToIndex[AI_CLASS_ANY] );

			RestoreMode();
		}

		// добавить реку для разрывов снарядов
		if ( bAdd )
		{
			tiles.clear();
			GetRiverTiles( river, j, 0.9f, &tiles );
			AddRiverTiles( tiles );
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStaticMap::LoadRivers( const STerrainInfo &terrainInfo )
{
	for ( int i = 0; i < terrainInfo.rivers.size(); ++i )
		UpdateRiverPassability( terrainInfo.rivers[i], true, true );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStaticMap::InitMaxes()
{
	for ( int i = 0; i < 5; ++i )
	{
		maxes[0][i].SetZero();
		maxes[1][i].SetZero();
	}
	
	for ( int y = 0; y < GetSizeY(); ++y )
	{
		for ( int x = 0; x < GetSizeX(); ++x )
		{
			const int dX = Min( x, GetSizeX() - x - 1 );
			const int dY = Min( y, GetSizeY() - y - 1 );

			if ( dX < SConsts::MAX_UNIT_TILE_RADIUS || dY < SConsts::MAX_UNIT_TILE_RADIUS )
			{
				for ( int i = 0; i < 5; ++i )
				{
					maxes[0][i].SetData( x, y, Min( dX, dY ) + 1 );
					maxes[1][i].SetData( x, y, Min( dX, dY ) + 1 );
				}
			}
			else
			{
				for ( int i = 0; i < 5; ++i )
				{
					maxes[0][i].SetData( x, y, SConsts::MAX_UNIT_TILE_RADIUS + 1 );
					maxes[1][i].SetData( x, y, SConsts::MAX_UNIT_TILE_RADIUS + 1 );
				}
			}

			NI_ASSERT_T( x + maxes[0][0].GetData( x, y ) - 1 < GetSizeX(), "Wrong maxes initialization" );
			NI_ASSERT_T( y + maxes[0][0].GetData( x, y ) - 1 < GetSizeY(), "Wrong maxes initialization" );
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool IsGoodPoint( const int i, const int j, const CArray2D<float> &heights )
{
	return i >= 0 && j >= 0 && i < heights.GetSizeY() && j < heights.GetSizeX();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStaticMap::LoadNormals( const STerrainInfo::TVertexAltitudeArray2D &initHeights )
{
	const int nInitSizeX = initHeights.GetSizeX();
	const int nInitSizeY = initHeights.GetSizeY();
	
	heights.SetSizes( nInitSizeX + 2, nInitSizeY + 2 );
	for ( int i = 0; i < nInitSizeY; ++i )
	{
		for ( int j = 0; j < nInitSizeX; ++j )
			heights[i+1][j+1] = initHeights[nInitSizeY - 1 - i][j].fHeight;
	}

	for ( int j = 0; j < nInitSizeX; ++j )
	{
		heights[0][j+1] = initHeights[nInitSizeY - 1 - 0][j].fHeight;
		heights[nInitSizeY + 1][j + 1] = initHeights[nInitSizeY - 1 - ( nInitSizeY - 1 )][j].fHeight;
	}

	for ( int i = 0; i < nInitSizeY; ++i )
	{
		heights[i+1][0] = initHeights[nInitSizeY - 1 - i][0].fHeight;
		heights[i+1][nInitSizeX + 1] = initHeights[nInitSizeY - 1 - i][nInitSizeX - 1].fHeight;
	}

	heights[0][0] = initHeights[nInitSizeY - 1 - 0][0].fHeight;
	heights[0][nInitSizeX + 1] = initHeights[nInitSizeY - 1 - 0][nInitSizeX - 1].fHeight;
	heights[nInitSizeY + 1][0] = initHeights[nInitSizeY - 1 - (nInitSizeY - 1)][0].fHeight;
	heights[nInitSizeY + 1][nInitSizeX + 1] = initHeights[nInitSizeY - 1 - (nInitSizeY - 1)][nInitSizeX - 1].fHeight;

	betaSpline3D.Init( 1.0f, -01.0f );

	tileHeights.SetSizes( GetSizeX(), GetSizeY() );
	for ( int i = 0; i < GetSizeY(); ++i )
	{
		for ( int j = 0; j < GetSizeX(); ++j )
		{
			const CVec2 vTileCenter( AICellsTiles::GetPointByTile( i, j ) );
			tileHeights[i][j] = GetVisZ( vTileCenter.x, vTileCenter.y );
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStaticMap::CommonInit()
{
	oneWayDirs.resize( 16 );
	oneWayDirs[0]	 = SVector(  0,  1 );
	oneWayDirs[1]  = SVector( -1,  2 );
	oneWayDirs[2]  = SVector( -1,  1 );
	oneWayDirs[3]  = SVector( -2,  1 );
	oneWayDirs[4]  = SVector( -1,  0 );
	oneWayDirs[5]  = SVector( -2, -1 );
	oneWayDirs[6]  = SVector( -1, -1 );
	oneWayDirs[7]  = SVector( -1, -2 );
	oneWayDirs[8]  = SVector(  0, -1 );
	oneWayDirs[9]  = SVector(  1, -2 );
	oneWayDirs[10] = SVector(  1, -1 );
	oneWayDirs[11] = SVector(  2, -1 );
	oneWayDirs[12] = SVector(  1,  0 );
	oneWayDirs[13] = SVector(  2,  1 );
	oneWayDirs[14] = SVector(  1,  1 );
	oneWayDirs[15] = SVector(  1,  2 );

	classToIndex.resize( 16 );
	classToIndex[AI_CLASS_WHEEL]		 = 0;
	classToIndex[AI_CLASS_HALFTRACK] = 1;
	classToIndex[AI_CLASS_TRACK]		 = 2;
	classToIndex[AI_CLASS_HUMAN]		 = 3;
	classToIndex[AI_CLASS_ANY]			 = 4;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStaticMap::LoadMap( const struct STerrainInfo &terrainInfo, const bool bLoadRivers )
{
	CommonInit();

	nSizeX = terrainInfo.tiles.GetSizeX() * 2;
	nSizeY = terrainInfo.tiles.GetSizeY() * 2;

	entrenchPossibility.SetSizes( nSizeX, nSizeY );
	entrenchPossibility.SetZero();

	nCellsSizeX = nSizeX / SConsts::CELL_COEFF;
	nCellsSizeY = nSizeY / SConsts::CELL_COEFF;
	
	nBigCellsSizeX = nSizeX / SConsts::BIG_CELL_COEFF;
	nBigCellsSizeY = nSizeY / SConsts::BIG_CELL_COEFF;

	for ( int i = 0; i < 5; ++i )
	{
		buf[i].SetSizes( nSizeX, nSizeY );
		buf[i].SetZero();
	}

	unitsBuf.SetSizes( nSizeX, nSizeY );
	unitsBuf.SetZero();

	for ( int i = 0; i < 5; ++i )
	{
		maxes[0][i].SetSizes( nSizeX, nSizeY );	
		maxes[1][i].SetSizes( nSizeX, nSizeY );
	}
	
	InitMaxes();
	LoadNormals( terrainInfo.altitudes );

	bridgeTiles.SetSizes( nSizeX, nSizeY );
	bridgeTiles.SetZero();
	
	transparency.SetSizes( nSizeX, nSizeY );
	transparency.SetZero();

	terrainTypes.SetSizes( GetSizeX(), GetSizeY() );
	terrainTypes.SetZero();
	
	soil.SetSizes( GetSizeX(), GetSizeY() );
	soil.SetZero();

	LoadPassabilities( terrainInfo );
	Load3DRoads( terrainInfo );

	if ( bLoadRivers )
		LoadRivers( terrainInfo );


	InitExplosionTerrainTypes();
	tmpUnlockID = 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStaticMap::UpdateMaxesForRemovedTiles( const int downX, const int upX, const int downY, const int upY, const BYTE cl )
{
	for ( int y = downY; y <= upY; ++y )
	{
		for ( int x = downX; x <= upX; ++x )
		{
			if ( theStaticMap.IsLocked4Class( x, y, cl ) )
				maxes[eMode][cl].SetData( x, y, 0 );
			else
			{
				if ( x == 0 || y == 0 )
					maxes[eMode][cl].SetData( x, y, 1 );
				else if ( !theStaticMap.IsLocked4Class( x, y, cl ) )
				{
					NI_ASSERT_T( fabs( (float)( maxes[eMode][cl].GetData( x, y-1 ) - maxes[eMode][cl].GetData( x-1, y ) ) ) <= 1, "Wrong maxes array" );

					if ( maxes[eMode][cl].GetData( x-1, y ) < maxes[eMode][cl].GetData( x, y-1 ) && 
							 maxes[eMode][cl].GetData( x  , y ) < maxes[eMode][cl].GetData( x, y-1 ) )
					{
						const BYTE mxy_1 = maxes[eMode][cl].GetData( x, y-1 );

						if ( y + mxy_1 - 1 >= theStaticMap.GetSizeY() )
							maxes[eMode][cl].SetData( x, y, mxy_1 - 1 );
						else
						{
							maxes[eMode][cl].SetData( x, y, mxy_1 );
							for ( int xx = x - mxy_1 + 1; xx <= x + mxy_1 - 1; ++xx )
							{
								if ( theStaticMap.IsLocked4Class( xx, y + mxy_1 - 1, cl ) )
								{
									maxes[eMode][cl].SetData( x, y, mxy_1 - 1 );
									break;
								}
							}
						}
					}
					
					else if ( maxes[eMode][cl].GetData( x-1, y ) == maxes[eMode][cl].GetData( x  ,y-1 ) &&
										maxes[eMode][cl].GetData( x  , y ) < maxes[eMode][cl].GetData( x-1, y ) + 1 )
					{
						const BYTE mx_1y = maxes[eMode][cl].GetData( x-1, y );

						if ( mx_1y == 0 )
							maxes[eMode][cl].SetData( x, y, 1 );
						else if ( theStaticMap.IsLocked4Class( x + mx_1y - 1, y + mx_1y - 1, cl ) )
							maxes[eMode][cl].SetData( x, y, mx_1y - 1 );
						else if ( x + mx_1y >= theStaticMap.GetSizeX() || y + mx_1y >= theStaticMap.GetSizeY() || 
											theStaticMap.IsLocked4Class( x - mx_1y, y - mx_1y, cl ) || mx_1y > SConsts::MAX_UNIT_TILE_RADIUS )
							maxes[eMode][cl].SetData( x, y, mx_1y );
						else
						{
							maxes[eMode][cl].SetData( x, y, mx_1y + 1 );
							for ( int xx = x - mx_1y; xx <= x + mx_1y; ++xx )
							{
								if ( theStaticMap.IsLocked4Class( xx, y + mx_1y, cl ) )
								{
									maxes[eMode][cl].SetData( x, y, mx_1y );
									break;
								}
							}

							if ( maxes[eMode][cl].GetData( x, y ) == mx_1y + 1 )
							{
								for ( int yy = y - mx_1y; yy <= y + mx_1y; ++yy )
								{
									if ( theStaticMap.IsLocked4Class( x + mx_1y, yy, cl ) )
									{
										maxes[eMode][cl].SetData( x, y, mx_1y );
										break;
									}
								}
							}
						}
					}

					else if ( maxes[eMode][cl].GetData( x, y ) < maxes[eMode][cl].GetData( x-1, y ) )
					{
						const BYTE mx_1y = maxes[eMode][cl].GetData( x-1, y );

						if ( x + mx_1y - 1 >= theStaticMap.GetSizeX() )
							maxes[eMode][cl].SetData( x, y, mx_1y - 1 );
						else
						{
							maxes[eMode][cl].SetData( x, y, mx_1y );
							for ( int yy = y - mx_1y + 1; yy <= y + mx_1y - 1; ++yy )
							{
								if ( theStaticMap.IsLocked4Class( x + mx_1y - 1, yy, cl ) )
								{
									maxes[eMode][cl].SetData( x, y, mx_1y - 1 );
									break;
								}
							}
						}
					}
				}
			}
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStaticMap::UpdateMaxesForAddedTiles( const int downX, const int upX, const int downY, const int upY, const BYTE cl )
{
	for ( int y = downY; y <= upY; ++y )
	{
		for ( int x = downX; x <= upX; ++x )
		{
			if ( theStaticMap.IsLocked4Class( x, y, cl ) )
				maxes[eMode][cl].SetData( x, y, 0 );
			else
			{
				if ( x == 0 || y == 0 )
					maxes[eMode][cl].SetData( x, y, 1 );
				else
				{
					NI_ASSERT_T( fabs( (float)( maxes[eMode][cl].GetData( x, y-1 ) - maxes[eMode][cl].GetData( x-1, y ) ) ) <= 1, NStr::Format( "Wrong maxes array: %d, %d", maxes[eMode][cl].GetData( x, y-1 ), maxes[eMode][cl].GetData( x-1, y ) ) );

					if ( maxes[eMode][cl].GetData( x-1, y ) < maxes[eMode][cl].GetData( x, y-1 ) )
					{
						const BYTE mxy_1 = maxes[eMode][cl].GetData( x, y-1 );

						if ( y + mxy_1 - 1 >= theStaticMap.GetSizeY() )
							maxes[eMode][cl].SetData( x, y, mxy_1 - 1 );
						else
						{
							maxes[eMode][cl].SetData( x, y, mxy_1 );
							for ( int xx = x - mxy_1 + 1; xx <= x + mxy_1 - 1; ++xx )
							{
								if ( theStaticMap.IsLocked4Class( xx, y + mxy_1 - 1, cl ) )
								{
									maxes[eMode][cl].SetData( x, y, mxy_1 - 1 );
									break;
								}
							}
						}
					}

					else if ( maxes[eMode][cl].GetData( x-1, y ) == maxes[eMode][cl].GetData( x, y-1 ) )
					{
						const BYTE mx_1y = maxes[eMode][cl].GetData( x-1, y );

						if ( mx_1y == 0 )
							maxes[eMode][cl].SetData( x, y, 1 );
						else if ( theStaticMap.IsLocked4Class( x + mx_1y - 1, y + mx_1y - 1, cl ) )
							maxes[eMode][cl].SetData( x, y, mx_1y - 1 );
						else if ( x + mx_1y >= theStaticMap.GetSizeX() || y + mx_1y >= theStaticMap.GetSizeY() || 
										  theStaticMap.IsLocked4Class( x - mx_1y, y - mx_1y, cl ) || mx_1y > SConsts::MAX_UNIT_TILE_RADIUS )
							maxes[eMode][cl].SetData( x, y, mx_1y );
						else
						{
							maxes[eMode][cl].SetData( x, y, mx_1y + 1 );
							for ( int xx = x - mx_1y; xx <= x + mx_1y; ++xx )
							{
								if ( theStaticMap.IsLocked4Class( xx, y + mx_1y, cl ) )
								{
									maxes[eMode][cl].SetData( x, y, mx_1y );
									break;
								}
							}

							if ( maxes[eMode][cl].GetData( x, y ) == mx_1y + 1 )
							{
								for ( int yy = y - mx_1y; yy <= y + mx_1y; ++yy )
								{
									if ( theStaticMap.IsLocked4Class( x + mx_1y, yy, cl ) )
									{
										maxes[eMode][cl].SetData( x, y, mx_1y );
										break;
									}
								}
							}
						}
					}
					else
					{
						const BYTE mx_1y = maxes[eMode][cl].GetData( x-1, y );						

						if ( x + mx_1y - 1 >= theStaticMap.GetSizeX() )
							maxes[eMode][cl].SetData( x, y, mx_1y - 1 );
						else
						{
							maxes[eMode][cl].SetData( x, y, mx_1y );
							for ( int yy = y - mx_1y + 1; yy <= y + mx_1y - 1; ++yy )
							{
								if ( theStaticMap.IsLocked4Class( x + mx_1y - 1, yy, cl ) )
								{
									maxes[eMode][cl].SetData( x, y, mx_1y - 1 );
									break;
								}
							}
						}
					}
				}
			}
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStaticMap::UnitLocksTile( const SVector &tile )
{
	return ( ++unitsBuf[tile.y][tile.x] == 1 );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStaticMap::UnitUnlocksTile( const SVector &tile )
{
	NI_ASSERT_T( unitsBuf[tile.y][tile.x] > 0, "Wrong locked tiles by units info" );	
	return ( --unitsBuf[tile.y][tile.x] == 0 );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStaticMap::AddLockedUnitTiles( const SRect &rect, const int id, bool bAddToRects, bool bUpdate )
{
	if ( tmpUnlockID == id )
		RemoveTemporaryUnlocking();

	MemMode();
	SetMode( ELM_ALL );
	
	CTilesSet tiles;
	//CRAP{ какие-то глюки с float
	GetTilesCoveredByQuadrangle( SVector(rect.v1).ToCVec2(), SVector(rect.v2).ToCVec2(), SVector(rect.v3).ToCVec2(), SVector(rect.v4).ToCVec2(), &tiles );
	//}CRAP

	int downX = 2 * SConsts::MAX_MAP_SIZE, downY = 2 * SConsts::MAX_MAP_SIZE;
	int upX = -10, upY = -10;
	
	for ( CTilesSet::const_iterator iter = tiles.begin(); iter != tiles.end(); ++iter )
	{
		if ( UnitLocksTile( *iter ) && bUpdate )
		{
			if ( (*iter).x - SConsts::MAX_UNIT_TILE_RADIUS - 1 < downX )
				downX = (*iter).x - SConsts::MAX_UNIT_TILE_RADIUS - 1;
			if ( (*iter).y - SConsts::MAX_UNIT_TILE_RADIUS - 1 < downY )
				downY = (*iter).y - SConsts::MAX_UNIT_TILE_RADIUS - 1;

			if ( (*iter).x + SConsts::MAX_UNIT_TILE_RADIUS + 1 > upX )
				upX = (*iter).x + SConsts::MAX_UNIT_TILE_RADIUS + 1;
			if ( (*iter).y + SConsts::MAX_UNIT_TILE_RADIUS + 1 > upY )
				upY = (*iter).y + SConsts::MAX_UNIT_TILE_RADIUS + 1;
		}
	}

	if ( bUpdate )
	{
		downX = Max( 0, downX );
		downY = Max( 0, downY );
		upX = Min( nSizeX - 1, upX );
		upY = Min( nSizeY - 1, upY );

		UpdateMaxesForAddedTiles( downX, upX, downY, upY, classToIndex[AI_CLASS_ANY] );
	}

	if ( bAddToRects )
		unitsRects[id] = rect;

//	CheckMaxes( downX, upX, downY, upY );
//	SetMode( ELM_STATIC );
//	CheckMaxes( downX, upX, downY, upY );

	RestoreMode();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStaticMap::RemoveLockedUnitTiles( const int id, const bool bUpdate )
{
	if ( tmpUnlockID == id )
		RemoveTemporaryUnlocking();
	
	MemMode();
	SetMode( ELM_ALL );
	
	CTilesSet tiles;
	const SRect rect = unitsRects[id];
	//CRAP{ какие-то глюки с float
	GetTilesCoveredByQuadrangle( SVector(rect.v1).ToCVec2(), SVector(rect.v2).ToCVec2(), SVector(rect.v3).ToCVec2(), SVector(rect.v4).ToCVec2(), &tiles );
	//}CRAP

	int downX = 2 * SConsts::MAX_MAP_SIZE, downY = 2 * SConsts::MAX_MAP_SIZE;
	int upX = -10, upY = -10;

	for ( CTilesSet::const_iterator iter = tiles.begin(); iter != tiles.end(); ++iter )
	{
		if ( UnitUnlocksTile( *iter ) && bUpdate )
		{
			if ( (*iter).x - SConsts::MAX_UNIT_TILE_RADIUS - 1 < downX )
				downX = (*iter).x - SConsts::MAX_UNIT_TILE_RADIUS - 1;
			if ( (*iter).y - SConsts::MAX_UNIT_TILE_RADIUS - 1 < downY )
				downY = (*iter).y - SConsts::MAX_UNIT_TILE_RADIUS- 1;

			if ( (*iter).x + SConsts::MAX_UNIT_TILE_RADIUS + 1 > upX )
				upX = (*iter).x + SConsts::MAX_UNIT_TILE_RADIUS + 1;
			if ( (*iter).y + SConsts::MAX_UNIT_TILE_RADIUS + 1 > upY )
				upY = (*iter).y + SConsts::MAX_UNIT_TILE_RADIUS + 1;
		}
	}

	if ( bUpdate )
	{
		downX = Max( 0, downX );
		downY = Max( 0, downY );
		upX = Min( nSizeX - 1, upX );
		upY = Min( nSizeY - 1, upY );

		UpdateMaxesForRemovedTiles( downX, upX, downY, upY, classToIndex[AI_CLASS_ANY] );
	}

	unitsRects.erase( id );

//	CheckMaxes( downX, upX, downY, upY );
//	SetMode( ELM_STATIC );
//	CheckMaxes( downX, upX, downY, upY );

	RestoreMode();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStaticMap::CanPut( const int x, const int y, const int d, const BYTE aiClass )
{
	if ( x - d < 0 )
		return false;
	if ( y - d < 0 )
		return false;
	if ( x + d >= theStaticMap.GetSizeX() )
		return false;
	if ( y + d >= theStaticMap.GetSizeY() )
		return false;

	for ( int yy = y - d; yy <= y + d; ++yy )
	{
		for ( int xx = x - d; xx <= x + d; ++xx )
		{
			if ( theStaticMap.IsLocked4Class( xx, yy, aiClass ) )
				return false;
		}
	}

	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStaticMap::CheckMaxes( const int downX, const int upX, const int downY, const int upY, const BYTE aiClass )
{
	for ( int y = downY; y <= upY; ++y )
	{
		for ( int x = downX; x <= upX; ++x )
		{
			if ( maxes[eMode][aiClass].GetData( x, y ) == 0 && CanPut( x, y, 1, aiClass ) )
			{
				NI_ASSERT_T( false, NStr::Format( "Wrong maxes array (%d, %d), maxes %d too small\n", x, y, maxes[eMode][aiClass].GetData( x, y ) ) );
			}
			else if ( !CanPut( x, y, maxes[eMode][aiClass].GetData( x, y ) - 1, aiClass ) )
			{
				NI_ASSERT_T( false, NStr::Format( "Wrong maxes array (%d, %d), maxes %d too big\n", x, y, maxes[eMode][aiClass].GetData( x, y ) - 1 ) );
			}
			else if ( maxes[eMode][aiClass].GetData( x, y ) < SConsts::MAX_UNIT_TILE_RADIUS && CanPut( x, y, maxes[eMode][aiClass].GetData( x, y ), aiClass ) )
			{
				NI_ASSERT_T( false, NStr::Format( "Wrong maxes array (%d, %d), maxes %d too small\n", x, y, maxes[eMode][aiClass].GetData( x, y ) ) );
			}
		}
	}

	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStaticMap::Clear()
{
	for ( int i = 0; i < 5; ++i )
		buf[i].Clear();

	transparency.Clear();
	passTypes.Clear();
	unitsBuf.Clear();

	for ( int i = 0; i < 5; ++i )
	{
		maxes[0][i].Clear();
		maxes[1][i].Clear();
	}

	unitsRects.clear();
	nSizeX = nSizeY = nCellsSizeX = nCellsSizeY = nBigCellsSizeX = nBigCellsSizeY = 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStaticMap::CalcMaxesBoundsByTiles( const CTilesSet &tiles, SVector *vMin, SVector *vMax )
{
	int minX = theStaticMap.GetSizeX()-1;
	int minY = theStaticMap.GetSizeY()-1;
	int maxX =0;
	int maxY =0;
	for( CTilesSet::const_iterator i = tiles.begin(); i != tiles.end(); ++i )
	{
		minX = Min( minX, (*i).x );
		maxX = Max( maxX, (*i).x );

		minY = Min( minY, (*i).y );
		maxY = Max( maxY, (*i).y );
	}
	minX = Max( 0, minX - SConsts::MAX_UNIT_TILE_RADIUS -1 );
	maxX = Min( theStaticMap.GetSizeX() - 1, maxX + SConsts::MAX_UNIT_TILE_RADIUS +1 );
	
	minY = Max( 0, minY - SConsts::MAX_UNIT_TILE_RADIUS -1 );
	maxY = Min( theStaticMap.GetSizeY() - 1, maxY + SConsts::MAX_UNIT_TILE_RADIUS +1 );

	vMin->x = minX;
	vMax->x = maxX;
	vMin->y = minY;
	vMax->y = maxY;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStaticMap::UpdateMaxesByTiles( const CTilesSet &tiles, const BYTE aiClasses, bool bAdd )
{
	int minTerrainX = theStaticMap.GetSizeX()-1;
	int minTerrainY = theStaticMap.GetSizeY()-1;
	int maxTerrainX = 0;
	int maxTerrainY = 0;
	
	for( CTilesSet::const_iterator i = tiles.begin(); i != tiles.end(); ++i )
	{
		if ( bAdd )
			theStaticMap.LockTile( (*i), aiClasses );
		else
			theStaticMap.UnlockTile( (*i), aiClasses );

		minTerrainX = Min( minTerrainX, (*i).x );
		maxTerrainX = Max( maxTerrainX, (*i).x );

		minTerrainY = Min( minTerrainY, (*i).y );
		maxTerrainY = Max( maxTerrainY, (*i).y );
	}
	SVector vMin, vMax;
	CalcMaxesBoundsByTiles( tiles, &vMin, &vMax );

	if ( bAdd )
		UpdateMaxesForAddedStObject( vMin.x, vMax.x, vMin.y, vMax.y, aiClasses );
	else
		UpdateMaxesForRemovedStObject( vMin.x, vMax.x, vMin.y, vMax.y, aiClasses );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStaticMap::UpdateMaxesForAddedStObject( const int downX, const int upX, const int downY, const int upY, const BYTE aiClasses )
{
	MemMode();

	SetMode( ELM_STATIC );
	
	if ( aiClasses == AI_CLASS_ANY )
		UpdateMaxesForAddedTiles( downX, upX, downY, upY, classToIndex[AI_CLASS_ANY] );
	else 
	{
		for ( int i = 1; i < 16; i *= 2 )
		{
			if ( aiClasses & i )
				UpdateMaxesForAddedTiles( downX, upX, downY, upY, classToIndex[i] );
		}
	}

	SetMode( ELM_ALL );

	if ( aiClasses == AI_CLASS_ANY )
		UpdateMaxesForAddedTiles( downX, upX, downY, upY, classToIndex[AI_CLASS_ANY] );
	else 
	{
		for ( int i = 1; i < 16; i *= 2 )
		{
			if ( aiClasses & i )
				UpdateMaxesForAddedTiles( downX, upX, downY, upY, classToIndex[i] );
		}
	}
/*
	for ( int i = 1; i < 16; i *= 2 )
	{
		if ( aiClasses & i )
			CheckMaxes( downX, upX, downY, upY, classToIndex[i] );
	}
	if ( aiClasses == AI_CLASS_ANY )
		CheckMaxes( downX, upX, downY, upY, classToIndex[AI_CLASS_ANY] );

	SetMode( ELM_STATIC );
	for ( int i = 1; i < 16; i *= 2 )
	{
		if ( aiClasses & i )
			CheckMaxes( downX, upX, downY, upY, classToIndex[i] );
	}
	if ( aiClasses == AI_CLASS_ANY )
		CheckMaxes( downX, upX, downY, upY, classToIndex[AI_CLASS_ANY] );
*/

	RestoreMode();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStaticMap::UpdateMaxesForRemovedStObject( const int downX, const int upX, const int downY, const int upY, const BYTE aiClasses )
{
	MemMode();

	SetMode( ELM_STATIC );

	if ( aiClasses == AI_CLASS_ANY )
		UpdateMaxesForRemovedTiles( downX, upX, downY, upY, classToIndex[AI_CLASS_ANY] );
	else 
	{
		for ( int i = 1; i < 16; i *= 2 )
		{
			if ( aiClasses & i )
				UpdateMaxesForRemovedTiles( downX, upX, downY, upY, classToIndex[i] );
		}
	}

	SetMode( ELM_ALL );

	if ( aiClasses == AI_CLASS_ANY )
		UpdateMaxesForRemovedTiles( downX, upX, downY, upY, classToIndex[AI_CLASS_ANY] );
	else 
	{
		for ( int i = 1; i < 16; i *= 2 )
		{
			if ( aiClasses & i )
				UpdateMaxesForRemovedTiles( downX, upX, downY, upY, classToIndex[i] );
		}
	}
/*
	for ( int i = 1; i < 16; i *= 2 )
	{
		if ( aiClasses & i )
			CheckMaxes( downX, upX, downY, upY, classToIndex[i] );
	}
	if ( aiClasses == AI_CLASS_ANY )
		CheckMaxes( downX, upX, downY, upY, classToIndex[AI_CLASS_ANY] );

	SetMode( ELM_STATIC );
	for ( int i = 1; i < 16; i *= 2 )
	{
		if ( aiClasses & i )
			CheckMaxes( downX, upX, downY, upY, classToIndex[i] );
	}
	if ( aiClasses == AI_CLASS_ANY )
		CheckMaxes( downX, upX, downY, upY, classToIndex[AI_CLASS_ANY] );
*/

	RestoreMode();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStaticMap::GetPoint4Spline( const CVec2 &vPoint, float *pu, float *pv, float ptCtrls[] ) const
{
	SVector visTile = AICellsTiles::GetTile( vPoint / 2 );
	if ( visTile.x > heights.GetSizeX() - 4 )
		visTile.x = heights.GetSizeX() - 4;
	if ( visTile.y > heights.GetSizeY() - 4 )
		visTile.y = heights.GetSizeY() - 4;

	CVec2 vTileStart( visTile.x * SConsts::TILE_SIZE * 2, visTile.y * SConsts::TILE_SIZE * 2 );
	*pu = ( vPoint.x - vTileStart.x )/ ( 2.0f * SConsts::TILE_SIZE );
	*pv = ( vPoint.y - vTileStart.y )/ ( 2.0f * SConsts::TILE_SIZE );

	NI_ASSERT_T( *pu >= 0 && *pu <= 1, "Wrong u" );
	NI_ASSERT_T( *pv >= 0 && *pv <= 1, "Wrong v" );

	// высоты сжимаются, т.к. для сплайна даётся сетка с шагом 1 ( а не 2 * TILE_SIZE )
	for ( int i = 0; i < 4; ++i )
	{
		for ( int j = 0; j < 4; ++j )
			ptCtrls[i * 4 + j] = heights[visTile.y + i][visTile.x + j] / ( 2.0f * SConsts::TILE_SIZE );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const DWORD CStaticMap::GetNormal( const float x, const float y ) const
{
	if ( !IsPointInside( x, y ) )
		return DWORD( BYTE(127.0f) ) << 16;
	else
	{
		float u, v;
		float ptCtrls[16];

		GetPoint4Spline( CVec2( x, y ), &u, &v, ptCtrls );

		CVec3 result;
		betaSpline3D.GetNormale( &result, u, v, ptCtrls );

		return Vec3ToDWORD( result );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const float CStaticMap::GetVisZ( float x, float y ) const
{
	float u, v;
	float ptCtrls[16];
	
	x = Clamp( x, 0.0f, float(SConsts::TILE_SIZE * theStaticMap.GetSizeX()) );
	y = Clamp( y, 0.0f, float(SConsts::TILE_SIZE * theStaticMap.GetSizeY()) );

	if ( (theStaticMap.GetSizeX() + theStaticMap.GetSizeY()) == 0 ) 
		return 0;
	//
	GetPoint4Spline( CVec2( x, y ), &u, &v, ptCtrls );
	// высоты разжимаются обратно, т.к. для сплайна даётся сетка с шагом 1 ( а не 2 * TILE_SIZE )
	// умножается на fAITileZCoeff1, чтобы перевести в AI высоты
	return betaSpline3D.Value( u, v, ptCtrls ) * 2.0f * SConsts::TILE_SIZE * fAITileZCoeff1;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const float CStaticMap::GetTileHeight( const SVector &tile ) const 
{ 
	const int inMapX = Min( Max( 0, tile.x ), GetSizeX() - 1 );
	const int inMapY = Min( Max( 0, tile.y ), GetSizeY() - 1 );
	
	return tileHeights[inMapY][inMapX];
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const bool CStaticMap::GetIntersectionWithTerrain( CVec3 *pvResult, const CVec3 &vBegin, const CVec3 &vEnd ) const
{
/*	
	float t = 0;
	const float tShift = -50.0f / vDir.z;
	CVec3 vOnRay( vPoint );
	while ( GetVisZ( vOnRay.x, vOnRay.y ) < vOnRay.z && IsPointInside( vOnRay.x, vOnRay.y ) )
	{
		t += tShift;
		vOnRay = vPoint + vDir * t;
	}

	if ( GetVisZ( vOnRay.x, vOnRay.y ) < vOnRay.z )
		return VNULL3;
*/
	
	// не проинициализирована
	if ( GetSizeX() + GetSizeY() == 0 ) 
		return false;
	// неправильные точки
	if ( GetVisZ( vBegin.x, vBegin.y ) >= vBegin.z ) 
		return false;

	NI_ASSERT_T( GetVisZ( vBegin.x, vBegin.y ) < vBegin.z, NStr::Format( "The first point (%g,%g,%g) is under terrain, terrain height (%g)", vBegin.x, vBegin.y, vBegin.z, GetVisZ( vBegin.x, vBegin.y ) ) );
	
	if ( GetVisZ( vEnd.x, vEnd.y ) < vEnd.z )
		return false;
	
	CVec3 leftPoint( vBegin );
	CVec3 rightPoint( vEnd );
	CVec3 vMiddlePoint;

	do
	{
		vMiddlePoint = ( leftPoint + rightPoint ) * 0.5f;

		if ( GetVisZ( vMiddlePoint.x, vMiddlePoint.y ) < vMiddlePoint.z )
			leftPoint = vMiddlePoint;
		else
			rightPoint = vMiddlePoint;
	} while ( fabs( leftPoint - rightPoint ) > 0.01f );

	*pvResult = vMiddlePoint;
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SFunc4TerrainHeight
{
	bool operator ()( const int nX, const int nY, const float fHeight )
	{
		theStaticMap.SetHeightForPatternApplying( nX, nY, fHeight );
		return true;
	}
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStaticMap::ApplyPattern( const SVAPattern &rPattern )
{
	const CTRect<int> rRect( 0, 0, heights.GetSizeX() - 1, heights.GetSizeY() - 1 );

	SFunc4TerrainHeight func;
	ApplyVAPattern( rRect, rPattern, func, true );

	const int nMinX = Max( 2 * rPattern.pos.x - 1, 0 );
	const int nMaxX = Min( 2 * ( rPattern.pos.x + rPattern.heights.GetSizeX() ) + 1, GetSizeX() );
	const int nMaxY = Min( 2 * ( heights.GetSizeY() - rPattern.pos.y ) + 1, GetSizeY() );
	const int nMinY = Max( 2 * ( heights.GetSizeY() - rPattern.pos.y - rPattern.heights.GetSizeY() - 3 ) - 1, 0 );

	const CVec2 center( AICellsTiles::GetPointByTile( ( nMinX + nMaxX ) / 2, ( nMinY + nMaxY ) / 2 ) );
	const CVec2 vAABBHalfSize( ( nMaxX - nMinX + 1 ) * SConsts::TILE_SIZE, ( nMaxY - nMinY + 1 ) * SConsts::TILE_SIZE );
	
	for ( CUnitsIter<0,2> iter( 0, ANY_PARTY, center, Max( vAABBHalfSize.x, vAABBHalfSize.y ) ); !iter.IsFinished(); iter.Iterate() )
		(*iter)->SetNewCoordinates( CVec3( (*iter)->GetCenter(), (*iter)->GetZ() ) );

	for ( CStObjCircleIter<false> iter( center, Max( vAABBHalfSize.x, vAABBHalfSize.y ) ); !iter.IsFinished(); iter.Iterate() )
	{
		CExistingObject *pObj = *iter;
		pObj->SetNewPlacement( pObj->GetCenter(), pObj->GetDir() );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStaticMap::SetHeightForPatternApplying( const int nX, const int nY, const float fHeight )
{
	heights[heights.GetSizeY() - 2 - nY][nX + 1] += fHeight;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStaticMap::UpdateAllHeights()
{
	IScene *pScene = GetSingleton<IScene>();
	ITerrain *pTerrain = pScene->GetTerrain();
	ITerrainEditor *pTerrainEditor = dynamic_cast<ITerrainEditor*>( pTerrain );
	STerrainInfo &rTerrainInfo = const_cast<STerrainInfo&>( pTerrainEditor->GetTerrainInfo() );

	LoadNormals( rTerrainInfo.altitudes );
	
	for ( CGlobalIter iter( 0, ANY_PARTY ); !iter.IsFinished(); iter.Iterate() )
		(*iter)->SetNewCoordinates( CVec3( (*iter)->GetCenter(), 0 ) );

	theStatObjs.UpdateAllObjectsPos();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStaticMap::InitExplosionTerrainTypes()
{
	CArray2D4Bit terrainTypesOld( terrainTypes );

	for ( int i = 0; i < terrainTypes.GetSizeY(); ++i )
	{
		for ( int j = 0; j < terrainTypes.GetSizeX(); ++j )
		{
			bool bRiverNear = false;
			bool bTerrainNear = false;
			for ( int di = -1; di <= 1; ++di )
			{
				for ( int dj = -1; dj <= 1; ++dj )
				{
					if ( di != 0 && dj != 0 )
					{
						const int nI = Clamp( i + di, 0, terrainTypes.GetSizeY() - 1 );
						const int nJ = Clamp( j + dj, 0, terrainTypes.GetSizeX() - 1 );

						if ( terrainTypesOld.GetData( nJ, nI ) == ETT_RIVER_TERRAIN )
							bRiverNear = true;
						if ( terrainTypesOld.GetData( nJ, nI ) == ETT_EARTH_TERRAIN	)
							bTerrainNear = true;
					}
				}
			}

			if ( terrainTypesOld.GetData( j, i ) == ETT_RIVER_TERRAIN && bTerrainNear )
				terrainTypes.SetData( j, i, ETT_EARTH_SEA_TERRAIN );
			if ( terrainTypesOld.GetData( j, i ) == ETT_EARTH_TERRAIN && bRiverNear )
				terrainTypes.SetData( j, i, ETT_EARTH_SEA_TERRAIN );
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CStaticMap::TemporaryUnlockUnitRect( const int id )
{
	if ( tmpUnlockID != id && tmpUnlockID != 0 )
		RemoveTemporaryUnlocking();

	if ( tmpUnlockID == 0 )
	{
		tmpUnlockID = id;
		CTilesSet tiles;
		SRect rect = units[id]->GetUnitRectForLock();
		rect.Compress( 1.25f );
		GetTilesCoveredByQuadrangle( SVector(rect.v1).ToCVec2(), SVector(rect.v2).ToCVec2(), SVector(rect.v3).ToCVec2(), SVector(rect.v4).ToCVec2(), &tiles );

		tmpUnlockUnitsBuf.clear();

		const int nDecrease = units[id]->IsLockingTiles();
		for ( CTilesSet::iterator iter = tiles.begin(); iter != tiles.end(); ++iter )
		{
			const SVector tile = *iter;
			
			if ( IsTileInside( tile ) )
			{
				tmpUnlockUnitsBuf.push_back( STmpUnlockUnitsBuf() );
				tmpUnlockUnitsBuf.back().tile = tile;
				tmpUnlockUnitsBuf.back().nUnitsBuf = unitsBuf[tile.y][tile.x];
				unitsBuf[tile.y][tile.x] = Max( 0, unitsBuf[tile.y][tile.x] - nDecrease );

				tmpUnlockUnitsBuf.back().bufLocking = 
					( buf[0].GetData( tile.x, tile.y ) << 0	) |
					( buf[1].GetData( tile.x, tile.y ) << 1 ) |
					( buf[2].GetData( tile.x, tile.y ) << 2 ) |
					( buf[3].GetData( tile.x, tile.y ) << 3 ) |
					( buf[4].GetData( tile.x, tile.y ) << 4 );

				buf[0].RemoveData( tile.x, tile.y );
				buf[1].RemoveData( tile.x, tile.y );
				buf[2].RemoveData( tile.x, tile.y );
				buf[3].RemoveData( tile.x, tile.y );
				buf[4].RemoveData( tile.x, tile.y );
			}
		}

		return true;
	}
	else
		return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStaticMap::RemoveTemporaryUnlocking()
{
	NI_ASSERT_T( tmpUnlockID != 0, "Wrong tmpUnlockID (0)" );
	
	for ( std::list<STmpUnlockUnitsBuf>::iterator iter = tmpUnlockUnitsBuf.begin(); iter != tmpUnlockUnitsBuf.end(); ++iter )
	{
		const int x = iter->tile.x;
		const int y = iter->tile.y;

		unitsBuf[y][x] = iter->nUnitsBuf;

		if ( iter->bufLocking & 0x1 )
			buf[0].SetData( x, y );
		if ( iter->bufLocking & 0x2 )
			buf[1].SetData( x, y );
		if ( iter->bufLocking & 0x4 )
			buf[2].SetData( x, y );
		if ( iter->bufLocking & 0x8 )
			buf[3].SetData( x, y );
		if ( iter->bufLocking & 0x10 )
			buf[4].SetData( x, y );
	}

	tmpUnlockID = 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStaticMap::RemoveTemporaryUnlockingByUnit( const int id )
{
	if ( tmpUnlockID == id )
		RemoveTemporaryUnlocking();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const int CStaticMap::GetTerrainPassabilityType( const int nX, const int nY ) const
{
	if ( !IsTileInside( nX, nY ) )
		return -1;
	else
		return passTypes[nY >> 1][nX >> 1];
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStaticMap::RemoveTerrainPassability( const int nX, const int nY )
{
	if ( IsTileInside( nX, nY ) )
	{
		const BYTE aiClass = GetPass( nX, nY ) == 0.0f ? AI_CLASS_ANY : (passClasses[ GetTerrainPassabilityType( nX, nY ) ] & ~0x80000000);
		UnlockTile( nX, nY, aiClass );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStaticMap::SetTerrainPassability( const int nX, const int nY, const int nTerrainType )
{
	if ( IsTileInside( nX, nY ) )
	{
		passTypes[nY >> 1][nX >> 1] = nTerrainType;		
		const BYTE aiClass = passabilities[nTerrainType] == 0.0f ? AI_CLASS_ANY : (passClasses[nTerrainType] & ~0x80000000);
		LockTile( nX, nY, aiClass );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStaticMap::UpdateMaxesForAddedRect( const int nDownX, const int nDownY, const int nUpX, const int nUpY )
{
	const int nMinX = Max( nDownX - SConsts::MAX_UNIT_RADIUS - 1, 0 );
	const int nMinY = Max( nDownY - SConsts::MAX_UNIT_RADIUS - 1, 0 );
	const int nMaxX = Min( nUpX + SConsts::MAX_UNIT_RADIUS + 1, GetSizeX() - 1 );
	const int nMaxY = Min( nUpY + SConsts::MAX_UNIT_RADIUS + 1, GetSizeY() - 1 );

	MemMode();

	SetMode( ELM_STATIC );
	UpdateMaxesForAddedTiles( nMinX, nMaxX, nMinY, nMaxY, classToIndex[AI_CLASS_ANY] );

	for ( int i = 1; i < 16; i *= 2 )
		UpdateMaxesForAddedTiles( nMinX, nMaxX, nMinY, nMaxY, classToIndex[i] );

	SetMode( ELM_ALL );
	UpdateMaxesForAddedTiles( nMinX, nMaxX, nMinY, nMaxY, classToIndex[AI_CLASS_ANY] );

	for ( int i = 1; i < 16; i *= 2 )
		UpdateMaxesForAddedTiles( nMinX, nMaxX, nMinY, nMaxY, classToIndex[i] );

/*	
	for ( int i = 1; i < 16; i *= 2 )
		CheckMaxes( nMinX, nMaxX, nMinY, nMaxY, classToIndex[i] );
	CheckMaxes( nMinX, nMaxX, nMinY, nMaxY, classToIndex[AI_CLASS_ANY] );

	SetMode( ELM_STATIC );
	for ( int i = 1; i < 16; i *= 2 )
		CheckMaxes( nMinX, nMaxX, nMinY, nMaxY, classToIndex[i] );
	CheckMaxes( nMinX, nMaxX, nMinY, nMaxY, classToIndex[AI_CLASS_ANY] );
*/
	RestoreMode();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStaticMap::UpdateMaxesForRemovedRect( const int nDownX, const int nDownY, const int nUpX, const int nUpY )
{
	const int nMinX = Max( nDownX - SConsts::MAX_UNIT_RADIUS - 1, 0 );
	const int nMinY = Max( nDownY - SConsts::MAX_UNIT_RADIUS - 1, 0 );
	const int nMaxX = Min( nUpX + SConsts::MAX_UNIT_RADIUS + 1, GetSizeX() - 1 );
	const int nMaxY = Min( nUpY + SConsts::MAX_UNIT_RADIUS + 1, GetSizeY() - 1 );

	MemMode();

	SetMode( ELM_STATIC );
	UpdateMaxesForRemovedTiles( nMinX, nMaxX, nMinY, nMaxY, classToIndex[AI_CLASS_ANY] );

	for ( int i = 1; i < 16; i *= 2 )
		UpdateMaxesForRemovedTiles( nMinX, nMaxX, nMinY, nMaxY, classToIndex[i] );

	SetMode( ELM_ALL );
	UpdateMaxesForRemovedTiles( nMinX, nMaxX, nMinY, nMaxY, classToIndex[AI_CLASS_ANY] );

	for ( int i = 1; i < 16; i *= 2 )
		UpdateMaxesForRemovedTiles( nMinX, nMaxX, nMinY, nMaxY, classToIndex[i] );

/*
	for ( int i = 1; i < 16; i *= 2 )
		CheckMaxes( nMinX, nMaxX, nMinY, nMaxY, classToIndex[i] );
	CheckMaxes( nMinX, nMaxX, nMinY, nMaxY, classToIndex[AI_CLASS_ANY] );

	SetMode( ELM_STATIC );
	for ( int i = 1; i < 16; i *= 2 )
		CheckMaxes( nMinX, nMaxX, nMinY, nMaxY, classToIndex[i] );
	CheckMaxes( nMinX, nMaxX, nMinY, nMaxY, classToIndex[AI_CLASS_ANY] );
*/
	RestoreMode();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStaticMap::UpdateTerrainPassabilityRect( const int nMinX, const int nMinY, const int nMaxX, const int nMaxY, bool bRemove )
{
	for ( int x = nMinX; x <= nMaxX; ++x )
	{
		for ( int y = nMinY; y <= nMaxY; ++y )
		{
			if ( bRemove )
				RemoveTerrainPassability( x, y );
			else
				SetTerrainPassability( x, y, GetTerrainPassabilityType( x, y ) );
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const bool CStaticMap::IsBridge( const SVector &tile ) const
{
	if ( IsTileInside( tile.x, tile.y ) )
		return bridgeTiles.GetData( tile.x, tile.y );
	else
		return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStaticMap::AddBridgeTile( const SVector &tile )
{
	if ( IsTileInside( tile.x, tile.y ) )
		bridgeTiles.SetData( tile.x, tile.y );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CStaticMap::RemoveBridgeTile( const SVector &tile )
{
	if ( IsTileInside( tile.x, tile.y ) )
		bridgeTiles.RemoveData( tile.x, tile.y );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//*******************************************************************
//*												CTemporaryUnitRectUnlocker								*
//*******************************************************************
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTemporaryUnitRectUnlocker::CTemporaryUnitRectUnlocker( const int nUnitID )
{
	bLocking = theStaticMap.TemporaryUnlockUnitRect( nUnitID );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CTemporaryUnitRectUnlocker::~CTemporaryUnitRectUnlocker()
{
	if ( bLocking )
		theStaticMap.RemoveTemporaryUnlocking();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
