#include "stdafx.h"

#include "RectTiles.h"
#include "AIStaticMap.h"
#include "UnitsIterators2.h"
#include "AIUnit.h"
#include "AIInternalConsts.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
extern CStaticMap theStaticMap;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <int N>
bool ProcessQuadrangleTiles( const CVec2 &v1, const CVec2 &v2, const CVec2 &v3, const CVec2 &v4, CTilesSet *pTiles, const BYTE aiClass, const SGenericNumber<N> &p )
{
	if ( N )
		pTiles->clear();

	// ����� �� ������� �����
	if ( !N )
	{
		const int nSizeX = theStaticMap.GetSizeX() * SConsts::TILE_SIZE;
		const int nSizeY = theStaticMap.GetSizeY() * SConsts::TILE_SIZE;
		if ( v1.x < 0 || v1.y < 0 || v2.x < 0 || v2.y < 0 || v3.x < 0 || v3.y < 0 || v4.x < 0 || v4.y < 0 ||
				 v1.x >= nSizeX || v1.y >= nSizeY || v2.x >= nSizeX || v2.y >= nSizeY ||
				 v3.x >= nSizeX || v3.y >= nSizeY || v4.x >= nSizeX || v4.y >= nSizeY )
			return true;
	}

	const CVec2 center = 0.25f * ( v1 + v2 + v3 + v4 );
	const CLine2 line1( v1, v2 );
	const int sign1 = line1.GetSign( center );
	const CLine2 line2( v2, v3 );
	const int sign2 = line2.GetSign( center );
	const CLine2 line3( v3, v4 );
	const int sign3 = line3.GetSign( center );
	const CLine2 line4( v4, v1 );
	const int sign4 = line4.GetSign( center );

	const SVector tile1 = AICellsTiles::GetTile( v1 );
	const SVector tile2 = AICellsTiles::GetTile( v2 );
	const SVector tile3 = AICellsTiles::GetTile( v3 );
	const SVector tile4 = AICellsTiles::GetTile( v4 );

	SVector p1( tile1 ), p2( tile1 );
	if ( tile2.y < p1.y ) p1 = tile2;
	if ( tile2.y > p2.y ) p2 = tile2;
	if ( tile3.y < p1.y ) p1 = tile3;
	if ( tile3.y > p2.y ) p2 = tile3;
	if ( tile4.y < p1.y ) p1 = tile4;
	if ( tile4.y > p2.y ) p2 = tile4;

	if ( p1 == p2 )
	{
		if ( N )
		{
			if ( theStaticMap.IsTileInside( p1 ) )
				pTiles->push_back( p1 );
		}
		else if ( theStaticMap.IsLocked( p1.x, p1.y, aiClass ) && theStaticMap.IsTileInside( p1 ) )
			return true;
	}
	else
	{
		CBres bres;
		bres.InitPoint( p1, p2 );

		bool bFlag = true;
		while ( bFlag )
		{
			SVector curTile( bres.GetDirection() );
			CVec2 curPoint( AICellsTiles::GetPointByTile( curTile ) );

			if ( line1.GetSign( curPoint ) == sign1 && line2.GetSign( curPoint ) == sign2 &&
					 line3.GetSign( curPoint ) == sign3 && line4.GetSign( curPoint ) == sign4 )
			{
				if ( theStaticMap.IsTileInside( curTile ) )
				{
					if ( N )
						pTiles->push_back( curTile );
					else if ( theStaticMap.IsLocked( curTile, aiClass ) )
						return true;
				}
			}

			--curTile.x;
			curPoint.x -= SConsts::TILE_SIZE;
			while ( line1.GetSign( curPoint ) == sign1 && line2.GetSign( curPoint ) == sign2 &&
							line3.GetSign( curPoint ) == sign3 && line4.GetSign( curPoint ) == sign4 )
			{
				if ( theStaticMap.IsTileInside( curTile ) )
				{
					if ( N )
						pTiles->push_back( curTile );
					else if ( theStaticMap.IsLocked( curTile, aiClass ) )
						return true;
				}

				--curTile.x;
				curPoint.x -= SConsts::TILE_SIZE;
			}

			curTile = bres.GetDirection();
			curPoint = AICellsTiles::GetPointByTile( curTile );

			++curTile.x;
			curPoint.x += SConsts::TILE_SIZE;
			while ( line1.GetSign( curPoint ) == sign1 && line2.GetSign( curPoint ) == sign2 &&
							line3.GetSign( curPoint ) == sign3 && line4.GetSign( curPoint ) == sign4 )
			{
				if ( theStaticMap.IsTileInside( curTile ) )
				{
					if ( N )
						pTiles->push_back( curTile );
					else if ( theStaticMap.IsLocked( curTile, aiClass ) )
						return true;
				}

				++curTile.x;
				curPoint.x += SConsts::TILE_SIZE;
			}
			
			if ( bres.GetDirection() == p2 )
				bFlag = false;
			else
			{
				const int y  = bres.GetDirection().y;
				do
				{
					bres.MakePointStep();
					if ( bres.GetDirection() == p2 )
					{
						bFlag = false;
						break;
					}
				} while ( bres.GetDirection().y == y );
			}
		}
	}
	
	if ( N )
	{ 
		const SVector tile( AICellsTiles::GetTile( center ) );
		if ( pTiles->empty() && theStaticMap.IsTileInside( tile ) )
			pTiles->push_back( tile );
	}

	return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void GetTilesCoveredByQuadrangle( const CVec2 &v1, const CVec2 &v2, const CVec2 &v3, const CVec2 &v4, CTilesSet *pTiles )
{
	ProcessQuadrangleTiles( v1, v2, v3, v4, pTiles, 0, SGenericNumber<1>() );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void GetTilesCoveredByRect( const SRect &rect, CTilesSet *pTiles )
{
	ProcessQuadrangleTiles( rect.v1, rect.v2, rect.v3, rect.v4, pTiles, 0, SGenericNumber<1>() );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool IsRectOnLockedTiles( const SRect &rect, const BYTE aiClass )
{
	return ProcessQuadrangleTiles( rect.v1, rect.v2, rect.v3, rect.v4, 0, aiClass, SGenericNumber<0>() );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void GetTilesCoveredByRectSides( const SRect & rect, CTilesSet * pTiles )
{
	CTilesCollector a(pTiles);

	MakeLine2( rect.v1.x/SConsts::TILE_SIZE, rect.v1.y/SConsts::TILE_SIZE, rect.v2.x/SConsts::TILE_SIZE, rect.v2.y/SConsts::TILE_SIZE, a );
	MakeLine2( rect.v2.x/SConsts::TILE_SIZE, rect.v2.y/SConsts::TILE_SIZE, rect.v3.x/SConsts::TILE_SIZE, rect.v3.y/SConsts::TILE_SIZE, a );
	MakeLine2( rect.v3.x/SConsts::TILE_SIZE, rect.v3.y/SConsts::TILE_SIZE, rect.v4.x/SConsts::TILE_SIZE, rect.v4.y/SConsts::TILE_SIZE, a );
	MakeLine2( rect.v4.x/SConsts::TILE_SIZE, rect.v4.y/SConsts::TILE_SIZE, rect.v1.x/SConsts::TILE_SIZE, rect.v1.y/SConsts::TILE_SIZE, a );
}
namespace
{
struct SVectorHash
{
	int operator()( const SVector & v) const
	{
		return (v.x * 65535) + v.y;
	}
};
typedef std::unordered_map<SVector, bool, SVectorHash> CSVectorHash;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void GetTilesNextToRect( const SRect &rect, CTilesSet *pTiles )
{
	// �����! ���-�� ����� �������� � �������� �������
	CTilesSet tilesUnderRect;
	GetTilesCoveredByRect( rect, &tilesUnderRect );
	CSVectorHash tilesUnderRect1;

	for ( CTilesSet::iterator i = tilesUnderRect.begin(); i != tilesUnderRect.end(); ++i )
		tilesUnderRect1[*i] = true;
	
	CSVectorHash adjustedTiles;
	
	for ( CTilesSet::iterator i = tilesUnderRect.begin(); i != tilesUnderRect.end(); ++i )
	{
		if ( tilesUnderRect1.find( SVector(i->x+1, i->y) ) == tilesUnderRect1.end() )
			adjustedTiles[ SVector(i->x+1, i->y) ];

		if ( tilesUnderRect1.find( SVector(i->x-1, i->y) ) == tilesUnderRect1.end() )
			adjustedTiles[ SVector(i->x-1, i->y) ];

		if ( tilesUnderRect1.find( SVector(i->x, i->y+1) ) == tilesUnderRect1.end() )
			adjustedTiles[ SVector(i->x, i->y+1) ];
		
		if ( tilesUnderRect1.find( SVector(i->x, i->y-1) ) == tilesUnderRect1.end() )
			adjustedTiles[ SVector(i->x, i->y-1) ];
	}

	for ( CSVectorHash::iterator i = adjustedTiles.begin(); i != adjustedTiles.end(); ++i )
	{
		pTiles->push_back( i->first );
	}
}
void GetTilesNextToRect( const SRect &rect, CTilesSet *pTiles, const WORD wDirExclude )
{
	GetTilesNextToRect( rect, pTiles );
	for ( CTilesSet::iterator it = pTiles->begin(); it != pTiles->end(); )
	{
		const WORD wDirToTile = GetDirectionByVector( AICellsTiles::GetPointByTile( *it ) - rect.center );
		if ( DirsDifference( wDirToTile, wDirExclude ) < (65535 / 8) )
			it = pTiles->erase( it );
		else 
			++it;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <class T> 
void GetTilesUnderRectSide( const SRect &rect, CTilesSet *pTiles, const WORD wDir, T need )
{
	CTilesCollector a(pTiles);

	//������� � DWORD ����� �� �������������
	DWORD arDir[4];
	DWORD dwDir = wDir;
	arDir[0] = GetDirectionByVector( (rect.v1 +rect.v2)/2 - rect.center );
	arDir[1] = GetDirectionByVector( (rect.v2 +rect.v3)/2 - rect.center );
	arDir[2] = GetDirectionByVector( (rect.v3 +rect.v4)/2 - rect.center );
	arDir[3] = GetDirectionByVector( (rect.v4 +rect.v1)/2 - rect.center );
	
	int iMin = 0;
	DWORD dwMin = 65535;
	for( int i=0; i< 4; ++i )
	{
		const WORD wDirsDiff = DirsDifference( arDir[i], dwDir );
		if( dwMin >  wDirsDiff )
		{
			iMin = i;
			dwMin  = wDirsDiff;
		}
	}

	if ( need(iMin,0) )
	{//wDir �� ����� ����� arDir[0] � arDir[1]
		MakeLine2( rect.v1.x/SConsts::TILE_SIZE, rect.v1.y/SConsts::TILE_SIZE, rect.v2.x/SConsts::TILE_SIZE, rect.v2.y/SConsts::TILE_SIZE, a );
	}

	if ( need(iMin,1) )
	{//wDir �� ����� ����� arDir[1] � arDir[2]
		MakeLine2( rect.v2.x/SConsts::TILE_SIZE, rect.v2.y/SConsts::TILE_SIZE, rect.v3.x/SConsts::TILE_SIZE, rect.v3.y/SConsts::TILE_SIZE, a );
	}

	if ( need(iMin,2) )
	{//wDir no ����� ����� arDir[1] � arDir[2]
		MakeLine2( rect.v3.x/SConsts::TILE_SIZE, rect.v3.y/SConsts::TILE_SIZE, rect.v4.x/SConsts::TILE_SIZE, rect.v4.y/SConsts::TILE_SIZE, a );
	}

	if ( need(iMin,3) )
	{//wDir no ����� ����� arDir[1] � arDir[2]
		MakeLine2( rect.v4.x/SConsts::TILE_SIZE, rect.v4.y/SConsts::TILE_SIZE, rect.v1.x/SConsts::TILE_SIZE, rect.v1.y/SConsts::TILE_SIZE, a );
	}
}
struct SExceptDirNeed
{
	bool operator()( const int nTest, const int nDesire) const
	{ return nTest != nDesire; }
};
struct SOnlyDirNeed
{
	bool operator()( const int nTest, const int nDesire) const
	{ return nTest == nDesire; }
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void GetTilesCoveredBySide( const SRect &rect, CTilesSet *pTiles, WORD wDir )
{
	SOnlyDirNeed a;
	GetTilesUnderRectSide( rect, pTiles, wDir, a );	
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void GetTilesCoveredByRectSides( const SRect &rect, CTilesSet *pTiles, WORD wDir )
{
	SExceptDirNeed a;
	GetTilesUnderRectSide( rect, pTiles, wDir, a );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool IsMapFullyFree( const SRect &rect, interface IBasePathUnit *pUnit )
{
	if ( IsRectOnLockedTiles( rect, AI_CLASS_ANY )				||
			 IsRectOnLockedTiles( rect, AI_CLASS_HUMAN )			||
			 IsRectOnLockedTiles( rect, AI_CLASS_HALFTRACK )	||
			 IsRectOnLockedTiles( rect, AI_CLASS_TRACK ) )
		return false;

	int nMinX = Min( Min( rect.v1.x, rect.v2.x ), Min( rect.v3.x, rect.v4.x ) );
	int nMinY = Min( Min( rect.v1.y, rect.v2.y ), Min( rect.v3.y, rect.v4.y ) );
	int nMaxX = Max( Max( rect.v1.x, rect.v2.x ), Max( rect.v3.x, rect.v4.x ) );
	int nMaxY = Max( Max( rect.v1.y, rect.v2.y ), Max( rect.v3.y, rect.v4.y ) );

	const CVec2 vAABBHalfSize( ( nMinX + nMaxX ) * 0.5f, ( nMinY + nMaxY ) * 0.5f );

	for ( CUnitsIter<0,3> iter( 0, ANY_PARTY, rect.center, Max( vAABBHalfSize.x, vAABBHalfSize.y ) ); !iter.IsFinished(); iter.Iterate() )
	{
		CAIUnit *pIterUnit = *iter;
		if ( pIterUnit && pIterUnit->IsValid() )
		{
			if ( pIterUnit != pUnit && 
					 ( pIterUnit->IsAlive() || !pIterUnit->GetStats()->IsInfantry() ) &&
						 pIterUnit->GetUnitRect().IsIntersected( rect ) )
				return false;
		}
	}

	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTilesCollector::operator() ( float x, float y ) 
{ 
	if ( theStaticMap.IsTileInside( x, y ) )
		pTiles->push_back( SVector( x, y ) );

	return true; 
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
