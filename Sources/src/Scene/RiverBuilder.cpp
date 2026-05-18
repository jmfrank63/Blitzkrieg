#include "StdAfx.h"

#include "TerrainWater.h"
#include "TerrainRoad.h"
#include "..\GFX\GFX.h"
#include "..\AILogic\AILogic.h"
#include "..\Misc\Checker.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static const DWORD LAYER_BORDER_LEFT	= 0x00000001;
static const DWORD LAYER_BORDER_RIGHT	= 0x00000002;
static const DWORD LAYER_CENTER				= 0x00000004;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
inline float Rand( float from, float to ) 
{ 
	return from != to ? from + (float( rand() ) / float( RAND_MAX )) * ( to - from ) : from;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CVec3 GetPos3( const CVec3 &vPos )
{
	CVec2 vPos2;
	Vis2AI( &vPos2, vPos.x, vPos.y );
	return CVec3( vPos.x, vPos.y, AI2VisZ(GetSingleton<IAILogic>()->GetZ(vPos2)) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DWORD GetShadeColor( const CVec3 &vPos, const CVec3 &vSunDir )
{
	CVec2 vPos2;
	Vis2AI( &vPos2, vPos.x, vPos.y );
	const DWORD dwNormal = GetSingleton<IAILogic>()->GetNormal( vPos2 );
	CVec3 vNormal = DWORDToVec3( dwNormal );
	::Normalize( &vNormal );
	const DWORD shade = BYTE( Clamp( -(vNormal * vSunDir), 0.6f, 1.0f ) * 255.0f );
	return (shade << 16) | (shade << 8) | shade;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
DWORD MakeOpacity( DWORD color, float fCoeff )
{
	return DWORD( ( color >> 24 ) * fCoeff ) << 24;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
inline void AddPatchCoords( const CVec3 &vPos, std::list<DWORD> &patch )
{
	const int nPosX = vPos.x >= 0 ? int( vPos.x / (fWorldCellSize * STerrainPatchInfo::nSizeX) ) & 0x0000ffff : 0;
	const int nPosY = vPos.y >= 0 ? int( vPos.y / (fWorldCellSize * STerrainPatchInfo::nSizeY) ) & 0x0000ffff : 0;
	const DWORD dwPatch = ( DWORD(nPosX) << 16 ) | DWORD(nPosY);
	if ( std::find(patch.begin(), patch.end(), dwPatch) == patch.end() ) 
		patch.push_back( dwPatch );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <class T1, class T2>
struct SLess1st
{
	bool operator()( const std::pair<T1, T2> &p1, const std::pair<T1, T2> &p2 ) const
	{
		return p1.first < p2.first;
	}
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void BuildLayer( const std::vector<SVectorStripeObjectPoint> &points, 
								 const int nMaxNumCells, float fRandomRange, 
								 const SVectorStripeObject::SLayer &layer, DWORD flags, 
								 STVOLayer *pLayer )
{
	CVec3 vSunDir( GetGlobalVar("Scene.SunLight.Direction.X", 1.0f),
								 GetGlobalVar("Scene.SunLight.Direction.Y", 1.0f),
								 GetGlobalVar("Scene.SunLight.Direction.Z", -2.0f) );
	Normalize( &vSunDir );
	//
	const int nCounter = points.size();
	const int WIDTH = layer.nNumCells;
	DWORD colorLeft, colorRight, colorCenter;
	float fTopWidth, fBottomWidth, fCenterRelWidth;
	int nTopCell, nBottomCell;
	float fStartTV, fTextureStepV;
	int N;
	//
	switch ( flags ) 
	{
		case LAYER_BORDER_LEFT:
			NI_ASSERT_T( layer.nNumCells == 1, "Wrong number of cells for left border - must be 1" );
			colorLeft = DWORD( layer.opacityBorder ) << 24;
			colorCenter = DWORD( layer.opacityCenter ) << 24;
			colorRight = colorCenter;
			N = WIDTH + 1;
			fTopWidth = 1.0f;
			fBottomWidth = 1.0f - layer.fRelWidth;
			nTopCell = WIDTH/2 - 1;
			nBottomCell = 0;
			fStartTV = 0;
			fTextureStepV = layer.fRelWidth / float ( N - 1 ) / 2.0f;
			fCenterRelWidth = 1.0f - layer.fRelWidth / 2.0f;
			break;
		case LAYER_BORDER_RIGHT:
			NI_ASSERT_T( layer.nNumCells == 1, "Wrong number of cells for right border - must be 1" );
			colorRight = DWORD( layer.opacityBorder ) << 24;
			colorCenter = DWORD( layer.opacityCenter ) << 24;
			colorLeft = colorCenter;
			N = WIDTH + 1;
			fTopWidth = -( 1.0f - layer.fRelWidth );
			fBottomWidth = -1.0f;
			nTopCell = WIDTH/2 - 1;
			nBottomCell = 0;
			fStartTV = 1.0f - layer.fRelWidth/2.0f;
			fTextureStepV = layer.fRelWidth / float ( N - 1 ) / 2.0f;
			fCenterRelWidth = -( 1.0f - layer.fRelWidth / 2.0f );
			break;
		case LAYER_CENTER:
			colorLeft = DWORD( layer.opacityBorder ) << 24;
			colorRight = DWORD( layer.opacityBorder ) << 24;
			colorCenter = DWORD( layer.opacityCenter ) << 24;
			N = WIDTH*2 + 1;
			fTopWidth = layer.fRelWidth;
			fBottomWidth = -layer.fRelWidth;
			nTopCell = WIDTH - 1;
			nBottomCell = -( WIDTH - 1 );
			fStartTV = ( 1.0f - layer.fRelWidth ) / 2.0f;
			fCenterRelWidth = 0;
			fTextureStepV = layer.fRelWidth / float ( N - 1 );
			break;
		default:
			NI_ASSERT_TF( false, "Unknown layer type", return );
	}
	//
	std::vector<STVOVertex> regularvertices( nCounter * N );
	const float fTextureStepU = layer.fTextureStep;
	//
	float tu = 0;
	STVOVertex *verts = &( regularvertices[0] );
	typedef std::list<DWORD> CPatchesList;
	typedef std::vector<CPatchesList> CPointPatchesList;
	CPointPatchesList pointpatches( points.size() );
	CPointPatchesList::iterator pointpatch = pointpatches.begin();
	for ( std::vector<SVectorStripeObjectPoint>::const_iterator it = points.begin(); it != points.end(); ++it, tu += fTextureStepU, ++pointpatch )
	{
		const float fCellSize = it->fWidth * layer.fRelWidth / float( nMaxNumCells );
		const float fOpacity = it->fOpacity;
		// top (left) border
		float tv = fStartTV;
		CVec3 vPos = it->vPos + it->vNorm*it->fWidth*fTopWidth;
		AddPatchCoords( vPos, *pointpatch );
		verts->Setup( GetPos3(vPos), GetShadeColor(vPos, vSunDir) | MakeOpacity(colorLeft, fOpacity), 0xff000000, CVec2(tu, tv) );
		++verts;
		tv += fTextureStepV;
		// center
		for ( int i = nTopCell; i >= nBottomCell; --i, tv += fTextureStepV )
		{
			vPos = it->vPos + it->vNorm*( fCellSize*i + fCenterRelWidth*it->fWidth );
			AddPatchCoords( vPos, *pointpatch );
			verts->Setup( GetPos3(vPos), GetShadeColor(vPos, vSunDir) | MakeOpacity(colorCenter, fOpacity), 0xff000000, CVec2(tu, tv) );
			++verts;
		}
		// bottom border
		vPos = it->vPos + it->vNorm*it->fWidth*fBottomWidth;
		AddPatchCoords( vPos, *pointpatch );
		verts->Setup( GetPos3(vPos), GetShadeColor(vPos, vSunDir) | MakeOpacity(colorRight, fOpacity), 0xff000000, CVec2(tu, tv) );
		++verts;
	}
	// make irregular vertices from regular ones
	pLayer->allvertices = regularvertices;
	pLayer->nNumVertsPerLine = N;
	if ( fRandomRange > 0 ) 
	{
		const float fRange = fRandomRange;
		for ( int i=1; i<points.size() - 1; ++i )
		{
			for ( int j = 1; j != N - 1; ++j )
			{
				const int nIndex = i*N + j;
				//
				// dP parallel
				float fAlpha = Rand( -fRange, fRange );
				const CVec3 dP1 = VNULL3;
				// dP perpendicular
				fAlpha = Rand( -fRange, fRange );
				const CVec3 dP2 = fAlpha < 0 ? 
					fAlpha*( regularvertices[nIndex].pos - regularvertices[nIndex - 1].pos ) :
					fAlpha*( regularvertices[nIndex + 1].pos - regularvertices[nIndex].pos );
				//
				const CVec3 vPos = regularvertices[nIndex].pos + dP1 + dP2;
				pLayer->allvertices[nIndex].pos = GetPos3( vPos );
				pLayer->allvertices[nIndex].color = GetShadeColor( vPos, vSunDir ) | ( pLayer->allvertices[nIndex].color & 0xff000000 );
			}
		}
	}
	// здесь нам нужно разделить вертексы дорог по используемым патчам, 
	// чтобы в дальнейшем рисовать только те части, которые попадают на экран
	//
	pLayer->patches.clear();
	// составим список точек по патчам
	typedef std::deque<int> SPointsList;
	typedef std::pair<DWORD, SPointsList> SPatch;
	typedef std::deque<SPatch> CPatchPointsList;
	CPatchPointsList patchpoints;
	for ( int nPoint = 0; nPoint < pointpatches.size(); ++nPoint )
	{
		for ( CPatchesList::const_iterator patch = pointpatches[nPoint].begin(); patch != pointpatches[nPoint].end(); ++patch )
		{
			CPatchPointsList::iterator pos = patchpoints.begin();
			for ( ; pos != patchpoints.end(); ++pos )
			{
				if ( pos->first == *patch ) 
					break;
			}
			//
			if ( pos != patchpoints.end() ) 
				pos->second.push_back( nPoint );
			else
			{
				patchpoints.emplace_back();
				patchpoints.back().first = *patch;
				patchpoints.back().second.push_back( nPoint );
			}
		}
	}
	// отсортируем патчи
	std::sort( patchpoints.begin(), patchpoints.end(), SLess1st<DWORD, SPointsList>() );
	pLayer->patches.reserve( patchpoints.size() );
	for ( CPatchPointsList::iterator patch = patchpoints.begin(); patch != patchpoints.end(); ++patch )
	{
		// добавим новый патч и запишем в него точки
		pLayer->patches.push_back( STVOLayer::SPatch() );
		STVOLayer::SPatch &layerPatch = pLayer->patches.back();
		layerPatch.dwPatch = patch->first;
		layerPatch.points.insert( layerPatch.points.end(), patch->second.begin(), patch->second.end() );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void BuildRiverLayer( const std::vector<SVectorStripeObjectPoint> &points, const int nMaxNumCells, float fRandomRange, 
										  const SVectorStripeObject::SLayer &layer, STVOLayer *pLayer )
{
	BuildLayer( points, nMaxNumCells, fRandomRange, layer, LAYER_CENTER, pLayer );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void BuildRoadLayer( const std::vector<SVectorStripeObjectPoint> &points, const int nMaxNumCells, float fRandomRange, 
										 const SVectorStripeObject::SLayer &layer, DWORD flags, STVOLayer *pLayer )
{
	BuildLayer( points, nMaxNumCells, fRandomRange, layer, flags, pLayer );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CreateRiver( const SVectorStripeObject &river, struct STerrainRiver *pRiver )
{
	// create regular vertices and indices
	const int nCounter = river.points.size();
	// determine max width (in tiles)
	int nMaxNumCells = river.bottom.nNumCells;
	for ( int k=0; k<river.layers.size(); ++k )
		nMaxNumCells = Max( nMaxNumCells, river.layers[k].nNumCells );
	// for each layer
	pRiver->layers.resize( river.layers.size() );
	for ( int k=0; k<river.layers.size(); ++k )
	{
		const float fRandom = river.layers[k].bAnimated ? 0 : river.layers[k].fDisturbance;
		BuildRiverLayer( river.points, nMaxNumCells, fRandom, river.layers[k], &(pRiver->layers[k]) );
	}
	// bottom
	BuildRiverLayer( river.points, nMaxNumCells, 0, river.bottom, &(pRiver->bottom) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CreateRoad( const SVectorStripeObject &road, struct STerrainRoad *pRoad )
{
	// for each border
	if ( road.bottomBorders.size() == 2 )
	{
		pRoad->borders.resize( road.bottomBorders.size() );
		BuildRoadLayer( road.points, 1, 0, road.bottomBorders[0], LAYER_BORDER_LEFT, &(pRoad->borders[0]) );
		BuildRoadLayer( road.points, 1, 0, road.bottomBorders[1], LAYER_BORDER_RIGHT, &(pRoad->borders[1]) );
	}
	else
		pRoad->borders.clear();
	// bottom
	BuildRoadLayer( road.points, road.bottom.nNumCells, 0, road.bottom, LAYER_CENTER, &(pRoad->center) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
