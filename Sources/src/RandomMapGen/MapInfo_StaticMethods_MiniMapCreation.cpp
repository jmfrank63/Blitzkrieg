#include "stdafx.h"

#include "MapInfo_Types.h"
#include "MiniMap_Types.h"
#include "VSO_Types.h"
#include "IB_Types.h"
#include "Resource_Types.h"
#include "Bresenham_Types.h"

#include "..\Formats\fmtTerrain.h"

#include "..\Image\Image.h"
#include "..\StreamIO\ProgressHook.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SSetColorFunctional
{
	CImageAccessor imageAccessor;
	SColor color;
	CTRect<int> imageRect;
	
	SSetColorFunctional( IImage *pImage, const SColor &rColor, const CTRect<int> &rImageRect ) : imageAccessor( pImage ), color( rColor ), imageRect( rImageRect ) {}
	
	inline void operator()( int nXIndex, int nYIndex )
	{ 
		if ( IsValidPointForNormalRect( imageRect, nXIndex, nYIndex ) )
		{
			imageAccessor[nYIndex][nXIndex] = color;
		}
	}
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct SSetColorWithoutCheckPointFunctional
{
	CImageAccessor imageAccessor;
	SColor color;
	CTRect<int> imageRect;
	
	SSetColorWithoutCheckPointFunctional( IImage *pImage, const SColor &rColor, const CTRect<int> &rImageRect ) : imageAccessor( pImage ), color( rColor ), imageRect( rImageRect ) {}
	
	inline void operator()( int nXIndex, int nYIndex )
	{ 
		imageAccessor[nYIndex][nXIndex] = color;
	}
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMapInfo::CreateMiniMapImage( const SLoadMapInfo &rLoadMapInfo, const CRMImageCreateParameterList &rImageCreateParameterList, IProgressHook *pProgressHook )
{
	CPtr<IImageProcessor> pImageProcessor = GetImageProcessor();
	CPtr<IDataStorage> pDataStorage = GetSingleton<IDataStorage>();
	CPtr<IObjectsDB> pODB = GetSingleton<IObjectsDB>();
	if ( ( !pImageProcessor ) || ( !pDataStorage ) || ( !pODB ) )
	{
		return false;
	}

	//PROGRESS_HOOK
	if ( pProgressHook )
	{
		pProgressHook->Step();
	}

	STilesetDesc tilesetDesc;
	LoadDataResource( rLoadMapInfo.terrain.szTilesetDesc, "", false, 0, "tileset", tilesetDesc );
	
	SRMMiniMapCreateParameter createParameter;
	const std::string szParameterPath = rLoadMapInfo.szSeasonFolder;
	LoadDataResource( szParameterPath + RMGC_MINIMAP_FILE_NAME, "", false, 0, RMGC_MINIMAP_XML_NAME, createParameter );

	//������� �������� � ��������� �� AI �����
	const CTPoint<int> imageSize( rLoadMapInfo.terrain.tiles.GetSizeX() * 2, rLoadMapInfo.terrain.tiles.GetSizeY() * 2 );
	//��� ��������� ��������� ��������
	const CTRect<int> imageRect( 0, 0, imageSize.x, imageSize.y );

	std::vector<CPtr<IImage> > layers;
	for ( int nLayerIndex = 0; nLayerIndex < SRMMiniMapCreateParameter::MML_COUNT; ++nLayerIndex )
	{
		layers.push_back( 0 );
		layers.back() = pImageProcessor->CreateImage( imageSize.x, imageSize.y );
		if ( layers.back() == 0 )
		{
			return false;
		}
		layers.back()->Set( 0 );
	}

	//PROGRESS_HOOK
	if ( pProgressHook )
	{
		pProgressHook->Step();
	}

	//terrain
	//������ ������ �� ��� Y
	{
		//�������� ImageAccessor ��� TileSet 
		CPtr<IDataStream> pTilesetStream = pDataStorage->OpenStream( ( rLoadMapInfo.terrain.szTilesetDesc  + GetDDSImageExtention( COMPRESSION_HIGH_QUALITY ) ).c_str(), STREAM_ACCESS_READ );
		if ( !pTilesetStream )
		{
			return false;
		}
		CPtr<IDDSImage> pTilesetImage = pImageProcessor->LoadDDSImage( pTilesetStream );
		if ( !pTilesetImage )
		{
			return false;
		}
		CTImageAccessor< SColor, IDDSImage, CPtr<IDDSImage> > tilesetImageAccessor = pTilesetImage;
		CImageAccessor layerImageAccessor = layers[SRMMiniMapCreateParameter::MML_TERRAIN];

		//���������� �������������� �����
		std::unordered_map<int, SColor> terrainColorsHashMap;
		for ( int nYIndex = 0; nYIndex < rLoadMapInfo.terrain.tiles.GetSizeY(); ++nYIndex )
		{
			for ( int nXIndex = 0; nXIndex < rLoadMapInfo.terrain.tiles.GetSizeX(); ++nXIndex )
			{
				SColor color;
				if ( terrainColorsHashMap.find( rLoadMapInfo.terrain.tiles[nYIndex][nXIndex].tile ) != terrainColorsHashMap.end() )
				{
					color = terrainColorsHashMap[rLoadMapInfo.terrain.tiles[nYIndex][nXIndex].tile];
				}
				else
				{
					//�������� ����
					const CVec2 *pVertices = tilesetDesc.tilemaps[ rLoadMapInfo.terrain.tiles[nYIndex][nXIndex].tile ].maps;
					CTRect<int> colorRect( ( pVertices[0].x * pTilesetImage->GetSizeX() + pVertices[2].x * pTilesetImage->GetSizeX() ) / 2,
																 ( pVertices[0].y * pTilesetImage->GetSizeY() + pVertices[2].y * pTilesetImage->GetSizeY() ) / 2,
																 ( pVertices[1].x * pTilesetImage->GetSizeX() + pVertices[3].x * pTilesetImage->GetSizeX() ) / 2,
																 ( pVertices[1].y * pTilesetImage->GetSizeY() + pVertices[3].y * pTilesetImage->GetSizeY() ) / 2 );
					colorRect.Normalize();

					DWORD dwRed = 0;
					DWORD dwGreen = 0;
					DWORD dwBlue = 0;
					for ( int nXTilePixelIndex = colorRect.left; nXTilePixelIndex < colorRect.right; ++nXTilePixelIndex )
					{
						for ( int nYTilePixelIndex = colorRect.top; nYTilePixelIndex < colorRect.bottom; ++nYTilePixelIndex )
						{
							const SColor &rColor = tilesetImageAccessor[nYTilePixelIndex][nXTilePixelIndex];
							dwRed += rColor.r;
							dwGreen += rColor.g;
							dwBlue += rColor.b;
						}
					}
					if ( ( colorRect.Width() * colorRect.Height() ) > 0 )
					{
						dwRed /= ( colorRect.Width() * colorRect.Height() );
						dwGreen /= ( colorRect.Width() * colorRect.Height() );
						dwBlue /= ( colorRect.Width() * colorRect.Height() );
					}

					//������������� ����
					SColor calculatedColor( 0xFF, dwRed & 0xFF, dwGreen & 0xFF, dwBlue & 0xFF );
					color = calculatedColor;
					terrainColorsHashMap[rLoadMapInfo.terrain.tiles[nYIndex][nXIndex].tile] = color;
				}
				
				for ( int nInnerYIndex = ( nYIndex * 2 ); nInnerYIndex < ( ( nYIndex + 1 ) * 2 ); ++nInnerYIndex )
				{
					for ( int nInnerXIndex = ( nXIndex * 2 ); nInnerXIndex < ( ( nXIndex + 1 ) * 2 ); ++nInnerXIndex )
					{
						layerImageAccessor[nInnerYIndex][nInnerXIndex] = color;
					}
				}
			}
		}
	}

	//PROGRESS_HOOK
	if ( pProgressHook )
	{
		pProgressHook->Step();
	}

	//Rivers
	//�������������� ������ �� ��� Y
	{
		CImageAccessor layerImageAccessor = layers[SRMMiniMapCreateParameter::MML_RIVERS];

		for ( int nRiverIndex = 0; nRiverIndex < rLoadMapInfo.terrain.rivers.size(); ++nRiverIndex )
		{
			StoreTilesFunctional storeCenterTiles;
			StoreTilesFunctional storeBorderTiles;

			std::string szDescName = rLoadMapInfo.terrain.rivers[nRiverIndex].szDescName;
			SVectorStripeObjectDesc desc = rLoadMapInfo.terrain.rivers[nRiverIndex];
			if ( !szDescName.empty() ) 
			{
				NStr::ToLower( szDescName );
				LoadDataResource( szDescName, "", false, 0, "VSODescription", desc );
			}
			
			for ( int nPointIndex = 0; nPointIndex < ( rLoadMapInfo.terrain.rivers[nRiverIndex].points.size() - 1 ); ++nPointIndex )
			{
				std::vector<CVec3> polygon;
				CVSOBuilder::GetVSOPointPolygon( rLoadMapInfo.terrain.rivers[nRiverIndex], nPointIndex, &polygon );
				ApplyTilesInPolygon<StoreTilesFunctional, std::vector<CVec3>, CVec3>( imageRect, polygon, fWorldCellSize / 2.0f, storeBorderTiles );
			}

			SColor miniMapCenterColor = desc.miniMapCenterColor;
			SColor miniMapBorderColor = desc.miniMapBorderColor;

			if ( miniMapCenterColor.a >= createParameter.dwMinAlpha )
			{
				for ( int nPointIndex = 0; nPointIndex < ( rLoadMapInfo.terrain.rivers[nRiverIndex].points.size() - 1 ); ++nPointIndex )
				{
					std::vector<CVec3> polygon;
					CVSOBuilder::GetVSOPointPolygon( rLoadMapInfo.terrain.rivers[nRiverIndex], nPointIndex, &polygon,  desc.bottom.fRelWidth );
					ApplyTilesInPolygon<StoreTilesFunctional, std::vector<CVec3>, CVec3>( imageRect, polygon, fWorldCellSize / 2.0f, storeCenterTiles );
				}
			}

			for ( int nTileIndex = 0; nTileIndex < storeBorderTiles.tiles.size(); ++nTileIndex )
			{
				if ( miniMapBorderColor.a >= createParameter.dwMinAlpha )
				{
					layerImageAccessor[imageSize.y - storeBorderTiles.tiles[nTileIndex].y - 1][storeBorderTiles.tiles[nTileIndex].x] = miniMapBorderColor;
				}
				else if ( miniMapCenterColor.a < createParameter.dwMinAlpha )
				{
					layerImageAccessor[imageSize.y - storeBorderTiles.tiles[nTileIndex].y - 1][storeBorderTiles.tiles[nTileIndex].x] = createParameter.layers[SRMMiniMapCreateParameter::MML_RIVERS].color;
				}
			}

			if ( miniMapCenterColor.a >= createParameter.dwMinAlpha )
			{
				for ( int nTileIndex = 0; nTileIndex < storeCenterTiles.tiles.size(); ++nTileIndex )
				{
					layerImageAccessor[imageSize.y - storeCenterTiles.tiles[nTileIndex].y - 1][storeCenterTiles.tiles[nTileIndex].x] = miniMapCenterColor;
				}
			}
		}
	}

	//PROGRESS_HOOK
	if ( pProgressHook )
	{
		pProgressHook->Step();
	}

	//Roads3D and RailRoads
	//�������������� ������ �� ��� Y
	{
		CImageAccessor railRoadLayerImageAccessor = layers[SRMMiniMapCreateParameter::MML_RAILROADS];
		CImageAccessor roadLayerImageAccessor = layers[SRMMiniMapCreateParameter::MML_ROADS3D];

		for ( int nRoad3DIndex = 0; nRoad3DIndex < rLoadMapInfo.terrain.roads3.size(); ++nRoad3DIndex )
		{
			StoreTilesFunctional storeCenterTiles;
			StoreTilesFunctional storeBorderTiles;

			std::string szDescName = rLoadMapInfo.terrain.roads3[nRoad3DIndex].szDescName;
			SVectorStripeObjectDesc desc = rLoadMapInfo.terrain.roads3[nRoad3DIndex];
			if ( !szDescName.empty() ) 
			{
				NStr::ToLower( szDescName );
				LoadDataResource( szDescName, "", false, 0, "VSODescription", desc );
			}
			
			for ( int nPointIndex = 0; nPointIndex < ( rLoadMapInfo.terrain.roads3[nRoad3DIndex].points.size() - 1 ); ++nPointIndex )
			{
				std::vector<CVec3> polygon;
				CVSOBuilder::GetVSOPointPolygon( rLoadMapInfo.terrain.roads3[nRoad3DIndex], nPointIndex, &polygon );
				ApplyTilesInPolygon<StoreTilesFunctional, std::vector<CVec3>, CVec3>( imageRect, polygon, fWorldCellSize / 2.0f, storeBorderTiles );
			}

			SColor miniMapCenterColor = desc.miniMapCenterColor;
			SColor miniMapBorderColor = desc.miniMapBorderColor;

			if ( miniMapCenterColor.a >= createParameter.dwMinAlpha )
			{
				for ( int nPointIndex = 0; nPointIndex < ( rLoadMapInfo.terrain.roads3[nRoad3DIndex].points.size() - 1 ); ++nPointIndex )
				{
					std::vector<CVec3> polygon;
					CVSOBuilder::GetVSOPointPolygon( rLoadMapInfo.terrain.roads3[nRoad3DIndex], nPointIndex, &polygon,  desc.bottom.fRelWidth );
					ApplyTilesInPolygon<StoreTilesFunctional, std::vector<CVec3>, CVec3>( imageRect, polygon, fWorldCellSize / 2.0f, storeCenterTiles );
				}
			}

			if ( desc.eType == SVectorStripeObjectDesc::TYPE_RAILROAD )
			{
				if ( miniMapBorderColor.a >= createParameter.dwMinAlpha )
				{
					for ( int nTileIndex = 0; nTileIndex < storeBorderTiles.tiles.size(); ++nTileIndex )
					{
						railRoadLayerImageAccessor[imageSize.y - storeBorderTiles.tiles[nTileIndex].y - 1][storeBorderTiles.tiles[nTileIndex].x] = miniMapBorderColor;
					}
				}
				else if ( miniMapCenterColor.a < createParameter.dwMinAlpha )
				{
					for ( int nTileIndex = 0; nTileIndex < storeBorderTiles.tiles.size(); ++nTileIndex )
					{
						railRoadLayerImageAccessor[imageSize.y - storeBorderTiles.tiles[nTileIndex].y - 1][storeBorderTiles.tiles[nTileIndex].x] = createParameter.layers[SRMMiniMapCreateParameter::MML_RAILROADS].color;
					}			
				}
				if ( miniMapCenterColor.a >= createParameter.dwMinAlpha )
				{
					for ( int nTileIndex = 0; nTileIndex < storeCenterTiles.tiles.size(); ++nTileIndex )
					{
						railRoadLayerImageAccessor[imageSize.y - storeCenterTiles.tiles[nTileIndex].y - 1][storeCenterTiles.tiles[nTileIndex].x] = miniMapCenterColor;
					}
				}
			}
			else
			{
				if ( miniMapBorderColor.a >= createParameter.dwMinAlpha )
				{
					for ( int nTileIndex = 0; nTileIndex < storeBorderTiles.tiles.size(); ++nTileIndex )
					{
						roadLayerImageAccessor[imageSize.y - storeBorderTiles.tiles[nTileIndex].y - 1][storeBorderTiles.tiles[nTileIndex].x] = miniMapBorderColor;
					}
				}
				else if ( miniMapCenterColor.a < createParameter.dwMinAlpha )
				{
					for ( int nTileIndex = 0; nTileIndex < storeBorderTiles.tiles.size(); ++nTileIndex )
					{
						roadLayerImageAccessor[imageSize.y - storeBorderTiles.tiles[nTileIndex].y - 1][storeBorderTiles.tiles[nTileIndex].x] = createParameter.layers[SRMMiniMapCreateParameter::MML_ROADS3D].color;
					}			
				}
				if ( miniMapCenterColor.a >= createParameter.dwMinAlpha )
				{
					for ( int nTileIndex = 0; nTileIndex < storeCenterTiles.tiles.size(); ++nTileIndex )
					{
						roadLayerImageAccessor[imageSize.y - storeCenterTiles.tiles[nTileIndex].y - 1][storeCenterTiles.tiles[nTileIndex].x] = miniMapCenterColor;
					}
				}
			}
		}
	}

	//PROGRESS_HOOK
	if ( pProgressHook )
	{
		pProgressHook->Step();
	}
	
	//Forests && Buildings && Bridges
	//�������������� ������ �� ��� Y
	std::unordered_map<int, SMapObjectInfo> nLinkID2BridgesHashMap;
	{
		SSetColorFunctional forestSetColorFunctional( layers[SRMMiniMapCreateParameter::MML_FORESTS], createParameter.layers[SRMMiniMapCreateParameter::MML_FORESTS].color, imageRect );
		SSetColorWithoutCheckPointFunctional buildingSetColorFunctional( layers[SRMMiniMapCreateParameter::MML_BUILDINGS], createParameter.layers[SRMMiniMapCreateParameter::MML_BUILDINGS].color, imageRect );
		SSetColorWithoutCheckPointFunctional bridgesSetColorFunctional( layers[SRMMiniMapCreateParameter::MML_BRIDGES], createParameter.layers[SRMMiniMapCreateParameter::MML_BRIDGES].color, imageRect );
		for ( int nObjectIndex = 0; nObjectIndex < rLoadMapInfo.objects.size(); ++nObjectIndex )
		{
			const SGDBObjectDesc *pGDBObjectDesc = pODB->GetDesc( rLoadMapInfo.objects[nObjectIndex].szName.c_str() );
			if ( pGDBObjectDesc )
			{
				if ( pGDBObjectDesc->eGameType == SGVOGT_BUILDING )
				{
					const SStaticObjectRPGStats* pStaticObjectRPGStats = NGDB::GetRPGStats<SStaticObjectRPGStats>( rLoadMapInfo.objects[nObjectIndex].szName.c_str() );
					const CVec2 &rOrigin = pStaticObjectRPGStats->GetOrigin( rLoadMapInfo.objects[nObjectIndex].nFrameIndex );
					const CArray2D<BYTE> &rPassability = pStaticObjectRPGStats->GetPassability( rLoadMapInfo.objects[nObjectIndex].nFrameIndex );
					CTPoint<int> start( ( rLoadMapInfo.objects[nObjectIndex].vPos.x - rOrigin.x + ( SAIConsts::TILE_SIZE / 2.0 ) ) / SAIConsts::TILE_SIZE,
															( rLoadMapInfo.objects[nObjectIndex].vPos.y - rOrigin.y + ( SAIConsts::TILE_SIZE / 2.0 ) ) / SAIConsts::TILE_SIZE );
					CTRect<int> indices( start.x, start.y, start.x + rPassability.GetSizeX(), start.y + rPassability.GetSizeY() );
					if ( ValidateIndices( imageRect, &indices ) >= 0 )
					{
						for ( int nXIndex = indices.minx; nXIndex < indices.maxx; ++nXIndex )
						{
							for ( int nYIndex = indices.miny; nYIndex < indices.maxy; ++nYIndex )
							{
								if ( createParameter.bAllBuildingPassability || ( rPassability[nYIndex - start.y][nXIndex - start.x] != RMGC_UNLOCKED ) )
								{
									buildingSetColorFunctional( nXIndex, imageSize.y - nYIndex - 1 );
								}
							}
						}
					}
				}
				else if ( pGDBObjectDesc->eGameType == SGVOGT_BRIDGE )
				{
					if ( rLoadMapInfo.objects[nObjectIndex].link.nLinkID != RMGC_INVALID_LINK_ID_VALUE )
					{
						nLinkID2BridgesHashMap[rLoadMapInfo.objects[nObjectIndex].link.nLinkID ] = rLoadMapInfo.objects[nObjectIndex];
					}
				}
				else
				{
					std::vector<std::string> splitNames;
					bool isWood = false;

					NStr::SplitString( pGDBObjectDesc->szPath, splitNames, '\\');
					for ( int splitNameIndex = 0; splitNameIndex < splitNames.size(); ++splitNameIndex )
					{
						std::string strName = splitNames[splitNameIndex];
						NStr::ToLower( strName );
						if ( strName.find( "flora" ) != std::string::npos )
						{
							isWood = true;
							break;
						}
					}
					if ( isWood )
					{
						int nXIndex = rLoadMapInfo.objects[nObjectIndex].vPos.x / SAIConsts::TILE_SIZE;
						int nYIndex = rLoadMapInfo.objects[nObjectIndex].vPos.y / SAIConsts::TILE_SIZE;
						BresenhamFilledCircle( nXIndex, imageRect.maxy - nYIndex - 1, createParameter.nWoodRadius, forestSetColorFunctional );
					}
				}
			}
		}

		for ( int nBridgeIndex = 0; nBridgeIndex < rLoadMapInfo.bridges.size(); ++nBridgeIndex )
		{
			CTRect<int> bridgeRect( -1, -1, -1, -1 );
			bool isVertical = true;
			int nWidth = 0;
			CTPoint<int> nLength( 0, 0 );
			for ( int nSpanIndex = 0; nSpanIndex < rLoadMapInfo.bridges[nBridgeIndex].size(); ++nSpanIndex )
			{ 
				if ( nLinkID2BridgesHashMap.find( rLoadMapInfo.bridges[nBridgeIndex][nSpanIndex] ) != nLinkID2BridgesHashMap.end() )
				{
					SMapObjectInfo spanInfo = nLinkID2BridgesHashMap[ rLoadMapInfo.bridges[nBridgeIndex][nSpanIndex] ];
					const SBridgeRPGStats* pBridgeRPGStats = NGDB::GetRPGStats<SBridgeRPGStats>( spanInfo.szName.c_str() );
					if ( pBridgeRPGStats )
					{
						isVertical = ( pBridgeRPGStats->direction == SBridgeRPGStats::VERTICAL );
						if( nWidth < static_cast<int>( pBridgeRPGStats->GetSpanStats( spanInfo.nFrameIndex ).fWidth ) )
						{
							nWidth = static_cast<int>( pBridgeRPGStats->GetSpanStats( spanInfo.nFrameIndex ).fWidth );
						}
						if ( pBridgeRPGStats->IsBeginIndex( spanInfo.nFrameIndex ) )
						{
							nLength.min = static_cast<int>( pBridgeRPGStats->GetSpanStats( spanInfo.nFrameIndex ).fLength );
							bridgeRect.minx = spanInfo.vPos.x / SAIConsts::TILE_SIZE;
							bridgeRect.miny = spanInfo.vPos.y / SAIConsts::TILE_SIZE;
						}
						else if ( pBridgeRPGStats->IsEndIndex( spanInfo.nFrameIndex ) )
						{
							nLength.max = static_cast<int>( pBridgeRPGStats->GetSpanStats( spanInfo.nFrameIndex ).fLength );
							bridgeRect.maxx = spanInfo.vPos.x / SAIConsts::TILE_SIZE;
							bridgeRect.maxy = spanInfo.vPos.y / SAIConsts::TILE_SIZE;
						}
					}
				}
			}
			if (  createParameter.dwBridgeWidth > 0 )
			{
				nWidth = createParameter.dwBridgeWidth;
			}
			if ( ( bridgeRect.minx >= 0 ) && ( bridgeRect.maxx >= 0 ) )
			{
				bridgeRect.Normalize();

				if ( isVertical )
				{
					bridgeRect.minx -= nWidth / 2;
					bridgeRect.maxx += nWidth / 2;
					bridgeRect.miny -= nLength.min;
					bridgeRect.maxy += nLength.max;
				}
				else
				{
					bridgeRect.minx -= nLength.min;
					bridgeRect.maxx += nLength.max;
					bridgeRect.miny -= nWidth / 2;
					bridgeRect.maxy += nWidth / 2;
				}
				CTRect<int> actualBridgeRect( bridgeRect.minx, bridgeRect.miny, bridgeRect.maxx + 1, bridgeRect.maxy + 1 );
				if ( ValidateIndices( imageRect, &actualBridgeRect ) >= 0 )
				{
					for ( int nYIndex = actualBridgeRect.miny; nYIndex < actualBridgeRect.maxy; ++nYIndex )
					{
						for ( int nXIndex = actualBridgeRect.minx; nXIndex < actualBridgeRect.maxx; ++nXIndex )
						{
							bridgesSetColorFunctional( nXIndex, imageSize.y - nYIndex - 1 );
						}
					}
				}
			}
		}
	}
	
	//PROGRESS_HOOK
	if ( pProgressHook )
	{
		pProgressHook->Step();
	}

	//���������� �����
	for ( int nLayerIndex = 0; nLayerIndex < SRMMiniMapCreateParameter::MML_COUNT; ++nLayerIndex )
	{
		if ( createParameter.layers[nLayerIndex].borderColor.a >= createParameter.dwMinAlpha )
		{
			std::vector<CPtr<IImage> > imagesToCompress;
			imagesToCompress.push_back( 0 );
			imagesToCompress.back() = CRMImageBuilder::GetEdge( layers[nLayerIndex], createParameter.layers[nLayerIndex].borderColor, 0,createParameter.dwMinAlpha );
			if ( imagesToCompress.back() == 0 )
			{
				return false;
			}
			imagesToCompress.push_back( 0 );
			imagesToCompress.back() = layers[nLayerIndex];
			layers[nLayerIndex] = CRMImageBuilder::FastComposeImagesByAlpha( imagesToCompress, createParameter.dwMinAlpha );
			if ( layers[nLayerIndex] == 0 )
			{
				return false;
			}
		}
	}

	//PROGRESS_HOOK
	if ( pProgressHook )
	{
		pProgressHook->Step();
	}

	//������������������ �����
	std::vector<CPtr<IImage> > shadows;
	{
		for ( int nLayerIndex = 0; nLayerIndex < SRMMiniMapCreateParameter::MML_COUNT; ++nLayerIndex )
		{
			shadows.push_back( 0 );
			if ( ( createParameter.layers[nLayerIndex].shadowPoint.x != 0 ) ||
					 ( createParameter.layers[nLayerIndex].shadowPoint.y != 0 ) )
			{
				shadows.back() = CRMImageBuilder::GetShadow( layers[nLayerIndex],
																										 CTPoint<int>( createParameter.layers[nLayerIndex].shadowPoint.x,
																																	 createParameter.layers[nLayerIndex].shadowPoint.y * ( -1 ) ),
																										 CRMImageBuilder::GRAY_LIGHTER_COLOR,
																										 CRMImageBuilder::WHITE_COLOR,
																										 createParameter.dwMinAlpha );
				if ( shadows.back() == 0 )
				{
					return false;
				}
			}
		}
	}

	//PROGRESS_HOOK
	if ( pProgressHook )
	{
		pProgressHook->Step();
	}

	//������������������ ������������
	std::vector<CPtr<IImage> > aphaEmbosses;
	{
		for ( int nLayerIndex = 0; nLayerIndex < SRMMiniMapCreateParameter::MML_COUNT; ++nLayerIndex )
		{
			aphaEmbosses.push_back( 0 );
			if ( ( createParameter.layers[nLayerIndex].embossPoint.x != 0 ) ||
					 ( createParameter.layers[nLayerIndex].embossPoint.y != 0 ) )
			{
				aphaEmbosses.back() = CRMImageBuilder::GetAlphaEmboss( layers[nLayerIndex],
																															 CTPoint<int>( createParameter.layers[nLayerIndex].embossPoint.x,
																																						 createParameter.layers[nLayerIndex].embossPoint.y * ( -1 ) ),
																															 createParameter.layers[nLayerIndex].embossFilterSize,
																															 createParameter.dwMinAlpha );
				if ( aphaEmbosses.back() == 0 )
				{
					return false;
				}
			}
		}
	}

	//PROGRESS_HOOK
	if ( pProgressHook )
	{
		pProgressHook->Step();
	}

	//��������� ���� �� 3d Terrain
	if ( createParameter.bTerrainShades )
	{
		CPtr<IImage> pTerrainShades = CVertexAltitudeInfo::GetShadesImage( rLoadMapInfo.terrain.altitudes, createParameter.fTerrainShadeRatio, true );
		if ( pTerrainShades )
		{
			CPtr<IImage> pCompressedTerrainShades = 0;
			if ( ( pTerrainShades->GetSizeX() != layers[SRMMiniMapCreateParameter::MML_TERRAIN]->GetSizeX() ) ||
					 ( pTerrainShades->GetSizeY() != layers[SRMMiniMapCreateParameter::MML_TERRAIN]->GetSizeY() ) )
			{
				pCompressedTerrainShades = pImageProcessor->CreateScaleBySize( pTerrainShades,
																																			 layers[SRMMiniMapCreateParameter::MML_TERRAIN]->GetSizeX(),
																																			 layers[SRMMiniMapCreateParameter::MML_TERRAIN]->GetSizeY(),
																																			 createParameter.layers[SRMMiniMapCreateParameter::MML_TERRAIN].scaleMethod );
				if ( pCompressedTerrainShades == 0 )
				{
					return false;
				}
			}
			else
			{
				pCompressedTerrainShades = pTerrainShades;
			}
			if ( !CRMImageBuilder::Noise( layers[SRMMiniMapCreateParameter::MML_TERRAIN], pCompressedTerrainShades, createParameter.dwMinAlpha ) )
			{
				return false;
			}
		}
	}
	
	//��������� ������ ��������� �� ������� ��������
	for ( CRMImageCreateParameterList::const_iterator imageCreateParameterIterator = rImageCreateParameterList.begin(); imageCreateParameterIterator != rImageCreateParameterList.end(); ++imageCreateParameterIterator )
	{
		//������������������ �����
		std::vector<CPtr<IImage> > noises;
		{
			for ( int nLayerIndex = 0; nLayerIndex < SRMMiniMapCreateParameter::MML_COUNT; ++nLayerIndex )
			{
				noises.push_back( layers[nLayerIndex]->Duplicate() );
				if ( !createParameter.layers[nLayerIndex].noiseImage.empty() ) 
				{
					CPtr<IDataStream> pNoiseStream = pDataStorage->OpenStream( ( szParameterPath + createParameter.layers[nLayerIndex].noiseImage + ".tga" ).c_str(), STREAM_ACCESS_READ );
					if ( !pNoiseStream )
					{
						continue;
					}
					CPtr<IImage> pNoiseImage = pImageProcessor->LoadImage( pNoiseStream );
					CPtr<IImage> pCompressedNoiseImage;
					if ( createParameter.layers[nLayerIndex].bScaleNoise )
					{
						if ( ( layers[nLayerIndex]->GetSizeX() != imageCreateParameterIterator->size.x ) || ( layers[nLayerIndex]->GetSizeY() != imageCreateParameterIterator->size.y ) )
						{
							pCompressedNoiseImage = pImageProcessor->CreateScaleBySize( pNoiseImage,
																																					( pNoiseImage->GetSizeX() * layers[nLayerIndex]->GetSizeX() ) / imageCreateParameterIterator->size.x,
																																					( pNoiseImage->GetSizeY() * layers[nLayerIndex]->GetSizeY() ) / imageCreateParameterIterator->size.y,
																																						createParameter.layers[nLayerIndex].scaleMethod );
							if ( pCompressedNoiseImage == 0 )
							{
								return false;
							}
						}
						else
						{
							pCompressedNoiseImage = pNoiseImage;
						}
					}
					else
					{
						pCompressedNoiseImage = pNoiseImage;
					}
					
					if( !CRMImageBuilder::Noise( noises.back(), pCompressedNoiseImage, createParameter.dwMinAlpha ) )
					{
						return false;
					}
				}
			}
		}
		
		//���������� ����� ��������
		//��� ������� ����:
		//��������� ����
		//������ Emboss ( ���� ���� )
		//������ ���� ( ���� ���� )
		CPtr<IImage> pMiniMapImage = 0;
		for ( int nLayerIndex = ( SRMMiniMapCreateParameter::MML_COUNT - 1 ); nLayerIndex >= 0; --nLayerIndex )
		{
			if ( nLayerIndex == ( SRMMiniMapCreateParameter::MML_COUNT - 1 ) )
			{
				pMiniMapImage = noises[nLayerIndex];
			}
			else
			{
				if ( !CRMImageBuilder::FastAddImageByAlpha( pMiniMapImage, noises[nLayerIndex], createParameter.dwMinAlpha ) )
				{
					return false;
				}
			}
			if ( aphaEmbosses[nLayerIndex] != 0 )
			{
				if ( !CRMImageBuilder::Noise( pMiniMapImage, aphaEmbosses[nLayerIndex], createParameter.dwMinAlpha ) )
				{
					return false;
				}
			}
			if ( shadows[nLayerIndex] != 0 )
			{
				if ( !CRMImageBuilder::Noise( pMiniMapImage, shadows[nLayerIndex], createParameter.dwMinAlpha ) )
				{
					return false;
				}
			}
		}

		//���� �������� �� ������� �������
		CPtr<IImage> pCompressedMiniMapImage = 0;
		if ( ( pMiniMapImage->GetSizeX() != imageCreateParameterIterator->size.x ) || ( pMiniMapImage->GetSizeY() != imageCreateParameterIterator->size.y ) )
		{
			pCompressedMiniMapImage = pImageProcessor->CreateScaleBySize( pMiniMapImage, imageCreateParameterIterator->size.x, imageCreateParameterIterator->size.y, createParameter.layers[SRMMiniMapCreateParameter::MML_TERRAIN].scaleMethod );
			if ( pCompressedMiniMapImage == 0 )
			{
				return false;
			}
		}
		else
		{
			pCompressedMiniMapImage = pMiniMapImage;
		}
		
		//���� ���� ������������ �����:
		CPtr<IImage> pColorCorrectedCompressedMiniMapImage = 0;
		if ( imageCreateParameterIterator->bColorCorrection )
		{
			pColorCorrectedCompressedMiniMapImage = pImageProcessor->CreateGammaCorrection( pCompressedMiniMapImage, imageCreateParameterIterator->fBrightness, imageCreateParameterIterator->fContrast, imageCreateParameterIterator->fGamma );
			if ( pColorCorrectedCompressedMiniMapImage == 0 )
			{
				return false;
			}
		}
		else
		{
			pColorCorrectedCompressedMiniMapImage = pCompressedMiniMapImage;
		}

		if ( imageCreateParameterIterator->bDDS )
		{
			if ( !SaveImageToDDSImageResource( pColorCorrectedCompressedMiniMapImage, imageCreateParameterIterator->szImageFileName ) )
			{
				return false;
			}
		}
		else
		{
			if ( !SaveImageToTGAImageResource( pColorCorrectedCompressedMiniMapImage, imageCreateParameterIterator->szImageFileName ) )
			{
				return false;
			}
		}
	}

	//PROGRESS_HOOK
	if ( pProgressHook )
	{
		pProgressHook->Step();
	}
	
	return true;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

