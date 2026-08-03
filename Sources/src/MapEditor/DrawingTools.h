#if !defined(__DrawingTools__)
#define __DrawingTools__

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "../GFX/GFX.h"
#include "../GFX/GFXHelper.h"
#include "../AILogic/AITypes.h"
#include "../Scene/Scene.h"
#include "../Scene/Terrain.h"
#include "../RandomMapGen/Polygons_Types.h"
#include "../RandomMapGen/LA_Types.h"

class CSceneDrawTool
{
	std::vector<SGFXLineVertex> vertices;
	std::vector<SAIPassabilityInfo> vAIMarkerTiles;
	std::vector<CTPoint<int> > vMarkerTiles;
public:
	bool bAIMarkerTilesValid;
	bool bMarkerTilesValid;

	CSceneDrawTool() : bAIMarkerTilesValid( false ), bMarkerTilesValid( false ) {}
	
	template<class Type>
	void DrawPolyLine( const Type &rPairs, DWORD dwColor, bool bClosed = false )
	{
		if ( rPairs.empty() )
		{
			return; 
		}
		Type::const_iterator beginIterator = rPairs.begin();
		Type::const_iterator endIterator = rPairs.begin();
		++endIterator;
		if ( endIterator == rPairs.end() )
		{
			return;
		}
		while ( beginIterator != rPairs.end() )
		{
			const CVec3 vBegin = GetPointType( *beginIterator, static_cast<CVec3*>( 0 ) );
			const CVec3 vEnd = GetPointType( *endIterator, static_cast<CVec3*>( 0 ) );
			
			SGFXLineVertex vertex;
			vertex.Setup( vBegin, dwColor );
			vertices.push_back( vertex );
			vertex.Setup( vEnd, dwColor );
			vertices.push_back( vertex );
			
			++beginIterator;
			++endIterator;
			if ( endIterator == rPairs.end() )
			{
				if ( bClosed )
				{
					endIterator = rPairs.begin();
				}
				else
				{
					break;
				}
			}
		}
	}

	template<class PointType>
	void DrawLine( const PointType &rBegin, const PointType &rEnd,  DWORD dwColor )
	{
		const CVec3 vBegin = GetPointType( rBegin, static_cast<CVec3*>( 0 ) );
		const CVec3 vEnd = GetPointType( rEnd, static_cast<CVec3*>( 0 ) );

		SGFXLineVertex vertex;
		vertex.Setup( vBegin, dwColor );
		vertices.push_back( vertex );
		vertex.Setup( vEnd, dwColor );
		vertices.push_back( vertex );
	}

	template<class PointType>
	void DrawCircle( const PointType &rCenter, float fRadius, int nParts, DWORD dwColor )
	{	
		if ( nParts < 2 )
		{
			return;
		}
		CVec3 vCenter = GetPointType( rCenter, static_cast<CVec3*>( 0 ) );
		for ( int nIndex = 0; nIndex < nParts; ++nIndex )
		{
			SGFXLineVertex vertex;
			float alpha = FP_2PI * nIndex / nParts;
			vertex.Setup( CVec3( fRadius * cos( alpha ) + vCenter.x,
													 fRadius * sin( alpha ) + vCenter.y,
													 vCenter.z ),
										dwColor );
			vertices.push_back( vertex );
			alpha = FP_2PI * ( nIndex + 1 ) / nParts;
			vertex.Setup( CVec3( fRadius * cos( alpha ) + vCenter.x,
													 fRadius * sin( alpha ) + vCenter.y,
													 vCenter.z ),
										dwColor );
			vertices.push_back( vertex );
		}
	}

	template<class Type>
	void DrawPolyLine3D( const Type &rPairs, DWORD dwColor, const STerrainInfo::TVertexAltitudeArray2D &rAltitude, bool bClosed = false )
	{
		if ( rPairs.empty() )
		{
			return; 
		}
		Type::const_iterator beginIterator = rPairs.begin();
		Type::const_iterator endIterator = rPairs.begin();
		++endIterator;
		if ( endIterator == rPairs.end() )
		{
			return;
		}
		while ( beginIterator != rPairs.end() )
		{
			CVec3 vBegin = GetPointType( *beginIterator, static_cast<CVec3*>( 0 ) );
			CVec3 vEnd = GetPointType( *endIterator, static_cast<CVec3*>( 0 ) );
			
			CVertexAltitudeInfo::GetHeight( rAltitude, vBegin.x, vBegin.y, &( vBegin.z ) );
			CVertexAltitudeInfo::GetHeight( rAltitude, vEnd.x, vEnd.y, &( vEnd.z ) );

			SGFXLineVertex vertex;
			vertex.Setup( vBegin, dwColor );
			vertices.push_back( vertex );
			vertex.Setup( vEnd, dwColor );
			vertices.push_back( vertex );
			
			++beginIterator;
			++endIterator;
			if ( endIterator == rPairs.end() )
			{
				if ( bClosed )
				{
					endIterator = rPairs.begin();
				}
				else
				{
					break;
				}
			}
		}
	}
	
	template<class PointType>
	void DrawLine3D( const PointType &rBegin, const PointType &rEnd,  DWORD dwColor, const STerrainInfo::TVertexAltitudeArray2D &rAltitude, int nPartsCount = 1 )
	{
		if ( nPartsCount < 1 )
		{
			nPartsCount = 1;
		}

		SGFXLineVertex vertex;
		const CVec3 vBegin = GetPointType( rBegin, static_cast<CVec3*>( 0 ) );
		const CVec3 vEnd = GetPointType( rEnd, static_cast<CVec3*>( 0 ) );
		const CVec3 vPart = ( vEnd - vBegin ) / nPartsCount;
		for ( int nPartIndex = 0; nPartIndex < nPartsCount; ++nPartIndex )
		{
			CVec3 vLocalBegin = vBegin + ( vPart * nPartIndex );
			CVec3 vLocalEnd = vBegin + ( vPart * ( nPartIndex + 1 ) );

			CVertexAltitudeInfo::GetHeight( rAltitude, vLocalBegin.x, vLocalBegin.y, &( vLocalBegin.z ) );
			CVertexAltitudeInfo::GetHeight( rAltitude, vLocalEnd.x, vLocalEnd.y, &( vLocalEnd.z ) );

			vertex.Setup( vLocalBegin, dwColor );
			vertices.push_back( vertex );
			vertex.Setup( vLocalEnd, dwColor );
			vertices.push_back( vertex );
		}
	}

	template<class PointType>
	void DrawCircle3D( const PointType &rCenter, float fRadius, int nParts, DWORD dwColor, const STerrainInfo::TVertexAltitudeArray2D &rAltitude, bool bOnlyCenter = true )
	{	
		if ( nParts < 2 )
		{
			return;
		}
		CVec3 vCenter = GetPointType( rCenter, static_cast<CVec3*>( 0 ) );
		CVertexAltitudeInfo::GetHeight( rAltitude, vCenter.x, vCenter.y, &( vCenter.z ) );

		for ( int nIndex = 0; nIndex < nParts; ++nIndex )
		{
			SGFXLineVertex vertex;
			float alpha = FP_2PI * nIndex / nParts;
			CVec3 point0( fRadius * cos( alpha ) + vCenter.x,
										fRadius * sin( alpha ) + vCenter.y,
										vCenter.z );
			if ( bOnlyCenter )
			{
				point0.z = vCenter.z;
			}
			else
			{
				CVertexAltitudeInfo::GetHeight( rAltitude, point0.x, point0.y, &( point0.z ) );
			}

			vertex.Setup( point0, dwColor );
			vertices.push_back( vertex );

			alpha = FP_2PI * ( nIndex + 1 ) / nParts;
			CVec3 point1( fRadius * cos( alpha ) + vCenter.x,
										fRadius * sin( alpha ) + vCenter.y,
										vCenter.z );
			if ( bOnlyCenter )
			{
				point1.z = vCenter.z;
			}
			else
			{
				CVertexAltitudeInfo::GetHeight( rAltitude, point1.x, point1.y, &( point1.z ) );
			}
			vertex.Setup( point1, dwColor );
			vertices.push_back( vertex );
		}
	}
	
	inline void DrawToScene( bool bClear = true )
	{
		if ( IScene *pScene = GetSingleton<IScene>() )
		{
			if ( ITerrain *pTerrain = pScene->GetTerrain() )
			{
				if( !vertices.empty() )
				{
					NI_ASSERT_T( ( vertices.size() & 0x1 ) == 0,
											 NStr::Format( "Wrong size: %d\n", vertices.size() ) );

					if ( !vertices.empty() )
					{
						pScene->AddMeshPair2( &( vertices[0] ),
																	vertices.size(),
																	sizeof( SGFXLineVertex ),
																	SGFXLineVertex::format,
																	0,
																	0,
																	GFXPT_LINELIST,
																	0,
																	3,
																	true );
					}
				}

				if(  bAIMarkerTilesValid )
				{
					if ( !vAIMarkerTiles.empty() )
					{
						pTerrain->SetAIMarker( &( vAIMarkerTiles[0] ), vAIMarkerTiles.size() );
					}

					else
					{
						pTerrain->SetAIMarker( 0, 0 );
					}
				}

				if ( bMarkerTilesValid )
				{
					if ( ITerrainEditor *pTerrainEditor = dynamic_cast<ITerrainEditor*>( pTerrain ) )
					{
						if ( !vMarkerTiles.empty() )
						{
							pTerrainEditor->SetMarker( &( vMarkerTiles[0] ), vMarkerTiles.size() );
						}
						else
						{
							pTerrainEditor->SetMarker( 0, 0 );
						}
					}
				}
				
				if ( bClear )
				{
					Clear();
				}
			}
		}
	}

	inline void AddAIMarkerTiles( const CTRect<int> &rRect, DWORD dwGreenPass, bool isBounds = false )
	{
		bAIMarkerTilesValid = true;
		
		CTRect<int> rect = rRect;
		rect.Normalize();
		if ( isBounds )
		{
			++( rect.maxx );
			++( rect.maxy );
		}

		for ( int nXIndex = rect.minx; nXIndex < rect.maxx; ++nXIndex )
		{
			for ( int nYIndex = rect.miny; nYIndex < rect.maxy; ++nYIndex )
			{
				for ( int nRectXIndex = 0; nRectXIndex < 2; ++nRectXIndex )
				{
					for ( int nRectYIndex = 0; nRectYIndex < 2; ++nRectYIndex )
					{
						SAIPassabilityInfo passabilityInfo;
						passabilityInfo.x = ( nXIndex * 2 ) + nRectXIndex;
						passabilityInfo.y = ( nYIndex * 2 ) + nRectYIndex;
						passabilityInfo.pass = dwGreenPass;
						vAIMarkerTiles.push_back( passabilityInfo );
					}
				}
			}
		}
	}
	
	inline void AddAIMarkerAITiles( const CTRect<int> &rRect, DWORD dwGreenPass, bool isBounds = false )
	{
		bAIMarkerTilesValid = true;

		CTRect<int> rect = rRect;
		rect.Normalize();
		if ( isBounds )
		{
			++( rect.maxx );
			++( rect.maxy );
		}

		for ( int nXIndex = rect.minx; nXIndex < rect.maxx; ++nXIndex )
		{
			for ( int nYIndex = rect.miny; nYIndex < rect.maxy; ++nYIndex )
			{
				SAIPassabilityInfo passabilityInfo;
				passabilityInfo.x = nXIndex;
				passabilityInfo.y = nYIndex;
				passabilityInfo.pass = dwGreenPass;
				vAIMarkerTiles.push_back( passabilityInfo );
			}
		}
	}


	inline void AddAIMarkerAITiles( const CArray2D<BYTE> &rArray, DWORD dwGreenPass, const CVec2 &vLT, const CVec2 &vRT, const CVec2 &vLB, const CVec2 &vRB )
	{
		bAIMarkerTilesValid = true;

		for ( int nYIndex = vLB.y; nYIndex <= vRT.y; ++nYIndex )
		{
			for ( int nXIndex = vLT.x; nXIndex <= vRB.x; ++nXIndex )
			{
				if ( IsValidIndices( rArray, nXIndex, nYIndex ) && ( rArray[nYIndex][nXIndex] > 0 ) )
				{
					SAIPassabilityInfo passabilityInfo;
					passabilityInfo.x = nXIndex;
					passabilityInfo.y = nYIndex;
					passabilityInfo.pass = dwGreenPass;
					vAIMarkerTiles.push_back( passabilityInfo );
				}
			}
		}
	}

	inline void AddMarkerTiles( const CTRect<int> &rRect, bool isBounds = false )
	{
		bMarkerTilesValid = true;
		
		CTRect<int> rect = rRect;
		rect.Normalize();
		if ( isBounds )
		{
			++( rect.maxx );
			++( rect.maxy );
		}

		for ( int nXIndex = rect.minx; nXIndex < rect.maxx; ++nXIndex )
		{
			for ( int nYIndex = rect.miny; nYIndex < rect.maxy; ++nYIndex )
			{
				vMarkerTiles.push_back( CTPoint<int>( nXIndex, nYIndex ) );
			}
		}			
	}

	inline void ClearAIMarker()
	{
		vAIMarkerTiles.clear();
	}
	
	inline void ClearMarker()
	{
		vMarkerTiles.clear();
	}

	inline void ClearVertices()
	{
		vertices.clear();
	}
	
	inline void Clear()
	{
		bAIMarkerTilesValid = false;
		bMarkerTilesValid = false;

		vertices.clear();
		vMarkerTiles.clear();
		vAIMarkerTiles.clear();
	}
};
#endif // !defined(__DrawingTools__)
