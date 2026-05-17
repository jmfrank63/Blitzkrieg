//��� ������ ���
//REMOVE_OBJECTS_FROM_RECT
//��� ���������� ������ linkID
//UPDATE_LINK_ID

#include "stdafx.h"

#include "MapInfo_Types.h"
#include "LA_Types.h"
#include "VA_Types.h"
#include "Polygons_Types.h"
#include "MiniMap_Types.h"
#include "VSO_Types.h"
#include "Resource_Types.h"
#include "IB_Types.h"

#include "..\Formats\fmtTerrain.h"

#include "..\AILogic\AIConsts.h"
#include "TerrainBuilder.h"

//#include "RMG_Polygons.h"
//#include "RMG_Image.h"
//#include "VSO_Types.h"
//#include "RMG_Types.h"
//#include "MiniMap_Types.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ������ �����-�� ������� �����
const int CMapInfo::RANDOM_SEED = 17;
int CMapInfo::nCurRandomSeed = 0;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMapInfo::PointInMap( const SLoadMapInfo &rLoadMapInfo, float x, float y, bool bAIPoint )
{
	if ( bAIPoint )
	{
		return ( ( x >= 0.0f ) &&
						 ( y >= 0.0f ) &&
						 ( x <= rLoadMapInfo.terrain.tiles.GetSizeX() * 2.0f * SAIConsts::TILE_SIZE ) &&
						 ( y <= rLoadMapInfo.terrain.tiles.GetSizeY() * 2.0f * SAIConsts::TILE_SIZE ) );
	}
	else
	{
		return ( ( x >= 0.0f ) &&
						 ( y >= 0.0f ) &&
						 ( x <= rLoadMapInfo.terrain.tiles.GetSizeX() * fWorldCellSize ) &&
						 ( y <= rLoadMapInfo.terrain.tiles.GetSizeY() * fWorldCellSize ) );
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMapInfo::GetTileIndicesInternal( const CVec3 &rPoint, int *pnXPosition, int *pnYPosition, const CTPoint<int> &rTerrainSize, float fCellSize, bool isYReverse )
{
	if ( rPoint.x >= 0 )
	{
		*pnXPosition = static_cast<int>( rPoint.x / fCellSize );
	}
	else
	{
		*pnXPosition = static_cast<int>( rPoint.x / fCellSize - 1.0f );
	}
	if ( rPoint.y >= 0 )
	{
		*pnYPosition = static_cast<int>( rPoint.y / fCellSize );
	}
	else
	{
		*pnYPosition = static_cast<int>( rPoint.y / fCellSize - 1.0f );
	}
	
	if ( isYReverse )
	{
		*pnYPosition = rTerrainSize.y - *pnYPosition - 1;
	}

	return ( ( *pnXPosition >= 0 ) &&
					 ( *pnXPosition < rTerrainSize.x ) &&
					 ( *pnYPosition >= 0 ) &&
					 ( *pnYPosition < rTerrainSize.y ) );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMapInfo::PackFrameIndex( IObjectsDB *pGDB, SMapObjectInfo *pInfo )
{
	const SGDBObjectDesc *pDesc = pGDB->GetDesc( pInfo->szName.c_str() );
	NI_ASSERT_T( pDesc != 0, NStr::Format( "Can't find descriptor for \"%s\"", pInfo->szName.c_str() ) );
	switch ( pDesc->eGameType ) 
	{
		case SGVOGT_FENCE:
		{
			TPackFrameIndex( pGDB, pInfo, NGDB::GetRPGStats<SFenceRPGStats>( pGDB, pDesc ), "fence" );
			break;
		}
		case SGVOGT_ENTRENCHMENT:
		{
			TPackFrameIndex( pGDB, pInfo, NGDB::GetRPGStats<SEntrenchmentRPGStats>( pGDB, pDesc ), "entrenchment" );
			break;
		}
		case SGVOGT_BRIDGE:
		{
			TPackFrameIndex( pGDB, pInfo, NGDB::GetRPGStats<SBridgeRPGStats>( pGDB, pDesc ), "bridge" );
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMapInfo::UnpackFrameIndex( IObjectsDB *pGDB, SMapObjectInfo *pInfo, int *pRandomSeed )
{
	const SGDBObjectDesc *pDesc = pGDB->GetDesc( pInfo->szName.c_str() );
	NI_ASSERT_T( pDesc != 0, NStr::Format( "Can't find descriptor for \"%s\"", pInfo->szName.c_str() ) );
	switch ( pDesc->eGameType ) 
	{
		case SGVOGT_FENCE:
		{
			TUnpackFrameIndex( pGDB, pInfo, NGDB::GetRPGStats<SFenceRPGStats>( pGDB, pDesc ), "fence", pRandomSeed );
			break;
		}
		case SGVOGT_ENTRENCHMENT:
		{
			TUnpackFrameIndex( pGDB, pInfo, NGDB::GetRPGStats<SEntrenchmentRPGStats>( pGDB, pDesc ), "entrenchment", pRandomSeed );
			break;
		}
		case SGVOGT_BRIDGE:
		{
			TUnpackFrameIndex( pGDB, pInfo, NGDB::GetRPGStats<SBridgeRPGStats>( pGDB, pDesc ), "bridge", pRandomSeed );
			break;
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMapInfo::GetTerrainTileIndices( const STerrainInfo &rTerrainInfo, const CVec3 &rPoint, CTPoint<int> *pPoint )
{
	NI_ASSERT_T( pPoint != 0,
							 NStr::Format( "Wrong parameter: %x\n", pPoint ) );
	return GetTileIndicesInternal( rPoint, &( pPoint->x ), &( pPoint->y ), CTPoint<int>( rTerrainInfo.tiles.GetSizeX(), rTerrainInfo.tiles.GetSizeY() ), fWorldCellSize,	true );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMapInfo::GetTileIndices( const STerrainInfo &rTerrainInfo, const CVec3 &rPoint, CTPoint<int> *pPoint )
{
	NI_ASSERT_T( pPoint != 0,
							 NStr::Format( "Wrong parameter: %x\n", pPoint ) );
	return GetTileIndicesInternal( rPoint, &( pPoint->x ), &( pPoint->y ), CTPoint<int>( rTerrainInfo.tiles.GetSizeX(), rTerrainInfo.tiles.GetSizeY() ), fWorldCellSize,	false );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMapInfo::GetAITileIndices( const STerrainInfo &rTerrainInfo, const CVec3 &rPoint, CTPoint<int> *pPoint )
{
	NI_ASSERT_T( pPoint != 0,
							 NStr::Format( "Wrong parameter: %x\n", pPoint ) );
	return GetTileIndicesInternal( rPoint, &( pPoint->x ), &( pPoint->y ), CTPoint<int>( rTerrainInfo.tiles.GetSizeX(), rTerrainInfo.tiles.GetSizeY() ), fWorldCellSize / 2.0f,	false );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMapInfo::InvertYTile( const STerrainInfo &rTerrainInfo, CTPoint<int> *pPoint )
{
	NI_ASSERT_T( pPoint != 0,
							 NStr::Format( "Wrong parameter: %x\n", pPoint ) );
	pPoint->y = rTerrainInfo.tiles.GetSizeY() - pPoint->y - 1;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMapInfo::InvertYPosition( const STerrainInfo &rTerrainInfo, CTPoint<float> *pPoint )
{
	NI_ASSERT_T( pPoint != 0,
							 NStr::Format( "Wrong parameter: %x\n", pPoint ) );
	pPoint->y = ( rTerrainInfo.tiles.GetSizeY() * fWorldCellSize ) - pPoint->y;
}
	
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMapInfo::FillDefaultDiplomacies( SLoadMapInfo *pLoadMapInfo )
{
	if ( pLoadMapInfo->diplomacies.empty() )
	{
		pLoadMapInfo->diplomacies.resize( 3 );
		pLoadMapInfo->diplomacies[0] = 0;
		pLoadMapInfo->diplomacies[1] = 1;
		pLoadMapInfo->diplomacies[2] = 2;

		pLoadMapInfo->unitCreation.units.resize( 2 );
		pLoadMapInfo->unitCreation.Validate();
	}
	
	if ( pLoadMapInfo->aiGeneralMapInfo.sidesInfo.empty() )
	{
		pLoadMapInfo->aiGeneralMapInfo.sidesInfo.push_back( SAIGeneralSideInfo() );
		pLoadMapInfo->aiGeneralMapInfo.sidesInfo.push_back( SAIGeneralSideInfo() );
	}
	
	if ( pLoadMapInfo->playersCameraAnchors.empty() )
	{
		pLoadMapInfo->playersCameraAnchors.push_back( VNULL3 );
		pLoadMapInfo->playersCameraAnchors.push_back( VNULL3 );
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMapInfo::Clear( SLoadMapInfo *pLoadMapInfo )
{
	NI_ASSERT_T( pLoadMapInfo != 0,
							 NStr::Format( "Wrong parameter: %x\n", pLoadMapInfo ) );

	pLoadMapInfo->szSeasonFolder.clear();
	pLoadMapInfo->szMODName.clear();
	pLoadMapInfo->szMODVersion.clear();

	pLoadMapInfo->terrain.szTilesetDesc.clear();
	pLoadMapInfo->terrain.szCrossetDesc.clear();
	//pLoadMapInfo->terrain.szRoadsetDesc.clear();
	pLoadMapInfo->terrain.szNoise.clear();

	pLoadMapInfo->terrain.patches.Clear();
	pLoadMapInfo->terrain.tiles.Clear();

	pLoadMapInfo->terrain.altitudes.Clear();

	pLoadMapInfo->terrain.roads3.clear();
	pLoadMapInfo->terrain.rivers.clear();

	//�������������� objects ( std::vector<SMapObjectInfo>  )
	pLoadMapInfo->objects.clear();

	//�������������� entrenchments ( std::vector<SEntrenchmentInfo>  )
	pLoadMapInfo->entrenchments.clear();

	//�������������� bridges ( std::vector< std::vector<int> > )
	pLoadMapInfo->bridges.clear();

	//�������������� reinforcements ( SReinforcementGroupInfo )
	pLoadMapInfo->reinforcements.groups.clear();

	//�������������� szScriptFile ( std::string )
	pLoadMapInfo->szScriptFile.clear();
	//�������������� scriptAreas ( std::vector<SScriptArea> )
	pLoadMapInfo->scriptAreas.clear();

	//�������������� vCameraAnchor ( CVec3 )
	pLoadMapInfo->vCameraAnchor = VNULL3;

	pLoadMapInfo->nSeason = REAL_SEASONS[SEASON_SUMMER];

	//�������������� startCommandsList ( TStartCommandsList )
	pLoadMapInfo->startCommandsList.clear();

	//�������������� reservePositionsList ( TReservePositionsList )
	pLoadMapInfo->reservePositionsList.clear();

	pLoadMapInfo->soundsList.clear();
	pLoadMapInfo->szForestCircleSounds.clear();
	pLoadMapInfo->szForestAmbientSounds.clear();

	//�������������� unitCreation ( SUnitCreationInfo )
	pLoadMapInfo->unitCreation.units.clear();

	pLoadMapInfo->diplomacies.clear();
	pLoadMapInfo->aiGeneralMapInfo.sidesInfo.clear();
	pLoadMapInfo->playersCameraAnchors.clear();
	
	pLoadMapInfo->nType = TYPE_SINGLE_PLAYER;

	FillDefaultDiplomacies( pLoadMapInfo );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMapInfo::Create( SLoadMapInfo *pLoadMapInfo, const CTPoint<int> &rSize, int _nSeason, const std::string &rszSeasonFolder, int nPlayersCount, int _nType )
{
	NI_ASSERT_TF( pLoadMapInfo != 0,
							  NStr::Format( "CMapInfo::Create(): Wrong parameter pLoadMapInfo %x\n", pLoadMapInfo ),
							  return false );
	NI_ASSERT_TF( ( _nSeason >= 0 ) && ( _nSeason < REAL_SEASONS_COUNT ),
							  NStr::Format( "CMapInfo::Create(): Invalid season: %d\n", _nSeason ),
							  return false );
	NI_ASSERT_TF( ( rSize.x > 0 ) && ( rSize.y > 0 ) && ( rSize.x <= 32 ) && ( rSize.y <= 32 ),
							  NStr::Format( "CMapInfo::Create(): Invalid size: [%dx%d]\n", rSize.x, rSize.y ),
							  return false );

	Clear( pLoadMapInfo );

	//�������������� nSeason ( int )
	pLoadMapInfo->nSeason = _nSeason;
	pLoadMapInfo->szSeasonFolder = rszSeasonFolder;

	pLoadMapInfo->nType = _nType;

	//�������������� terrain ( STerrainInfo )
	pLoadMapInfo->terrain.szTilesetDesc = pLoadMapInfo->szSeasonFolder + RMGC_TILESET_FILE_NAME;
	pLoadMapInfo->terrain.szCrossetDesc = pLoadMapInfo->szSeasonFolder + RMGC_CROSSSET_FILE_NAME;
	//pLoadMapInfo->terrain.szRoadsetDesc = pLoadMapInfo->szSeasonFolder + RMGC_ROADSET_FILE_NAME;
	pLoadMapInfo->terrain.szNoise = pLoadMapInfo->szSeasonFolder + RMGC_NOISE_FILE_NAME;

	pLoadMapInfo->terrain.patches.SetSizes( rSize.x, rSize.y );
	for ( int nXIndex = 0; nXIndex < rSize.x; ++nXIndex )
	{
		for ( int nYIndex = 0; nYIndex < rSize.y; ++nYIndex )
		{
			STerrainPatchInfo terrainPatchInfo;
			terrainPatchInfo.nStartX = nXIndex * STerrainPatchInfo::nSizeX;
			terrainPatchInfo.nStartY = nYIndex * STerrainPatchInfo::nSizeY;
			pLoadMapInfo->terrain.patches[nYIndex][nXIndex] = terrainPatchInfo;
		}
	}

	pLoadMapInfo->terrain.tiles.SetSizes( rSize.x * STerrainPatchInfo::nSizeX, rSize.y * STerrainPatchInfo::nSizeY );
	for ( int nXIndex = 0; nXIndex < ( rSize.x * STerrainPatchInfo::nSizeX ); ++nXIndex )
	{
		for ( int nYIndex = 0; nYIndex < ( rSize.y * STerrainPatchInfo::nSizeY ); ++nYIndex )
		{
			SMainTileInfo mainTileInfo;
			mainTileInfo.tile = 0;
			mainTileInfo.noise = 1;
			pLoadMapInfo->terrain.tiles[nYIndex][nXIndex] = mainTileInfo;
		}
	}

	//�� 1 ������ ��� ������!
	pLoadMapInfo->terrain.altitudes.SetSizes( rSize.x * STerrainPatchInfo::nSizeX + 1, rSize.y * STerrainPatchInfo::nSizeY + 1 );
	pLoadMapInfo->terrain.altitudes.SetZero();

	if ( nPlayersCount > 0 )
	{
		pLoadMapInfo->diplomacies.resize( nPlayersCount );
		for ( int nPlayerIndex = 0; nPlayerIndex < pLoadMapInfo->diplomacies.size(); ++nPlayerIndex )
		{
			pLoadMapInfo->diplomacies[nPlayerIndex] = 2;
		}
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMapInfo::IsValid( const SLoadMapInfo rLoadMapInfo )
{
	if ( ( rLoadMapInfo.terrain.patches.GetSizeX() <= 0 ) || ( rLoadMapInfo.terrain.patches.GetSizeY() <= 0 ) )
	{
		return false;
	}
	if ( ( rLoadMapInfo.terrain.tiles.GetSizeX() <= 0 ) || ( rLoadMapInfo.terrain.tiles.GetSizeY() <= 0 ) )
	{
		return false;
	}
	if ( ( rLoadMapInfo.terrain.altitudes.GetSizeX() <= 0 ) || ( rLoadMapInfo.terrain.altitudes.GetSizeY() <= 0 ) )
	{
		return false;
	}
	if ( ( rLoadMapInfo.nSeason < 0 ) || ( rLoadMapInfo.nSeason >= REAL_SEASONS_COUNT ) )
	{
		return false;
	}
	if ( rLoadMapInfo.szSeasonFolder.empty() )
	{
		return false;
	}
	if ( rLoadMapInfo.diplomacies.empty() )
	{
		return false;
	}
	if ( rLoadMapInfo.unitCreation.units.size() < ( rLoadMapInfo.diplomacies.size() - 1 ) )
	{
		return false;
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CMapInfo::GetSelectedSeason( int nSeason, const std::string &rszSeasonFolder )
{
	std::string szSeasonFolder = rszSeasonFolder;
	NStr::ToLower( szSeasonFolder );
	int nSelectedSeason = nSeason;
	if ( nSelectedSeason == CMapInfo::SEASON_SUMMER )
	{
		if ( szSeasonFolder != CMapInfo::SEASON_FOLDERS[nSelectedSeason] )
		{
			nSelectedSeason = CMapInfo::SEASON_SPRING;
		}
	}
	return nSelectedSeason;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMapInfo::RemoveObject( SLoadMapInfo *pLoadMapInfo, int nObjectIndex )
{
	NI_ASSERT_TF( pLoadMapInfo != 0,
							NStr::Format( "Wrong parameter: %x\n", pLoadMapInfo ),
							return false );
	NI_ASSERT_TF( ( nObjectIndex >= 0 ) && ( nObjectIndex < pLoadMapInfo->objects.size() ),
							NStr::Format( "Wrong object index: %d\n", nObjectIndex ),
							return false );
	
	//������� ������
	pLoadMapInfo->objects.erase( pLoadMapInfo->objects.begin() + nObjectIndex );
	return true;	
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMapInfo::RemoveObjects( SLoadMapInfo *pLoadMapInfo, const std::list<CVec2> &rClearPolygon )
{
	NI_ASSERT_TF( pLoadMapInfo != 0,
							NStr::Format( "Wrong parameter: %x\n", pLoadMapInfo ),
							return false );
	
	//����� ��� �������� ������ �������� � �������
	CArray2D<BYTE> tileMap( pLoadMapInfo->terrain.tiles.GetSizeX() * 2, pLoadMapInfo->terrain.tiles.GetSizeY() * 2 );
	tileMap.Set( RMGC_UNLOCKED );
	ModifyTilesFunctional<CArray2D<BYTE>, BYTE> tileMapModifyTiles( RMGC_LOCKED, &tileMap );
	CheckTilesFunctional<CArray2D<BYTE>, BYTE> tileMapCheckTiles( RMGC_LOCKED, &tileMap );
	CTRect<int>						tileMapRect( 0, 0, tileMap.GetSizeX(), tileMap.GetSizeY() );
	
	//����� �����, ������� �������� � ������
	ApplyTilesInPolygon<ModifyTilesFunctional<CArray2D<BYTE>, BYTE>, std::list<CVec2>, CVec2>( tileMapRect, rClearPolygon, fWorldCellSize / 2.0f, tileMapModifyTiles );

	//������� �������, ������� ����� passability �������� �� ���������� �����
	for ( int nObjectIndex = 0; nObjectIndex < pLoadMapInfo->objects.size(); )
	{
		tileMapCheckTiles.isPresent = false;
		if ( !ApplyTilesInObjectsPassability( tileMapRect, pLoadMapInfo->objects[nObjectIndex], tileMapCheckTiles ) )
		{
			if ( RemoveObject( pLoadMapInfo, nObjectIndex ) )
			{
				//������ ��������� ��� �� ������ ������, ����������� �� ����
				continue;
			}
		}
		//������ ��������� �� ��� �� �������� ����������� ���
		++nObjectIndex;
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMapInfo::UpdateTerrain( struct STerrainInfo *pTerrainInfo, const CTRect<int> &rUpdateRect, const struct STilesetDesc &rTilesetDesc, const struct SCrossetDesc &rCrossetDesc, /*const struct SRoadsetDesc &rRoadsetDesc,*/ const struct SGFXLightDirectional &rSunlight )
{
	NI_ASSERT_TF( pTerrainInfo != 0,
							  NStr::Format( "Wrong parameter: %x\n", pTerrainInfo ),
							  return false );
	bool result = UpdateTerrainCrosses( pTerrainInfo, rUpdateRect, rTilesetDesc, rCrossetDesc/*, rRoadsetDesc*/ );
	if ( result )
	{
		CTRect<int> riversUpdateRect( rUpdateRect.minx * STerrainPatchInfo::nSizeX,
																	rUpdateRect.miny * STerrainPatchInfo::nSizeY,
																	rUpdateRect.maxx * STerrainPatchInfo::nSizeX,
																	rUpdateRect.maxy * STerrainPatchInfo::nSizeY );
		result = UpdateTerrainRivers( pTerrainInfo, riversUpdateRect ); 
		if ( result )
		{
			CTRect<int> roads3DUpdateRect( rUpdateRect.minx * STerrainPatchInfo::nSizeX,
																		 rUpdateRect.miny * STerrainPatchInfo::nSizeY,
																		 rUpdateRect.maxx * STerrainPatchInfo::nSizeX,
																		 rUpdateRect.maxy * STerrainPatchInfo::nSizeY );
			result = UpdateTerrainRoads3D( pTerrainInfo, roads3DUpdateRect ); 
			if ( result )
			{
				CTRect<int> shadeUpdateRect( rUpdateRect.minx * STerrainPatchInfo::nSizeX,
																		 rUpdateRect.miny * STerrainPatchInfo::nSizeY,
																		 ( rUpdateRect.maxx * STerrainPatchInfo::nSizeX ) + 1,
																		 ( rUpdateRect.maxy * STerrainPatchInfo::nSizeY ) + 1 );
				result = UpdateTerrainShades( pTerrainInfo, shadeUpdateRect, rSunlight ); 
			}
		}
	}
	return result;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMapInfo::UpdateTerrainCrosses( STerrainInfo *pTerrainInfo, const CTRect<int> &rUpdateRect, const STilesetDesc &rTilesetDesc, const SCrossetDesc &rCrossetDesc/*, const SRoadsetDesc &rRoadsetDesc*/ )
{
	NI_ASSERT_TF( pTerrainInfo != 0,
							  NStr::Format( "Wrong parameter: %x\n", pTerrainInfo ),
							  return false );
	
	int nTerrainSizeX = pTerrainInfo->patches.GetSizeX() * STerrainPatchInfo::nSizeX;
	int nTerrainSizeY = pTerrainInfo->patches.GetSizeY() * STerrainPatchInfo::nSizeY;
	CTRect<int> rcSuper( 0, 0, nTerrainSizeX, nTerrainSizeY );

	CTerrainBuilder builder( rTilesetDesc, rCrossetDesc/*, rRoadsetDesc*/ );

	{
		CTRect<int> rcPatchesRect;
		CTRect<int> rcTemp;

		CTPoint<int> point = rUpdateRect.GetLeftTop();
		builder.GetPatchRect( point.x, point.y, &rcTemp );
		rcPatchesRect.left = rcTemp.left;
		rcPatchesRect.top = rcTemp.top;

		point = rUpdateRect.GetRightBottom();
		builder.GetPatchRect( point.x - 1, point.y - 1, &rcTemp );
		rcPatchesRect.right = rcTemp.right;
		rcPatchesRect.bottom = rcTemp.bottom;

		builder.PreprocessMapSegment( pTerrainInfo->tiles, rcPatchesRect );
	}

	for ( int nPatchXIndex = rUpdateRect.minx; nPatchXIndex < rUpdateRect.maxx; ++nPatchXIndex )
	{
		for ( int nPatchYIndex = rUpdateRect.miny; nPatchYIndex < rUpdateRect.maxy; ++nPatchYIndex )
		{
			CTerrainBuilder::SComplexCrosses newCrosses;
			CTRect<int> rcRect;
			builder.GetPatchRect( nPatchXIndex, nPatchYIndex, &rcRect );
			builder.MapSegmentGenerateCrosses( pTerrainInfo->tiles, rcRect, rcSuper, &newCrosses );
			{
				builder.CopyCrosses( &pTerrainInfo->patches[nPatchYIndex][nPatchXIndex], newCrosses );
			}
		}
	}	
	return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMapInfo::UpdateTerrainRivers( STerrainInfo *pTerrainInfo, const CTRect<int> &rUpdateRect )
{
	NI_ASSERT_TF( pTerrainInfo != 0,
							  NStr::Format( "Wrong parameter: %x\n", pTerrainInfo ),
							  return false );
	//������������ ID'����� ���
	for ( int nRiverIndex = 0; nRiverIndex < pTerrainInfo->rivers.size(); ++nRiverIndex )
	{
		pTerrainInfo->rivers[nRiverIndex].nID = nRiverIndex;
		CVSOBuilder::UpdateZ( pTerrainInfo->altitudes, &( pTerrainInfo->rivers[nRiverIndex] ) );
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMapInfo::UpdateTerrainRoads3D( STerrainInfo *pTerrainInfo, const CTRect<int> &rUpdateRect )
{
	NI_ASSERT_TF( pTerrainInfo != 0,
							  NStr::Format( "Wrong parameter: %x\n", pTerrainInfo ),
							  return false );
	//������������ ID'����� �����
	for ( int nRoad3DIndex = 0; nRoad3DIndex < pTerrainInfo->roads3.size(); ++nRoad3DIndex )
	{
		pTerrainInfo->roads3[nRoad3DIndex].nID = nRoad3DIndex;
		CVSOBuilder::UpdateZ( pTerrainInfo->altitudes, &( pTerrainInfo->roads3[nRoad3DIndex] ) );
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMapInfo::UpdateTerrainShades( STerrainInfo *pTerrainInfo, const CTRect<int> &rUpdateRect, const SGFXLightDirectional &rSunlight )
{
	NI_ASSERT_TF( pTerrainInfo != 0,
							  NStr::Format( "Wrong parameter: %x\n", pTerrainInfo ),
							  return false );
	
	return CVertexAltitudeInfo::UpdateShades( &( pTerrainInfo->altitudes ), rUpdateRect, rSunlight );
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int CMapInfo::UpdateObjects( SLoadMapInfo *pLoadMapInfo, const CTRect<int> &rUpdateRect )
{
	NI_ASSERT_TF( pLoadMapInfo != 0,
							  NStr::Format( "Wrong parameter: %x\n", pLoadMapInfo ),
							  return false );

	//UPDATE_LINK_ID
	//usedIDs[old nLinkID] = new nLinkID;
	std::unordered_map<int, int> usedIDs;

	int nCurrentLinkID = RMGC_INVALID_LINK_ID_VALUE + 1;
	
	//������� ����� nLinkID
	for ( int nObjectIndex = 0; nObjectIndex < pLoadMapInfo->objects.size(); ++nObjectIndex )
	{	
		if ( pLoadMapInfo->objects[nObjectIndex].link.nLinkID != RMGC_INVALID_LINK_ID_VALUE )
		{
			usedIDs[ pLoadMapInfo->objects[nObjectIndex].link.nLinkID ] = nCurrentLinkID;
		}
		pLoadMapInfo->objects[nObjectIndex].link.nLinkID = nCurrentLinkID;
		++nCurrentLinkID;
	}
	for ( int nObjectIndex = 0; nObjectIndex < pLoadMapInfo->scenarioObjects.size(); ++nObjectIndex )
	{	
		if ( pLoadMapInfo->scenarioObjects[nObjectIndex].link.nLinkID != RMGC_INVALID_LINK_ID_VALUE )
		{
			usedIDs[ pLoadMapInfo->scenarioObjects[nObjectIndex].link.nLinkID ] = nCurrentLinkID;
		}
		pLoadMapInfo->scenarioObjects[nObjectIndex].link.nLinkID = nCurrentLinkID;
		++nCurrentLinkID;
	}

	
	//���������� ����� nLinkID � SMapObjectInfo.link.nLinkWith
	for ( int nObjectIndex = 0; nObjectIndex < pLoadMapInfo->objects.size(); ++nObjectIndex )
	{
		if ( ( pLoadMapInfo->objects[nObjectIndex].link.nLinkWith != RMGC_INVALID_LINK_ID_VALUE ) &&
			   ( usedIDs.find( pLoadMapInfo->objects[nObjectIndex].link.nLinkWith ) != usedIDs.end() ) )
		{
			pLoadMapInfo->objects[nObjectIndex].link.nLinkWith = usedIDs[ pLoadMapInfo->objects[nObjectIndex].link.nLinkWith ];
		}
		else
		{
			pLoadMapInfo->objects[nObjectIndex].link.nLinkWith = RMGC_INVALID_LINK_ID_VALUE;
			pLoadMapInfo->objects[nObjectIndex].link.bIntention = true;
		}
	}
	for ( int nObjectIndex = 0; nObjectIndex < pLoadMapInfo->scenarioObjects.size(); ++nObjectIndex )
	{
		if ( ( pLoadMapInfo->scenarioObjects[nObjectIndex].link.nLinkWith != RMGC_INVALID_LINK_ID_VALUE ) &&
			   ( usedIDs.find( pLoadMapInfo->scenarioObjects[nObjectIndex].link.nLinkWith ) != usedIDs.end() ) )
		{
			pLoadMapInfo->scenarioObjects[nObjectIndex].link.nLinkWith = usedIDs[ pLoadMapInfo->scenarioObjects[nObjectIndex].link.nLinkWith ];
		}
		else
		{
			pLoadMapInfo->scenarioObjects[nObjectIndex].link.nLinkWith = RMGC_INVALID_LINK_ID_VALUE;
			pLoadMapInfo->scenarioObjects[nObjectIndex].link.bIntention = true;
		}
	}
	
	//���������� ����� nLinkID � entrenchments
	for ( int nEntrenchmentIndex = 0; nEntrenchmentIndex < pLoadMapInfo->entrenchments.size(); ++nEntrenchmentIndex )
	{
		for ( int nSectionIndex = 0; nSectionIndex < pLoadMapInfo->entrenchments[nEntrenchmentIndex].sections.size(); ++nSectionIndex )
		{
			for ( int nTSegmentIndex = 0; nTSegmentIndex < pLoadMapInfo->entrenchments[nEntrenchmentIndex].sections[nSectionIndex].size(); ++nTSegmentIndex )
			{
				pLoadMapInfo->entrenchments[nEntrenchmentIndex].sections[nSectionIndex][nTSegmentIndex] = usedIDs[ pLoadMapInfo->entrenchments[nEntrenchmentIndex].sections[nSectionIndex][nTSegmentIndex] ];
			}
		}
	}

	//���������� ����� nLinkID � bridges
	for ( int nBrigeIndex = 0; nBrigeIndex < pLoadMapInfo->bridges.size(); ++nBrigeIndex )
	{
		for ( int nBrigeElementIndex = 0; nBrigeElementIndex < pLoadMapInfo->bridges[nBrigeIndex].size(); ++nBrigeElementIndex )
		{
			pLoadMapInfo->bridges[nBrigeIndex][nBrigeElementIndex] = usedIDs[ pLoadMapInfo->bridges[nBrigeIndex][nBrigeElementIndex] ];
		}
	}

	//���������� ����� linkID � startCommandsList
	for ( SLoadMapInfo::TStartCommandsList::iterator it = pLoadMapInfo->startCommandsList.begin();
			it != pLoadMapInfo->startCommandsList.end();
			++it )
	{
		for( int nLinkIDIndex = 0; nLinkIDIndex < it->unitLinkIDs.size(); ++nLinkIDIndex )
		{
			it->unitLinkIDs[nLinkIDIndex] = usedIDs[ it->unitLinkIDs[nLinkIDIndex] ];
		}
		if ( it->linkID != RMGC_INVALID_LINK_ID_VALUE )
		{
			it->linkID = usedIDs[it->linkID];
		}
	}

	//���������� ����� linkID � reservePositionsList
	for ( SLoadMapInfo::TReservePositionsList::iterator it = pLoadMapInfo->reservePositionsList.begin();
	      it != pLoadMapInfo->reservePositionsList.end();
				++it )
	{
		if ( it->nArtilleryLinkID != RMGC_INVALID_LINK_ID_VALUE )
		{
			it->nArtilleryLinkID = usedIDs[it->nArtilleryLinkID];
		}
		if ( it->nTruckLinkID != RMGC_INVALID_LINK_ID_VALUE )
		{
			it->nTruckLinkID = usedIDs[it->nTruckLinkID];
		}
	}
	
	return nCurrentLinkID;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMapInfo::PackFrameIndices( struct SLoadMapInfo *pLoadMapInfo )
{
	CPtr<IObjectsDB> pODB = GetSingleton<IObjectsDB>();
	NI_ASSERT_T( pODB != 0,
							 NStr::Format( "CMapInfo::PackFrameIndices() GetSingleton<IObjectsDB>() == 0" ) );

	for ( int nObjectIndex = 0; nObjectIndex < pLoadMapInfo->objects.size(); ++nObjectIndex )
	{
		PackFrameIndex( pODB, &( pLoadMapInfo->objects[nObjectIndex] ) );
	}
	
	for ( int nObjectIndex = 0; nObjectIndex < pLoadMapInfo->scenarioObjects.size(); ++nObjectIndex )
	{
		PackFrameIndex( pODB, &( pLoadMapInfo->scenarioObjects[nObjectIndex] ) );
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMapInfo::UnpackFrameIndices( struct SLoadMapInfo *pLoadMapInfo )
{
	CPtr<IObjectsDB> pODB = GetSingleton<IObjectsDB>();
	NI_ASSERT_T( pODB != 0,
							 NStr::Format( "CMapInfo::UnpackFrameIndices() GetSingleton<IObjectsDB>() == 0" ) );

	nCurRandomSeed = RANDOM_SEED;
	for ( int nObjectIndex = 0; nObjectIndex < pLoadMapInfo->objects.size(); ++nObjectIndex )
	{
		UnpackFrameIndex( pODB, &( pLoadMapInfo->objects[nObjectIndex] ), &nCurRandomSeed );
		nCurRandomSeed += RANDOM_SEED;
	}
	
	for ( int nObjectIndex = 0; nObjectIndex < pLoadMapInfo->scenarioObjects.size(); ++nObjectIndex )
	{
		UnpackFrameIndex( pODB, &( pLoadMapInfo->scenarioObjects[nObjectIndex] ), &nCurRandomSeed );
		nCurRandomSeed += RANDOM_SEED;
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CRemoveNonExistingVSOFunctor
{
private:
	IDataStorage *pDataStorage;
	std::string *pszOutputString;
	//
	std::set<std::string> existingTextures;
	std::set<std::string> existingDescriptions;
	//
	std::set<std::string> notExistingTextures;
	std::set<std::string> notExistingDescriptions;
	//
	bool CheckTexture( const std::string &rszTexture )
	{
		if ( existingTextures.find( rszTexture ) != existingTextures.end() )
		{
			return true;
		}
		if ( notExistingTextures.find( rszTexture ) != notExistingTextures.end() )
		{
			return false;
		}
		CPtr<IDataStream> pDDSStream = pDataStorage->OpenStream( ( rszTexture + GetDDSImageExtention( COMPRESSION_HIGH_QUALITY ) ).c_str(), STREAM_ACCESS_READ );
		if ( pDDSStream )
		{
			existingTextures.insert( rszTexture );
			return true;
		}
		else
		{
			notExistingTextures.insert( rszTexture );
			return false;
		}
	}
	//
	bool CheckDescription( const std::string &rszDescription )
	{
		if ( existingDescriptions.find( rszDescription ) != existingDescriptions.end() )
		{
			return true;
		}
		if ( notExistingDescriptions.find( rszDescription ) != notExistingDescriptions.end() )
		{
			return false;
		}
		CPtr<IDataStream> pXMLStream = pDataStorage->OpenStream( ( rszDescription + ".xml" ).c_str(), STREAM_ACCESS_READ );
		if ( pXMLStream )
		{
			existingDescriptions.insert( rszDescription );
			return true;
		}
		else
		{
			notExistingDescriptions.insert( rszDescription );
			return false;
		}
	}

public:
	std::list<SVectorStripeObject> removedVSOs;
	//
	CRemoveNonExistingVSOFunctor( IDataStorage *_pDataStorage, std::string *_pszOutputString ) : pDataStorage( _pDataStorage ), pszOutputString( _pszOutputString )	{	}
	//
	bool operator()( const SVectorStripeObject &rVectorStripeObject )
	{
		bool bExists = true;
		bExists = CheckTexture( rVectorStripeObject.bottom.szTexture );
		if ( bExists )
		{
			for ( int nLayerIndex = 0; nLayerIndex < rVectorStripeObject.bottomBorders.size(); ++nLayerIndex )
			{
				const SVectorStripeObjectDesc::SLayer &rLayer = rVectorStripeObject.bottomBorders[nLayerIndex];
				bExists = CheckTexture(rLayer.szTexture );
				if ( !bExists )
				{
					break;
				}
			}
		}
		if ( bExists )
		{
			for ( int nLayerIndex = 0; nLayerIndex < rVectorStripeObject.layers.size(); ++nLayerIndex )
			{
				const SVectorStripeObjectDesc::SLayer &rLayer = rVectorStripeObject.layers[nLayerIndex];
				bExists = CheckTexture(rLayer.szTexture );
				if ( !bExists )
				{
					break;
				}
			}
		}

		if ( !bExists )
		{
			bExists = CheckDescription( rVectorStripeObject.szDescName );
		}
		if ( !bExists )
		{
			removedVSOs.push_back( rVectorStripeObject );
			if ( !rVectorStripeObject.points.empty() )
			{
				if ( pszOutputString )
				{
					( *pszOutputString ) += NStr::Format( "VSO: %s, texture: %s, start point: [%.2f, %.2f]\r\n",
																								rVectorStripeObject.szDescName.c_str(),
																								rVectorStripeObject.bottom.szTexture.c_str(),
																								rVectorStripeObject.points[0].vPos.x / fWorldCellSize,
																								rVectorStripeObject.points[0].vPos.y / fWorldCellSize );
				}
			}
		}
		return !bExists;
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CRemoveNonExistingVSOFunctorProxy
{
	CRemoveNonExistingVSOFunctor *pFunctor;

public:
	CRemoveNonExistingVSOFunctorProxy( CRemoveNonExistingVSOFunctor *_pFunctor )
		: pFunctor( _pFunctor )	{}

	bool operator()( const SVectorStripeObject &rVectorStripeObject )
	{
		return ( *pFunctor )( rVectorStripeObject );
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CRemoveNonExistingObjectsFunctor
{
private:
	IDataStorage *pDataStorage;
	IObjectsDB *pObjectsDB;
	std::string *pszOutputString;

	std::set<std::string> existingObjects;
	std::set<std::string> existingObjectFolders;
	//
	std::set<std::string> notExistingObjects;
	std::set<std::string> notExistingObjectFolders;
	//
	bool CheckObjectFolder( const std::string &rszObjectFolder )
	{
		if ( existingObjectFolders.find( rszObjectFolder ) != existingObjectFolders.end() )
		{
			return true;
		}
		if ( notExistingObjectFolders.find( rszObjectFolder ) != notExistingObjectFolders.end() )
		{
			return false;
		}
		CPtr<IDataStream> pXMLStream = pDataStorage->OpenStream( ( rszObjectFolder + "\\1.xml" ).c_str(), STREAM_ACCESS_READ );
		if ( pXMLStream )
		{
			existingObjectFolders.insert( rszObjectFolder );
			return true;
		}
		else
		{
			notExistingObjectFolders.insert( rszObjectFolder );
			return false;
		}
	}
	//
	bool CheckObject( const std::string &rszObject )
	{
		if ( existingObjects.find( rszObject ) != existingObjects.end() )
		{
			return true;
		}
		if ( existingObjectFolders.find( rszObject ) != existingObjectFolders.end() )
		{
			return false;
		}
		const SGDBObjectDesc *pGDBObjectDesc = pObjectsDB->GetDesc( rszObject.c_str() );
		if ( pGDBObjectDesc )
		{
			if ( CheckObjectFolder( pGDBObjectDesc->szPath ) )
			{
				existingObjects.insert( rszObject );
				return true;
			}
		}
		existingObjectFolders.insert( rszObject );
		return false;
	}

public:
	std::list<SMapObjectInfo> removedObjects;
	std::set<int> removedLinkIDs;
	//
	CRemoveNonExistingObjectsFunctor( IDataStorage *_pDataStorage, IObjectsDB *_pObjectsDB, std::string *_pszOutputString ) : pDataStorage( _pDataStorage ), pObjectsDB( _pObjectsDB ), pszOutputString( _pszOutputString )	{	}
	bool operator()( const SMapObjectInfo &rMapObjectInfo )
	{
		bool bExists = true;
		bExists = CheckObject( rMapObjectInfo.szName );
		if ( !bExists )
		{
			removedObjects.push_back( rMapObjectInfo );
			removedLinkIDs.insert( rMapObjectInfo.link.nLinkID );
			if ( pszOutputString )
			{
				( *pszOutputString ) += NStr::Format( "Object: %s, pos: [%.2f, %.2f], player: %d, scriptID: %d\r\n",
																							rMapObjectInfo.szName.c_str(),
																							rMapObjectInfo.vPos.x / ( SAIConsts::TILE_SIZE * 2.0f ),
																							rMapObjectInfo.vPos.y / ( SAIConsts::TILE_SIZE * 2.0f ),
																							rMapObjectInfo.nPlayer,
																							rMapObjectInfo.nScriptID );
			}
		}
		return !bExists;
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CRemoveNonExistingObjectsFunctorProxy
{
	CRemoveNonExistingObjectsFunctor *pFunctor;
public:
	CRemoveNonExistingObjectsFunctorProxy( CRemoveNonExistingObjectsFunctor *_pFunctor )
		: pFunctor( _pFunctor )	{}

	bool operator()( const SMapObjectInfo &rMapObjectInfo )
	{
		return ( *pFunctor )( rMapObjectInfo );
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CRemoveNonExistingEntrenchmentsFunctor
{
private:
	std::set<int> *pRemovedLinkIDs;

public:
	CRemoveNonExistingEntrenchmentsFunctor( std::set<int> *_pRemovedLinkIDs ) : pRemovedLinkIDs( _pRemovedLinkIDs )	{	}
	//
	bool operator()( const SEntrenchmentInfo &rEntrenchmentInfo )
	{
		for ( int nSectionIndex = 0; nSectionIndex < rEntrenchmentInfo.sections.size(); ++nSectionIndex )
		{
			for ( int nTSegmentIndex = 0; nTSegmentIndex < rEntrenchmentInfo.sections[nSectionIndex].size(); ++nTSegmentIndex )
			{
				if ( pRemovedLinkIDs->find( rEntrenchmentInfo.sections[nSectionIndex][nTSegmentIndex] ) != pRemovedLinkIDs->end() )
				{
					return true;
				}
			}
		}		
		return false;
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CRemoveNonExistingBridgesFunctor
{
private:
	std::set<int> *pRemovedLinkIDs;

public:
	CRemoveNonExistingBridgesFunctor( std::set<int> *_pRemovedLinkIDs ) : pRemovedLinkIDs( _pRemovedLinkIDs )	{	}
	//
	bool operator()( const std::vector<int> &rBridgeInfo )
	{
		for ( int nBrigeElementIndex = 0; nBrigeElementIndex < rBridgeInfo.size(); ++nBrigeElementIndex )
		{
			if ( pRemovedLinkIDs->find( rBridgeInfo[nBrigeElementIndex] ) != pRemovedLinkIDs->end() )
			{
				return true;
			}
		}
		return false;
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CRemoveNonExistingStartCommandFunctor
{
private:
	std::set<int> *pRemovedLinkIDs;

public:
	CRemoveNonExistingStartCommandFunctor( std::set<int> *_pRemovedLinkIDs ) : pRemovedLinkIDs( _pRemovedLinkIDs )	{	}
	//
	bool operator()( const SAIStartCommand &rAIStartCommand )
	{
		for( int nLinkIDIndex = 0; nLinkIDIndex < rAIStartCommand.unitLinkIDs.size(); ++nLinkIDIndex )
		{
			if ( pRemovedLinkIDs->find( rAIStartCommand.unitLinkIDs[nLinkIDIndex] ) != pRemovedLinkIDs->end() )
			{
				return true;
			}
		}
		if ( rAIStartCommand.linkID != RMGC_INVALID_LINK_ID_VALUE )
		{
			if ( pRemovedLinkIDs->find( rAIStartCommand.linkID ) != pRemovedLinkIDs->end() )
			{
				return true;
			}
		}
		return false;
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class CRemoveNonExistingReservePositionFunctor
{
private:
	std::set<int> *pRemovedLinkIDs;

public:
	CRemoveNonExistingReservePositionFunctor( std::set<int> *_pRemovedLinkIDs ) : pRemovedLinkIDs( _pRemovedLinkIDs )	{	}
	//
	bool operator()( const SBattlePosition &rBattlePosition )
	{
		if ( rBattlePosition.nArtilleryLinkID != RMGC_INVALID_LINK_ID_VALUE )
		{
			if ( pRemovedLinkIDs->find( rBattlePosition.nArtilleryLinkID ) != pRemovedLinkIDs->end() )
			{
				return true;
			}
		}
		if ( rBattlePosition.nTruckLinkID != RMGC_INVALID_LINK_ID_VALUE )
		{
			if ( pRemovedLinkIDs->find( rBattlePosition.nTruckLinkID ) != pRemovedLinkIDs->end() )
			{
				return true;
			}
		}
		return false;
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMapInfo::RemoveNonExistingObjects( struct SLoadMapInfo *pLoadMapInfo, IDataStorage *pDataStorage, IObjectsDB *pObjectsDB, std::string *pszOutputString )
{
	NI_ASSERT_TF( ( pDataStorage != 0 ) && ( pObjectsDB != 0 ),
							  NStr::Format( "Wrong parameters: pDataStorage %x, pObjectsDB %x\n", pDataStorage, pObjectsDB ),
							  return false );

	bool bSomeRemoved = false;
	{
		CRemoveNonExistingVSOFunctor vsoFunctor( pDataStorage, pszOutputString );

		if ( pszOutputString )
		{
			( *pszOutputString ) += std::string( "Roads deleted:\r\n" );
		}
		//roads
		{
			TVSOList::iterator vsoIterator = std::remove_if( pLoadMapInfo->terrain.roads3.begin(), pLoadMapInfo->terrain.roads3.end(), CRemoveNonExistingVSOFunctorProxy( &vsoFunctor ) );
			if ( vsoIterator != pLoadMapInfo->terrain.roads3.end() )
			{
				pLoadMapInfo->terrain.roads3.erase( vsoIterator, pLoadMapInfo->terrain.roads3.end() );
				bSomeRemoved = true;
			}
		}
		if ( pszOutputString )
		{
			( *pszOutputString ) += std::string( "\r\n" );
			( *pszOutputString ) += std::string( "Rivers deleted:\r\n" );
		}
		//rivers
		{
			TVSOList::iterator vsoIterator = std::remove_if( pLoadMapInfo->terrain.rivers.begin(), pLoadMapInfo->terrain.rivers.end(), CRemoveNonExistingVSOFunctorProxy( &vsoFunctor ) );
			if ( vsoIterator != pLoadMapInfo->terrain.rivers.end() )
			{
				pLoadMapInfo->terrain.rivers.erase( vsoIterator, pLoadMapInfo->terrain.rivers.end() );
				bSomeRemoved = true;
			}
		}
		if ( pszOutputString )
		{
			( *pszOutputString ) += std::string( "\r\n" );
		}
	}

	//objects & scenarioObjects
	{
		if ( pszOutputString )
		{
			( *pszOutputString ) += std::string( "Objects deleted:\r\n" );
		}
		CRemoveNonExistingObjectsFunctor objectsFunctor( pDataStorage, pObjectsDB, pszOutputString );
		{
			std::vector<SMapObjectInfo>::iterator objectIterator = std::remove_if( pLoadMapInfo->objects.begin(), pLoadMapInfo->objects.end(), CRemoveNonExistingObjectsFunctorProxy( &objectsFunctor ) );
			if ( objectIterator != pLoadMapInfo->objects.end() )
			{
				pLoadMapInfo->objects.erase( objectIterator, pLoadMapInfo->objects.end() );
				bSomeRemoved = true;
			}
		}
		{
			std::vector<SMapObjectInfo>::iterator objectIterator = std::remove_if( pLoadMapInfo->scenarioObjects.begin(), pLoadMapInfo->scenarioObjects.end(), CRemoveNonExistingObjectsFunctorProxy( &objectsFunctor ) );
			if ( objectIterator != pLoadMapInfo->scenarioObjects.end() )
			{
				pLoadMapInfo->scenarioObjects.erase( objectIterator, pLoadMapInfo->scenarioObjects.end() );
				bSomeRemoved = true;
			}
		}

		if ( !objectsFunctor.removedLinkIDs.empty() )
		{
			bSomeRemoved = true;
			//������� nLinkID � SMapObjectInfo.link.nLinkWith
			for ( int nObjectIndex = 0; nObjectIndex < pLoadMapInfo->objects.size(); ++nObjectIndex )
			{
				if ( ( pLoadMapInfo->objects[nObjectIndex].link.nLinkWith != RMGC_INVALID_LINK_ID_VALUE ) &&
						 ( objectsFunctor.removedLinkIDs.find( pLoadMapInfo->objects[nObjectIndex].link.nLinkWith ) != objectsFunctor.removedLinkIDs.end() ) )
				{
					pLoadMapInfo->objects[nObjectIndex].link.nLinkWith = RMGC_INVALID_LINK_ID_VALUE;
				}
			}
			for ( int nObjectIndex = 0; nObjectIndex < pLoadMapInfo->scenarioObjects.size(); ++nObjectIndex )
			{
				if ( ( pLoadMapInfo->scenarioObjects[nObjectIndex].link.nLinkWith != RMGC_INVALID_LINK_ID_VALUE ) &&
						 ( objectsFunctor.removedLinkIDs.find( pLoadMapInfo->scenarioObjects[nObjectIndex].link.nLinkWith ) != objectsFunctor.removedLinkIDs.end() ) )
				{
					pLoadMapInfo->scenarioObjects[nObjectIndex].link.nLinkWith = RMGC_INVALID_LINK_ID_VALUE;
				}
			}
	
			//������� entrenchments
			{
				CRemoveNonExistingEntrenchmentsFunctor entrenchmentsFunctor( &( objectsFunctor.removedLinkIDs ) );
				std::vector<SEntrenchmentInfo>::iterator entrenchmentIterator = std::remove_if( pLoadMapInfo->entrenchments.begin(), pLoadMapInfo->entrenchments.end(), entrenchmentsFunctor );
				if ( entrenchmentIterator != pLoadMapInfo->entrenchments.end() )
				{
					pLoadMapInfo->entrenchments.erase( entrenchmentIterator, pLoadMapInfo->entrenchments.end() );
				}
			}

			//������� bridges
			{
				CRemoveNonExistingBridgesFunctor bridgeFunctor( &( objectsFunctor.removedLinkIDs ) );
				std::vector<std::vector<int> >::iterator bridgeIterator = std::remove_if( pLoadMapInfo->bridges.begin(), pLoadMapInfo->bridges.end(), bridgeFunctor );
				if ( bridgeIterator != pLoadMapInfo->bridges.end() )
				{
					pLoadMapInfo->bridges.erase( bridgeIterator, pLoadMapInfo->bridges.end() );
				}
			}

			//������� startCommandsList
			{
				CRemoveNonExistingStartCommandFunctor startCommandFunctor( &( objectsFunctor.removedLinkIDs ) );
				SLoadMapInfo::TStartCommandsList::iterator startCommandIterator = std::remove_if( pLoadMapInfo->startCommandsList.begin(), pLoadMapInfo->startCommandsList.end(), startCommandFunctor );
				if ( startCommandIterator != pLoadMapInfo->startCommandsList.end() )
				{
					pLoadMapInfo->startCommandsList.erase( startCommandIterator, pLoadMapInfo->startCommandsList.end() );
				}
			}

			//������� reservePositionsList
			{
				CRemoveNonExistingReservePositionFunctor reservePositionFunctor( &( objectsFunctor.removedLinkIDs ) );
				SLoadMapInfo::TReservePositionsList::iterator reservePositionIterator = std::remove_if( pLoadMapInfo->reservePositionsList.begin(), pLoadMapInfo->reservePositionsList.end(), reservePositionFunctor );
				if ( reservePositionIterator != pLoadMapInfo->reservePositionsList.end() )
				{
					pLoadMapInfo->reservePositionsList.erase( reservePositionIterator, pLoadMapInfo->reservePositionsList.end() );
				}
			}
		}
	}
	return bSomeRemoved;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMapInfo::TerrainHitTest( const STerrainInfo &rTerrainInfo, const CVec3 &rPoint, TERRAIN_HIT_TEST_TYPE type, std::vector<int> *pTerrainObjects )
{
	NI_ASSERT_TF( pTerrainObjects != 0,
							  NStr::Format( "Wrong parameter: %x\n", pTerrainObjects ),
							  return false );

	pTerrainObjects->clear();
	switch( type )
	{
		case THT_ROADS3D:
		{
			for ( int nRoad3DIndex = 0; nRoad3DIndex < rTerrainInfo.roads3.size(); ++nRoad3DIndex )
			{
				std::vector<CVec3> boundingPolygon;
				GetBoundingPolygon( rTerrainInfo.roads3[nRoad3DIndex], &boundingPolygon );
				if ( ClassifyPolygon( boundingPolygon, rPoint ) != CP_OUTSIDE )
				{ 
					pTerrainObjects->push_back( nRoad3DIndex );
				}
			}
			break;
		}
		case THT_RIVERS:
		{
			for ( int nRiverIndex = 0; nRiverIndex < rTerrainInfo.rivers.size(); ++nRiverIndex )
			{
				std::vector<CVec3> boundingPolygon;
				GetBoundingPolygon( rTerrainInfo.rivers[nRiverIndex], &boundingPolygon );
				if ( ClassifyPolygon( boundingPolygon, rPoint ) != CP_OUTSIDE )
				{ 
					pTerrainObjects->push_back( nRiverIndex );
				}
			}
			break;
		}
		default:
		{
			break;
		}
	}
	return true;
}

const SVectorStripeObject* CMapInfo::GetRiver( const STerrainInfo &rTerrainInfo, int nID )
{
	for ( int nRiverIndex = 0; nRiverIndex < rTerrainInfo.rivers.size(); ++nRiverIndex )
	{
		if ( rTerrainInfo.rivers[nRiverIndex].nID == nID )
		{
			return &( rTerrainInfo.rivers[nRiverIndex] );
		}
	}
	return 0;
}

const SVectorStripeObject* CMapInfo::GetRoad3D( const STerrainInfo &rTerrainInfo, int nID )
{
	for ( int nRoad3DIndex = 0; nRoad3DIndex < rTerrainInfo.roads3.size(); ++nRoad3DIndex )
	{
		if ( rTerrainInfo.roads3[nRoad3DIndex].nID == nID )
		{
			return &( rTerrainInfo.roads3[nRoad3DIndex] );
		}
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMapInfo::GetScenarioObjects( const std::string &rszMapInfoFileName, std::vector<SMapObjectInfo> *pMapObjects )
{
	CPtr<IDataStorage> pDataStorage = GetSingleton<IDataStorage>();
	if ( !pDataStorage )
	{
		return false;
	}
	NI_ASSERT_TF( pMapObjects != 0,
							  NStr::Format( "GetScenarioObjects, Wrong parameter pMapObjects: %x\n", pMapObjects ),
							  return false );

	const std::string szMapInfoFileName = rszMapInfoFileName.substr( 0, rszMapInfoFileName.rfind( '.' ) );
	// load map info
	CMapInfo mapinfo;
	SStorageElementStats statsXML, statsBZM;
	{
		// get stats from XML and BZM files
		Zero( statsXML );
		const bool bHasXML = pDataStorage->GetStreamStats( ( szMapInfoFileName + ".xml" ).c_str(), &statsXML );
		Zero( statsBZM );
		const bool bHasBZM = pDataStorage->GetStreamStats( ( szMapInfoFileName + ".bzm" ).c_str(), &statsBZM );
		NI_ASSERT_TF( bHasXML || bHasBZM, NStr::Format("Can't load neither XML nor BZM map \"%s\" to get scenario objects - check mission stats resources", szMapInfoFileName.c_str()), return false );
	}		
	// load newest one
	if ( statsXML.mtime > statsBZM.mtime ) 
	{
		LoadDataResource( szMapInfoFileName, ".bzm", false, 1, "ScenarioObjects", ( *pMapObjects ) );
	}
	else
	{
		CMapInfo mapInfo;
		if ( !LoadTypedSuperDataResource( szMapInfoFileName, ".bzm", true, 1, mapInfo ) )
		{
			return false;
		}
		( *pMapObjects ) = mapInfo.scenarioObjects;
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMapInfo::GetUsedLinkIDs( const SLoadMapInfo &rLoadMapInfo, CUsedLinkIDs *pUsedLinkIDs )
{
	NI_ASSERT_TF( pUsedLinkIDs != 0,
							  NStr::Format( "GetUsedLinkIDs, Wrong parameter pUsedLinkIDs: %x\n", pUsedLinkIDs ),
							  return false );
	
	for ( int nObjectIndex = 0; nObjectIndex < rLoadMapInfo.objects.size(); ++nObjectIndex )
	{
		if ( rLoadMapInfo.objects[nObjectIndex].link.nLinkWith != RMGC_INVALID_LINK_ID_VALUE )
		{
			pUsedLinkIDs->insert( rLoadMapInfo.objects[nObjectIndex].link.nLinkWith );
		}
	}
	for ( int nObjectIndex = 0; nObjectIndex < rLoadMapInfo.scenarioObjects.size(); ++nObjectIndex )
	{
		if ( rLoadMapInfo.scenarioObjects[nObjectIndex].link.nLinkWith != RMGC_INVALID_LINK_ID_VALUE )
		{
			pUsedLinkIDs->insert( rLoadMapInfo.scenarioObjects[nObjectIndex].link.nLinkWith );
		}
	}

	for ( int nEntrenchmentIndex = 0; nEntrenchmentIndex < rLoadMapInfo.entrenchments.size(); ++nEntrenchmentIndex )
	{
		for ( int nSectionIndex = 0; nSectionIndex < rLoadMapInfo.entrenchments[nEntrenchmentIndex].sections.size(); ++nSectionIndex )
		{
			for ( int nTSegmentIndex = 0; nTSegmentIndex < rLoadMapInfo.entrenchments[nEntrenchmentIndex].sections[nSectionIndex].size(); ++nTSegmentIndex )
			{
				if ( rLoadMapInfo.entrenchments[nEntrenchmentIndex].sections[nSectionIndex][nTSegmentIndex] != RMGC_INVALID_LINK_ID_VALUE )
				{
					pUsedLinkIDs->insert( rLoadMapInfo.entrenchments[nEntrenchmentIndex].sections[nSectionIndex][nTSegmentIndex] );
				}
			}
		}
	}

	for ( int nBrigeIndex = 0; nBrigeIndex < rLoadMapInfo.bridges.size(); ++nBrigeIndex )
	{
		for ( int nBrigeElementIndex = 0; nBrigeElementIndex < rLoadMapInfo.bridges[nBrigeIndex].size(); ++nBrigeElementIndex )
		{
			if ( rLoadMapInfo.bridges[nBrigeIndex][nBrigeElementIndex] != RMGC_INVALID_LINK_ID_VALUE )
			{
				pUsedLinkIDs->insert( rLoadMapInfo.bridges[nBrigeIndex][nBrigeElementIndex] );
			}
		}
	}

	for ( SLoadMapInfo::TStartCommandsList::const_iterator it = rLoadMapInfo.startCommandsList.begin();
			it != rLoadMapInfo.startCommandsList.end();
			++it )
	{
		for( int nLinkIDIndex = 0; nLinkIDIndex < it->unitLinkIDs.size(); ++nLinkIDIndex )
		{
			if ( it->unitLinkIDs[nLinkIDIndex] != RMGC_INVALID_LINK_ID_VALUE )
			{
				pUsedLinkIDs->insert( it->unitLinkIDs[nLinkIDIndex] );
			}
		}
		if ( it->linkID != RMGC_INVALID_LINK_ID_VALUE )
		{
			pUsedLinkIDs->insert( it->linkID );
		}
	}

	for ( SLoadMapInfo::TReservePositionsList::const_iterator it = rLoadMapInfo.reservePositionsList.begin();
	      it != rLoadMapInfo.reservePositionsList.end();
				++it )
	{
		if ( it->nArtilleryLinkID != RMGC_INVALID_LINK_ID_VALUE )
		{
			pUsedLinkIDs->insert( it->nArtilleryLinkID );
		}
		if ( it->nTruckLinkID != RMGC_INVALID_LINK_ID_VALUE )
		{
			pUsedLinkIDs->insert( it->nTruckLinkID );
		}
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMapInfo::GetUsedScriptIDs( const SLoadMapInfo &rLoadMapInfo, CUsedScriptIDs *pUsedScriptIDs )
{
	NI_ASSERT_TF( pUsedScriptIDs != 0,
							  NStr::Format( "GetUsedScriptIDs, Wrong parameter pUsedScriptIDs: %x\n", pUsedScriptIDs ),
							  return false );
	
	for ( int nObjectIndex = 0; nObjectIndex < rLoadMapInfo.objects.size(); ++nObjectIndex )
	{
		if ( rLoadMapInfo.objects[nObjectIndex].nScriptID != RMGC_INVALID_SCRIPT_ID_VALUE )
		{
			pUsedScriptIDs->insert( rLoadMapInfo.objects[nObjectIndex].nScriptID );
		}
	}
	for ( int nObjectIndex = 0; nObjectIndex < rLoadMapInfo.scenarioObjects.size(); ++nObjectIndex )
	{
		if ( rLoadMapInfo.scenarioObjects[nObjectIndex].nScriptID != RMGC_INVALID_SCRIPT_ID_VALUE )
		{
			pUsedScriptIDs->insert( rLoadMapInfo.scenarioObjects[nObjectIndex].nScriptID );
		}
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMapInfo::GetUsedScriptAreas( const SLoadMapInfo &rLoadMapInfo, CUsedScriptAreas *pUsedScriptAreas )
{
	NI_ASSERT_TF( pUsedScriptAreas != 0,
							  NStr::Format( "GetUsedScriptAreas, Wrong parameter pUsedScripAreas: %x\n", pUsedScriptAreas ),
							  return false );

	for ( int nScriptAreaIndex = 0; nScriptAreaIndex < rLoadMapInfo.scriptAreas.size(); ++nScriptAreaIndex )
	{
		pUsedScriptAreas->insert( rLoadMapInfo.scriptAreas[nScriptAreaIndex].szName );
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// obsolete storage  
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
bool CMapInfo::UpdateTerrainRoads( STerrainInfo *pTerrainInfo, const CTRect<int> &rUpdateRect, const SRoadsetDesc &rRoadsetDesc )
{
	NI_ASSERT_TF( pTerrainInfo != 0,
							  NStr::Format( "Wrong parameter: %x\n", pTerrainInfo ),
							  return false );
	// ������ ��� ������ �� ���� ������
	for ( int nPatchXIndex = 0; nPatchXIndex < pTerrainInfo->patches.GetSizeX(); ++nPatchXIndex )
	{
		for ( int nPatchYIndex = 0; nPatchYIndex < pTerrainInfo->patches.GetSizeY() ; ++nPatchYIndex )
		{
			for ( int index = 0; index < nNumRoadTypes; ++index )
			{
				pTerrainInfo->patches[nPatchYIndex][nPatchXIndex].roads[index].clear();
			}
		}
	}

	if ( pTerrainInfo->roads.size() < 1 ) return true;
	//��������� ���������� ��������� �����
	for ( int index = 0; index < pTerrainInfo->roads.size(); ++index )
	{	
		pTerrainInfo->roads[index].rect.Normalize();
	}
	
	//���������� �����
	bool isChanged = true;
	while( isChanged )
	{
		isChanged = false;
		for ( int index = 0; index < ( pTerrainInfo->roads.size() - 1 ); ++index )
		{	
			for ( int innerIndex = ( index + 1 ); innerIndex < pTerrainInfo->roads.size(); )
			{	
				if ( //����������� ���������
						 ( pTerrainInfo->roads[index].nDir == pTerrainInfo->roads[innerIndex].nDir ) &&
						 //���� ���������
						 ( pTerrainInfo->roads[index].nType == pTerrainInfo->roads[innerIndex].nType ) &&
						 //������������
						 pTerrainInfo->roads[index].rect.IsIntersectEdges( pTerrainInfo->roads[innerIndex].rect ) &&
					   //���� ������ ������� ����� �� ������ � ��� ���� �� ����� ������ ������������ ����� ����
						 ( ( ( pTerrainInfo->roads[index].rect.Width() == pTerrainInfo->roads[innerIndex].rect.Width() ) &&
						 		 ( pTerrainInfo->roads[index].rect.minx == pTerrainInfo->roads[innerIndex].rect.minx ) ) ||
							 ( ( pTerrainInfo->roads[index].rect.Height() == pTerrainInfo->roads[innerIndex].rect.Height() ) &&
								 ( pTerrainInfo->roads[index].rect.miny == pTerrainInfo->roads[innerIndex].rect.miny ) ) ) )
				
				{
					pTerrainInfo->roads[index].rect.Union( pTerrainInfo->roads[innerIndex].rect );
					pTerrainInfo->roads.erase( pTerrainInfo->roads.begin() + innerIndex );
					//std::vector<SRoadItem>::iterator pos = pTerrainInfo->roads.begin();
					//std::advance( pos, innerIndex );
					//pTerrainInfo->roads.erase( pos );
					isChanged = true;
				}
				else
				{
					++innerIndex;
				}
			}
		}
	}

	//�������� ������� ����� �����
	CArray2D<DWORD> roadBitsArray[nNumRoadTypes];
	for ( int index = 0; index < nNumRoadTypes; ++index )
	{
		roadBitsArray[index].SetSizes( pTerrainInfo->tiles.GetSizeX(), pTerrainInfo->tiles.GetSizeY() );
		roadBitsArray[index].SetZero();
	}
	for ( int index = 0; index < pTerrainInfo->roads.size(); ++index )
	{
		if ( pTerrainInfo->roads[index].nDir == SRoadItem::HORIZONTAL )
		{
			if ( pTerrainInfo->roads[index].rect.Width() < 1 )
			{
				roadBitsArray[pTerrainInfo->roads[index].nType][pTerrainInfo->roads[index].rect.top][pTerrainInfo->roads[index].rect.left] |= ROAD_BITS[7];
				for ( int nYIndex = ( pTerrainInfo->roads[index].rect.top + 1 ); nYIndex < pTerrainInfo->roads[index].rect.bottom; ++nYIndex )
				{
					roadBitsArray[pTerrainInfo->roads[index].nType][nYIndex][pTerrainInfo->roads[index].rect.left] |= ROAD_BITS[8];	
				}
				roadBitsArray[pTerrainInfo->roads[index].nType][pTerrainInfo->roads[index].rect.bottom][pTerrainInfo->roads[index].rect.left] |= ROAD_BITS[3];
			}
			else
			{
				roadBitsArray[pTerrainInfo->roads[index].nType][pTerrainInfo->roads[index].rect.top][pTerrainInfo->roads[index].rect.left] |= ROAD_BITS[0];
				roadBitsArray[pTerrainInfo->roads[index].nType][pTerrainInfo->roads[index].rect.top][pTerrainInfo->roads[index].rect.right] |= ROAD_BITS[6];
				for ( int nYIndex = ( pTerrainInfo->roads[index].rect.top + 1 ); nYIndex < pTerrainInfo->roads[index].rect.bottom; ++nYIndex )
				{
					roadBitsArray[pTerrainInfo->roads[index].nType][nYIndex][pTerrainInfo->roads[index].rect.left] |= ROAD_BITS[1];	
					roadBitsArray[pTerrainInfo->roads[index].nType][nYIndex][pTerrainInfo->roads[index].rect.right] |= ROAD_BITS[5];	
				}
				roadBitsArray[pTerrainInfo->roads[index].nType][pTerrainInfo->roads[index].rect.bottom][pTerrainInfo->roads[index].rect.left] |= ROAD_BITS[2];
				roadBitsArray[pTerrainInfo->roads[index].nType][pTerrainInfo->roads[index].rect.bottom][pTerrainInfo->roads[index].rect.right] |= ROAD_BITS[4];
				if ( pTerrainInfo->roads[index].rect.Width() > 1 )
				{
					for ( int nXIndex = ( pTerrainInfo->roads[index].rect.left + 1 ); nXIndex < pTerrainInfo->roads[index].rect.right; ++nXIndex )
					{
						roadBitsArray[pTerrainInfo->roads[index].nType][pTerrainInfo->roads[index].rect.top][nXIndex] |= ROAD_BITS[7];
						for ( int nYIndex = ( pTerrainInfo->roads[index].rect.top + 1 ); nYIndex < pTerrainInfo->roads[index].rect.bottom; ++nYIndex )
						{
							roadBitsArray[pTerrainInfo->roads[index].nType][nYIndex][nXIndex] |= ROAD_BITS[8];	
						}
						roadBitsArray[pTerrainInfo->roads[index].nType][pTerrainInfo->roads[index].rect.bottom][nXIndex] |= ROAD_BITS[3];
					}
				}
			}
		}
		else if ( pTerrainInfo->roads[index].nDir == SRoadItem::VERTICAL )
		{
			if ( pTerrainInfo->roads[index].rect.Height() < 1 )
			{
				roadBitsArray[pTerrainInfo->roads[index].nType][pTerrainInfo->roads[index].rect.top][pTerrainInfo->roads[index].rect.left] |= ROAD_BITS[10];
				for ( int nXIndex = ( pTerrainInfo->roads[index].rect.left + 1 ); nXIndex < pTerrainInfo->roads[index].rect.right; ++nXIndex )
				{
					roadBitsArray[pTerrainInfo->roads[index].nType][pTerrainInfo->roads[index].rect.top][nXIndex] |= ROAD_BITS[17];	
				}
				roadBitsArray[pTerrainInfo->roads[index].nType][pTerrainInfo->roads[index].rect.bottom][pTerrainInfo->roads[index].rect.left] |= ROAD_BITS[14];
			}
			else
			{
				roadBitsArray[pTerrainInfo->roads[index].nType][pTerrainInfo->roads[index].rect.top][pTerrainInfo->roads[index].rect.left] |= ROAD_BITS[9];
				roadBitsArray[pTerrainInfo->roads[index].nType][pTerrainInfo->roads[index].rect.bottom][pTerrainInfo->roads[index].rect.left] |= ROAD_BITS[11];
				for ( int nXIndex = ( pTerrainInfo->roads[index].rect.left + 1 ); nXIndex < pTerrainInfo->roads[index].rect.right; ++nXIndex )
				{
					roadBitsArray[pTerrainInfo->roads[index].nType][pTerrainInfo->roads[index].rect.top][nXIndex] |= ROAD_BITS[16];
					roadBitsArray[pTerrainInfo->roads[index].nType][pTerrainInfo->roads[index].rect.bottom][nXIndex] |= ROAD_BITS[12];
				}
				roadBitsArray[pTerrainInfo->roads[index].nType][pTerrainInfo->roads[index].rect.top][pTerrainInfo->roads[index].rect.right] |= ROAD_BITS[15];
				roadBitsArray[pTerrainInfo->roads[index].nType][pTerrainInfo->roads[index].rect.bottom][pTerrainInfo->roads[index].rect.right] |= ROAD_BITS[13];
				if ( pTerrainInfo->roads[index].rect.Height() > 1 )
				{
					for ( int nYIndex = ( pTerrainInfo->roads[index].rect.top + 1 ); nYIndex < pTerrainInfo->roads[index].rect.bottom; ++nYIndex )
					{
						roadBitsArray[pTerrainInfo->roads[index].nType][nYIndex][pTerrainInfo->roads[index].rect.left] |= ROAD_BITS[10];
						for ( int nXIndex = ( pTerrainInfo->roads[index].rect.left + 1 ); nXIndex < pTerrainInfo->roads[index].rect.right; ++nXIndex )
						{
							roadBitsArray[pTerrainInfo->roads[index].nType][nYIndex][nXIndex] |= ROAD_BITS[17];	
						}
						roadBitsArray[pTerrainInfo->roads[index].nType][nYIndex][pTerrainInfo->roads[index].rect.right] |= ROAD_BITS[14];
					}
				}
			}
		}
	}
	//���������� ���������� �� ������� ����� ������ �� ��� ���������
	//���� ������ ����� ����� �����, ������ - �������
	for ( int nRoadIndex = 0; nRoadIndex < nNumRoadTypes; ++nRoadIndex )
	{
		for ( int nPatchXIndex = 0; nPatchXIndex < pTerrainInfo->patches.GetSizeX(); ++nPatchXIndex )
		{
			for ( int nPatchYIndex = 0; nPatchYIndex < pTerrainInfo->patches.GetSizeY() ; ++nPatchYIndex )
			{
				for ( int nXIndex = nPatchXIndex * STerrainPatchInfo::nSizeX; nXIndex < ( nPatchXIndex + 1 ) * STerrainPatchInfo::nSizeX; ++nXIndex )
				{
					for ( int nYIndex = nPatchYIndex * STerrainPatchInfo::nSizeY; nYIndex < ( nPatchYIndex + 1 ) * STerrainPatchInfo::nSizeY; ++nYIndex )
					{
						if ( roadBitsArray[nRoadIndex][nYIndex][nXIndex] > 0 )
						{
							int nValidBitMask = -1;
							for ( int nBitMaskIndex = 0; nBitMaskIndex < ROAD_CROSS_BITS_DIMENSION; ++nBitMaskIndex )
							{
								for ( int nBitMaskCaseIndex = 0; nBitMaskCaseIndex < ROAD_CROSS_BITS_CASES; ++nBitMaskCaseIndex )
								{
									if ( ROAD_CROSS_BITS[nBitMaskIndex][nBitMaskCaseIndex] == ROAD_BITS_NULL_VALUE ) break;
									if ( ( roadBitsArray[nRoadIndex][nYIndex][nXIndex] & ROAD_CROSS_BITS[nBitMaskIndex][nBitMaskCaseIndex] ) == ROAD_CROSS_BITS[nBitMaskIndex][nBitMaskCaseIndex] )
									{
										nValidBitMask = nBitMaskIndex;
										break;
									}
								}
								if ( nValidBitMask >= 0 )
								{
									break;
								}
							}
							if ( nValidBitMask >= 0 )
							{
								//���� ��� ���� ���������� ������ �� ���� �����, �� ��������� ���������� ����:
								int index = -1;
								if ( ( nXIndex == 0 ) || 
										 ( nXIndex == ( pTerrainInfo->tiles.GetSizeX() - 1 ) ) ||
										 ( nYIndex == 0 ) || 
										 ( nYIndex == ( pTerrainInfo->tiles.GetSizeY() - 1 ) ) ) 
								{
									index = rRoadsetDesc.roads[nRoadIndex].tiles[ ROAD_EDGE_MAP_CROSS_TILE_INDICES[nValidBitMask] ].GetMapsIndex();
								}
								else
								{
									index = rRoadsetDesc.roads[nRoadIndex].tiles[ ROAD_CROSS_TILE_INDICES[nValidBitMask] ].GetMapsIndex();
								}
								pTerrainInfo->patches[nPatchYIndex][nPatchXIndex].roads[nRoadIndex].push_back( SRoadTileInfo( nXIndex - ( nPatchXIndex * STerrainPatchInfo::nSizeX ), nYIndex - ( nPatchYIndex * STerrainPatchInfo::nSizeY ), index ) );
							}
						}
					}
				}
			}
		}
	}
	return true;
}
/**/
/**
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMapInfo::AddRoad( SLoadMapInfo *pLoadMapInfo, const CTRect<int> &rRoadRect, int nRoadType, int nRoadDirection, std::vector<SRoadItem> *pRoad )
{
	NI_ASSERT_TF( pLoadMapInfo != 0,
							  NStr::Format( "Wrong parameter: %x\n", pLoadMapInfo ),
							  return false );

	SRoadItem roadItem;
	//����������� � ����������� � ������� �������� ����� ��������
	roadItem.rect = CTRect<int>( rRoadRect.minx,
															 pLoadMapInfo->terrain.tiles.GetSizeY() - rRoadRect.miny - 1,
															 rRoadRect.maxx,
															 pLoadMapInfo->terrain.tiles.GetSizeY() - rRoadRect.maxy - 1 );
	roadItem.rect.Normalize();
	//���������� ������ ���� ��� ������� ����� �� ������
	if ( ( roadItem.rect.minx >= pLoadMapInfo->terrain.tiles.GetSizeX() ) ||
			 ( roadItem.rect.maxx < 0 ) ||
			 ( roadItem.rect.miny >= pLoadMapInfo->terrain.tiles.GetSizeY() ) ||
			 ( roadItem.rect.maxy < 0 ) )
	{
		return false;
	}
	//��������� ������ �� ���� �����
	if ( roadItem.rect.minx < 0 )
	{
		roadItem.rect.minx = 0;
	}
	if ( roadItem.rect.maxx >= pLoadMapInfo->terrain.tiles.GetSizeX() )
	{
		roadItem.rect.maxx = pLoadMapInfo->terrain.tiles.GetSizeX() - 1;
	}
	if ( roadItem.rect.miny < 0 )
	{
		roadItem.rect.miny = 0;
	}
	if ( roadItem.rect.maxy >= pLoadMapInfo->terrain.tiles.GetSizeY() )
	{
		roadItem.rect.maxy = pLoadMapInfo->terrain.tiles.GetSizeY() - 1;
	}

	//������� ����������� ������� � ����� ��� �������

	//REMOVE_OBJECTS_FROM_RECT
/**/
	/**
	CTRect<int> roadRect( roadItem.rect.minx,
												pLoadMapInfo->terrain.tiles.GetSizeY() - roadItem.rect.miny - 1,
												roadItem.rect.maxx,
												pLoadMapInfo->terrain.tiles.GetSizeY() - roadItem.rect.maxy - 1 );
	roadRect.Normalize();
	std::vector<CVec3>	roadPolygon;
	roadPolygon.push_back( CVec3( ( roadRect.minx + 0 ) * fWorldCellSize, ( roadRect.miny + 0 ) * fWorldCellSize, 0.0f ) );
	roadPolygon.push_back( CVec3( ( roadRect.minx + 0 ) * fWorldCellSize, ( roadRect.maxy + 1 ) * fWorldCellSize, 0.0f ) );
	roadPolygon.push_back( CVec3( ( roadRect.maxx + 1 ) * fWorldCellSize, ( roadRect.maxy + 1 ) * fWorldCellSize, 0.0f ) );
	roadPolygon.push_back( CVec3( ( roadRect.maxx + 1 ) * fWorldCellSize, ( roadRect.miny + 0 ) * fWorldCellSize, 0.0f ) );
	if ( !RemoveObjects( pLoadMapInfo, roadPolygon ) )
	{
		return false;
	}
	/**/
/**
	roadItem.nType = nRoadType;
	roadItem.nDir = nRoadDirection;
	pLoadMapInfo->terrain.roads.push_back( roadItem );
	if ( pRoad )
	{
		pRoad->push_back( roadItem );
	}
	return true;
}
/**/
/**
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMapInfo::MakeRoad( SLoadMapInfo *pLoadMapInfo, const SRoadPoint &rFrom, const SRoadPoint &rTo, int nRoadType, const SRoadMakeParameter &rRoadMakeParamerer, std::vector<SRoadItem> *pRoad )
{
	NI_ASSERT_TF( pLoadMapInfo != 0,
							  NStr::Format( "Wrong parameter: %x", pLoadMapInfo ),
							  return false );
	NI_ASSERT_T( ( rFrom.nDirection >= RMGC_HORIZONTAL_TO_ZERO ) &&
							 ( rFrom.nDirection <= RMGC_VERTICAL_FROM_ZERO ),
							 NStr::Format( "Road point direction not set!" ) );
	NI_ASSERT_T( ( rTo.nDirection >= RMGC_HORIZONTAL_TO_ZERO ) &&
							 ( rTo.nDirection <= RMGC_VERTICAL_FROM_ZERO ),
							 NStr::Format( "Road point direction not set!" ) );
	NI_ASSERT_T( ( rFrom.nWidth > SRoadPoint::INVALID_WIDTH ) &&
							 ( rFrom.nWidth == rTo.nWidth ),
							 NStr::Format( "Invalid Road width!" ) );

	//�������������� ��� ����������� ������
	std::vector<CTRect<int> > rectsToLock;
	for ( int nRectIndex = 0; nRectIndex < rRoadMakeParamerer.lockedRects.size(); ++nRectIndex )
	{
		rectsToLock.push_back( rRoadMakeParamerer.lockedRects[nRectIndex] );
	}
	rectsToLock.push_back( CTRect<int>( rFrom, rFrom ) );
	rectsToLock.push_back( CTRect<int>( rTo, rTo ) );
	
	//������ ������ ��� ������ ����
	CArray2D<BYTE> tileMap( pLoadMapInfo->terrain.tiles.GetSizeX(), pLoadMapInfo->terrain.tiles.GetSizeY() );
	tileMap.Set( RMGC_UNLOCKED );
	ModifyTilesFunctional<CArray2D<BYTE>, BYTE> tileMapModifyTiles( RMGC_LOCKED, &tileMap );
	CTRect<int>						tileMapRect( 0, 0, tileMap.GetSizeX(), tileMap.GetSizeY() );

	//������ �����
	for ( int nRectIndex = 0; nRectIndex < rectsToLock.size(); ++nRectIndex )
	{
		rectsToLock[nRectIndex].Normalize();
		rectsToLock[nRectIndex].minx -= ( rFrom.GetMajorWidth() + rRoadMakeParamerer.nMinMiddleDistance );
		rectsToLock[nRectIndex].maxx += ( rFrom.GetMinorWidth() + rRoadMakeParamerer.nMinMiddleDistance );
		rectsToLock[nRectIndex].miny -= ( rFrom.GetMajorWidth() + rRoadMakeParamerer.nMinMiddleDistance );
		rectsToLock[nRectIndex].maxy += ( rFrom.GetMinorWidth() + rRoadMakeParamerer.nMinMiddleDistance );
		
		ApplyTilesInRange( tileMapRect, rectsToLock[nRectIndex], tileMapModifyTiles );
	}
	CTPoint<int> shiftPoints[4] = { RMGC_SHIFT_POINTS[0] * ( rFrom.GetMajorWidth() + rRoadMakeParamerer.nMinMiddleDistance ),
																	RMGC_SHIFT_POINTS[1] * ( rFrom.GetMinorWidth() + rRoadMakeParamerer.nMinMiddleDistance ),
																	RMGC_SHIFT_POINTS[2] * ( rFrom.GetMajorWidth() + rRoadMakeParamerer.nMinMiddleDistance ),
																	RMGC_SHIFT_POINTS[3] * ( rFrom.GetMinorWidth() + rRoadMakeParamerer.nMinMiddleDistance ) };

	CTPoint<int> startPoint = rFrom + shiftPoints[rFrom.nDirection - RMGC_HORIZONTAL_TO_ZERO];
	CTPoint<int> finishPoint = rTo + shiftPoints[rTo.nDirection - RMGC_HORIZONTAL_TO_ZERO];

	if ( ( !IsValidIndices( tileMap, startPoint ) ) ||
			 ( !IsValidIndices( tileMap, finishPoint ) ) )
	{
		return false;
	}
	
	std::vector<CTPoint<int> > roadPoints;

	roadPoints.push_back( rFrom );
	int nRoadAdded = FindPath( startPoint, finishPoint, &tileMap, &roadPoints );
	roadPoints.push_back( rTo );

	if ( nRoadAdded == 0 )
	{
		return false;
	}

	for ( int nRoadIndex = 0; nRoadIndex < ( roadPoints.size() - 1 ); ++nRoadIndex )
	{
		SRoadPoint from( roadPoints[nRoadIndex], RMGC_INVALID_DIRECTION, rFrom.nWidth, nRoadType );
		SRoadPoint to( roadPoints[nRoadIndex + 1], RMGC_INVALID_DIRECTION, rFrom.nWidth, nRoadType );
		CTRect<int> roadRect = from.GetCrossRect();
		roadRect.Union( to.GetCrossRect() );
		int nRoadDirection = ( from.x == to.x ) ? SRoadItem::VERTICAL : SRoadItem::HORIZONTAL;
		
		AddRoad( pLoadMapInfo, roadRect, nRoadType, nRoadDirection, pRoad );
	}
	return true;
}
/**/
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// basement storage  
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
				/**
				if ( isVertical )
				{
					int nXIndex =  bridgeRect.minx - 1;
					if ( ( nXIndex >= 0 ) && ( nXIndex < imageRect.maxx ) )
					{
						for ( int nYIndex = bridgeRect.miny; nYIndex <= bridgeRect.maxy; ++nYIndex )
						{
							if ( ( nYIndex >= 0 ) && ( nYIndex < imageRect.maxy ) )
							{
								bridgesImageAccessor[imageRect.maxy - nYIndex - 1][nXIndex] = rParameter.bridgeBordersColor;
							}
						}
					}
					nXIndex =  bridgeRect.maxx + 1;
					if ( ( nXIndex >= 0 ) && ( nXIndex < imageRect.maxx ) )
					{
						for ( int nYIndex = bridgeRect.miny; nYIndex <= bridgeRect.maxy; ++nYIndex )
						{
							if ( ( nYIndex >= 0 ) && ( nYIndex < imageRect.maxy ) )
							{
								bridgesImageAccessor[imageRect.maxy - nYIndex - 1][nXIndex] = rParameter.bridgeBordersColor;
							}
						}
					}
				}
				else
				{
					int nYIndex =  bridgeRect.miny - 1;
					if ( ( nYIndex >= 0 ) && ( nYIndex < imageRect.maxy ) )
					{
						for ( int nXIndex = bridgeRect.minx; nXIndex <= bridgeRect.maxx; ++nXIndex )
						{
							if ( ( nXIndex >= 0 ) && ( nXIndex < imageRect.maxx ) )
							{
								bridgesImageAccessor[imageRect.maxy - nYIndex - 1][nXIndex] = rParameter.bridgeBordersColor;
							}
						}
					}
					nYIndex =  bridgeRect.maxy + 1;
					if ( ( nYIndex >= 0 ) && ( nYIndex < imageRect.maxy ) )
					{
						for ( int nXIndex = bridgeRect.minx; nXIndex <= bridgeRect.maxx; ++nXIndex )
						{
							if ( ( nXIndex >= 0 ) && ( nXIndex < imageRect.maxx ) )
							{
								bridgesImageAccessor[imageRect.maxy - nYIndex - 1][nXIndex] = rParameter.bridgeBordersColor;
							}
						}
					}
				}
				/**/


/**

	{ SColor( 0xFF, 7, 122, 149 ),
														 SColor( 0xFF, 82, 141, 147 ),
														 SColor( 0xFF, 0, 0, 0xFF ) };

		SColor roadsColors[3][4] = {	{ SColor( 0xFF, 210, 175, 100 ),
																		SColor( 0xFF, 125, 125, 110 ),
																		SColor( 0xFF, 175, 175, 175 ),
																		SColor( 0xFF, 100, 100, 80 ) },
																	{ SColor( 0xFF, 201, 200, 164 ),
																		SColor( 0xFF, 192, 206, 202 ),
																		SColor( 0xFF, 175, 175, 175 ),
																		SColor( 0xFF, 142, 156, 147 ) },
																	{ SColor( 0xFF, 0x00, 0x00, 0x00 ),
																		SColor( 0xFF, 0x00, 0x00, 0x00 ),
																		SColor( 0xFF, 175, 175, 175 ),
																		SColor( 0xFF, 0x00, 0x00, 0x00 ) } };
		SColor forestsColors[3] = { SColor( 0xFF, 19, 75, 30 ),
																SColor( 0xFF, 131, 149, 154 ),
																SColor( 0xFF, 0, 0xFF, 0 ) };

		SColor buildingsColors[3] = { SColor( 0xFF, 0x00, 0x00, 0x00 ),
																	SColor( 0xFF, 0x00, 0x00, 0x00 ),
																	SColor( 0xFF, 0x00, 0x00, 0x00 ) };

		SColor bridgesColors[3] = { SColor( 0xFF, 0xFF, 0xFF, 0xFF ),
																SColor( 0xFF, 0xFF, 0xFF, 0xFF ),
																SColor( 0xFF, 0xFF, 0xFF, 0xFF ) };
		SColor bridgeBordersColors[3] = { SColor( 0xFF, 0x00, 0x00, 0x00 ),
																			SColor( 0xFF, 0x00, 0x00, 0x00 ),
																			SColor( 0xFF, 0x00, 0x00, 0x00 ) };
		int nWoodRadius = 4; 
/**/  
//DWORD dwTimer = GetTickCount();
	//dwTimer = GetTickCount() - dwTimer;
	//NStr::DebugTrace("CMapInfo::CreateMiniMapImage: create image: %d\n", dwTimer );
	//dwTimer = GetTickCount();
	/**
	SColor blackColor( 0xFF, 0, 0, 0 );
	SColor whilteColor( 0xFF, 0xFF, 0xFF, 0xFF );
	SColor whilteColor( 0xFF, 0xFF, 0xFF, 0xFF );
	SColor halfBlackColor( 0xFF, 0x7F, 0x7F, 0x7F );
	SColor halfWhiteColor( 0xFF, 0x80, 0x80, 0x80 );
	CTPoint<int> shiftPoint( 1, 1 );
	/**/


/**
	//int nErasedLinkID = pLoadMapInfo->objects[nObjectIndex].link.nLinkID;
	if ( nErasedLinkID != RMGC_INVALID_LINK_ID_VALUE )
	{
		//�������� ������� (link.nLinkWith)
		for ( int nInnerObjectIndex = 0; nInnerObjectIndex < pLoadMapInfo->objects.size(); ++nInnerObjectIndex )
		{
			if ( pLoadMapInfo->objects[nInnerObjectIndex].link.nLinkWith == nErasedLinkID )
			{
				pLoadMapInfo->objects[nInnerObjectIndex].link.nLinkWith = RMGC_INVALID_LINK_ID_VALUE;
				pLoadMapInfo->objects[nInnerObjectIndex].link.bIntention = true;
			}
		}

		//���������� ����� nLinkID � entrenchments
		for ( int nEntrenchmentIndex = 0; nEntrenchmentIndex < pLoadMapInfo->entrenchments.size(); )
		{
			for ( int nSectionIndex = 0; nSectionIndex < pLoadMapInfo->entrenchments[nEntrenchmentIndex].sections.size(); )
			{
				for ( int nTSegmentIndex = 0; nTSegmentIndex < pLoadMapInfo->entrenchments[nEntrenchmentIndex].sections[nSectionIndex].size(); )
				{
					if ( pLoadMapInfo->entrenchments[nEntrenchmentIndex].sections[nSectionIndex][nTSegmentIndex] == nErasedLinkID )
					{
						pLoadMapInfo->entrenchments[nEntrenchmentIndex].sections[nSectionIndex].erase(pLoadMapInfo->entrenchments[nEntrenchmentIndex].sections[nSectionIndex].begin() + nTSegmentIndex );
					}
					else
					{
						++nTSegmentIndex;
					}
				}
				if ( pLoadMapInfo->entrenchments[nEntrenchmentIndex].sections[nSectionIndex].empty() )
				{
					pLoadMapInfo->entrenchments[nEntrenchmentIndex].sections.erase(pLoadMapInfo->entrenchments[nEntrenchmentIndex].sections.begin() + nSectionIndex );
				}
				else
				{
					++nSectionIndex;
				}
			}
			if ( pLoadMapInfo->entrenchments[nEntrenchmentIndex].sections.empty() )
			{
				pLoadMapInfo->entrenchments.erase( pLoadMapInfo->entrenchments.begin() + nEntrenchmentIndex );
			}
			else
			{
				++nEntrenchmentIndex;
			}
		}

		//���������� ����� nLinkID � bridges
		for ( int nBrigeIndex = 0; nBrigeIndex < pLoadMapInfo->bridges.size(); )
		{
			for ( int nBrigeElementIndex = 0; nBrigeElementIndex < pLoadMapInfo->bridges[nBrigeIndex].size(); )
			{
				if ( pLoadMapInfo->bridges[nBrigeIndex][nBrigeElementIndex] == nErasedLinkID )
				{
					pLoadMapInfo->bridges[nBrigeIndex].erase( pLoadMapInfo->bridges[nBrigeIndex].begin() + nBrigeElementIndex );
				}
				else
				{
					 ++nBrigeElementIndex;
				}
			}
			if ( pLoadMapInfo->bridges[nBrigeIndex].empty() )
			{
				pLoadMapInfo->bridges.erase( pLoadMapInfo->bridges.begin() + nBrigeIndex );
			}
			else
			{
				 ++nBrigeIndex;
			}
		}

		//���������� ����� nLinkID � logics
		bool isElementErased = true;
		while ( isElementErased )
		{
			isElementErased = false;
			for ( SLoadMapInfo::TLogicsMap::iterator it = pLoadMapInfo->logics.begin(); it != pLoadMapInfo->logics.end(); ++it )
			{
				for ( int nLogicElementIndex = 0; nLogicElementIndex < it->second.size(); )
				{
					if ( it->second[nLogicElementIndex] == nErasedLinkID )
					{
						it->second.erase(it->second.begin() + nLogicElementIndex );
					}
					else
					{
						++nLogicElementIndex;
					}
				}
				if ( it->second.empty() )
				{
					pLoadMapInfo->logics.erase( it );
					isElementErased = true;
					break;
				}
			}
		}
	}
/**/
/**
	const int nDiplomaciesSize = 3;
	pLoadMapInfo->diplomacies.SetSizes( nDiplomaciesSize, nDiplomaciesSize );
	for ( int nXIndex = 0; nXIndex < ( nDiplomaciesSize - 1 ); ++nXIndex )
	{
		for ( int nYIndex = 0; nYIndex < ( nDiplomaciesSize - 1 ); ++nYIndex )
		{
			pLoadMapInfo->diplomacies[nYIndex][nXIndex] = EDI_ENEMY;
		}
	}
	for ( int index = 0; index < ( nDiplomaciesSize - 1 ); ++index )
	{
		pLoadMapInfo->diplomacies[index][nDiplomaciesSize - 1] = EDI_NEUTRAL;
		pLoadMapInfo->diplomacies[nDiplomaciesSize - 1][index] = EDI_NEUTRAL;
	}

	for ( int index = 0; index < nDiplomaciesSize; ++index )
	{
		pLoadMapInfo->diplomacies[index][index] = EDI_FRIEND;
	}
/**/

	/**
	// CRAP{ ����� ��� ������� diplomacies
	if ( saver.IsReading() )
	{
		if ( (  diplomacies.GetSizeX() == 0 ) || 
				 ( diplomacies.GetSizeY() == 0 ) )
		{
			const int nDiplomaciesSize = 3;
			diplomacies.SetSizes( nDiplomaciesSize, nDiplomaciesSize );
			for ( int nXIndex = 0; nXIndex < ( nDiplomaciesSize - 1 ); ++nXIndex )
			{
				for ( int nYIndex = 0; nYIndex < ( nDiplomaciesSize - 1 ); ++nYIndex )
				{
					diplomacies[nYIndex][nXIndex] = EDI_ENEMY;
				}
			}
			for ( int index = 0; index < ( nDiplomaciesSize - 1 ); ++index )
			{
				diplomacies[index][nDiplomaciesSize - 1] = EDI_NEUTRAL;
				diplomacies[nDiplomaciesSize - 1][index] = EDI_NEUTRAL;
			}

			for ( int index = 0; index < nDiplomaciesSize; ++index )
			{
				diplomacies[index][index] = EDI_FRIEND;
			}
		}
	}
	// CRAP}
	/**/

	/**
	// CRAP{ ����� ��� ������� diplomacies
	if ( saver.IsReading() )
	{
		if ( (  diplomacies.GetSizeX() == 0 ) || 
				 ( diplomacies.GetSizeY() == 0 ) )
		{
			const int nDiplomaciesSize = 3;
			diplomacies.SetSizes( nDiplomaciesSize, nDiplomaciesSize );
			for ( int nXIndex = 0; nXIndex < ( nDiplomaciesSize - 1 ); ++nXIndex )
			{
				for ( int nYIndex = 0; nYIndex < ( nDiplomaciesSize - 1 ); ++nYIndex )
				{
					diplomacies[nYIndex][nXIndex] = EDI_ENEMY;
				}
			}
			for ( int index = 0; index < ( nDiplomaciesSize - 1 ); ++index )
			{
				diplomacies[index][nDiplomaciesSize - 1] = EDI_NEUTRAL;
				diplomacies[nDiplomaciesSize - 1][index] = EDI_NEUTRAL;
			}

			for ( int index = 0; index < nDiplomaciesSize; ++index )
			{
				diplomacies[index][index] = EDI_FRIEND;
			}
		}
	}
	// CRAP}
	/**/
/**

	/**
	//���������� ������ ���������� ���������������
	std::vector<CTRect<int> > rectsToLock;
	for ( int nPatchIndex = 0; nPatchIndex < placedPatches.size(); ++nPatchIndex )
	{
		rectsToLock.push_back( CTRect<int>( placedPatches[nPatchIndex].minXYCorner.x,
																				placedPatches[nPatchIndex].minXYCorner.y,
																				placedPatches[nPatchIndex].minXYCorner.x + placedPatches[nPatchIndex].size.x * STerrainPatchInfo::nSizeX - 1,
																				placedPatches[nPatchIndex].minXYCorner.y + placedPatches[nPatchIndex].size.y * STerrainPatchInfo::nSizeY - 1 ) );
	}
	for ( int nRoadIndex = 0; nRoadIndex < roads.size(); ++nRoadIndex )
	{
		for ( int nRoadItemIndex = 0; nRoadItemIndex < roads[nRoadIndex].size(); ++nRoadItemIndex )
		{
			rectsToLock.push_back( CTRect<int>( roads[nRoadIndex][nRoadItemIndex].rect.minx,
																				  mapInfo.terrain.tiles.GetSizeY() - roads[nRoadIndex][nRoadItemIndex].rect.miny - 1,
																				  roads[nRoadIndex][nRoadItemIndex].rect.maxx,
																				  mapInfo.terrain.tiles.GetSizeY() - roads[nRoadIndex][nRoadItemIndex].rect.maxy - 1 ) );
		}
	}
	
	for ( int nAdditionalRectIndex = 0; nAdditionalRectIndex < ( rParameter.size.x * rParameter.size.y ); ++nAdditionalRectIndex )
	{
		CTPoint<int> start( rand() * ( mapInfo.terrain.tiles.GetSizeX() - rParameter.nEdge - 1 ) / ( RAND_MAX + 1 ),
												rand() * ( mapInfo.terrain.tiles.GetSizeY() - rParameter.nEdge - 1 ) / ( RAND_MAX + 1 ) );
		CTPoint<int> size( rParameter.nEdge, rParameter.nEdge );
/**
		CTPoint<int> start( rand() * ( mapInfo.terrain.tiles.GetSizeX() - STerrainPatchInfo::nSizeX ) / ( RAND_MAX + 1 ),
												rand() * ( mapInfo.terrain.tiles.GetSizeY() - STerrainPatchInfo::nSizeY ) / ( RAND_MAX + 1 ) );
		CTPoint<int> size( rand() * STerrainPatchInfo::nSizeX / ( 4 * ( RAND_MAX + 1 ) ), 
											 rand() * STerrainPatchInfo::nSizeY / ( 4 * ( RAND_MAX + 1 ) ) );
/**

		rectsToLock.push_back( CTRect<int>( start.x,
																				start.y,
																				start.x + size.x,
																				start.y + size.y ) );
	}

	//������ ��� ����������� ��������������
	CArray2D<BYTE> tileMap( mapInfo.terrain.tiles.GetSizeX() * 2, mapInfo.terrain.tiles.GetSizeY() * 2 );
	tileMap.Set( RMGC_UNLOCKED );
	ModifyTilesFunctional tileMapModifyTiles( RMGC_LOCKED, &tileMap );
	CTRect<int>						tileMapRect( 0, 0, tileMap.GetSizeX(), tileMap.GetSizeY() );

	for ( int nRectIndex = 0; nRectIndex < rectsToLock.size(); ++nRectIndex )
	{
		rectsToLock[nRectIndex].Normalize();

		
		/**
		//�������� ������� �����
		std::vector<CVec3>	mapPolygon;
		mapPolygon.push_back( CVec3( ( rectsToLock[nRectIndex].minx + 0 ) * fWorldCellSize, ( rectsToLock[nRectIndex].miny + 0 ) * fWorldCellSize, 0.0f ) );
		mapPolygon.push_back( CVec3( ( rectsToLock[nRectIndex].minx + 0 ) * fWorldCellSize, ( rectsToLock[nRectIndex].maxy + 1 ) * fWorldCellSize, 0.0f ) );
		mapPolygon.push_back( CVec3( ( rectsToLock[nRectIndex].maxx + 1 ) * fWorldCellSize, ( rectsToLock[nRectIndex].maxy + 1 ) * fWorldCellSize, 0.0f ) );
		mapPolygon.push_back( CVec3( ( rectsToLock[nRectIndex].maxx + 1 ) * fWorldCellSize, ( rectsToLock[nRectIndex].miny + 0 ) * fWorldCellSize, 0.0f ) );

		//�������� ������� ������
		mapInfo.FillTerrainTiles( mapPolygon, rParameter.nPlainTerrainIndex, false );
		/**
		CTRect<int> rectToLock( rectsToLock[nRectIndex].minx * 2,
			                      rectsToLock[nRectIndex].miny * 2,
														rectsToLock[nRectIndex].maxx * 2,
														rectsToLock[nRectIndex].maxy * 2 );
		ApplyTilesInRange( tileMapRect, rectToLock, tileMapModifyTiles );
	}
	
	//dwTimer = GetTickCount() - dwTimer;
	//NStr::DebugTrace("CMapInfo::CreateRandomMap: lock rects: %d\n", dwTimer );
	//dwTimer = GetTickCount();

	//������ ����
	TForestHashMap forests;
	forests.clear();
	{
		std::string szFileName = rszTemplateSubFolder + "forest.xml";
		CPtr<IDataStream> pStream = pDataStorage->OpenStream( szFileName .c_str(), STREAM_ACCESS_READ );
		if ( !pStream )
		{
			return false;
		}
		CTreeAccessor tree = CreateDataTreeSaver( pStream, IDataTree::READ );
		tree.Add( "forests", &forests );
	}

	//�������� ����� ����, ������� ���������� �����
	int nForestRandomIndex = ( rand() * forests.size() ) / ( RAND_MAX + 1 );
	TForestHashMap::iterator forestRandomIteratorPosition = forests.begin();
	for ( int nIndex = 0; nIndex < nForestRandomIndex; ++nIndex )
	{
		++forestRandomIteratorPosition;
	}

	//dwTimer = GetTickCount() - dwTimer;
	//NStr::DebugTrace("CMapInfo::CreateRandomMap: load forests: %d\n", dwTimer );
	//dwTimer = GetTickCount();
	
	//���������� ��� ��������� �����
	int nEdgesAdded = 0;
	int nHeartsAdded = 0;
	//mapInfo.FillObjectSet( mapPolygon, forestRandomIteratorPosition->second, rParameter.nRatio, rParameter.nEdge, rParameter.nRadius, &tileMap, &nEdgesAdded, &nHeartsAdded );
	
	//dwTimer = GetTickCount() - dwTimer;
	//NStr::DebugTrace("CMapInfo::CreateRandomMap: place forest: %d ( heart: %d, edge: %d )\n", dwTimer, nHeartsAdded, nEdgesAdded );
	//dwTimer = GetTickCount();

	//�������� �����
	mapInfo.UpdateTerrain();

	//dwTimer = GetTickCount() - dwTimer;
	//NStr::DebugTrace("CMapInfo::CreateRandomMap: update terrain: %d\n", dwTimer );
	//dwTimer = GetTickCount();

	mapInfo.szScriptFile = rszRandomMapName.substr( 0, rszRandomMapName.rfind( '.' ) );

	//��������� �����
	{
		std::string szFileName = pDataStorage->GetName() + rszRandomMapName;
		CPtr<IDataStream> pStream = CreateFileStream( szFileName .c_str(), STREAM_ACCESS_WRITE );
		CTreeAccessor saver = CreateDataTreeSaver( pStream, IDataTree::WRITE );
		saver.AddTypedSuper( &mapInfo );
	}

	//dwTimer = GetTickCount() - dwTimer;
	//NStr::DebugTrace("CMapInfo::CreateRandomMap: save map: %d\n", dwTimer );

/**/
			//�������� ��� ������ ����� �� �����
			/**
			std::vector<SRoadPoint> roadPoints;
			for ( int nPatchRoadIndex = 0; nPatchRoadIndex < patchInfo.terrain.roads.size(); ++nPatchRoadIndex )
			{
				//������ ��� ���������� ������ � ���������� SRoadPoint'��
				CTRect<int> roadRect( patchInfo.terrain.roads[nPatchRoadIndex].rect.minx,
					                    patchInfo.terrain.tiles.GetSizeY() - patchInfo.terrain.roads[nPatchRoadIndex].rect.miny - 1,
															patchInfo.terrain.roads[nPatchRoadIndex].rect.maxx,
					                    patchInfo.terrain.tiles.GetSizeY() - patchInfo.terrain.roads[nPatchRoadIndex].rect.maxy - 1 );
				roadRect.Normalize();
				//��������������� SRoadPoint'� ����������� �� ����� ������
				if ( patchInfo.terrain.roads[nPatchRoadIndex].nDir == SRoadItem::HORIZONTAL )
				{
					if ( roadRect.minx == 0 )
					{
						roadPoints.push_back( SRoadPoint( placedPatches[nPlacedPatchIndex].minXYCorner.x + roadRect.minx,
																							placedPatches[nPlacedPatchIndex].minXYCorner.y + roadRect.miny + ( roadRect.Height() / 2 ),
																							RMGC_HORIZONTAL_TO_ZERO,
																							roadRect.Height() + 1,
																							patchInfo.terrain.roads[nPatchRoadIndex].nType ) );
					}	
					if ( roadRect.maxx == ( patchInfo.terrain.tiles.GetSizeX() - 1 ) )
					{
						roadPoints.push_back( SRoadPoint( placedPatches[nPlacedPatchIndex].minXYCorner.x + roadRect.maxx,
																							placedPatches[nPlacedPatchIndex].minXYCorner.y + roadRect.miny + ( roadRect.Height() / 2 ),
																							RMGC_HORIZONTAL_FROM_ZERO,
																							roadRect.Height() + 1,
																							patchInfo.terrain.roads[nPatchRoadIndex].nType ) );
					}
				}
				else
				{
					if ( roadRect.miny == 0 )
					{
						roadPoints.push_back( SRoadPoint( placedPatches[nPlacedPatchIndex].minXYCorner.x + roadRect.minx + ( roadRect.Width() / 2),
																							placedPatches[nPlacedPatchIndex].minXYCorner.y + roadRect.miny,
																							RMGC_VERTICAL_TO_ZERO,
																							roadRect.Width() + 1,
																							patchInfo.terrain.roads[nPatchRoadIndex].nType ) );
						
					}
					if ( roadRect.maxy == ( patchInfo.terrain.tiles.GetSizeY() - 1 ) )
					{
						roadPoints.push_back( SRoadPoint( placedPatches[nPlacedPatchIndex].minXYCorner.x + roadRect.minx + ( roadRect.Width() / 2),
																							placedPatches[nPlacedPatchIndex].minXYCorner.y + roadRect.maxy,
																							RMGC_VERTICAL_FROM_ZERO,
																							roadRect.Width() + 1,
																							patchInfo.terrain.roads[nPatchRoadIndex].nType ) );
					}
				}
			}
			patchRoadPoints.push_back( roadPoints );
			//��������� ������������� ����� � �������� ���������������
			roadMakeParameter.lockedRects.push_back( CTRect<int>( placedPatches[nPlacedPatchIndex].minXYCorner.x,
																														placedPatches[nPlacedPatchIndex].minXYCorner.y,
																														placedPatches[nPlacedPatchIndex].minXYCorner.x + patchInfo.terrain.tiles.GetSizeX() - 1,
																														placedPatches[nPlacedPatchIndex].minXYCorner.y + patchInfo.terrain.tiles.GetSizeY() - 1 ) );
/**/
		/**
		// CRAP{ ��� ��������� placeholder'��
		for (int nPlaceHolderIndex = 0; nPlaceHolderIndex < placeHolders.size(); ++nPlaceHolderIndex )
		{
			std::vector<CVec3> polygon;
			polygon.push_back( CVec3( placeHolders[nPlaceHolderIndex].minx * fWorldCellSize, 
																placeHolders[nPlaceHolderIndex].miny * fWorldCellSize,
																0.0f ) );
			polygon.push_back( CVec3( placeHolders[nPlaceHolderIndex].maxx * fWorldCellSize, 
																placeHolders[nPlaceHolderIndex].miny * fWorldCellSize,
																0.0f ) );
			polygon.push_back( CVec3( placeHolders[nPlaceHolderIndex].maxx * fWorldCellSize, 
																placeHolders[nPlaceHolderIndex].maxy * fWorldCellSize,
																0.0f ) );
			polygon.push_back( CVec3( placeHolders[nPlaceHolderIndex].minx * fWorldCellSize, 
																placeHolders[nPlaceHolderIndex].maxy * fWorldCellSize,
																0.0f ) );
			CMapInfo::FillTerrainTiles( &( pLoadMapInfo->terrain ), polygon, tilesetDesc, 2 );
		}
		// CRAP}
		/**/
		//�������� ��� ���������� �����, ���� ������������ �������������� ������
		//SRoadMakeParameter roadMakeParameter;
		//roadMakeParameter.nMinMiddleDistance = 1;

		//����� ������ ����� �� ������ (������ �� ������)
		//std::vector<std::vector<SRoadPoint> > patchRoadPoints;

/**
		//�������� ������
		for ( int nLinkIndex = 0; nLinkIndex < rRMTemplate.graphs[nGraphIndex].links.size(); ++nLinkIndex )
		{
			std::vector<SRoadItem> addedRoad;

			int nPatchFromIndex = rRMTemplate.graphs[nGraphIndex].links[nLinkIndex].link.x;
			int nPatchToIndex = rRMTemplate.graphs[nGraphIndex].links[nLinkIndex].link.y;
			int nRoadType = rRMTemplate.graphs[nGraphIndex].links[nLinkIndex].nType;
			if ( ( nRoadType < SRMGraphLink::TYPE_RIVER ) &&
				   ( !patchRoadPoints[nPatchFromIndex].empty() ) &&
					 ( !patchRoadPoints[nPatchToIndex].empty() ) &&
					 ( nPatchFromIndex != nPatchToIndex) ) 
			{
				//������� ���������� ������
				int nRoadPointFromIndex = -1;
				float fMinDistance = sqr( pLoadMapInfo->terrain.tiles.GetSizeX() ) + 
					                   sqr( pLoadMapInfo->terrain.tiles.GetSizeY() );
				for ( int nFromIndex = 0; nFromIndex < patchRoadPoints[nPatchFromIndex].size(); ++nFromIndex )
				{
					if ( patchRoadPoints[nPatchFromIndex][nFromIndex].nRoadType == nRoadType )
					{
						float fDistance = sqr( placedPatches[nPatchToIndex].minXYCorner.x + 
																	 ( placedPatches[nPatchToIndex].size.x * STerrainPatchInfo::nSizeX / 2.0f ) - 
																	 patchRoadPoints[nPatchFromIndex][nFromIndex].x ) + 
															sqr( placedPatches[nPatchToIndex].minXYCorner.y + 
																	 ( placedPatches[nPatchToIndex].size.y * STerrainPatchInfo::nSizeY / 2.0f ) -
																	 patchRoadPoints[nPatchFromIndex][nFromIndex].y );
						if ( fDistance < fMinDistance )
						{
							fMinDistance = fDistance;
							nRoadPointFromIndex = nFromIndex;
						}
					}
				}
				int nRoadPointToIndex = -1;
				fMinDistance = sqr( pLoadMapInfo->terrain.tiles.GetSizeX() ) + 
					             sqr( pLoadMapInfo->terrain.tiles.GetSizeY() );
				for ( int nToIndex = 0; nToIndex < patchRoadPoints[nPatchToIndex].size(); ++nToIndex )
				{
					if ( patchRoadPoints[nPatchToIndex][nToIndex].nRoadType == nRoadType )
					{
						float fDistance = sqr( placedPatches[nPatchFromIndex].minXYCorner.x + 
																	 ( placedPatches[nPatchFromIndex].size.x * STerrainPatchInfo::nSizeX / 2.0f ) - 
																	 patchRoadPoints[nPatchToIndex][nToIndex].x ) + 
															sqr( placedPatches[nPatchFromIndex].minXYCorner.y + 
																	 ( placedPatches[nPatchFromIndex].size.y * STerrainPatchInfo::nSizeY / 2.0f ) -
																	 patchRoadPoints[nPatchToIndex][nToIndex].y );
						if ( fDistance < fMinDistance )
						{
							fMinDistance = fDistance;
							nRoadPointToIndex = nToIndex;
						}
					}
				}

				//��������� ����� ������ from � to ���� ��� ����������
				if ( ( nRoadPointFromIndex >= 0 ) &&
					   ( nRoadPointToIndex >= 0 ) )
				{
					SRoadPoint from = patchRoadPoints[nPatchFromIndex][nRoadPointFromIndex];
					SRoadPoint to = patchRoadPoints[nPatchToIndex][nRoadPointToIndex];

					//������ ������
					int nWidth = (from.nWidth >= to.nWidth ) ? from.nWidth : to.nWidth;
					
					**
					if ( roadsetDesc.roads[from.nRoadType].nPriority > roadsetDesc.roads[to.nRoadType].nPriority )
					{
						nWidth = from.nWidth;
						nRoadType = from.nRoadType;
					}
					else if ( roadsetDesc.roads[from.nRoadType].nPriority < roadsetDesc.roads[to.nRoadType].nPriority )
					{
						nWidth = to.nWidth;
						nRoadType = to.nRoadType;
					}
					else
					{
						nWidth = from.nWidth >= from.nWidth ? from.nWidth : to.nWidth;
						nRoadType = from.nRoadType;
					}
					**

					from.nWidth = nWidth;
					from.nRoadType = nRoadType;
					to.nWidth = nWidth;
					to.nRoadType = nRoadType;
					CMapInfo::MakeRoad( pLoadMapInfo, from, to, nRoadType, roadMakeParameter, &addedRoad );
				}
			}
		}
/**/
