#include "stdafx.h"

#include "frames.h"
#include "mainFrm.h"
#include "TemplateEditorFrame1.h"
#include "..\AILogic\AITypes.h"
#include "..\Formats\fmtMap.h"
#include "..\GFX\GFX.h"
#include "..\Scene\Terrain.h"
#include "..\RandomMapGen\IB_Types.h"
#include "..\RandomMapGen\Resource_Types.h"
#include "..\Image\Image.h"
#include "..\Scene\Scene.h"
#include "..\RandomMapGen\VA_Types.h"
#include "MiniMapTypes.h"
#include "MapEditorBarWnd.h"


#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CScreenFrame::Update( CDC *pDC )
{
	NI_ASSERT_SLOW_T( pDC != 0,
										NStr::Format( "Wrong parameter <pDC>: %x", pDC ) );

	IScene *pScene = GetSingleton<IScene>();
	IGFX *pGFX = GetSingleton<IGFX>();
	CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame();
	if( pScene && pGFX && pFrame )
	{
		if ( ITerrain *pTerrain = pScene->GetTerrain() )
		{
			CTRect<float> screenRect( 0, 0, GAME_SIZE_X, GAME_SIZE_Y );

			CRect clientRect;
			pFrame->GetClientRect( &clientRect );

			if ( clientRect.Width() < GAME_SIZE_X )
			{
				screenRect.right = clientRect.right;
			}
			if ( clientRect.Height() < GAME_SIZE_Y )
			{
				screenRect.bottom = clientRect.bottom;
			}
			
			CVec3 v0;	//left top
			CVec3 v1;	//left bottom
			CVec3 v2;	//right bottom
			CVec3 v3;	//right top

			pScene->GetPos3( &v0, CVec2( screenRect.left, screenRect.top ), true );
			pScene->GetPos3( &v1, CVec2( screenRect.left, screenRect.bottom ), true );
			pScene->GetPos3( &v2, CVec2( screenRect.right, screenRect.bottom ), true );
			pScene->GetPos3( &v3, CVec2( screenRect.right, screenRect.top ), true );

			point0.x = v0.x;
			point0.y = v0.y;
			point1.x = v1.x;
			point1.y = v1.y;
			point2.x = v2.x;
			point2.y = v2.y;
			point3.x = v3.x;
			point3.y = v3.y;
			return;
		}
	}
	point0.x = 0;
	point0.y = 0;
	point1.x = 0;
	point1.y = 0;
	point2.x = 0;
	point2.y = 0;
	point3.x = 0;
	point3.y = 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFireRangeAreas::Update( CDC *pDC )
{
	NI_ASSERT_SLOW_T( pDC != 0,
										NStr::Format( "Wrong parameter <pDC>: %x", pDC ) );

	if( IScene *pScene = GetSingleton<IScene>() )
	{
		SShootAreas* pShootAreas = 0;
		int nNumAreas = 0;
		pScene->GetAreas( &pShootAreas, &nNumAreas );

		areas.clear();
		if ( nNumAreas > 0 )
			areas.assign( pShootAreas, pShootAreas + nNumAreas );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
COLORREF GetRGBColorFromARGB( DWORD dwColor )
{
	return RGB( 0xFF & ( ( dwColor & 0xFF0000 ) >> 16 ), 0xFF & ( ( dwColor & 0xFF00 ) >> 8 ), dwColor & 0xFF );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFireRangeAreas::DrawShootArea( IMiniMapDrawTool* pTool, const SShootArea &area )
{
	if ( area.eType != SShootArea::ESAT_LINE )
	{
		pTool->SetPen( circlePenStyle, circlePenWidth, GetRGBColorFromARGB( area.GetColor() ) );
		pTool->Circle( CTPoint<int>( area.vCenter3D.x, area.vCenter3D.y ), area.fMaxR );
		pTool->DestroyPen();
		
		if ( area.wStartAngle != area.wFinishAngle )
		{
			pTool->SetPen( sectorPenStyle, sectorPenWidth, GetRGBColorFromARGB( area.GetColor() ) );
			pTool->MoveTo( CTPoint<int>( area.vCenter3D.x, area.vCenter3D.y ) );
			{
				const float fAngle = fmod( float( area.wStartAngle ) / 65535.0f * FP_2PI + FP_PI2, FP_2PI );
				const float fCos = cos( fAngle );
				const float fSin = sin( fAngle );

				const int x = area.vCenter3D.x + area.fMaxR * fCos;
				const int y = area.vCenter3D.y + area.fMaxR * fSin;

				pTool->LineTo( CTPoint<int>( x, y ) );
			}
			pTool->MoveTo( CTPoint<int>( area.vCenter3D.x, area.vCenter3D.y ) );
			{
				const float fAngle = fmod( float( area.wFinishAngle ) / 65535.0f * FP_2PI + FP_PI2, FP_2PI );
				const float fCos = cos( fAngle );
				const float fSin = sin( fAngle );

				const int x = area.vCenter3D.x + area.fMaxR * fCos;
				const int y = area.vCenter3D.y + area.fMaxR * fSin;

				pTool->LineTo( CTPoint<int>( x, y ) );
			}
			pTool->DestroyPen();
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CFireRangeAreas::Draw( IMiniMapDrawTool* pTool )
{
	NI_ASSERT_SLOW_T( pTool != 0, NStr::Format( "Wrong parameter <pTool>: %x", pTool ) );

	for ( int index = 0; index < areas.size(); ++index )
	{
		for ( std::list<SShootArea>::iterator iter = areas[index].areas.begin(); iter != areas[index].areas.end(); ++iter )
			DrawShootArea( pTool, *iter );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const BYTE MxHC = 0x80;
const int MINIMAP_PLAYER_COLORS_COUNT = 17;
const COLORREF MINIMAP_PLAYER_COLORS[MINIMAP_PLAYER_COLORS_COUNT] = { RGB( 0x00, 0xFF, 0x00 ),	//0
																																			RGB( 0xFF, 0x00, 0x00 ),	//1
																																			RGB( 0x00, 0x00, 0xFF ),	//2
																																			RGB( 0xFF, 0xFF, 0x00 ),	//3
																																			RGB( 0x00, 0xFF, 0xFF ),	//4
																																			RGB( 0xFF, 0x00, 0xFF ),	//5
																																			RGB( 0xFF, 0xFF, 0xFF ),	//6
																																			RGB( 0xFF, MxHC, 0x00 ),	//7
																																			RGB( 0xFF, 0x00, MxHC ),	//8
																																			RGB( MxHC, 0xFF, 0x00 ),	//9
																																			RGB( 0x00, 0xFF, MxHC ),	//10
																																			RGB( MxHC, 0x00, 0xFF ),	//11
																																			RGB( 0x00, MxHC, 0xFF ),	//12
																																			RGB( 0x00, MxHC, MxHC ),	//13
																																			RGB( MxHC, 0x00, MxHC ),	//14
																																			RGB( MxHC, MxHC, 0x00 ),	//15
																																			RGB( MxHC, MxHC, MxHC ) }; //16

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CUnitsSelection::Update( CDC *pDC )
{
	NI_ASSERT_SLOW_T( pDC != 0,
										NStr::Format( "Wrong parameter <pDC>: %x", pDC ) );
	
	units.clear();
	if ( CPtr<IObjectsDB> pODB = GetSingleton<IObjectsDB>() )
	{
		if( IScene *pScene = GetSingleton<IScene>() )
		{
			if ( ITerrain *pTerrain = pScene->GetTerrain() )
			{
				ITerrainEditor *pTerrainEditor = dynamic_cast<ITerrainEditor*>( pTerrain );
				STerrainInfo &rTerrainInfo = const_cast<STerrainInfo&>( pTerrainEditor->GetTerrainInfo() );
				CTRect<int> aiRect( 0, 0, rTerrainInfo.tiles.GetSizeX() * 2, rTerrainInfo.tiles.GetSizeY() * 2 );
				
				if ( CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame() )
				{
					if ( CTabSimpleObjectsDialog* pObjectWnd = pFrame->m_mapEditorBarPtr->GetObjectWnd() )
					{
						if ( IAIEditor* pAIEditor = GetSingleton<IAIEditor>() )
						{
							IObjectsDB *pGDB = GetSingleton<IObjectsDB>();
							std::set<IRefCount*> squads;
							for ( std::unordered_map< SMapObject*, SEditorObjectItem*, SDefaultPtrHash >::iterator objectsIterator = pFrame->m_objectsAI.begin(); objectsIterator != pFrame->m_objectsAI.end(); ++objectsIterator )
							{
								IRefCount *pSquad = pAIEditor->GetFormationOfUnit( objectsIterator->first->pAIObj );
								if ( pSquad )
								{
									if ( squads.find( pSquad ) == squads.end() )
									{
										squads.insert( pSquad );
									}
								}
								else
								{
									if ( pObjectWnd->FilterName( objectsIterator->first->pDesc->szPath ) )
									{
										units.push_back();
										SUnitInfo &unitInfo = units.back();
										if ( ( objectsIterator->second->nPlayer >= 0 ) && ( objectsIterator->second->nPlayer < MINIMAP_PLAYER_COLORS_COUNT ) )
										{
											unitInfo.color = MINIMAP_PLAYER_COLORS[objectsIterator->second->nPlayer];
										}
										else
										{
											unitInfo.color = MINIMAP_PLAYER_COLORS[MINIMAP_PLAYER_COLORS_COUNT - 1];
										}
								
										CVec3 vPos = objectsIterator->first->pVisObj->GetPosition();
										Vis2AI( &vPos );

										//unitInfo.position;
										CTPoint<int> center( 0, 0 );
										
										if ( IsObjectHasPassability( objectsIterator->first->pDesc->eGameType ) )
										{
											const SStaticObjectRPGStats* pStaticObjectRPGStats = static_cast<const SStaticObjectRPGStats*>( pGDB->GetRPGStats( objectsIterator->first->pDesc ) );
											const CVec2 &rOrigin = pStaticObjectRPGStats->GetOrigin( objectsIterator->second->frameIndex );
											const CArray2D<BYTE> &rPassability = pStaticObjectRPGStats->GetPassability( objectsIterator->second->frameIndex );
											
											const CTPoint<int> start( ( vPos.x - rOrigin.x + ( SAIConsts::TILE_SIZE / 2.0 ) ) / SAIConsts::TILE_SIZE,
																								( vPos.y - rOrigin.y + ( SAIConsts::TILE_SIZE / 2.0 ) ) / SAIConsts::TILE_SIZE );
											unitInfo.position.minx = start.x;
											unitInfo.position.miny = start.y;
											unitInfo.position.maxx = start.x + rPassability.GetSizeX();
											unitInfo.position.maxy = start.y + rPassability.GetSizeY();
											
											center.x = unitInfo.position.minx + unitInfo.position.Width() / 2;
											center.y = unitInfo.position.miny + unitInfo.position.Height() / 2;
										}
										else
										{
											center.x = vPos.x / SAIConsts::TILE_SIZE;
											center.y = vPos.y / SAIConsts::TILE_SIZE;
											
											unitInfo.position.minx = center.x;
											unitInfo.position.miny = center.y;
											unitInfo.position.maxx = unitInfo.position.minx;
											unitInfo.position.maxy = unitInfo.position.miny;
										}
										
										if ( unitInfo.position.Width() < 5 )
										{
											unitInfo.position.minx =  center.x - 2;
											unitInfo.position.maxx =  center.x + 2;
										}
										if ( unitInfo.position.Height() < 5 )
										{
											unitInfo.position.miny =  center.y - 2;
											unitInfo.position.maxy =  center.y + 2;
										}
										ValidateIndices( aiRect, &( unitInfo.position ) );

										unitInfo.position.maxx += 1;
										unitInfo.position.maxy += 1;
									}
								}			
							}

							if ( pObjectWnd->FilterName( "squads\\german_hmg" ) )
							{
								for( std::set< IRefCount* >::iterator squadIterator = squads.begin(); squadIterator != squads.end(); ++squadIterator )
								{
									IRefCount **pUnits;
									int nLength;
									pAIEditor->GetUnitsInFormation( ( *squadIterator ), &pUnits, &nLength );
									nLength = 1;
									for ( int nUnitNumber = 0; nUnitNumber < nLength; ++nUnitNumber )
									{
										SMapObject *pMapObject = pFrame->FindByAI( pUnits[nUnitNumber] );
										if ( pMapObject )
										{
											SEditorObjectItem *tmpEditiorObj =	pFrame->m_objectsAI[ pMapObject ];
											units.push_back();
											SUnitInfo &unitInfo = units.back();
											if ( ( tmpEditiorObj->nPlayer >= 0 ) && ( tmpEditiorObj->nPlayer < MINIMAP_PLAYER_COLORS_COUNT ) )
											{
												unitInfo.color = MINIMAP_PLAYER_COLORS[tmpEditiorObj->nPlayer];
											}
											else
											{
												unitInfo.color = MINIMAP_PLAYER_COLORS[MINIMAP_PLAYER_COLORS_COUNT - 1];
											}
											
											CVec3 vPos = pMapObject->pVisObj->GetPosition();
											Vis2AI( &vPos );
											const CTPoint<int> center( vPos.x / SAIConsts::TILE_SIZE, vPos.y / SAIConsts::TILE_SIZE );

											unitInfo.position.minx = center.x - 2;
											unitInfo.position.miny = center.y - 2;
											unitInfo.position.maxx = center.x + 2;
											unitInfo.position.maxy = center.y + 2;
											ValidateIndices( aiRect, &( unitInfo.position ) );
											
											unitInfo.position.maxx += 1;
											unitInfo.position.maxy += 1;
										}
									}
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
void CMiniMapTerrain::UpdateColor()
{
	if( IScene *pScene = GetSingleton<IScene>() )
	{
		if (ITerrain *pTerrain = pScene->GetTerrain() )
		{
			if ( CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame() )
			{
				if ( bGame ||
						 ( ( pFrame->inputStates.GetActiveState() == CTemplateEditorFrame::STATE_TERRAIN ) &&
							 ( pFrame->m_mapEditorBarPtr->GetTerrainState() == CTemplateEditorFrame::STATE_TERRAIN_ALTITUDES ) ) )
				{
					return;
				}
				
				const STerrainInfo &rTerrainInfo = dynamic_cast<ITerrainEditor*>( pTerrain )->GetTerrainInfo();
				const STilesetDesc &rTilesetDesc = dynamic_cast<ITerrainEditor*>( pTerrain )->GetTilesetDesc();
				
				//��������� �������� � ������:
				//rTerrainInfo.szTilesetDesc;
				IImageProcessor *pImageProcessor = GetImageProcessor();
				IDataStorage *pStorage = GetSingleton<IDataStorage>();
				
				CPtr<IDataStream> pStream = pStorage->OpenStream( (rTerrainInfo.szTilesetDesc + "_h.dds").c_str(), STREAM_ACCESS_READ );
				NI_ASSERT( pStream != 0 );
				CTImageAccessor< SColor, IDDSImage, CPtr<IDDSImage> > imageAccessor = pImageProcessor->LoadDDSImage( pStream );
				NI_ASSERT_T( imageAccessor != 0, NStr::Format("Can't load DDS file from stream \"%s\"", (rTerrainInfo.szTilesetDesc + "_h.dds").c_str()) );

				for ( int index = 0; index < rTilesetDesc.terrtypes.size(); ++index )
				{
					const CVec2 *pVertices = rTilesetDesc.tilemaps[ rTilesetDesc.terrtypes[index].tiles[0].nIndex ].maps;
					CTRect<int> colorRect( ( pVertices[0].x * imageAccessor->GetSizeX() + pVertices[2].x * imageAccessor->GetSizeX() ) / 2,
																 ( pVertices[0].y * imageAccessor->GetSizeY() + pVertices[2].y * imageAccessor->GetSizeY() ) / 2,
																 ( pVertices[1].x * imageAccessor->GetSizeX() + pVertices[3].x * imageAccessor->GetSizeX() ) / 2,
																 ( pVertices[1].y * imageAccessor->GetSizeY() + pVertices[3].y * imageAccessor->GetSizeY() ) / 2 );
					colorRect.Normalize();

					DWORD red = 0;
					DWORD green = 0;
					DWORD blue = 0;
					for ( int nYIndex = colorRect.top; nYIndex < colorRect.bottom; ++nYIndex )
					{
						for ( int nXIndex = colorRect.left; nXIndex < colorRect.right; ++nXIndex )
						{
							const SColor &rColor = imageAccessor[nYIndex][nXIndex];
							red += rColor.r;
							green += rColor.g;
							blue += rColor.b;
						}
					}
					if ( ( colorRect.Width() * colorRect.Height() ) > 0 )
					{
						red /= ( colorRect.Width() * colorRect.Height() );
						green /= ( colorRect.Width() * colorRect.Height() );
						blue /= ( colorRect.Width() * colorRect.Height() );
					}
					colors[index] = RGB( red & 0xFF, green & 0xFF, blue & 0xFF );
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMiniMapTerrain::Update( CDC *pDC )
{
	NI_ASSERT_SLOW_T( pDC != 0,
										NStr::Format( "Wrong parameter <pDC>: %x", pDC ) );
	Clear();

	if( IScene *pScene = GetSingleton<IScene>() )
	{
		if (ITerrain *pTerrain = pScene->GetTerrain() )
		{
			if ( CTemplateEditorFrame *pFrame = g_frameManager.GetTemplateEditorFrame() )
			{
				int nBorderGreenColor = 0x40;
				if ( bGame )
				{
					std::string szMiniMapImage = pFrame->m_currentMapName;
					IImage *pImage = LoadImageFromTGAImageResource( szMiniMapImage );
					if ( !pImage )
					{
						pImage = LoadImageFromDDSImageResource( szMiniMapImage );
					}
					if ( pImage )
					{
						CTPoint<int> coverageSize;
						if ( pFrame->bShowStorageCoverage )
						{
							ITerrainEditor *pTerrainEditor = dynamic_cast<ITerrainEditor*>( pTerrain );
							STerrainInfo &rTerrainInfo = const_cast<STerrainInfo&>( pTerrainEditor->GetTerrainInfo() );
							
							coverageSize.x = rTerrainInfo.tiles.GetSizeX() * 2;
							coverageSize.y = rTerrainInfo.tiles.GetSizeY() * 2;
						}

						size.x = pImage->GetSizeX();
						size.y = pImage->GetSizeY();

						if ( ( size.x > 0 ) && ( size.y > 0 ) )
						{
							Create( pDC );
							
							CImageAccessor imageAccessor = pImage;
							for( int nYIndex = 0; nYIndex < size.y; ++nYIndex )
							{
								for( int nXIndex = 0; nXIndex < size.x; ++nXIndex )
								{
									if ( pFrame->bShowStorageCoverage && ( pFrame->storageCoverageTileArray[ ( size.y - nYIndex - 1 ) * coverageSize.y / size.y][nXIndex * coverageSize.y / size.x] > 0 ) )
									{
										if ( imageAccessor[nYIndex][nXIndex].g > nBorderGreenColor )
										{
											dc.SetPixel( nXIndex, nYIndex, RGB( 0, imageAccessor[nYIndex][nXIndex].g, 0 ) );
										}
										else
										{
											dc.SetPixel( nXIndex, nYIndex, RGB( 0, nBorderGreenColor, 0 ) );
										}
									}
									else
									{
										dc.SetPixel( nXIndex, nYIndex, RGB( imageAccessor[nYIndex][nXIndex].r, imageAccessor[nYIndex][nXIndex].g, imageAccessor[nYIndex][nXIndex].b ) );
									}
								}
							}
						}
					}
				}
				else
				{
					if ( ( pFrame->inputStates.GetActiveState() == CTemplateEditorFrame::STATE_TERRAIN ) &&
							 ( pFrame->m_mapEditorBarPtr->GetTerrainState() == CTemplateEditorFrame::STATE_TERRAIN_ALTITUDES ) )
					{
						ITerrainEditor *pTerrainEditor = dynamic_cast<ITerrainEditor*>( pTerrain );
						STerrainInfo &rTerrainInfo = const_cast<STerrainInfo&>( pTerrainEditor->GetTerrainInfo() );
						
						size.x = rTerrainInfo.altitudes.GetSizeX() - 1;
						size.y = rTerrainInfo.altitudes.GetSizeY() - 1;

						if ( ( size.x > 0 ) && ( size.y > 0 ) )
						{
							Create( pDC );
							
							CTPoint<float> heightRange( rTerrainInfo.altitudes[0][0].fHeight, rTerrainInfo.altitudes[0][0].fHeight );

							for( int nYIndex = 0; nYIndex < size.y; ++nYIndex )
							{
								for( int nXIndex = 0; nXIndex < size.x; ++nXIndex )
								{
									if ( rTerrainInfo.altitudes[nYIndex][nXIndex].fHeight < heightRange.min )
									{
										heightRange.min = rTerrainInfo.altitudes[nYIndex][nXIndex].fHeight;
									}
									else if ( rTerrainInfo.altitudes[nYIndex][nXIndex].fHeight > heightRange.max )
									{
										heightRange.max = rTerrainInfo.altitudes[nYIndex][nXIndex].fHeight;
									}
								}
							}
							if ( heightRange.max == heightRange.min )
							{
								heightRange.min = rTerrainInfo.altitudes[0][0].fHeight;
								heightRange.max = rTerrainInfo.altitudes[0][0].fHeight - 1.0f;
							}
							for( int nYIndex = 0; nYIndex < size.y; ++nYIndex )
							{
								for( int nXIndex = 0; nXIndex < size.x; ++nXIndex )
								{
									if ( CVertexAltitudeInfo::IsValidHeight( rTerrainInfo.altitudes, nXIndex, nYIndex ) )
									{
										DWORD dwGradient = 0xFF * ( rTerrainInfo.altitudes[nYIndex][nXIndex].fHeight - heightRange.min ) / ( heightRange.max - heightRange.min );
										if ( pFrame->bShowStorageCoverage && ( pFrame->storageCoverageTileArray[( size.y - nYIndex - 1 ) * 2][nXIndex * 2] > 0 ) )
										{
											if ( dwGradient > nBorderGreenColor )
											{
												dc.SetPixel( nXIndex, nYIndex, RGB( 0, dwGradient, 0 ) );
											}
											else
											{
												dc.SetPixel( nXIndex, nYIndex, RGB( 0, nBorderGreenColor, 0 ) );
											}
										}
										else
										{
											dc.SetPixel( nXIndex, nYIndex, RGB( dwGradient, dwGradient, dwGradient ) );
										}
									}
									else
									{
										dc.SetPixel( nXIndex, nYIndex, RGB( 0xFF, 0x00, 0x00 ) );
									}
								}
							}
						}
					}
					else
					{
						CPtr<IImage> pImage = dynamic_cast<ITerrainEditor*>( pTerrain )->Export();
						if( !pImage.IsEmpty() )
						{
							size.x = pImage->GetSizeX();
							size.y = pImage->GetSizeY();

							if ( ( size.x > 0 ) && ( size.y > 0 ) )
							{
								Create( pDC );
								
								CImageAccessor imageAccessor = pImage;
								for( int nYIndex = 0; nYIndex < size.y; ++nYIndex )
								{
									for( int nXIndex = 0; nXIndex < size.x; ++nXIndex )
									{
										COLORREF color = colors[ imageAccessor[nYIndex][nXIndex].b ];
										if ( pFrame->bShowStorageCoverage && ( pFrame->storageCoverageTileArray[( size.y - nYIndex - 1 ) * 2][nXIndex * 2] > 0 ) )
										{
											color = ( color & ( 0x0000FF00 ) ) >> 8;
											if ( color > nBorderGreenColor )
											{
												dc.SetPixel( nXIndex, nYIndex, RGB( 0, color, 0 ) );
											}
											else
											{
												dc.SetPixel( nXIndex, nYIndex, RGB( 0, nBorderGreenColor, 0 ) );
											}
										}
										else
										{
											dc.SetPixel( nXIndex, nYIndex, color );
										}
									}
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
void CMiniMapTerrainGrid::Update( CDC *pDC )
{
	if( IScene *pScene = GetSingleton<IScene>() )
	{
		if ( ITerrain *pTerrain = pScene->GetTerrain() )
		{
			size.x = pTerrain->GetSizeX();
			size.y = pTerrain->GetSizeY();

			glidLines.x = pTerrain->GetPatchesX();
			glidLines.y = pTerrain->GetPatchesY();
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
