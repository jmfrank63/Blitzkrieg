#include "StdAfx.h"

#include "Builders.h"

#include "..\AILogic\AITypes.h"
#include "TerrainInternal.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const float fHeightCoeff = 0.86602540378444f;// cos( ToRadian(30.0f) );
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** defines for terrain coords generation
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define MAKE_COORD_X1 ( fX + (j - i)*fCellSizeX )
//#define MAKE_COORD_Y1 ( fY + (j + i)*fCellSizeY )
#define MAKE_COORD_Y1 ( fY + (j + i)*fCellSizeY - heights[nMapY][nMapX].fHeight*fHeightCoeff )

#define MAKE_COORD_X2 ( fX + (j - i + 1)*fCellSizeX )
//#define MAKE_COORD_Y2 ( fY + (j + i + 1)*fCellSizeY )
#define MAKE_COORD_Y2 ( fY + (j + i + 1)*fCellSizeY - heights[nMapY][nMapX1].fHeight*fHeightCoeff )

#define MAKE_COORD_X3 ( fX + (j - i - 1)*fCellSizeX )
//#define MAKE_COORD_Y3 ( fY + (j + i + 1)*fCellSizeY )
#define MAKE_COORD_Y3 ( fY + (j + i + 1)*fCellSizeY - heights[nMapY1][nMapX].fHeight*fHeightCoeff )

#define MAKE_COORD_X4 ( fX + (j - i)*fCellSizeX )
//#define MAKE_COORD_Y4 ( fY + (j + i + 2)*fCellSizeY )
#define MAKE_COORD_Y4 ( fY + (j + i + 2)*fCellSizeY - heights[nMapY1][nMapX1].fHeight*fHeightCoeff )
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ************************************************************************************************************************ //
// **
// ** defines for noise coords generation
// **
// **
// **
// ************************************************************************************************************************ //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define MAKE_NOISE_X1 ( fPosX*fNoiseRcprX )
#define MAKE_NOISE_Y1 ( fPosY*fNoiseRcprY )

#define MAKE_NOISE_X2 ( (fPosX + fTileSize)*fNoiseRcprX )
#define MAKE_NOISE_Y2 ( fPosY*fNoiseRcprY )

#define MAKE_NOISE_X3 ( fPosX*fNoiseRcprX )
#define MAKE_NOISE_Y3 ( (fPosY + fTileSize)*fNoiseRcprY )

#define MAKE_NOISE_X4 ( (fPosX + fTileSize)*fNoiseRcprX )
#define MAKE_NOISE_Y4 ( (fPosY + fTileSize)*fNoiseRcprY )
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <class TVertex>
void Reserve( std::vector<TVertex> &vertices, const int nReserveSize )
{
	vertices.clear();
	vertices.reserve( nReserveSize );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <class TVertex>
TVertex* ResizeToAdd( std::vector<TVertex> &vertices, const int nReSize )
{
	const int nNumVertices = vertices.size();
	vertices.resize( nNumVertices + nReSize );
	return &( vertices[nNumVertices] );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CreateTiles( const float fX, const float fY, 
								  const STerrainPatchInfo &patch, const STerrainInfo &info, const STilesetDesc &tileset, 
									const float fNoiseSizeX, const float fNoiseSizeY, struct STerrainPatch *pPatch )
{
	const CArray2D<SMainTileInfo> &tiles = info.tiles;
	const CArray2D<SVertexAltitude> &heights = info.altitudes;
	//
	const float fStartX = patch.nStartX * STerrainPatchInfo::nSizeX;
	const float fStartY = patch.nStartY * STerrainPatchInfo::nSizeY;
	const float fTileSize = 32;
	//
	const float fNoiseRcprX = 1.0f / fNoiseSizeX;
	const float fNoiseRcprY = 1.0f / fNoiseSizeY;
	//
	Reserve( pPatch->mainverts1, STerrainPatchInfo::nSizeX*STerrainPatchInfo::nSizeY*4 );
	Reserve( pPatch->mainverts2, STerrainPatchInfo::nSizeX*STerrainPatchInfo::nSizeY*4 );
	//
	for ( int i=0; i<STerrainPatchInfo::nSizeY; ++i )
	{
		const float fPosY = ( fStartX + i ) * fTileSize;
		const int nMapY = patch.nStartY + i;
		const int nMapY1 = nMapY + 1;
		for ( int j=0; j<STerrainPatchInfo::nSizeX; ++j )
		{
			const float fPosX = ( fStartX + j ) * fTileSize;
			const int nMapX = patch.nStartX + j;
			const int nMapX1 = nMapX + 1;
			const int nTileMapsIndex = tiles[nMapY][nMapX].tile;
			const CVec2 *maps = tileset.tilemaps[nTileMapsIndex].maps;

			if ( tiles[nMapY][nMapX].noise == 1 ) 
			{
				STerrainPatch::SVertex1 *pVerts = ResizeToAdd( pPatch->mainverts1, 4 );
				//
				pVerts->Setup( MAKE_COORD_X1, MAKE_COORD_Y1, 1, 1, MakeShade(heights[nMapY][nMapX].shade), 
											 maps[0].u, maps[0].v, MAKE_NOISE_X1, MAKE_NOISE_Y1 );
				++pVerts;
				pVerts->Setup( MAKE_COORD_X2, MAKE_COORD_Y2, 1, 1, MakeShade(heights[nMapY][nMapX1].shade), 
											 maps[1].u, maps[1].v, MAKE_NOISE_X2, MAKE_NOISE_Y2 );
				++pVerts;
				pVerts->Setup( MAKE_COORD_X3, MAKE_COORD_Y3, 1, 1, MakeShade(heights[nMapY1][nMapX].shade), 
											 maps[2].u, maps[2].v, MAKE_NOISE_X3, MAKE_NOISE_Y3 );
				++pVerts;
				pVerts->Setup( MAKE_COORD_X4, MAKE_COORD_Y4, 1, 1, MakeShade(heights[nMapY1][nMapX1].shade), 
											 maps[3].u, maps[3].v, MAKE_NOISE_X4, MAKE_NOISE_Y4 );
				++pVerts;
			}
			else
			{
				STerrainPatch::SVertex2 *pVerts = ResizeToAdd( pPatch->mainverts2, 4 );
				//
				pVerts->Setup( MAKE_COORD_X1, MAKE_COORD_Y1, 1, 1, 
											 MakeShade(heights[nMapY][nMapX].shade), 0xff000000, 
											 maps[0].u, maps[0].v );
				++pVerts;
				pVerts->Setup( MAKE_COORD_X2, MAKE_COORD_Y2, 1, 1, 
											 MakeShade(heights[nMapY][nMapX1].shade), 0xff000000,
											 maps[1].u, maps[1].v );
				++pVerts;
				pVerts->Setup( MAKE_COORD_X3, MAKE_COORD_Y3, 1, 1, 
											 MakeShade(heights[nMapY1][nMapX].shade), 0xff000000,
											 maps[2].u, maps[2].v );
				++pVerts;
				pVerts->Setup( MAKE_COORD_X4, MAKE_COORD_Y4, 1, 1, 
											 MakeShade(heights[nMapY1][nMapX1].shade), 0xff000000,
											 maps[3].u, maps[3].v );
				++pVerts;
			}
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CreateCrosses( const float fX, const float fY, const STerrainPatchInfo &patch, const STerrainInfo &info,
									  const STilesetDesc &tileset, const SCrossetDesc &crosset, 
										const float fNoiseSizeX, const float fNoiseSizeY, struct STerrainPatch *pPatch )
{
	if ( !patch.HasCrosses() )
		return;
	//
	const CArray2D<SMainTileInfo> &tiles = info.tiles;
	const CArray2D<SVertexAltitude> &heights = info.altitudes;
	//
	const float fStartX = patch.nStartX * STerrainPatchInfo::nSizeX;
	const float fStartY = patch.nStartY * STerrainPatchInfo::nSizeY;
	const float fTileSize = 32;
	//
	const float fNoiseRcprX = 1.0f / fNoiseSizeX;
	const float fNoiseRcprY = 1.0f / fNoiseSizeY;
	// create base crosses (w/o noise)
	Reserve( pPatch->basecrossverts, patch.basecrosses.size()*4 );
	for ( STerrainPatchInfo::CCrossesList::const_iterator it = patch.basecrosses.begin(); it != patch.basecrosses.end(); ++it )
	{
		const int i = it->y, j = it->x;
		const int nMapY = patch.nStartY + i;
		const int nMapY1 = nMapY + 1;
		const int nMapX = patch.nStartX + j;
		const int nMapX1 = nMapX + 1;
		const CVec2 *maps = tileset.tilemaps[it->tile].maps;
		const CVec2 *maskmaps = crosset.tilemaps[it->cross].maps;
		//
		STerrainPatch::SVertex1 *pVerts = ResizeToAdd( pPatch->basecrossverts, 4 );
		// 0
		pVerts->Setup( MAKE_COORD_X1, MAKE_COORD_Y1, 1, 1, MakeShade(heights[nMapY][nMapX].shade), 
									 maps[0].u, maps[0].v, maskmaps[0].u, maskmaps[0].v );
		++pVerts;
		// 1
		pVerts->Setup( MAKE_COORD_X2, MAKE_COORD_Y2, 1, 1, MakeShade(heights[nMapY][nMapX1].shade), 
									 maps[1].u, maps[1].v, maskmaps[1].u, maskmaps[1].v );
		++pVerts;
		// 2
		pVerts->Setup( MAKE_COORD_X3, MAKE_COORD_Y3, 1, 1, MakeShade(heights[nMapY1][nMapX].shade), 
									 maps[2].u, maps[2].v, maskmaps[2].u, maskmaps[2].v );
		++pVerts;
		// 3
		pVerts->Setup( MAKE_COORD_X4, MAKE_COORD_Y4, 1, 1, MakeShade(heights[nMapY1][nMapX1].shade), 
									 maps[3].u, maps[3].v, maskmaps[3].u, maskmaps[3].v );
		++pVerts;
	}
	// create noises (w/o crosses)
	Reserve( pPatch->noiseverts, patch.noisecrosses.size()*4 );
	for ( STerrainPatchInfo::CCrossesList::const_iterator it = patch.noisecrosses.begin(); it != patch.noisecrosses.end(); ++it )
	{
		const int i = it->y, j = it->x;
		const int nMapY = patch.nStartY + i;
		const int nMapY1 = nMapY + 1;
		const int nMapX = patch.nStartX + j;
		const int nMapX1 = nMapX + 1;
		//
		const float fPosY = ( fStartX + i ) * fTileSize;
		const float fPosX = ( fStartX + j ) * fTileSize;

		STerrainPatch::SVertex2 *pVerts = ResizeToAdd( pPatch->noiseverts, 4 );
		// 0
		pVerts->Setup( MAKE_COORD_X1, MAKE_COORD_Y1, 1, 1, 0xffffffff, 0xff000000, MAKE_NOISE_X1, MAKE_NOISE_Y1 );
		++pVerts;
		// 1
		pVerts->Setup( MAKE_COORD_X2, MAKE_COORD_Y2, 1, 1, 0xffffffff, 0xff000000, MAKE_NOISE_X2, MAKE_NOISE_Y2 );
		++pVerts;
		// 2
		pVerts->Setup( MAKE_COORD_X3, MAKE_COORD_Y3, 1, 1, 0xffffffff, 0xff000000, MAKE_NOISE_X3, MAKE_NOISE_Y3 );
		++pVerts;
		// 3
		pVerts->Setup( MAKE_COORD_X4, MAKE_COORD_Y4, 1, 1, 0xffffffff, 0xff000000, MAKE_NOISE_X4, MAKE_NOISE_Y4 );
		++pVerts;
	}
	// layered crosses and noises
	pPatch->layercrossverts.clear();
	pPatch->layernoiseverts.clear();
	pPatch->layercrossverts.resize( patch.layercrosses.size() );
	pPatch->layernoiseverts.resize( patch.layercrosses.size() );
	for ( int nLayer = 0; nLayer != patch.layercrosses.size(); ++nLayer )
	{
		Reserve( pPatch->layercrossverts[nLayer], patch.layercrosses[nLayer].size()*4 );
		Reserve( pPatch->layernoiseverts[nLayer], patch.layercrosses[nLayer].size()*4 );
		for ( STerrainPatchInfo::CCrossesList::const_iterator it = patch.layercrosses[nLayer].begin(); it != patch.layercrosses[nLayer].end(); ++it )
		{
			const int i = it->y, j = it->x;
			const int nMapY = patch.nStartY + i;
			const int nMapY1 = nMapY + 1;
			const int nMapX = patch.nStartX + j;
			const int nMapX1 = nMapX + 1;
			const CVec2 *maps = tileset.tilemaps[it->tile].maps;
			const CVec2 *maskmaps = crosset.tilemaps[it->cross].maps;
			//
			const float fPosY = ( fStartX + i ) * fTileSize;
			const float fPosX = ( fStartX + j ) * fTileSize;
			//
			if ( it->flags & SCrossTileInfo::CROSS ) 
			{
				STerrainPatch::SVertex1 *pVerts = ResizeToAdd( pPatch->layercrossverts[nLayer], 4 );
				// 0
				pVerts->Setup( MAKE_COORD_X1, MAKE_COORD_Y1, 1, 1, MakeShade(heights[nMapY][nMapX].shade), 
											 maps[0].u, maps[0].v, maskmaps[0].u, maskmaps[0].v );
				++pVerts;
				// 1
				pVerts->Setup( MAKE_COORD_X2, MAKE_COORD_Y2, 1, 1, MakeShade(heights[nMapY][nMapX1].shade), 
											 maps[1].u, maps[1].v, maskmaps[1].u, maskmaps[1].v );
				++pVerts;
				// 2
				pVerts->Setup( MAKE_COORD_X3, MAKE_COORD_Y3, 1, 1, MakeShade(heights[nMapY1][nMapX].shade), 
											 maps[2].u, maps[2].v, maskmaps[2].u, maskmaps[2].v );
				++pVerts;
				// 3
				pVerts->Setup( MAKE_COORD_X4, MAKE_COORD_Y4, 1, 1, MakeShade(heights[nMapY1][nMapX1].shade), 
											 maps[3].u, maps[3].v, maskmaps[3].u, maskmaps[3].v );
				++pVerts;
			}
			// check for crossed NOISE tile
			if ( it->flags == SCrossTileInfo::MIXED )
			{
				const float fPosY = ( fStartX + i ) * fTileSize;
				const float fPosX = ( fStartX + j ) * fTileSize;

				STerrainPatch::SVertex1 *pVerts = ResizeToAdd( pPatch->layernoiseverts[nLayer], 4 );
				// 0
				pVerts->Setup( MAKE_COORD_X1, MAKE_COORD_Y1, 1, 1, 0xffffffff, 
											 MAKE_NOISE_X1, MAKE_NOISE_Y1, maskmaps[0].u, maskmaps[0].v );
				++pVerts;
				// 1
				pVerts->Setup( MAKE_COORD_X2, MAKE_COORD_Y2, 1, 1, 0xffffffff, 
											 MAKE_NOISE_X2, MAKE_NOISE_Y2, maskmaps[1].u, maskmaps[1].v );
				++pVerts;
				// 2
				pVerts->Setup( MAKE_COORD_X3, MAKE_COORD_Y3, 1, 1, 0xffffffff, 
											 MAKE_NOISE_X3, MAKE_NOISE_Y3, maskmaps[2].u, maskmaps[2].v );
				++pVerts;
				// 3
				pVerts->Setup( MAKE_COORD_X4, MAKE_COORD_Y4, 1, 1, 0xffffffff, 
											 MAKE_NOISE_X4, MAKE_NOISE_Y4, maskmaps[3].u, maskmaps[3].v );
				++pVerts;
			}
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
void CreateWarFog( const float fX, const float fY, int nStartX, int nStartY, const std::unordered_map<DWORD, DWORD> &visibilities, 
									 const STerrainInfo &info, struct STerrainPatch *pPatch )
{
	const CArray2D<SVertexAltitude> &heights = info.altitudes;
	// vertices
	pPatch->warfogverts.resize( (STerrainPatchInfo::nSizeX + 1) * (STerrainPatchInfo::nSizeY + 1) );
	STerrainPatch::SVertex2 *pVerts = &( pPatch->warfogverts[0] );
	for ( int i=0; i<STerrainPatchInfo::nSizeY + 1; ++i )
	{
		const int nMapY = nStartY + i;
		for ( int j=0; j<STerrainPatchInfo::nSizeX + 1; ++j )
		{
			const int nMapX = nStartX + j;
			//
			pVerts->Setup( MAKE_COORD_X1, MAKE_COORD_Y1, 1, 1, GetVisibilityColor(nMapX, nMapY, visibilities), 0xff000000, 0, 0 );
			++pVerts;
		}
	}
	// indices
	pPatch->warfoginds.resize( STerrainPatchInfo::nSizeX * STerrainPatchInfo::nSizeY * 6 );
	WORD *pInds = &( pPatch->warfoginds[0] );
	for ( int i=0; i<STerrainPatchInfo::nSizeY; ++i )
	{
		for ( int j=0; j<STerrainPatchInfo::nSizeX; ++j )
		{
			const int nIdx0 = i*(STerrainPatchInfo::nSizeX + 1) + j;
			const int nIdx1 = nIdx0 + 1;
			const int nIdx2 = (i + 1) * (STerrainPatchInfo::nSizeX + 1) + j;
			const int nIdx3 = nIdx2 + 1;
			//
			*pInds++ = nIdx0;
			*pInds++ = nIdx2;
			*pInds++ = nIdx1;

			*pInds++ = nIdx1;
			*pInds++ = nIdx2;
			*pInds++ = nIdx3;
		}
	}
}
*/
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CreateWarFog( const float fX, const float fY, int nStartX, int nStartY, const std::unordered_map<DWORD, DWORD> &visibilities, 
									 const STerrainInfo &info, struct STerrainPatch *pPatch )
{
	const CArray2D<SVertexAltitude> &heights = info.altitudes;
	// vertices
	const int nNumSubPatches = pPatch->bSubPatches[0] + pPatch->bSubPatches[1] + pPatch->bSubPatches[2] + pPatch->bSubPatches[3];
	const int nHalfSizeX = STerrainPatchInfo::nSizeX/2;
	const int nHalfSizeY = STerrainPatchInfo::nSizeY/2;
	const int nSizeX1 = nHalfSizeX + 1;
	const int nSizeY1 = nHalfSizeY + 1;
	pPatch->warfogverts.resize( nSizeX1 * nSizeY1 * nNumSubPatches );
	pPatch->warfoginds.resize( nHalfSizeX * nHalfSizeY * 6 * nNumSubPatches );
	// for each subpatch
	int nFirstVertex = 0, nFirstIndex = 0;
	for ( int k = 0; k < 2; ++k )	// y
	{
		for ( int l = 0; l < 2; ++l )	// x
		{
			if ( pPatch->bSubPatches[(1 - k)*2 + l] == false ) 
				continue;
			STerrainPatch::SVertex2 *pVerts = &( pPatch->warfogverts[nFirstVertex] );
			for ( int i = nHalfSizeY*k; i < nHalfSizeY*(k + 1) + 1; ++i )
			{
				const int nMapY = nStartY + i;
				for ( int j = nHalfSizeX*l; j < nHalfSizeX*(l + 1) + 1; ++j )
				{
					const int nMapX = nStartX + j;
					//
					pVerts->Setup( MAKE_COORD_X1, MAKE_COORD_Y1, 1, 1, GetVisibilityColor(nMapX, nMapY, visibilities), 0xff000000, 0, 0 );
					++pVerts;
				}
			}
			// indices
			WORD *pInds = &( pPatch->warfoginds[nFirstIndex] );
			for ( int i = 0; i < nHalfSizeY; ++i )
			{
				for ( int j = 0; j < nHalfSizeX; ++j )
				{
					const int nIdx0 = nFirstVertex + i*nSizeX1 + j;
					const int nIdx1 = nIdx0 + 1;
					const int nIdx2 = nFirstVertex + (i + 1) * nSizeX1 + j;
					const int nIdx3 = nIdx2 + 1;
					//
					*pInds++ = nIdx0;
					*pInds++ = nIdx2;
					*pInds++ = nIdx1;

					*pInds++ = nIdx1;
					*pInds++ = nIdx2;
					*pInds++ = nIdx3;
				}
			}
			//
			nFirstVertex += nSizeX1 * nSizeY1;
			nFirstIndex += nHalfSizeX * nHalfSizeY * 6;
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CreateMarker( const float fX, const float fY, const std::vector< CTPoint<int> > &marker, const STerrainInfo &info,
									 IGFXVertices *pVertices, IGFXIndices *pIndices )
{
	const CArray2D<SVertexAltitude> &heights = info.altitudes;
	{
		CVerticesLock<SMarkerVertex> vertices( pVertices );
		SMarkerVertex *pVerts = vertices.GetBuffer();
		DWORD dwColor = 0x40ff0000, dwSpecular = 0xff000000;
		for ( int k=0; k<marker.size(); ++k )
		{
			const int i = marker[k].y;
			const int j = marker[k].x;
			
			const int nMapY = i;
			const int nMapY1 = nMapY + 1;
			const int nMapX = j;
			const int nMapX1 = nMapX + 1;

			pVerts->Setup( MAKE_COORD_X1, MAKE_COORD_Y1, 1, 1,
										 dwColor, dwSpecular, 0, 0 );
			++pVerts;
			pVerts->Setup( MAKE_COORD_X2, MAKE_COORD_Y2, 1, 1,
										 dwColor, dwSpecular, 0, 0 );
			++pVerts;
			pVerts->Setup( MAKE_COORD_X3, MAKE_COORD_Y3, 1, 1,
										 dwColor, dwSpecular, 0, 0 );
			++pVerts;
			pVerts->Setup( MAKE_COORD_X4, MAKE_COORD_Y4, 1, 1,
										 dwColor, dwSpecular, 0, 0 );
			++pVerts;
		}
	}
	// indices
	{
		const int nNumMainTiles = marker.size();
		CIndicesLock<WORD> indices( pIndices );
		WORD *pInds = indices.GetBuffer();
		for ( int i=0, nIndex=0; i<nNumMainTiles; ++i, nIndex+=4 )
		{
			*pInds++ = nIndex + 0;
			*pInds++ = nIndex + 2;
			*pInds++ = nIndex + 1;
			*pInds++ = nIndex + 1;
			*pInds++ = nIndex + 2;
			*pInds++ = nIndex + 3;
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
inline const float GetHeight( int x, int y, const STerrainInfo &info )
{
	const CArray2D<SVertexAltitude> &heights = info.altitudes;
	const int nMapX = x/2;
	const int nMapY = y/2;
	if ( (nMapX < info.tiles.GetSizeX()) && (nMapY < info.tiles.GetSizeY()) && (nMapX >= 0) && (nMapY >= 0) ) 
	{
		if ( (x & 1) && (y & 1) ) 
			return ( heights[nMapY + 1][nMapX].fHeight + heights[nMapY][nMapX + 1].fHeight ) / 2.0f * fHeightCoeff;
		else if ( x & 1 )
			return ( heights[nMapY][nMapX + 1].fHeight + heights[nMapY + 1][nMapX + 1].fHeight ) / 2.0f * fHeightCoeff;
		else if ( y & 1 ) 
			return ( heights[nMapY + 1][nMapX].fHeight + heights[nMapY + 1][nMapX + 1].fHeight ) / 2.0f * fHeightCoeff;
		else
			return heights[nMapY][nMapX].fHeight * fHeightCoeff;
	}
	else
		return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CreateAIMarker( const float fX, const float fY, const STerrainInfo &info, 
										 struct SAIPassabilityInfo *marker, int nMarkerSize, IGFXVertices *pVertices, IGFXIndices *pIndices )
{
	const CArray2D<SVertexAltitude> &heights = info.altitudes;
	{
		CVerticesLock<SMarkerVertex> vertices( pVertices );
		SMarkerVertex *pVerts = vertices.GetBuffer();
		const DWORD dwSpecular = 0xff000000;
		for ( int k = 0; k < nMarkerSize; ++k )
		{
			// humans, track, half-track, wheel <= �����: �� ����-������� (128) �� ������-������� (255)
			// 3, 2, 1, 0
			DWORD dwColor = marker[k].pass == 0 ? 0 : ( DWORD( 255UL - (GetMSB(DWORD(marker[k].pass)) + 1) * 32 ) << 8 ) | 0x80000000;
			
			const int i = info.AIY2Terra( int( marker[k].y ) );
			const int j = marker[k].x;
			//
			pVerts->Setup( fX + ( j - i ) * fCellSizeX / 2.0f,  
										 fY + ( j + i ) * fCellSizeY / 2.0f - GetHeight( j, i, info ), 
										 1, 1, dwColor, dwSpecular, 0, 0 );
			++pVerts;
			pVerts->Setup( fX + ( j - i + 1 ) * fCellSizeX / 2.0f, 
										 fY + ( j + i + 1 ) * fCellSizeY / 2.0f - GetHeight( j + 1, i, info ), 
										 1, 1, dwColor, dwSpecular, 0, 0 );
			++pVerts;
			pVerts->Setup( fX + ( j - i - 1 ) * fCellSizeX / 2.0f, 
										 fY + ( j + i + 1 ) * fCellSizeY / 2.0f - GetHeight( j, i + 1, info ), 
										 1, 1, dwColor, dwSpecular, 0, 0 );
			++pVerts;
			pVerts->Setup( fX + ( j - i ) * fCellSizeX / 2.0f, 
										 fY + ( j + i + 2 ) * fCellSizeY / 2.0f - GetHeight( j + 1, i + 1, info ), 
										 1, 1, dwColor, dwSpecular, 0, 0 );
			++pVerts;
		}
	}
	// indices
	{
		const int nNumMainTiles = nMarkerSize;
		CIndicesLock<WORD> indices( pIndices );
		WORD *pInds = indices.GetBuffer();
		for ( int i=0, nIndex=0; i<nNumMainTiles; ++i, nIndex+=4 )
		{
			*pInds++ = nIndex + 0;
			*pInds++ = nIndex + 2;
			*pInds++ = nIndex + 1;
			*pInds++ = nIndex + 1;
			*pInds++ = nIndex + 2;
			*pInds++ = nIndex + 3;
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CreateGrid( const STerrainPatch &patch, const STerrainInfo &info, 
								 std::vector<SGFXLineVertex> &vertices, std::vector<WORD> &indices )
{
	const CArray2D<SMainTileInfo> &tiles = info.tiles;
	const CArray2D<SVertexAltitude> &heights = info.altitudes;
	const DWORD dwColor = 0x80ffffff;
	//
	vertices.resize( (STerrainPatchInfo::nSizeX + 1)*(STerrainPatchInfo::nSizeY + 1) );
	indices.resize( (STerrainPatchInfo::nSizeX + 1)*STerrainPatchInfo::nSizeY*2 + 
		              STerrainPatchInfo::nSizeX*(STerrainPatchInfo::nSizeY + 1)*2 );
	//
	const float fLineSizeX = STerrainPatchInfo::nSizeX * fWorldCellSize;
	const float fLineSizeY = STerrainPatchInfo::nSizeY * fWorldCellSize;
	const float fX = patch.nX * fLineSizeX;
	const float fY = ( info.patches.GetSizeY() - patch.nY ) * fLineSizeY;
	// vertices 
	SGFXLineVertex *pVertices = &( vertices[0] );
	for ( int i=0; i<STerrainPatchInfo::nSizeY + 1; ++i )
	{
		for ( int j=0; j<STerrainPatchInfo::nSizeX + 1; ++j )
		{
			pVertices->Setup( fX + j*fWorldCellSize, fY - i*fWorldCellSize, 
				                heights[patch.nY*STerrainPatchInfo::nSizeY + i][patch.nX*STerrainPatchInfo::nSizeX + j].fHeight, dwColor );
			++pVertices;
		}
	}
	// indices
	// horizontal lines
	WORD *pIndices = &( indices[0] );
	for ( int i = 0; i != STerrainPatchInfo::nSizeY + 1; ++i )
	{
		for ( int j = 0; j != STerrainPatchInfo::nSizeX; ++j )
		{
			*pIndices++ = i*(STerrainPatchInfo::nSizeY + 1) + j;
			*pIndices++ = i*(STerrainPatchInfo::nSizeY + 1) + j + 1;
		}
	}
	// vertical lines
	for ( int i = 0; i != STerrainPatchInfo::nSizeX + 1; ++i )
	{
		for ( int j = 0; j != STerrainPatchInfo::nSizeY; ++j )
		{
			*pIndices++ = j*(STerrainPatchInfo::nSizeX + 1) + i;
			*pIndices++ = (j + 1)*(STerrainPatchInfo::nSizeX + 1) + i;
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
