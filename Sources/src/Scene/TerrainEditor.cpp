#include "StdAfx.h"

#include "TerrainInternal.h"
#include "../Image/Image.h"
#include "TerrainBuilder.h"
#include "../Misc/Spline.h"
inline bool GetTileIndexLocal( const CVec3 &point, int *pnX, int *pnY, 
															 const float fTerraSizeX, const float fTerraSizeY, 
															 const float fCellSize,
															 bool bTerrainCoords, bool isExact )
{
	CVec3 vAxisX, vAxisY;
	GetLineEq( 0, 0, 1, 0, &vAxisX.x, &vAxisX.y, &vAxisX.z );
	GetLineEq( 0, 1, 0, 0, &vAxisY.x, &vAxisY.y, &vAxisY.z );

	bool result = true;
	{
		const float fDist = vAxisY.x*point.x + vAxisY.y*point.y + vAxisY.z;
		if ( isExact )
		{
			*pnX = int( fDist / fCellSize );
		}
		else
		{
			*pnX = int( fDist / fCellSize + 0.5f );
		}
		if ( (fDist < 0) || (fDist > fTerraSizeX) )
		{
			result = false;
		}
	}
	{
		const float fDist = vAxisX.x*point.x + vAxisX.y*point.y + vAxisX.z;
		if ( bTerrainCoords )
		{
			if ( isExact )
			{
				*pnY = int( (fTerraSizeY - fDist) / fCellSize );
			}
			else
			{
				*pnY = int( (fTerraSizeY - fDist) / fCellSize + 0.5f );
			}
		}
		else
		{
			if ( isExact )
			{
				*pnY = int( fDist / fCellSize );
			}
			else
			{
				*pnY = int( fDist / fCellSize + 0.5f );
			}
		}
		if ( (fDist < 0) || (fDist > fTerraSizeY) )
		{
			result = false;
		}
	}
	return result;
}
bool CTerrain::GetTileIndex( const CVec3 &point, int *pnX, int *pnY, bool isExact )
{
	const float fTerraSizeX = terrainInfo.patches.GetSizeX() * fWorldCellSize * STerrainPatchInfo::nSizeX;
	const float fTerraSizeY = terrainInfo.patches.GetSizeY() * fWorldCellSize * STerrainPatchInfo::nSizeY;
	return GetTileIndexLocal( point, pnX, pnY, fTerraSizeX, fTerraSizeY, fWorldCellSize, true, isExact );
}
bool CTerrain::GetAITileIndex( const CVec3 &point, int *pnX, int *pnY, bool isExact )
{
	const float fTerraSizeX = terrainInfo.patches.GetSizeX() * fWorldCellSize * STerrainPatchInfo::nSizeX;
	const float fTerraSizeY = terrainInfo.patches.GetSizeY() * fWorldCellSize * STerrainPatchInfo::nSizeY;
	return GetTileIndexLocal( point, pnX, pnY, fTerraSizeX, fTerraSizeY, fWorldCellSize * 0.5f, false, isExact );
}
class CCrossTileEqualFunctional
{
	const SCrossTileInfo &cross;
public:
	explicit CCrossTileEqualFunctional( const SCrossTileInfo &_cross ) : cross( _cross ) {  }
	bool operator()( const SCrossTileInfo &_cross ) const
	{
		return (cross.x == _cross.x) && (cross.y == _cross.y) && (cross.tile == _cross.tile);
	}
};
void CTerrain::SetTile( int x, int y, BYTE tile )
{
	terrainInfo.tiles[y][x].tile = tile;//tilesetDesc.terrtypes[tile].GetMapsIndex();
	terrainInfo.tiles[y][x].noise = terrabuild.HasNoise( tile );
}
BYTE CTerrain::GetTile( int x, int y )
{
	return 	terrainInfo.tiles[y][x].tile ;//tilesetDesc.terrtypes[tile].GetMapsIndex();
}

void CTerrain::SetShade( int x, int y, BYTE shade )
{
	terrainInfo.altitudes[y][x].shade = shade;
}
BYTE CTerrain::GetShade( int x, int y )
{
	return terrainInfo.altitudes[y][x].shade;
}
void CTerrain::Update( const CTRect<int> &rcPatches )
{
	int nTerrainSizeX = terrainInfo.patches.GetSizeX() * STerrainPatchInfo::nSizeX;
	int nTerrainSizeY = terrainInfo.patches.GetSizeY() * STerrainPatchInfo::nSizeY;
	if ( nTerrainSizeX * nTerrainSizeY )
	{
		CTRect<int> rcSuper( 0, 0, nTerrainSizeX, nTerrainSizeY );
		CTerrainBuilder &builder = terrabuild;

		{
			CTRect<int> rcPatchesRect;
			CTRect<int> rcTemp;

			CTPoint<int> point = rcPatches.GetLeftTop();
			builder.GetPatchRect( point.x, point.y, &rcTemp );
			rcPatchesRect.left = rcTemp.left;
			rcPatchesRect.top = rcTemp.top;

			point = rcPatches.GetRightBottom();
			builder.GetPatchRect( point.x, point.y, &rcTemp );
			rcPatchesRect.right = rcTemp.right;
			rcPatchesRect.bottom = rcTemp.bottom;

			builder.PreprocessMapSegment( terrainInfo.tiles, rcPatchesRect );
		}

		for ( int i = rcPatches.top; i <= rcPatches.bottom; ++i )
		{
			for ( int j = rcPatches.left; j <= rcPatches.right; ++j )
			{
				CTerrainBuilder::SComplexCrosses newCrosses;
				CTRect<int> rcRect;
				builder.GetPatchRect( j, i, &rcRect );
				builder.MapSegmentGenerateCrosses( terrainInfo.tiles, rcRect, rcSuper, &newCrosses );
				{
					builder.CopyCrosses( &terrainInfo.patches[i][j], newCrosses );
					/*
					STerrainPatchInfo::CCrossesList &oldCrosses = terrainInfo.patches[i][j].crosses;
					terrainInfo.patches[i][j].crosses.clear();
					terrainInfo.patches[i][j].crosses.reserve( newCrosses.size() );
					for ( CTerrainBuilder::CCrossesList::const_iterator it = newCrosses.begin(); it != newCrosses.end(); ++it )
						terrainInfo.patches[i][j].crosses.push_back( *it );
						*/
					for ( std::list<STerrainPatch>::iterator it = patches.begin(); it != patches.end(); ++it )
					{
						if ( (it->nX == j) && (it->nY == i) )
						{
							patches.erase( it );
							break;
						}
					}
				}
			}
		}
	}
	
	vOldAnchor.x = -1000000;
	vOldAnchor.y = -1000000;
}
bool CTerrain::Import( IImage *pImage )
{
	CTerrainBuilder builder( tilesetDesc, crossetDesc/*, roadsetDesc*/ );
	terrainInfo.patches.Clear();
	for ( int i=0; i<terrainInfo.patches.GetSizeX()*terrainInfo.patches.GetSizeY(); ++i )
	{
		terrainInfo.patches.GetBuffer()[i].basecrosses.clear();
		terrainInfo.patches.GetBuffer()[i].layercrosses.clear();
		terrainInfo.patches.GetBuffer()[i].noisecrosses.clear();
		/**
		for ( int j=0; j<nNumRoadTypes; ++j )
			terrainInfo.patches.GetBuffer()[i].roads[j].clear();
		/**/
	}
	terrainInfo.tiles.Clear();
	builder.PreprocessMap( pImage, &terrainInfo );
	patches.clear();
	vOldAnchor.x = -1000000;
	vOldAnchor.y = -1000000;
	return true;
}
IImage* CTerrain::Export()
{
	if ( (terrainInfo.tiles.GetSizeX() == 0) || (terrainInfo.tiles.GetSizeY() == 0) )
		return 0;
	IImageProcessor *pIP = GetImageProcessor();
	IImage *pImage = pIP->CreateImage( terrainInfo.tiles.GetSizeX(), terrainInfo.tiles.GetSizeY() );
	pImage->Set( 0 );
	SColor *pColors = pImage->GetLFB();
	SMainTileInfo *pTiles = terrainInfo.tiles.GetBuffer();
	CTerrainBuilder builder( tilesetDesc, crossetDesc/*, roadsetDesc*/ );
	for ( int i=0; i<pImage->GetSizeX()*pImage->GetSizeY(); ++i )
	{
		pColors[i].r = builder.GetTerrainType( pTiles[i].tile ) * 16;
		pColors[i].b = builder.GetTerrainType( pTiles[i].tile );//pTiles[i].shade;
		pColors[i].g = 255;
	}
	return pImage;
}
/**
void CTerrain::SetRoads( const SRoadItem *pItems, int nNumItems ) 
{
	terrainInfo.roads.resize( nNumItems );
	if ( nNumItems > 0 )
	{
		memcpy( &( terrainInfo.roads[0] ), pItems, nNumItems * sizeof( SRoadItem ) );
	}
}
/**/
int CTerrain::AddRiver( const SVectorStripeObject &river )
{
	int nRiverID = -1;
	bool bContinue = true;
	while ( bContinue )
	{
		bContinue = false;
		nRiverID = rand();
		for ( TVSOList::const_iterator it = terrainInfo.rivers.begin(); it != terrainInfo.rivers.end(); ++it )
		{
			if ( it->nID == nRiverID )
			{
				bContinue = true;
				break;
			}
		}
	}
	terrainInfo.rivers.push_back( river );
	terrainInfo.rivers.back().nID = nRiverID;
	rivers.push_back( CTerrainWater() );
	rivers.back().Init( terrainInfo.rivers.back(), this );
	rivers.back().SelectPatches( roadPatches );
	return nRiverID;
}

bool CTerrain::UpdateRiver( const int nRiverID )
{
	for ( int i = 0; i != terrainInfo.rivers.size(); ++i )
	{
		if ( terrainInfo.rivers[i].nID == nRiverID )
		{
			rivers[i].Init( terrainInfo.rivers[i], this );
			rivers[i].SelectPatches( roadPatches );
			return true;
		}
	}
	return false;
}

bool CTerrain::RemoveRiver( const int nRiverID )
{
	for ( int i = 0; i != terrainInfo.rivers.size(); ++i )
	{
		if ( terrainInfo.rivers[i].nID == nRiverID )
		{
			terrainInfo.rivers.erase( terrainInfo.rivers.begin() + i );
			rivers.erase( rivers.begin() + i );
			return true;
		}
	}
	return false;
}
int CTerrain::AddRoad( const SVectorStripeObject &road )
{
	int nRoadID = -1;
	bool bContinue = true;
	while ( bContinue )
	{
		bContinue = false;
		nRoadID = rand();
		for ( TVSOList::const_iterator it = terrainInfo.roads3.begin(); it != terrainInfo.roads3.end(); ++it )
		{
			if ( it->nID == nRoadID )
			{
				bContinue = true;
				break;
			}
		}
	}
	terrainInfo.roads3.push_back( road );
	terrainInfo.roads3.back().nID = nRoadID;
	roads.push_back( CTerrainRoad() );
	roads.back().Init( terrainInfo.roads3.back(), this );
	roads.back().SelectPatches( roadPatches );
	std::sort( roads.begin(), roads.end() );
	return nRoadID;
}

bool CTerrain::UpdateRoad( const int nRoadID )
{
	CTerrainRoad *pRoad = 0;
	SVectorStripeObject *pRoadDesc = 0;
	for ( int i = 0; i != terrainInfo.roads3.size(); ++i )
	{
		if ( roads[i].GetID() == nRoadID )
			pRoad = &( roads[i] );
		if ( terrainInfo.roads3[i].nID == nRoadID )
			pRoadDesc = &( terrainInfo.roads3[i] );
		if ( pRoad && pRoadDesc )
		{
			pRoad->Init( *pRoadDesc, this );
			pRoad->SelectPatches( roadPatches );
			return true;
		}
	}
	return false;
}

bool CTerrain::RemoveRoad( const int nRoadID )
{
	for ( int i = 0; i != terrainInfo.roads3.size(); ++i )
	{
		if ( terrainInfo.roads3[i].nID == nRoadID )
		{
			terrainInfo.roads3.erase( terrainInfo.roads3.begin() + i );
			break;
		}
	}
	for ( int i = 0; i != roads.size(); ++i )
	{
		if ( roads[i].GetID() == nRoadID )
		{
			roads.erase( roads.begin() + i );
			break;
		}
	}
	return false;
}

int SliceSpline( const CAnalyticBSpline2 &spline, std::list<SVectorStripeObjectPoint> *points, float *pfRest, const float fStep )
{
	const float fSplineLength = spline.GetLengthAdaptive( 1 );
	const float dt = fStep / ( fSplineLength * 10.0f );
	int nCounter = 0;
	float fLen = *pfRest + fStep;
	float fLastT, fT = 0;
	CVec2 vLastPos, vPos = spline.Get( fT );
	while ( 1 ) 
	{
		fLen -= fStep;
		while ( fLen < fStep ) 
		{
			vLastPos = vPos;
			fT += dt;
			vPos = spline.Get( fT );
			fLen += fabs( vLastPos - vPos );
			if ( fT > 1 ) 
				break;
		}
		if ( (fT > 1) || (fLen < fStep) ) 
			break;
		SVectorStripeObjectPoint point;
		point.vPos = spline( fT );
		point.vNorm = spline.GetDiff1( fT );
		point.vNorm.Set( -point.vNorm.y, point.vNorm.x, 0 );
		point.bKeyPoint = false;
		Normalize( &point.vNorm );
		point.fRadius = spline.GetCurvatureRadius( fT );
		points->push_back( point );
		fLastT = fT;
		++nCounter;
	}
	points->back().bKeyPoint = true;
	*pfRest = spline.GetLength( fLastT, 1.0f, 100 );
	return nCounter;
}
void CTerrain::SampleCurve( const CVec3 *_plots, int nNumPlots, float fStep, 
													  SVectorStripeObjectPoint **ppSamples, int *pnNumSamples )
{
	std::vector<CVec3> plots( nNumPlots + 2 );
	memcpy( &(plots[1]), _plots, nNumPlots*sizeof(CVec3) );
	plots[0] = _plots[0] - ( _plots[1] - _plots[0] );
	plots[nNumPlots + 1] = _plots[nNumPlots - 1] + (_plots[nNumPlots - 1] - _plots[nNumPlots - 2]);
	const float fSplineStep = fStep;
	float fRestLength = fSplineStep - 1e-8f;
	std::list<SVectorStripeObjectPoint> points;
	CAnalyticBSpline2 spline;
	int nCounter = 0;
	for ( int i = 0; i != (plots.size() - 3); ++i )
	{
		spline.Setup( plots[i], plots[i + 1], plots[i + 2], plots[i + 3] );
		nCounter += SliceSpline( spline, &points, &fRestLength, fSplineStep );
	}
	if ( !points.empty() )
		points.front().bKeyPoint = true;
	*ppSamples = GetTempBuffer<SVectorStripeObjectPoint>( nCounter );
	int i = 0;
	for ( std::list<SVectorStripeObjectPoint>::const_iterator it = points.begin(); it != points.end(); ++it, ++i )
	{
		(*ppSamples)[i] = *it;
		(*ppSamples)[i].fWidth = fWorldCellSize*4;
	}
	*pnNumSamples = nCounter;
}
void CTerrain::SmoothCurveWidth( SVectorStripeObjectPoint *points, const int nNumPoints )
{
	int nNumKeyPoints = 0;
	for ( SVectorStripeObjectPoint *it = points; it != points + nNumPoints; ++it )
		nNumKeyPoints += it->bKeyPoint;
	std::vector<int> keypoints;
	keypoints.reserve( nNumKeyPoints );
	for ( int i = 0; i != nNumPoints; ++i )
	{
		if ( points[i].bKeyPoint )
			keypoints.push_back( i );
	}
	std::vector<float> widthes( nNumPoints );
	for ( int i = 0; i != nNumPoints; ++i )
		widthes[i] = points[i].fWidth;
	std::vector<int> indices;
	indices.reserve( nNumKeyPoints + 4 );
	indices.push_back( 0 );
	indices.push_back( 0 );
	for ( int i=0; i<nNumKeyPoints; ++i )
		indices.push_back( i );
	indices.push_back( nNumKeyPoints - 1 );
	indices.push_back( nNumKeyPoints - 1 );
	CAnalyticBSpline spline;
	for ( int i = 0; i != (indices.size() - 3); ++i )
	{
		const int idx0 = keypoints[ indices[i + 0] ];
		const int idx1 = keypoints[ indices[i + 1] ];
		const int idx2 = keypoints[ indices[i + 2] ];
		const int idx3 = keypoints[ indices[i + 3] ];
		if ( idx2 <= idx1 )
			continue;
		spline.Setup( points[idx0].fWidth, points[idx1].fWidth, points[idx2].fWidth, points[idx3].fWidth );
		for ( int j = idx1; j != idx2; ++j )
		{
			const float t = float( j - idx1 ) / float( idx2 - idx1 );
			widthes[j] = spline( t );
		}
	}
	for ( int i = 0; i != nNumPoints; ++i )
		points[i].fWidth = widthes[i];
}
