#include "StdAfx.h"

#include "TerrainInternal.h"
#include "SceneScreenScale.h"

#include "..\Scene\Scene.h"

#include "..\AILogic\AILogic.h"
#include "..\AILogic\aiconsts.h"
#include "..\GFX\GFXHelper.h"
#include "..\Misc\Intersection.h"
/**
inline int GetNextRoadByPriority( int nPriority, const SRoadsetDesc &roadset )
{
	int nIndex = -1;
	int nLastPriority = 1000000000;
	for ( int i = 0; i != roadset.roads.size(); ++i )
	{
		if ( (roadset.roads[i].nPriority > nPriority) && (roadset.roads[i].nPriority < nLastPriority) )
		{
			nIndex = i;
			nLastPriority = roadset.roads[i].nPriority;
		}
	}
	return nIndex;
}
/**/
bool STerrainCurrMeshData::Draw( IGFX *pGFX, IGFXTexture *pTileset, IGFXTexture *pCrosset, IGFXTexture *pNoise, bool bEnableNoise )
{
	{
		pGFX->SetTexture( 0, pTileset );
		if ( !mshNoiseTiles.IsEmpty() ) 
		{
			pGFX->SetTexture( 1, bEnableNoise ? pNoise : 0 );
			pGFX->SetShadingEffect( 101 );
			DrawTemp( pGFX, mshNoiseTiles.vertices, mshNoiseTiles.indices );
		}
		if ( !mshNoNoiseTiles.IsEmpty() ) 
		{
			pGFX->SetShadingEffect( 2 );
			DrawTemp( pGFX, mshNoNoiseTiles.vertices, mshNoNoiseTiles.indices );
		}
	}
	if ( !mshBaseCrosses.IsEmpty() )
	{
		pGFX->SetTexture( 1, pCrosset );
		pGFX->SetShadingEffect( 100 );
		DrawTemp( pGFX, mshBaseCrosses.vertices, mshBaseCrosses.indices );
	}
	if ( !mshCrossLayers.empty() )
	{
		pGFX->SetTexture( 1, pCrosset );
		for ( std::vector<SCrossesLayer>::const_iterator layer = mshCrossLayers.begin(); layer != mshCrossLayers.end(); ++layer )
		{
			pGFX->SetTexture( 0, pTileset );
			pGFX->SetShadingEffect( 100 );
			DrawTemp( pGFX, layer->mshCrosses.vertices, layer->mshCrosses.indices );
			if ( !layer->mshNoises.IsEmpty() && bEnableNoise )
			{
				pGFX->SetTexture( 0, pNoise );
				pGFX->SetShadingEffect( 104 );
				DrawTemp( pGFX, layer->mshNoises.vertices, layer->mshNoises.indices );
			}
		}
	}
	if ( !mshNoises.IsEmpty() && bEnableNoise )
	{
		pGFX->SetTexture( 0, pNoise );
		pGFX->SetShadingEffect( 103 );
		DrawTemp( pGFX, mshNoises.vertices, mshNoises.indices );
	}
	return true;
}
bool CTerrain::Draw( ICamera *pCamera )
{
	if ( ExtractVisiblePatches( pCamera ) )
		MovePatches();
	pGFX->SetDepthBufferMode( GFXDB_NONE );
	mshCurrent.Draw( pGFX, pTileset, pCrosset, pNoise, bEnableNoise );
	pGFX->SetDepthBufferMode( GFXDB_USE_Z );
		
	return true;
}
bool CTerrain::DrawVectorObjects()
{
	pGFX->SetDepthBufferMode( GFXDB_NONE );
	pGFX->SetWorldTransforms( 0, &MONE, 1 );
	pGFX->SetShadingEffect( 8 );
	for ( int i = 0; i != roads.size(); ++i )
		roads[i].DrawBorder( pGFX );
	pGFX->SetShadingEffect( 8 );
	for ( int i = 0; i != roads.size(); ++i )
		roads[i].DrawCenter( pGFX );
	pGFX->SetShadingEffect( 8 );
	for ( int i = 0; i != rivers.size(); ++i )
		rivers[i].DrawBase( pGFX );
	pGFX->SetShadingEffect( 303 );
	for ( int i = 0; i != rivers.size(); ++i )
		rivers[i].DrawWater( pGFX );
	pGFX->SetShadingEffect( 304 );
	pGFX->SetDepthBufferMode( GFXDB_USE_Z );
	return true;
}
bool CTerrain::DrawMarkers()
{
	pGFX->SetDepthBufferMode( GFXDB_NONE );
	DrawMarker();
	if ( bGridOn )
	{
		pGFX->SetDepthBufferMode( GFXDB_NONE );
		pGFX->SetShadingEffect( 3 );
		pGFX->SetTexture( 0, 0 );
		pGFX->SetWorldTransforms( 0, &MONE, 1 );
		for ( CPatchesList::iterator it = patches.begin(); it != patches.end(); ++it )
			DrawGrid( *it );
	}
	pGFX->SetDepthBufferMode( GFXDB_USE_Z );
	return true;
}
void CreateGrid( const struct STerrainPatch &patch, const STerrainInfo &info, 
								 std::vector<SGFXLineVertex> &vertices, std::vector<WORD> &indices );
bool CTerrain::DrawGrid( const STerrainPatch &patch )
{
	std::vector<SGFXLineVertex> vertices;
	std::vector<WORD> indices;
	CreateGrid( patch, terrainInfo, vertices, indices );
	return DrawTemp( pGFX, vertices, indices, GFXPT_LINELIST );
}
bool CTerrain::DrawWarFog()
{
	pGFX->SetDepthBufferMode( GFXDB_NONE );
	pGFX->SetupDirectTransform();
	const CTRect<float> rcScreen = pGFX->GetScreenRect();
	pGFX->SetTexture( 0, 0 );
	pGFX->SetShadingEffect( 13 );
	for ( CPatchesList::iterator it = patches.begin(); it != patches.end(); ++it )
	{
		if ( it->warfogverts.empty() || it->warfoginds.empty() )
			continue;
#ifndef _USE_HWTL
		STerrainPatch::CVertex2List warfogverts = it->warfogverts;
		for ( STerrainPatch::CVertex2List::iterator vertex = warfogverts.begin(); vertex != warfogverts.end(); ++vertex )
			NSceneScreenScale::ScaleGameplayScreenVertex( &(*vertex), rcScreen );
		::DrawTemp( pGFX, warfogverts, it->warfoginds );
#else
		::DrawTemp( pGFX, it->warfogverts, it->warfoginds );
#endif
	}
	pGFX->RestoreTransform();
	DrawBorder( 0x80000000, 32, true );
	pGFX->SetDepthBufferMode( GFXDB_USE_Z );

	return true;
}
void CTerrain::DrawMarker()
{
	if ( vismarker.info.empty() && aimarker.info.empty() )
		return;
	static CMatrixStack<4> mstack;
	mstack.Push( pGFX->GetViewportMatrix() );
	mstack.Push( pGFX->GetProjectionMatrix() );
	mstack.Push( pGFX->GetViewMatrix() );
	SHMatrix matTransform = mstack();
	mstack.Pop( 3 );
	const float fPatchSize = fWorldCellSize * STerrainPatchInfo::nSizeX;
	const CVec3 vO( 0, fPatchSize * terrainInfo.patches.GetSizeY(), 0 );
	CVec3 vScreenO;											// screen space position of the terrain's origin
	matTransform.RotateHVector( &vScreenO, vO );
	pGFX->SetShadingEffect( 3 );
	pGFX->SetTexture( 0, 0 );
	if ( !vismarker.info.empty() && !vismarker.vertices.empty() && !vismarker.indices.empty() )
	{
		CPtr<IGFXVertices> pVertices = pGFX->CreateVertices( vismarker.vertices.size(), SMarkerVertex::format, GFXPT_TRIANGLELIST, GFXD_DYNAMIC );
		CPtr<IGFXIndices> pIndices = pGFX->CreateIndices( vismarker.indices.size(), GFXIF_INDEX16, GFXPT_TRIANGLELIST, GFXD_DYNAMIC );
		CreateMarker( vScreenO.x, vScreenO.y, vismarker.info, terrainInfo, pVertices, pIndices );
		pGFX->Draw( pVertices, pIndices );
	}
	if ( !aimarker.info.empty() && !aimarker.vertices.empty() && !aimarker.indices.empty() )
	{
		CPtr<IGFXVertices> pVertices = pGFX->CreateVertices( aimarker.vertices.size(), SMarkerVertex::format, GFXPT_TRIANGLELIST, GFXD_DYNAMIC );
		CPtr<IGFXIndices> pIndices = pGFX->CreateIndices( aimarker.indices.size(), GFXIF_INDEX16, GFXPT_TRIANGLELIST, GFXD_DYNAMIC );
		CreateAIMarker( vScreenO.x, vScreenO.y, terrainInfo, &(aimarker.info[0]), aimarker.info.size(), 
										pVertices, pIndices );
		pGFX->Draw( pVertices, pIndices );
	}
}
inline bool IsVisible( const float fMinX, const float fMinY, const float fMinHeight,
											 const float fMaxX, const float fMaxY, const float fMaxHeight,
											 const SPlane *pViewVolumePlanes )
{
	const DWORD dwCrossFlags = 
		CheckViewVolume4( CVec3(fMaxX, fMaxY, fMaxHeight), pViewVolumePlanes ) &
		CheckViewVolume4( CVec3(fMaxX, fMaxY, fMinHeight), pViewVolumePlanes ) &
		CheckViewVolume4( CVec3(fMaxX, fMinY, fMaxHeight), pViewVolumePlanes ) &
		CheckViewVolume4( CVec3(fMaxX, fMinY, fMinHeight), pViewVolumePlanes ) &
		CheckViewVolume4( CVec3(fMinX, fMaxY, fMaxHeight), pViewVolumePlanes ) &
		CheckViewVolume4( CVec3(fMinX, fMaxY, fMinHeight), pViewVolumePlanes ) &
		CheckViewVolume4( CVec3(fMinX, fMinY, fMaxHeight), pViewVolumePlanes ) &
		CheckViewVolume4( CVec3(fMinX, fMinY, fMinHeight), pViewVolumePlanes );
	return dwCrossFlags == 0;
}
bool IsPatchVisible( const STerrainPatchInfo &patch, const STerrainInfo &info, const SPlane *pViewVolumePlanes, bool *flags )
{
	const float fMinX = patch.nStartX * fWorldCellSize;
	const float fMinY = float( info.tiles.GetBoundY() - patch.nStartY - 16 ) * fWorldCellSize;
	const float fMaxX = fMinX + 17.0f*fWorldCellSize;
	const float fMaxY = fMinY + 17.0f*fWorldCellSize;
	if ( IsVisible(fMinX, fMinY, patch.fMinHeight, fMaxX, fMaxY, patch.fMaxHeight, pViewVolumePlanes) )
	{
		flags[0] = IsVisible( fMinX                       , fMinY                       , patch.fSubMinHeight[0], 
			                    fMinX + 10.0f*fWorldCellSize, fMinY + 10.0f*fWorldCellSize, patch.fSubMaxHeight[0], pViewVolumePlanes );

		flags[1] = IsVisible( fMinX +  7.0f*fWorldCellSize, fMinY                       , patch.fSubMinHeight[1], 
			                    fMinX + 17.0f*fWorldCellSize, fMinY + 10.0f*fWorldCellSize, patch.fSubMaxHeight[1], pViewVolumePlanes );

		flags[2] = IsVisible( fMinX                       , fMinY +  7.0f*fWorldCellSize, patch.fSubMinHeight[2], 
			                    fMinX + 10.0f*fWorldCellSize, fMinY + 17.0f*fWorldCellSize, patch.fSubMaxHeight[2], pViewVolumePlanes );

		flags[3] = IsVisible( fMinX +  7.0f*fWorldCellSize, fMinY +  7.0f*fWorldCellSize, patch.fSubMinHeight[3], 
			                    fMinX + 17.0f*fWorldCellSize, fMinY + 17.0f*fWorldCellSize, patch.fSubMaxHeight[3], pViewVolumePlanes );
		return flags[0] || flags[1] || flags[2] || flags[3];
	}
	return false;
}
bool CTerrain::ExtractVisiblePatches( ICamera *pCamera )
{
	const CVec3 vCamera = pCamera->GetAnchor();
	if ( (vCamera.x == vOldAnchor.x) && (vCamera.y == vOldAnchor.y) && (vCamera.z == vOldAnchor.z) )
		return false;
	vOldAnchor = vCamera;
	SPlane viewVolumePlanes[6];
	pGFX->GetViewVolume( &(viewVolumePlanes[0]) );
	const float fPatchHalfAxis = fCellSizeX * STerrainPatchInfo::nSizeX;
	const float fPatchSize = fWorldCellSize * STerrainPatchInfo::nSizeX;
	const float fTerrainPatchesX = terrainInfo.patches.GetSizeX();
	const float fTerrainPatchesY = terrainInfo.patches.GetSizeY();
	const float fTerrainSizeX = fTerrainPatchesX * fPatchSize;
	const float fTerrainSizeY = fTerrainPatchesY * fPatchSize;
	CVec3 vAxisX, vAxisY;
	GetLineEq( 0, 0, 1, 0, &vAxisX.x, &vAxisX.y, &vAxisX.z );
	GetLineEq( 0, 1, 0, 0, &vAxisY.x, &vAxisY.y, &vAxisY.z );
	const RECT rcScreen = pGFX->GetScreenRect();
	const float fWidth = ( rcScreen.right - rcScreen.left ) / 2;
	const float fHeight = rcScreen.bottom - rcScreen.top;					// height * 2 due to camera yaw = 30 degrees
	CVec2 vCameraX( fWidth / FP_SQRT_2, fWidth / FP_SQRT_2 ), vCameraY( -fHeight / FP_SQRT_2, fHeight / FP_SQRT_2 );
	const CVec2 vCameraO( vCamera.x, vCamera.y );
	CTRect<int> rcL0Rect;								// level 0 of roughness rect
	{
		const CVec2 point = vCameraO + vCameraY - vCameraX;
		const float fDist = vAxisY.x*point.x + vAxisY.y*point.y + vAxisY.z;
		rcL0Rect.minx = floor( Clamp( fDist / fPatchSize, 0.0f, fTerrainPatchesX ) );
	}
	{
		const CVec2 point = vCameraO + vCameraY + vCameraX;
		const float fDist = vAxisX.x*point.x + vAxisX.y*point.y + vAxisX.z;
		rcL0Rect.miny = floor( Clamp( fTerrainPatchesY - (fDist / fPatchSize), 0.0f, fTerrainPatchesY ) );
	}
	{
		const CVec2 point = vCameraO + vCameraX - vCameraY;
		const float fDist = vAxisY.x*point.x + vAxisY.y*point.y + vAxisY.z;
		rcL0Rect.maxx = floor( Clamp( fDist / fPatchSize, 0.0f, fTerrainPatchesX ) + 0.99f );
	}
	{
		const CVec2 point = vCameraO - vCameraY - vCameraX;
		const float fDist = vAxisX.x*point.x + vAxisX.y*point.y + vAxisX.z;
		rcL0Rect.maxy = floor( Clamp( fTerrainPatchesY - (fDist / fPatchSize), 0.0f, fTerrainPatchesY ) + 0.99f );
	}
	rcL0Rect.Inflate( 1, 1 );
	rcL0Rect.minx = Clamp( rcL0Rect.minx, 0, terrainInfo.patches.GetSizeX() );
	rcL0Rect.miny = Clamp( rcL0Rect.miny, 0, terrainInfo.patches.GetSizeY() );
	rcL0Rect.maxx = Clamp( rcL0Rect.maxx, 0, terrainInfo.patches.GetSizeX() );
	rcL0Rect.maxy = Clamp( rcL0Rect.maxy, 0, terrainInfo.patches.GetSizeY() );

	std::list< std::pair<int, int> > accepted;
	for ( int j = rcL0Rect.miny; j < rcL0Rect.maxy; ++j )
	{
		for ( int i = rcL0Rect.minx; i < rcL0Rect.maxx; ++i )
		{
			bool flags[4];
			if ( IsPatchVisible(terrainInfo.patches[j][i], terrainInfo, viewVolumePlanes, flags) )
			{
				accepted.push_back( std::pair<int, int>( i, j ) );
				bool bExist = false;
				for ( CPatchesList::iterator it = patches.begin(); it != patches.end(); ++it )
				{
					if ( (it->nX == i) && (it->nY == j) )
					{
						it->bSubPatches[0] = flags[0];
						it->bSubPatches[1] = flags[1];
						it->bSubPatches[2] = flags[2];
						it->bSubPatches[3] = flags[3];
						bExist = true;
						break;
					}
				}
				if ( !bExist )
				{
					patches.push_back( STerrainPatch() );
					STerrainPatch &patch = patches.back();
					patch.nX = i;
					patch.nY = j;
					patch.bSubPatches[0] = flags[0];
					patch.bSubPatches[1] = flags[1];
					patch.bSubPatches[2] = flags[2];
					patch.bSubPatches[3] = flags[3];
				}
			}
		}
	}
	for ( CPatchesList::iterator it = patches.begin(); it != patches.end(); )
	{
		if ( std::find( accepted.begin(), accepted.end(), std::pair<int, int>(it->nX, it->nY) ) == accepted.end() )
			it = patches.erase( it );
		else
			++it;
	}
	roadPatches.clear();
	roadPatches.reserve( patches.size() );
	for ( CPatchesList::const_iterator it = patches.begin(); it != patches.end(); ++it )
	{
		const DWORD dwPatch = ( DWORD(it->nX) << 16 ) | DWORD( terrainInfo.patches.GetBoundY() - it->nY );
		roadPatches.push_back( dwPatch );
	}
	std::sort( roadPatches.begin(), roadPatches.end() );
	for ( int i = 0; i != roads.size(); ++i )
		roads[i].SelectPatches( roadPatches );	
	for ( int i = 0; i != rivers.size(); ++i )
		rivers[i].SelectPatches( roadPatches );	
	return true;
}
bool CTerrain::DrawAISurface( const STerrainPatch &patch )
{
	IAILogic *pAILogic = GetSingleton<IAILogic>();
	if ( pAILogic == 0 ) 
		return false;
	const float fStartX = patch.nX * STerrainPatchInfo::nSizeX;
	const float fStartY = ( terrainInfo.patches.GetSizeY() - patch.nY ) * STerrainPatchInfo::nSizeY;
	CTempBufferLock<SGFXVertex> vertices = pGFX->GetTempVertices( (STerrainPatchInfo::nSizeY + 1)*(STerrainPatchInfo::nSizeX + 1)*4, 
		                                                             SGFXVertex::format, GFXPT_TRIANGLELIST );
	SGFXVertex *pVerts = vertices.GetBuffer();
	for ( int i = 0; i != (STerrainPatchInfo::nSizeY + 1)*2; ++i )
	{
		for ( int j = 0; j != (STerrainPatchInfo::nSizeX + 1)*2; ++j )
		{
			const float fX = fStartX*SAIConsts::TILE_SIZE*2 + j*SAIConsts::TILE_SIZE;
			const float fY = fStartY*SAIConsts::TILE_SIZE*2 - i*SAIConsts::TILE_SIZE;
			CVec3 vPos( fX, fY, pAILogic->GetZ(CVec2(fX, fY)) );
			DWORD dwNormal = pAILogic->GetNormal( CVec2(fX, fY) );
			CVec3 vNormal = DWORDToVec3( dwNormal );
			Normalize( &vNormal );
			AI2Vis( &vPos );
			pVerts->Setup( vPos, vNormal, VNULL2 );
			++pVerts;
		}
	}
	CTempBufferLock<WORD> indices = pGFX->GetTempIndices( STerrainPatchInfo::nSizeX*STerrainPatchInfo::nSizeY*6*4, 
		                                                    GFXIF_INDEX16, GFXPT_TRIANGLELIST );
	WORD *pInds = indices.GetBuffer();
	for ( int i = 0; i != STerrainPatchInfo::nSizeY*2; ++i )
	{
		for ( int j = 0; j != STerrainPatchInfo::nSizeX*2; ++j )
		{
			const int nIdx0 = i*( STerrainPatchInfo::nSizeX + 1 )*2 + j;
			const int nIdx1 = i*( STerrainPatchInfo::nSizeX + 1 )*2 + j + 1;
			const int nIdx2 = (i + 1)*( STerrainPatchInfo::nSizeX + 1 )*2 + j;
			const int nIdx3 = (i + 1)*( STerrainPatchInfo::nSizeX + 1 )*2 + j + 1;
			*pInds++ = nIdx0;
			*pInds++ = nIdx2;
			*pInds++ = nIdx1;
			*pInds++ = nIdx1;
			*pInds++ = nIdx2;
			*pInds++ = nIdx3;
		}
	}
	return pGFX->DrawTemp();
}
void CTerrain::DrawBorder( DWORD dwColor, int nTiles, bool bUseFog )
{
	typedef std::pair<int, int> CIndexPair;
	float fCellSize = fCellSizeX * FP_SQRT_2;
	float fPatchSizeX = fCellSize * STerrainPatchInfo::nSizeX;
	float fPatchSizeY = fCellSize * STerrainPatchInfo::nSizeY;
	float fBorderWidth = fCellSize * nTiles;
	std::list<CIndexPair> patchIndexes;
	int maxX = 0;
	int maxY = 0;
	int minX = terrainInfo.patches.GetSizeX() - 1;
	int minY = terrainInfo.patches.GetSizeY() - 1;
	for ( CPatchesList::const_iterator it = patches.begin(); it != patches.end(); ++it )
	{	
		if ( it->nX == 0 || it->nY == 0 || it->nX == terrainInfo.patches.GetSizeX() - 1 || it->nY == terrainInfo.patches.GetSizeY() - 1 )
		{
			patchIndexes.push_back( CIndexPair(it->nX, it->nY) );
			if ( maxX < it->nX )
				maxX = it->nX;
			if ( maxY < it->nY )
				maxY = it->nY;
			if ( minX > it->nX )
				minX = it->nX;
			if ( minY > it->nY )
				minY = it->nY;
		}
	}
	for ( std::list<CIndexPair>::const_iterator it = patchIndexes.begin(); it != patchIndexes.end(); ++it )
	{
		if ( it->first == minX || it->first == maxX ) 
		{
			if ( it->second != 0 && it->second == minY )
			{
				patchIndexes.push_front( CIndexPair(it->first,it->second - 1) );
			}
			if ( it->second != terrainInfo.patches.GetSizeY() - 1 && it->second == maxY)
			{
				patchIndexes.push_front( CIndexPair(it->first,it->second + 1) );
			}
		}
		if ( it->second == minY || it->second == maxY )
		{
			if ( it->first != 0 && it->first == minX)
			{
				patchIndexes.push_front( CIndexPair(it->first - 1,it->second) );
			}
			if ( it->first != terrainInfo.patches.GetSizeX() - 1 && it->first == maxX )
			{
				patchIndexes.push_front( CIndexPair(it->first + 1,it->second) );
			}
		}
	}	
	const float fBorderOverflow = 1.0f;
	for ( std::list<CIndexPair>::const_iterator it = patchIndexes.begin(); it != patchIndexes.end(); ++it )
	{
		int nX = it->first;
		int nY = it->second;
		if ( nX == 0 )
		{
			CVec3 vStartPos = CVec3( fBorderOverflow, (terrainInfo.patches.GetSizeY() - nY) * fPatchSizeY, 0 );
			CVec3 vFarShift = CVec3( -fBorderWidth, 0, 0 );
			CVec3 vStepShift = CVec3( 0, -fCellSize, 0 );
			DrawPatchBorder( vStartPos, vFarShift, vStepShift, true, true, STerrainPatchInfo::nSizeY * nY, 0, dwColor, bUseFog );
			if ( nY == 0 )
			{
				CTempBufferLock<SGFXLineVertex> vertices = pGFX->GetTempVertices( 4, SGFXLineVertex::format, GFXPT_TRIANGLELIST );
				CTempBufferLock<WORD> indices = pGFX->GetTempIndices( 6, GFXIF_INDEX16, GFXPT_TRIANGLELIST );
				SGFXLineVertex *pVertex = vertices.GetBuffer();
				WORD *pIndex = indices.GetBuffer();
				const float fZCoord = terrainInfo.altitudes[0][0].fHeight;
				const DWORD dwVertColor = bUseFog ? GetVisibilityColor(0, visibilities) : dwColor;
				pVertex->Setup( fBorderOverflow, terrainInfo.patches.GetSizeY() * fPatchSizeY - fBorderOverflow, fZCoord, dwVertColor );
				++pVertex;
				pVertex->Setup( -fBorderWidth, terrainInfo.patches.GetSizeY() * fPatchSizeY - fBorderOverflow, fZCoord, dwVertColor );
				++pVertex;
				pVertex->Setup( fBorderOverflow, terrainInfo.patches.GetSizeY() * fPatchSizeY + fBorderWidth, fZCoord, dwVertColor );
				++pVertex;
				pVertex->Setup( -fBorderWidth, terrainInfo.patches.GetSizeY() * fPatchSizeY + fBorderWidth, fZCoord, dwVertColor );
				++pVertex;
				*pIndex++ = 3;
				*pIndex++ = 1;
				*pIndex++ = 0;
				*pIndex++ = 2;
				*pIndex++ = 3;
				*pIndex++ = 0;
				pGFX->DrawTemp();
			}
		}
		if ( nY == 0 )
		{
			CVec3 vStartPos = CVec3( fPatchSizeX * nX, terrainInfo.patches.GetSizeY() * fPatchSizeY - fBorderOverflow, 0 );
			CVec3 vFarShift = CVec3( 0, fBorderWidth, 0 );
			CVec3 vStepShift = CVec3( fCellSize, 0, 0 );
			DrawPatchBorder( vStartPos, vFarShift, vStepShift, false, false, 0, STerrainPatchInfo::nSizeX * nX, dwColor, bUseFog );
			if ( nX == terrainInfo.patches.GetSizeX() - 1 )
			{
				CTempBufferLock<SGFXLineVertex> vertices = pGFX->GetTempVertices( 4, SGFXLineVertex::format, GFXPT_TRIANGLELIST );
				CTempBufferLock<WORD> indices = pGFX->GetTempIndices( 6, GFXIF_INDEX16, GFXPT_TRIANGLELIST );
				SGFXLineVertex *pVertex = vertices.GetBuffer();
				WORD *pIndex = indices.GetBuffer();
				const float fZCoord = terrainInfo.altitudes[0][STerrainPatchInfo::nSizeX * terrainInfo.patches.GetSizeX()].fHeight;
				const DWORD dwVertColor = bUseFog ? GetVisibilityColor(STerrainPatchInfo::nSizeX * terrainInfo.patches.GetSizeX() - 1, visibilities) : dwColor;
				pVertex->Setup( terrainInfo.patches.GetSizeX() * fPatchSizeX - fBorderOverflow, terrainInfo.patches.GetSizeY() * fPatchSizeY - fBorderOverflow, fZCoord, dwVertColor );
				++pVertex;
				pVertex->Setup( terrainInfo.patches.GetSizeX() * fPatchSizeX - fBorderOverflow, terrainInfo.patches.GetSizeY() * fPatchSizeY + fBorderWidth, fZCoord, dwVertColor );
				++pVertex;
				pVertex->Setup( terrainInfo.patches.GetSizeX() * fPatchSizeX + fBorderWidth, terrainInfo.patches.GetSizeY() * fPatchSizeY - fBorderOverflow, fZCoord, dwVertColor );
				++pVertex;
				pVertex->Setup( terrainInfo.patches.GetSizeX() * fPatchSizeX + fBorderWidth, terrainInfo.patches.GetSizeY() * fPatchSizeY + fBorderWidth, fZCoord, dwVertColor );
				++pVertex;
				*pIndex++ = 3;
				*pIndex++ = 1;
				*pIndex++ = 0;
				*pIndex++ = 2;
				*pIndex++ = 3;
				*pIndex++ = 0;
				pGFX->DrawTemp();
			}
		}
		if ( nX == terrainInfo.patches.GetSizeX() - 1 )
		{
			CVec3 vStartPos = CVec3( terrainInfo.patches.GetSizeX() * fPatchSizeX - fBorderOverflow, (terrainInfo.patches.GetSizeY() - nY) * fPatchSizeY, 0 );
			CVec3 vFarShift = CVec3( fBorderWidth, 0, 0 );
			CVec3 vStepShift = CVec3( 0, -fCellSize, 0 );
			DrawPatchBorder( vStartPos, vFarShift, vStepShift, false, true, STerrainPatchInfo::nSizeY * nY, STerrainPatchInfo::nSizeX * terrainInfo.patches.GetSizeX(), dwColor, bUseFog );
			if ( nY == terrainInfo.patches.GetSizeY() - 1 )
			{
				CTempBufferLock<SGFXLineVertex> vertices = pGFX->GetTempVertices( 4, SGFXLineVertex::format, GFXPT_TRIANGLELIST );
				CTempBufferLock<WORD> indices = pGFX->GetTempIndices( 6, GFXIF_INDEX16, GFXPT_TRIANGLELIST );
				SGFXLineVertex *pVertex = vertices.GetBuffer();
				WORD *pIndex = indices.GetBuffer();
				const float fZCoord = terrainInfo.altitudes[STerrainPatchInfo::nSizeY * terrainInfo.patches.GetSizeY()][STerrainPatchInfo::nSizeX * terrainInfo.patches.GetSizeX()].fHeight;
				const DWORD dwVertColor = bUseFog ? GetVisibilityColor(((STerrainPatchInfo::nSizeY * terrainInfo.patches.GetSizeY() - 1) << 16) | (STerrainPatchInfo::nSizeX * terrainInfo.patches.GetSizeX() - 1), visibilities) : dwColor;
				pVertex->Setup( terrainInfo.patches.GetSizeX() * fPatchSizeX - fBorderOverflow, fBorderOverflow, fZCoord, dwVertColor );
				++pVertex;
				pVertex->Setup( terrainInfo.patches.GetSizeX() * fPatchSizeX + fBorderWidth, fBorderOverflow, fZCoord, dwVertColor );
				++pVertex;
				pVertex->Setup( terrainInfo.patches.GetSizeX() * fPatchSizeX - fBorderOverflow, -fBorderWidth, fZCoord, dwVertColor );
				++pVertex;
				pVertex->Setup( terrainInfo.patches.GetSizeX() * fPatchSizeX + fBorderWidth, -fBorderWidth, fZCoord, dwVertColor );
				++pVertex;
				*pIndex++ = 3;
				*pIndex++ = 1;
				*pIndex++ = 0;
				*pIndex++ = 2;
				*pIndex++ = 3;
				*pIndex++ = 0;
				pGFX->DrawTemp();
			}
		}
		if ( nY == terrainInfo.patches.GetSizeY() - 1 )
		{
			CVec3 vStartPos = CVec3( nX * fPatchSizeX, fBorderOverflow, 0 );
			CVec3 vFarShift = CVec3( 0, -fBorderWidth, 0 );
			CVec3 vStepShift = CVec3( fCellSize, 0, 0 );
			DrawPatchBorder( vStartPos, vFarShift, vStepShift, true, false, STerrainPatchInfo::nSizeY * terrainInfo.patches.GetSizeY(), STerrainPatchInfo::nSizeX * nX, dwColor, bUseFog );
			if ( nX == 0 )
			{
				CTempBufferLock<SGFXLineVertex> vertices = pGFX->GetTempVertices( 4, SGFXLineVertex::format, GFXPT_TRIANGLELIST );
				CTempBufferLock<WORD> indices = pGFX->GetTempIndices( 6, GFXIF_INDEX16, GFXPT_TRIANGLELIST );
				SGFXLineVertex *pVertex = vertices.GetBuffer();
				WORD *pIndex = indices.GetBuffer();
				const float fZCoord = terrainInfo.altitudes[STerrainPatchInfo::nSizeY * terrainInfo.patches.GetSizeY()][0].fHeight;
				const DWORD dwVertColor = bUseFog ? GetVisibilityColor((STerrainPatchInfo::nSizeY * terrainInfo.patches.GetSizeY() - 1) << 16, visibilities) : dwColor;
				pVertex->Setup( fBorderOverflow, fBorderOverflow, fZCoord, dwVertColor );
				++pVertex;
				pVertex->Setup( fBorderOverflow, -fBorderWidth, fZCoord, dwVertColor );
				++pVertex;
				pVertex->Setup( -fBorderWidth, fBorderOverflow, fZCoord, dwVertColor );
				++pVertex;
				pVertex->Setup( -fBorderWidth, -fBorderWidth, fZCoord, dwVertColor );
				++pVertex;
				*pIndex++ = 3;
				*pIndex++ = 1;
				*pIndex++ = 0;
				*pIndex++ = 2;
				*pIndex++ = 3;
				*pIndex++ = 0;
				pGFX->DrawTemp();
			}
		}
	}
}
void CTerrain::DrawPatchBorder( CVec3 &vStartPos, const CVec3 &vFarOffset, const CVec3 &vStepOffset, bool bRightOrder, bool bXIncrement, int nXStart, int nYStart, DWORD dwColor, bool bUseFog )
{
	CTempBufferLock<SGFXLineVertex> vertices = pGFX->GetTempVertices( (STerrainPatchInfo::nSizeY + 1) * 2, SGFXLineVertex::format, GFXPT_TRIANGLELIST );
	CTempBufferLock<WORD> indices = pGFX->GetTempIndices( STerrainPatchInfo::nSizeY * 6, GFXIF_INDEX16, GFXPT_TRIANGLELIST );
	SGFXLineVertex *pVertex = vertices.GetBuffer();
	WORD *pIndex = indices.GetBuffer();
	WORD wCurrVertex = 0;
	DWORD dwVertColor = bUseFog ? GetVisibilityColor((nXStart << 16) | nYStart, visibilities) : dwColor;
	for ( int i = 0; i <= STerrainPatchInfo::nSizeY; ++i )
	{
		if ( bXIncrement )
			vStartPos.z = terrainInfo.altitudes[nXStart + i][nYStart].fHeight;
		else
			vStartPos.z = terrainInfo.altitudes[nXStart][nYStart + i].fHeight;
		pVertex->Setup( vStartPos, dwVertColor);
		++pVertex;
		pVertex->Setup( vStartPos + vFarOffset, dwVertColor);
		++pVertex;
		if ( i != 0 )
		{
			if ( bRightOrder )
			{
				*pIndex++ = wCurrVertex - 2;
				*pIndex++ = wCurrVertex - 1;
				*pIndex++ = wCurrVertex;
				*pIndex++ = wCurrVertex + 1;
				*pIndex++ = wCurrVertex;
				*pIndex++ = wCurrVertex - 1;
			}
			else
			{
				*pIndex++ = wCurrVertex;
				*pIndex++ = wCurrVertex - 1;
				*pIndex++ = wCurrVertex - 2;
				*pIndex++ = wCurrVertex - 1;
				*pIndex++ = wCurrVertex;
				*pIndex++ = wCurrVertex + 1;
			}
		}
		wCurrVertex += 2;
		vStartPos += vStepOffset;
		if ( bXIncrement )
			dwVertColor = bUseFog ? GetVisibilityColor(((nXStart + i) << 16) | nYStart, visibilities) : dwColor;
		else
			dwVertColor = bUseFog ? GetVisibilityColor((nXStart << 16) | (nYStart + i), visibilities) : dwColor;
	}
	pGFX->DrawTemp();
}
