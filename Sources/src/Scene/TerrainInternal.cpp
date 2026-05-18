#include "StdAfx.h"

#include "TerrainInternal.h"

#include "..\Image\Image.h"
#include "TerrainBuilder.h"
#include "..\AILogic\AITypes.h"
#include "..\AILogic\AILogic.h"
#include "..\Scene\Scene.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SLoadTerrain
{
	STerrainInfo info;
	//
	int operator&( IStructureSaver &ss )
	{
		CSaverAccessor saver = &ss;
		saver.Add( 1, &info );
		return 0;
	}
};
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTerrain::Init( ISingleton *pSingleton )
{
	pGFX = GetSingleton<IGFX>( pSingleton );
	pTM = GetSingleton<ITextureManager>( pSingleton );
	pStorage = GetSingleton<IDataStorage>( pSingleton );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTerrain::Clear()
{
	pTileset = 0;
	pCrosset = 0;
	pNoise = 0;
	/*
	for ( int i=0; i<nNumRoadTypes; ++i )
		roadsets[i] = 0;
	*/
	//
	vismarker.Clear();
	aimarker.Clear();
	//
	visibilities.clear();
	//
	tilesetDesc.terrtypes.clear();
	tilesetDesc.tilemaps.clear();
	crossetDesc.crosses.clear();
	crossetDesc.tilemaps.clear();
	//roadsetDesc.roads.clear();
	//roadsetDesc.tilemaps.clear();
	//
	terrainInfo.patches.Clear();
	terrainInfo.tiles.Clear();
	terrainInfo.rivers.clear();
	terrainInfo.roads3.clear();
	patches.clear();
	bGridOn = false;
	vOldAnchor.Set( -1000000, -1000000, -1000000 );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
float CTerrain::GetHeight( const CVec2 &vPos )
{
	CVec2 vPos2;
	Vis2AI( &vPos2, vPos.x, vPos.y );
	return AI2VisZ( GetSingleton<IAILogic>()->GetZ(vPos2) );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const CVec3 CalcNormale( int y, int x, const CArray2D<SVertexAltitude> &heights )
{
	const float fZ = heights[y][x].fHeight;
	const CVec3 v1( - fWorldCellSize, + fWorldCellSize, heights[y - 1][x - 1].fHeight - fZ );
	const CVec3 v2( 0               , + fWorldCellSize, heights[y - 1][x].fHeight - fZ );
	const CVec3 v3( + fWorldCellSize, 0               , heights[y][x + 1].fHeight - fZ );
	const CVec3 v4( + fWorldCellSize, - fWorldCellSize, heights[y + 1][x + 1].fHeight - fZ );
	const CVec3 v5( 0               , - fWorldCellSize, heights[y + 1][x].fHeight - fZ );
	const CVec3 v6( - fWorldCellSize, 0               , heights[y][x - 1].fHeight - fZ );
	//
	CVec3 vNorm = ( v2 ^ v1 ) + (v3 ^ v2) + (v4 ^ v3) + (v5 ^ v4) + (v6 ^ v5) + (v1 ^ v6);
	::Normalize( &vNorm );
	return vNorm;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CorrectUVMaps( std::vector<STileMapsDesc> &tilemaps, const float fW, const float fH )
{
	for ( std::vector<STileMapsDesc>::iterator it = tilemaps.begin(); it != tilemaps.end(); ++it )
	{
		if ( it->maps[0].v < it->maps[3].v ) 
		{
			it->maps[0].v += fH;
			it->maps[3].v -= fH;
		}
		else
		{
			it->maps[0].v -= fH;
			it->maps[3].v += fH;
		}
		//
		it->maps[1].u -= fW;
		it->maps[2].u += fW;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTerrain::LoadLocal( const std::string &szName, const STerrainInfo &_terrainInfo, bool bMinimizeRoadsets, bool bLoadTextures )
{
	Clear();
	//
	szMapName = szName;
	//
	vOldAnchor.Set( -1000000, -1000000, -1000000 );
	// load terrain info
	terrainInfo = _terrainInfo;
	terrainInfo.FillMinMaxHeights();
	// load tileset descriptor
	{
		CPtr<IDataStream> pStream = pStorage->OpenStream( NStr::Format("%s.xml", terrainInfo.szTilesetDesc.c_str()), STREAM_ACCESS_READ );
		CTreeAccessor tree = CreateDataTreeSaver( pStream, IDataTree::READ );
		tree.Add( "tileset", &tilesetDesc );		
	}
	// load crosset descriptors
	{
		CPtr<IDataStream> pStream = pStorage->OpenStream( NStr::Format("%s.xml", terrainInfo.szCrossetDesc.c_str()), STREAM_ACCESS_READ );
		CTreeAccessor tree = CreateDataTreeSaver( pStream, IDataTree::READ );
		tree.Add( "crosset", &crossetDesc );
	}
	// load roadset descriptors
	/**
	{
		CPtr<IDataStream> pStream = pStorage->OpenStream( NStr::Format("%s.xml", terrainInfo.szRoadsetDesc.c_str()), STREAM_ACCESS_READ );
		CTreeAccessor tree = CreateDataTreeSaver( pStream, IDataTree::READ );
		tree.Add( "roadset", &roadsetDesc );
	}
	/**/
	//
	if ( bLoadTextures )
	{
		if ( terrainInfo.szNoise.empty() ) 
			terrainInfo.szNoise = "terrain\\sets\\1\\noise";
		// load textures
		pTileset = pTM->GetTexture( terrainInfo.szTilesetDesc.c_str() );
		nTilesetSizeX = pTileset->GetSizeX( 0 );
		nTilesetSizeY = pTileset->GetSizeY( 0 );

		pCrosset = pTM->GetTexture( terrainInfo.szCrossetDesc.c_str() );
		nCrossetSizeX = pCrosset->GetSizeX( 0 );
		nCrossetSizeY = pCrosset->GetSizeY( 0 );

		pNoise = pTM->GetTexture( terrainInfo.szNoise.c_str() );
		nNoiseSizeX = pNoise->GetSizeX( 0 );
		nNoiseSizeY = pNoise->GetSizeY( 0 );
	}
	//
	//
	// post-process map info
	//
	// set default height (0) and re-calc shading in the case of empty altitudes array
	if ( (terrainInfo.altitudes.GetBoundX() != terrainInfo.tiles.GetSizeX()) || (terrainInfo.altitudes.GetBoundY() != terrainInfo.tiles.GetSizeY()) )
	{
		terrainInfo.altitudes.SetSizes( terrainInfo.tiles.GetSizeX() + 1, terrainInfo.tiles.GetSizeY() + 1 );
		//
		CVec3 vSunDir( GetGlobalVar("Scene.SunLight.Direction.X", 1.0f),
									 GetGlobalVar("Scene.SunLight.Direction.Y", 1.0f),
									 GetGlobalVar("Scene.SunLight.Direction.Z", -2.0f) );
		Normalize( &vSunDir );
		//
		SVertexAltitude altitude;
		altitude.shade = BYTE( Clamp( -(vSunDir * V3_AXIS_Z), 0.6f, 1.0f ) * 255.0f );
		terrainInfo.altitudes.Set( altitude );
	}
	// shift tile maps
	{
		float fTW = 1.0f / float( nTilesetSizeX );
		float fTH = 1.0f / float( nTilesetSizeY );
		float fCW = 1.0f / float( nCrossetSizeX );
		float fCH = 1.0f / float( nCrossetSizeY );
		CTRect<long> rcScreen = pGFX->GetScreenRect();
		if ( rcScreen.Width() <= 800 ) 
		{
			fTW = 2.0f / float( nTilesetSizeX );
			fTH = 2.0f / float( nTilesetSizeY );
			fCW = 2.0f / float( nCrossetSizeX );
			fCH = 2.0f / float( nCrossetSizeY );
		}
		// tileset
		CorrectUVMaps( tilesetDesc.tilemaps, fTW, fTH );
		// crosset
		CorrectUVMaps( crossetDesc.tilemaps, fCW, fCH );
	}
	//
	// build rivers & roads
	//
	rivers.resize( terrainInfo.rivers.size() );
	for ( int i = 0; i != rivers.size(); ++i )
		rivers[i].Init( terrainInfo.rivers[i], this );
	roads.resize( terrainInfo.roads3.size() );
	for ( int i = 0; i != roads.size(); ++i )
		roads[i].Init( terrainInfo.roads3[i], this );
	std::sort( roads.begin(), roads.end() );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CTerrain::Load( const char *pszName, const STerrainInfo &_terrainInfo )
{
	LoadLocal( pszName, _terrainInfo, false, true );
	GetSingleton<IScene>()->InitTerrainSound( this );
	return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTerrain::SetMarker( const CTPoint<int> *pPoints, int nNumPoints )
{
	vismarker.info.resize( nNumPoints );
	if ( nNumPoints > 0 )
	{
		const int nNumTiles = nNumPoints;
		memcpy( &(vismarker.info[0]), pPoints, nNumPoints * sizeof(CTPoint<int>) );
		vismarker.vertices.resize( nNumTiles * 4 );
		vismarker.indices.resize( nNumTiles * 6 );
	}
	else
		vismarker.Clear();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTerrain::SetAIMarker( SAIPassabilityInfo *infos, int nNumPoints )
{
	aimarker.info.resize( nNumPoints );
	if ( nNumPoints > 0 )
	{
		const int nNumTiles = nNumPoints;
		memcpy( &(aimarker.info[0]), infos, nNumPoints * sizeof(SAIPassabilityInfo) );
		aimarker.vertices.resize( nNumTiles * 4 );
		aimarker.indices.resize( nNumTiles * 6 );
	}
	else
		aimarker.Clear();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTerrain::SetWarFog( SAIVisInfo *vises, int nNumVises )
{
	visibilities.clear();
	visibilities.reserve( nNumVises );
	//
	for ( const SAIVisInfo *pVis = vises; pVis != vises + nNumVises; ++pVis )
	{
		const DWORD y = DWORD( terrainInfo.tiles.GetSizeY() - int(pVis->y) - 1 );
		const DWORD key = ( y << 16 ) | DWORD( pVis->x );
		visibilities[key] = DWORD( pVis->vis );
	}
	/*
	for ( int i = 0; i < nNumVises; ++i )
	{
		const DWORD y = DWORD( terrainInfo.tiles.GetSizeY() - int(vises[i].y) - 1 );
		const DWORD key = ( y << 16 ) | DWORD( vises[i].x );
		visibilities[key] = vises[i].vis;
	}
	*/
	//
	MoveWarFog();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static CMatrixStack<4> mstack;
void CTerrain::MovePatches()
{
	mstack.Set( pGFX->GetViewportMatrix() );
	mstack.Push( pGFX->GetProjectionMatrix() );
	mstack.Push( pGFX->GetViewMatrix() );
	SHMatrix matTransform = mstack();
	//
	const float fPatchSize = fWorldCellSize * STerrainPatchInfo::nSizeX;
	const CVec3 vO( 0, fPatchSize * terrainInfo.patches.GetSizeY(), 0 );
	CVec3 vScreenO;											// screen space position of the terrain's origin
	matTransform.RotateHVector( &vScreenO, vO );
	//
	const int nPatchHalfAxisX = fCellSizeX * STerrainPatchInfo::nSizeX;
	const int nPatchHalfAxisY = fCellSizeY * STerrainPatchInfo::nSizeY;
	for ( CPatchesList::iterator it = patches.begin(); it != patches.end(); ++it )
	{
		int nX = int( vScreenO.x ) + (it->nX - it->nY)*nPatchHalfAxisX;
		int nY = int( vScreenO.y ) + (it->nX + it->nY)*nPatchHalfAxisY;
		MovePatch( nX, nY, terrainInfo.patches[it->nY][it->nX], &(*it) );
	}
	//
	ReBuildMeshes();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTerrain::MoveWarFog()
{
	mstack.Set( pGFX->GetViewportMatrix() );
	mstack.Push( pGFX->GetProjectionMatrix() );
	mstack.Push( pGFX->GetViewMatrix() );
	SHMatrix matTransform = mstack();
	//
	const float fPatchSize = fWorldCellSize * STerrainPatchInfo::nSizeX;
	const CVec3 vO( 0, fPatchSize * terrainInfo.patches.GetSizeY(), 0 );
	CVec3 vScreenO;											// screen space position of the terrain's origin
	matTransform.RotateHVector( &vScreenO, vO );
	//
	const int nPatchHalfAxisX = fCellSizeX * STerrainPatchInfo::nSizeX;
	const int nPatchHalfAxisY = fCellSizeY * STerrainPatchInfo::nSizeY;
	for ( CPatchesList::iterator it = patches.begin(); it != patches.end(); ++it )
	{
		int nX = int( vScreenO.x ) + (it->nX - it->nY)*nPatchHalfAxisX;
		int nY = int( vScreenO.y ) + (it->nX + it->nY)*nPatchHalfAxisY;
		MoveWarFogPatch( nX, nY, &(*it) );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTerrain::CreatePatch( const STerrainPatchInfo &patch, STerrainPatch *pPatch )
{
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTerrain::MovePatch( int nX, int nY, const STerrainPatchInfo &patch, STerrainPatch *pPatch )
{
	const float fTilesetSizeX = pTileset->GetSizeX( 0 );
	const float fTilesetSizeY = pTileset->GetSizeY( 0 );
	const float fCrossetSizeX = pCrosset->GetSizeX( 0 );
	const float fCrossetSizeY = pCrosset->GetSizeY( 0 );
	//
	CreateTiles( nX, nY, patch, terrainInfo, tilesetDesc, nNoiseSizeX, nNoiseSizeY, pPatch );
	CreateCrosses( nX, nY, patch, terrainInfo, tilesetDesc, crossetDesc, nNoiseSizeX, nNoiseSizeY, pPatch );
	MoveWarFogPatch( nX, nY, pPatch );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTerrain::MoveWarFogPatch( int nX, int nY, STerrainPatch *pPatch )
{
	CreateWarFog( nX, nY, pPatch->nX * STerrainPatchInfo::nSizeX, pPatch->nY * STerrainPatchInfo::nSizeY, 
		            visibilities, terrainInfo, pPatch );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
inline const DWORD GetFlags( const float x1, const float y1, const float x2, const float y2, const float x, const float y )
{
	const float t1 = x - x1, t2 = y - y1, t3 = x2 - x - 1.0f, t4 = y2 - y - 1.0f;
	return (  FP_BITS_CONST(t1) >> 31 ) |
				 (( FP_BITS_CONST(t2) >> 30 ) & 2 ) |
				 (( FP_BITS_CONST(t3) >> 29 ) & 4 ) |
				 (( FP_BITS_CONST(t4) >> 28 ) & 8 );
}
inline const DWORD CheckForRect( const CTRect<float> &rect, const float x, const float y )
{
	return GetFlags( rect.x1, rect.y1, rect.x2, rect.y2, x, y );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <class TVertex>
void AddVertices( const std::vector<TVertex> &src, std::vector<TVertex> &dst, 
								  std::vector<WORD> &indices, const CTRect<float> &rcScreen )
{
	for ( std::vector<TVertex>::const_iterator vertex = src.begin(); vertex != src.end(); vertex += 4 )
	{
		const TVertex &v0 = *vertex;
		const TVertex &v1 = *(vertex + 1);
		const TVertex &v2 = *(vertex + 2);
		const TVertex &v3 = *(vertex + 3);
		const DWORD dwPoint0 = CheckForRect( rcScreen, v2.x, v0.y );
		const DWORD dwPoint1 = CheckForRect( rcScreen, v1.x, v0.y );
		const DWORD dwPoint2 = CheckForRect( rcScreen, v2.x, v3.y );
		const DWORD dwPoint3 = CheckForRect( rcScreen, v1.x, v3.y );
		if ( (dwPoint0 & dwPoint1 & dwPoint2 & dwPoint3) != 0 ) 
			continue;
		//
		const int nNumVertices = dst.size();
		// 
		dst.push_back( v0 );
		dst.push_back( v1 );
		dst.push_back( v2 );
		dst.push_back( v3 );
		// 
		// 0, 2, 1, 1, 2, 3
		indices.push_back( nNumVertices + 0 );
		indices.push_back( nNumVertices + 2 );
		indices.push_back( nNumVertices + 1 );
		indices.push_back( nNumVertices + 1 );
		indices.push_back( nNumVertices + 2 );
		indices.push_back( nNumVertices + 3 );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CountLayerCrosses( std::vector<int> &counts, const std::vector<STerrainPatch::CVertex1List> &layers )
{
	counts.resize( Max(counts.size(), layers.size()) );
	for ( int i = 0; i != layers.size(); ++i )
		counts[i] += layers[i].size();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTerrain::ReBuildMeshes()
{
	int nNumMainVerts1 = 0, nNumMainVerts2 = 0, nNumBaseCrosses = 0, nNumNoises = 0, nNumLayers = 0;
	std::vector<int> nNumLayerCrosses, nNumLayerNoises;
	for ( CPatchesList::iterator it = patches.begin(); it != patches.end(); ++it )
	{
		nNumMainVerts1 += it->mainverts1.size();
		nNumMainVerts2 += it->mainverts2.size();
		nNumBaseCrosses += it->basecrossverts.size();
		nNumNoises += it->noiseverts.size();
		CountLayerCrosses( nNumLayerCrosses, it->layercrossverts );
		CountLayerCrosses( nNumLayerNoises, it->layernoiseverts );
		nNumLayers = Max( nNumLayers, int(it->layercrossverts.size()) );
	}
	//
	mshCurrent.mshNoiseTiles.Reserve( nNumMainVerts1, nNumMainVerts1/4*6 );
	mshCurrent.mshNoNoiseTiles.Reserve( nNumMainVerts2, nNumMainVerts2/4*6 );
	mshCurrent.mshBaseCrosses.Reserve( nNumBaseCrosses, nNumBaseCrosses/4*6 );
	mshCurrent.mshNoises.Reserve( nNumNoises, nNumNoises/4*6 );
	// layered crosses
	mshCurrent.mshCrossLayers.clear();
	mshCurrent.mshCrossLayers.resize( nNumLayers );
	for ( int i = 0; i != nNumLayers; ++i )
	{
		mshCurrent.mshCrossLayers[i].mshCrosses.Reserve( nNumLayerCrosses[i], nNumLayerCrosses[i]/4*6 );
		mshCurrent.mshCrossLayers[i].mshNoises.Reserve( nNumLayerNoises[i], nNumLayerNoises[i]/4*6 );
	}
	//
	const CTRect<float> rcScreen = pGFX->GetScreenRect();
	for ( CPatchesList::iterator it = patches.begin(); it != patches.end(); ++it )
	{
		AddVertices( it->mainverts1, mshCurrent.mshNoiseTiles.vertices, mshCurrent.mshNoiseTiles.indices, rcScreen );
		AddVertices( it->mainverts2, mshCurrent.mshNoNoiseTiles.vertices, mshCurrent.mshNoNoiseTiles.indices, rcScreen );
		AddVertices( it->basecrossverts, mshCurrent.mshBaseCrosses.vertices, mshCurrent.mshBaseCrosses.indices, rcScreen );
		AddVertices( it->noiseverts, mshCurrent.mshNoises.vertices, mshCurrent.mshNoises.indices, rcScreen );
		const int nNumLocalLayers = it->layercrossverts.size();
		for ( int i = 0; i != nNumLocalLayers; ++i )
		{
			AddVertices( it->layercrossverts[i], mshCurrent.mshCrossLayers[i].mshCrosses.vertices, mshCurrent.mshCrossLayers[i].mshCrosses.indices, rcScreen );
			AddVertices( it->layernoiseverts[i], mshCurrent.mshCrossLayers[i].mshNoises.vertices, mshCurrent.mshCrossLayers[i].mshNoises.indices, rcScreen );
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CTerrain::ReservePatchesData()
{
	std::vector<int> numLayerCrosses;
	const int nNumPatches = patches.size();
	const int nNumTilesInPatch = STerrainPatchInfo::nSizeX * STerrainPatchInfo::nSizeY;
	int nNumBaseCrosses = 0, nNumNoises = 0, nNumLayers = 0;
	for ( CPatchesList::const_iterator it = patches.begin(); it != patches.end(); ++it )
	{
		const STerrainPatchInfo &patch = terrainInfo.patches[it->nY][it->nX];
		nNumBaseCrosses += patch.basecrosses.size();
		nNumNoises += patch.noisecrosses.size();
		const int nNumLayerCrosses = patch.layercrosses.size();
		if ( nNumLayers < nNumLayerCrosses ) 
		{
			nNumLayers = nNumLayerCrosses;
			numLayerCrosses.resize( nNumLayerCrosses );
		}
		for ( int nLayer = 0; nLayer < nNumLayerCrosses; ++nLayer ) 
			numLayerCrosses[nLayer] += patch.layercrosses[nLayer].size();
	}
	mshCurrent.mshNoiseTiles.Reserve( nNumPatches * nNumTilesInPatch*4, nNumPatches * nNumTilesInPatch*6 );
	mshCurrent.mshNoNoiseTiles.Reserve( nNumPatches * nNumTilesInPatch*4, nNumPatches * nNumTilesInPatch*6 );
	mshCurrent.mshBaseCrosses.Reserve( nNumPatches * nNumBaseCrosses*4, nNumPatches * nNumBaseCrosses*6 );
	mshCurrent.mshNoises.Reserve( nNumPatches * nNumNoises*4, nNumPatches * nNumNoises*6 );
	// layered crosses
	mshCurrent.mshCrossLayers.clear();
	mshCurrent.mshCrossLayers.resize( nNumLayers );
	for ( int i = 0; i != nNumLayers; ++i )
	{
		mshCurrent.mshCrossLayers[i].mshCrosses.Reserve( nNumPatches * numLayerCrosses[i]*4, nNumPatches * numLayerCrosses[i]*6 );
		mshCurrent.mshCrossLayers[i].mshNoises.Reserve( nNumPatches * numLayerCrosses[i]*4, nNumPatches * numLayerCrosses[i]*6 );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CTerrain::operator&( IStructureSaver &ss )
{
	CSaverAccessor saver = &ss;
	//
	if ( saver.IsReading() )
		Init( GetSingletonGlobal() );
	//
	saver.Add( 1, &szMapName );
	saver.Add( 2, &nTilesetSizeX );
	saver.Add( 3, &nTilesetSizeY );
	saver.Add( 4, &nCrossetSizeX );
	saver.Add( 5, &nCrossetSizeY );
	saver.Add( 6, &nNoiseSizeX );
	saver.Add( 7, &nNoiseSizeY );
	// load external map data
	if ( saver.IsReading() )
	{
		SLoadTerrain loadTerrain;
		{
			IDataStorage *pStorage = GetSingleton<IDataStorage>();
			if ( CPtr<IDataStream> pStream = pStorage->OpenStream((szMapName + ".xml").c_str(), STREAM_ACCESS_READ) )
			{
				CTreeAccessor tree = CreateDataTreeSaver( pStream, IDataTree::READ );
				tree.Add( "Terrain", &loadTerrain.info );
			}
			else if ( CPtr<IDataStream> pStream = pStorage->OpenStream((szMapName + ".bzm").c_str(), STREAM_ACCESS_READ) )
			{
				CPtr<IStructureSaver> pSaver = CreateStructureSaver( pStream, IStructureSaver::READ );
				CSaverAccessor saver = pSaver;
				saver.Add( 1, &loadTerrain );
			}
			else
			{
				NI_ASSERT_TF( false, NStr::Format("Can't open stream \"%s\" to read map", szMapName.c_str()), return false );
			}
		}
		// load terrain data from map
		LoadLocal( szMapName.c_str(), loadTerrain.info, true, false );
		// invalidate old anchor to move all dependent patches
		vOldAnchor.Set( -1000000, -1000000, 0 );
	}
	//
	saver.Add( 8, &bGridOn );
	saver.Add( 9, &bEnableNoise );
	saver.Add( 11, &pTileset );
	saver.Add( 12, &pCrosset );
	saver.Add( 13, &pNoise );
	saver.Add( 16, &collectedInfo );
	saver.Add( 17, &roadPatches );
	saver.Add( 18, &rivers );
	saver.Add( 19, &roads );
	saver.Add( 20, &visibilities );
	// re-build road and river layers geometry data
	if ( saver.IsReading() ) 
	{
		/*
		for ( int i = 0; i < roads.size(); ++i )
			roads[i].BuildLayers();
		for ( int i = 0; i < rivers.size(); ++i )
			rivers[i].BuildLayers();
		*/
	}
	//
	return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
