//��� ������ ���
//REMOVE_OBJECTS_FROM_RECT
//��� ���������� ������ linkID
//UPDATE_LINK_ID

#include "stdafx.h"

#include "MapInfo_Types.h"
#include "RMG_Types.h"
#include "LA_Types.h"
#include "VA_Types.h"
#include "Polygons_Types.h"
#include "VSO_Types.h"
#include "Resource_Types.h"

#include "..\Formats\fmtTerrain.h"
#include "TerrainBuilder.h"
#include "..\Main\GameStats.h"
#include "..\AILogic\AIConsts.h"
#include "..\StreamIO\ProgressHook.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

//FOR_DEBUG_ONLY
//int _DEBUG_RECT_COUNT;
//int _DEBUG_OUTSIDE_COUNT;
//int _DEBUG_INSIDE_COUNT;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
struct STraceTimeKeeper
{
	DWORD dwTime;
	
	STraceTimeKeeper()
	{
		dwTime = GetTickCount();
	}
	
	void Init( )
	{
		dwTime = GetTickCount();
	}
	
	void Trace( const std::string &rszLabel )
	{
		dwTime = GetTickCount() - dwTime;
		NStr::DebugTrace( "%s %d ms\n", rszLabel.c_str(), dwTime );
		dwTime = GetTickCount();
	}
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMapInfo::AddMapInfo( SLoadMapInfo *pDestLoadMapInfo, const CTPoint<int> &rDestPoint, const SLoadMapInfo &rSourceLoadMapInfo )
{
	NI_ASSERT_TF( pDestLoadMapInfo != 0,
							  NStr::Format( "Wrong parameter: %x\n", pDestLoadMapInfo ),
							  return false );
	
	NI_ASSERT_TF( rSourceLoadMapInfo.nSeason == pDestLoadMapInfo->nSeason,
							  NStr::Format( "Wrong season: %d (%d)", rSourceLoadMapInfo.nSeason, pDestLoadMapInfo->nSeason ),
							  return false );
	
	NI_ASSERT_TF( rSourceLoadMapInfo.szSeasonFolder.compare( pDestLoadMapInfo->szSeasonFolder ) == 0,
							  NStr::Format( "Wrong season folder: %s (%s)", rSourceLoadMapInfo.szSeasonFolder.c_str(), pDestLoadMapInfo->szSeasonFolder.c_str() ),
							  return false );

	NI_ASSERT_TF( ( ( ( rDestPoint.x ) >= 0 ) &&
								  ( ( rDestPoint.y ) >= 0 ) &&
			            ( ( rSourceLoadMapInfo.terrain.tiles.GetSizeX() + rDestPoint.x ) <= pDestLoadMapInfo->terrain.tiles.GetSizeX() ) &&
			            ( ( rSourceLoadMapInfo.terrain.tiles.GetSizeY() + rDestPoint.y ) <= pDestLoadMapInfo->terrain.tiles.GetSizeY() ) ),
								NStr::Format( "Wrong point or size: point %d, %d, size %d x %d  (%d x %d)",
								              rDestPoint.x,
															rDestPoint.y,
															rSourceLoadMapInfo.terrain.tiles.GetSizeX(),
															rSourceLoadMapInfo.terrain.tiles.GetSizeY(),
															pDestLoadMapInfo->terrain.tiles.GetSizeX(),
															pDestLoadMapInfo->terrain.tiles.GetSizeY() ),
								return false );

	/**
	//������� ����������� ������� � ����� ��� ������ 
	//REMOVE_OBJECTS_FROM_RECT
	std::vector<CVec3>	mapPolygon;
	mapPolygon.push_back( CVec3( ( destPoint.x )																							 * fWorldCellSize, ( destPoint.y )																							 * fWorldCellSize, 0.0f ) );
	mapPolygon.push_back( CVec3( ( destPoint.x )																							 * fWorldCellSize, ( rSourceLoadMapInfo.terrain.tiles.GetSizeY() + destPoint.y ) * fWorldCellSize, 0.0f ) );
	mapPolygon.push_back( CVec3( ( rSourceLoadMapInfo.terrain.tiles.GetSizeX() + destPoint.x ) * fWorldCellSize, ( rSourceLoadMapInfo.terrain.tiles.GetSizeY() + destPoint.y ) * fWorldCellSize, 0.0f ) );
	mapPolygon.push_back( CVec3( ( rSourceLoadMapInfo.terrain.tiles.GetSizeX() + destPoint.x ) * fWorldCellSize, ( destPoint.y )																							 * fWorldCellSize, 0.0f ) );
	if ( !RemoveObjects( pDestLoadMapInfo, mapPolygon ) )
	{
		return false;
	}
	/**/
	// ����� � ��������������� ���� Y ��� ������� ������� ����� � �����
	const CTPoint<int> terrainDestPoint( rDestPoint.x, pDestLoadMapInfo->terrain.tiles.GetSizeY() - rDestPoint.y - rSourceLoadMapInfo.terrain.tiles.GetSizeY() );

	//��������  terrain ( STerrainInfo )
	//��������� tiles
	for ( int nXIndex = 0; nXIndex < rSourceLoadMapInfo.terrain.tiles.GetSizeX(); ++nXIndex )
	{
		for ( int nYIndex = 0; nYIndex < rSourceLoadMapInfo.terrain.tiles.GetSizeY(); ++nYIndex )
		{
			pDestLoadMapInfo->terrain.tiles[nYIndex + terrainDestPoint.y][nXIndex + terrainDestPoint.x] = rSourceLoadMapInfo.terrain.tiles[nYIndex][nXIndex];
		}
	}
	
	//��������� altitude
	if ( ( rSourceLoadMapInfo.terrain.altitudes.GetSizeX() > 0 ) && ( rSourceLoadMapInfo.terrain.altitudes.GetSizeY() > 0 ) )
	{
		for ( int nXIndex = 0; nXIndex < rSourceLoadMapInfo.terrain.altitudes.GetSizeX(); ++nXIndex )
		{
			for ( int nYIndex = 0; nYIndex < rSourceLoadMapInfo.terrain.altitudes.GetSizeY(); ++nYIndex )
			{
				pDestLoadMapInfo->terrain.altitudes[nYIndex + terrainDestPoint.y][nXIndex + terrainDestPoint.x].fHeight += rSourceLoadMapInfo.terrain.altitudes[nYIndex][nXIndex].fHeight;
			}
		}
	}
	
	//��������� ������
	/**
	for ( int nRoadIndex = 0; nRoadIndex < rSourceLoadMapInfo.terrain.roads.size(); ++nRoadIndex )
	{
		SRoadItem roadItem = rSourceLoadMapInfo.terrain.roads[nRoadIndex];
		roadItem.rect.left += terrainDestPoint.x;
		roadItem.rect.right += terrainDestPoint.x;
		roadItem.rect.top += terrainDestPoint.y;
		roadItem.rect.bottom += terrainDestPoint.y;
		pDestLoadMapInfo->terrain.roads.push_back( roadItem );
	}
	/**/

	//��������� ����
	for ( int nRiverIndex = 0; nRiverIndex < rSourceLoadMapInfo.terrain.rivers.size(); ++nRiverIndex )
	{
		const SVectorStripeObject &rSourceRiverInfo = rSourceLoadMapInfo.terrain.rivers[nRiverIndex];
		pDestLoadMapInfo->terrain.rivers.push_back( rSourceRiverInfo );		
		SVectorStripeObject &rDestRiverInfo = pDestLoadMapInfo->terrain.rivers.back();
		
		for ( int nControlPointIndex = 0; nControlPointIndex < rDestRiverInfo.controlpoints.size(); ++nControlPointIndex )
		{
			rDestRiverInfo.controlpoints[nControlPointIndex].x += rDestPoint.x * fWorldCellSize;
			rDestRiverInfo.controlpoints[nControlPointIndex].y += rDestPoint.y * fWorldCellSize;
		}
		for ( int nPointndex = 0; nPointndex < rDestRiverInfo.points.size(); ++nPointndex )
		{
			rDestRiverInfo.points[nPointndex].vPos.x += rDestPoint.x * fWorldCellSize;
			rDestRiverInfo.points[nPointndex].vPos.y += rDestPoint.y * fWorldCellSize;
		}
		rDestRiverInfo.nID = pDestLoadMapInfo->terrain.rivers.size();
	}

	//��������� 3D ������
	for ( int nRoad3dIndex = 0; nRoad3dIndex < rSourceLoadMapInfo.terrain.roads3.size(); ++nRoad3dIndex )
	{
		const SVectorStripeObject &rSourceRoad3DInfo = rSourceLoadMapInfo.terrain.roads3[nRoad3dIndex];
		pDestLoadMapInfo->terrain.roads3.push_back( rSourceRoad3DInfo );		
		SVectorStripeObject &rDestRoad3DInfo = pDestLoadMapInfo->terrain.roads3.back();
		
		for ( int nControlPointIndex = 0; nControlPointIndex < rDestRoad3DInfo.controlpoints.size(); ++nControlPointIndex )
		{
			rDestRoad3DInfo.controlpoints[nControlPointIndex].x += rDestPoint.x * fWorldCellSize;
			rDestRoad3DInfo.controlpoints[nControlPointIndex].y += rDestPoint.y * fWorldCellSize;
		}
		for ( int nPointndex = 0; nPointndex < rDestRoad3DInfo.points.size(); ++nPointndex )
		{
			rDestRoad3DInfo.points[nPointndex].vPos.x += rDestPoint.x * fWorldCellSize;
			rDestRoad3DInfo.points[nPointndex].vPos.y += rDestPoint.y * fWorldCellSize;
		}
		rDestRoad3DInfo.nID = pDestLoadMapInfo->terrain.roads3.size();
	}

	//��������� �������
	//����������� ��������� ��������
	const int nMaxLinkID = UpdateObjects( pDestLoadMapInfo, CTRect<int>( 0, 0, pDestLoadMapInfo->terrain.tiles.GetSizeX(), pDestLoadMapInfo->terrain.tiles.GetSizeY() ) );

	//UPDATE_LINK_ID
	//usedIDs[old nLinkID] = new nLinkID;
	std::unordered_map<int, int> usedIDs;

	//��������� usedID
	int nCurrentLinkID = nMaxLinkID;
	for ( int nObjectIndex = 0; nObjectIndex < rSourceLoadMapInfo.objects.size(); ++nObjectIndex )
	{	
		if ( rSourceLoadMapInfo.objects[nObjectIndex].link.nLinkID  != RMGC_INVALID_LINK_ID_VALUE )
		{
			usedIDs[rSourceLoadMapInfo.objects[nObjectIndex].link.nLinkID] = nCurrentLinkID;
		}
		nCurrentLinkID++;
	}
	for ( int nObjectIndex = 0; nObjectIndex < rSourceLoadMapInfo.scenarioObjects.size(); ++nObjectIndex )
	{	
		if ( rSourceLoadMapInfo.scenarioObjects[nObjectIndex].link.nLinkID  != RMGC_INVALID_LINK_ID_VALUE )
		{
			usedIDs[rSourceLoadMapInfo.scenarioObjects[nObjectIndex].link.nLinkID] = nCurrentLinkID;
		}
		nCurrentLinkID++;
	}

	//��������� SMapObjectInfo � objects � ������� ����� nLinkID
	nCurrentLinkID = nMaxLinkID;
	for ( int nObjectIndex = 0; nObjectIndex < rSourceLoadMapInfo.objects.size(); ++nObjectIndex )
	{	
		const SMapObjectInfo &rSourceMapObjectInfo = rSourceLoadMapInfo.objects[nObjectIndex];
		pDestLoadMapInfo->objects.push_back( rSourceMapObjectInfo );
		SMapObjectInfo &rDestMapObjectInfo = pDestLoadMapInfo->objects.back();

		rDestMapObjectInfo.vPos.x += rDestPoint.x * SAIConsts::TILE_SIZE * 2;
		rDestMapObjectInfo.vPos.y += rDestPoint.y * SAIConsts::TILE_SIZE * 2;
		rDestMapObjectInfo.link.nLinkID = nCurrentLinkID;
		rDestMapObjectInfo.link.nLinkWith = usedIDs[rSourceMapObjectInfo.link.nLinkWith];
		
		NI_ASSERT_TF( CMapInfo::PointInMap( ( *pDestLoadMapInfo ), rDestMapObjectInfo.vPos, true ),
									NStr::Format( "Object %s (%d), is outside map, press <Ignore> to get patch information!", rDestMapObjectInfo.szName.c_str(), nObjectIndex ),
									return false );
		
		nCurrentLinkID++;
	}
	for ( int nObjectIndex = 0; nObjectIndex < rSourceLoadMapInfo.scenarioObjects.size(); ++nObjectIndex )
	{	
		const SMapObjectInfo &rSourceMapObjectInfo = rSourceLoadMapInfo.scenarioObjects[nObjectIndex];
		pDestLoadMapInfo->scenarioObjects.push_back( rSourceMapObjectInfo );
		SMapObjectInfo &rDestMapObjectInfo = pDestLoadMapInfo->scenarioObjects.back();

		rDestMapObjectInfo.vPos.x += rDestPoint.x * SAIConsts::TILE_SIZE * 2;
		rDestMapObjectInfo.vPos.y += rDestPoint.y * SAIConsts::TILE_SIZE * 2;
		rDestMapObjectInfo.link.nLinkID = nCurrentLinkID;
		rDestMapObjectInfo.link.nLinkWith = usedIDs[rSourceMapObjectInfo.link.nLinkWith];

		NI_ASSERT_TF( CMapInfo::PointInMap( ( *pDestLoadMapInfo ), rDestMapObjectInfo.vPos, true ),
									NStr::Format( "Scenario Object %s (%d), is outside map, press <Ignore> to get patch information!", rDestMapObjectInfo.szName.c_str(), nObjectIndex ),
									return false );
		
		nCurrentLinkID++;
	}

	//���������� ����� nLinkID � entrenchments
	for ( int nEntrenchmentIndex = 0; nEntrenchmentIndex < rSourceLoadMapInfo.entrenchments.size(); ++nEntrenchmentIndex )
	{
		const SEntrenchmentInfo &rSourceEntrenchmentInfo = rSourceLoadMapInfo.entrenchments[nEntrenchmentIndex];
		pDestLoadMapInfo->entrenchments.push_back( rSourceEntrenchmentInfo );
		SEntrenchmentInfo &rDestEntrenchmentInfo = pDestLoadMapInfo->entrenchments.back();

		for ( int nSectionIndex = 0; nSectionIndex < rDestEntrenchmentInfo.sections.size(); ++nSectionIndex )
		{
			for ( int nTSegmentIndex = 0; nTSegmentIndex < rDestEntrenchmentInfo.sections[nSectionIndex].size(); ++nTSegmentIndex )
			{
				rDestEntrenchmentInfo.sections[nSectionIndex][nTSegmentIndex] = usedIDs[ rSourceEntrenchmentInfo.sections[nSectionIndex][nTSegmentIndex] ];
			}
		}
	}

	//��������� std::vector<int> � bridges
	for ( int nBridgeIndex = 0; nBridgeIndex < rSourceLoadMapInfo.bridges.size(); ++nBridgeIndex )
	{
		const std::vector<int> &rSourceBridge = rSourceLoadMapInfo.bridges[nBridgeIndex];
		pDestLoadMapInfo->bridges.push_back( rSourceBridge );
		std::vector<int> &rDestBridge = pDestLoadMapInfo->bridges.back();
		
		for ( int nBridgeElementIndex = 0; nBridgeElementIndex < rDestBridge.size(); ++nBridgeElementIndex )
		{
			rDestBridge[nBridgeElementIndex] = usedIDs[ rSourceBridge[nBridgeElementIndex] ];
		}
	}

	//��������� reinforcements
	pDestLoadMapInfo->reinforcements.groups.insert( rSourceLoadMapInfo.reinforcements.groups.begin(), rSourceLoadMapInfo.reinforcements.groups.end() );

	//��������� scripAreas
	for ( int nScriptAreaIndex = 0; nScriptAreaIndex < rSourceLoadMapInfo.scriptAreas.size(); ++nScriptAreaIndex )
	{
		const SScriptArea &rSourceScriptArea = rSourceLoadMapInfo.scriptAreas[nScriptAreaIndex];
		pDestLoadMapInfo->scriptAreas.push_back( rSourceScriptArea );
		SScriptArea &rDestScriptArea = pDestLoadMapInfo->scriptAreas.back();
		
		rDestScriptArea.center.x += rDestPoint.x * SAIConsts::TILE_SIZE * 2;
		rDestScriptArea.center.y += rDestPoint.y * SAIConsts::TILE_SIZE * 2;
	}

	//��������� TStartCommandsList � startCommandsList
	for ( SLoadMapInfo::TStartCommandsList::const_iterator it = rSourceLoadMapInfo.startCommandsList.begin();
	      it != rSourceLoadMapInfo.startCommandsList.end();
				++it )
	{
		pDestLoadMapInfo->startCommandsList.push_back( *it );
		SAIStartCommand &rDestStartCommand = pDestLoadMapInfo->startCommandsList.back();

		for( int nLinkIDIndex = 0; nLinkIDIndex < rDestStartCommand.unitLinkIDs.size(); ++nLinkIDIndex )
		{
			rDestStartCommand.unitLinkIDs[nLinkIDIndex] = usedIDs[it->unitLinkIDs[nLinkIDIndex] ];
		}
		if ( rDestStartCommand.linkID != RMGC_INVALID_LINK_ID_VALUE )
		{
			rDestStartCommand.linkID = usedIDs[it->linkID];
		}
		rDestStartCommand.vPos.x += rDestPoint.x * SAIConsts::TILE_SIZE * 2;
		rDestStartCommand.vPos.y += rDestPoint.y * SAIConsts::TILE_SIZE * 2;
	}

	//��������� TReservePositionsList � reservePositionsList
	for ( SLoadMapInfo::TReservePositionsList::const_iterator it = rSourceLoadMapInfo.reservePositionsList.begin();
	      it != rSourceLoadMapInfo.reservePositionsList.end();
				++it )
	{
		pDestLoadMapInfo->reservePositionsList.push_back( *it );
		SBattlePosition &rDestReservePosition = pDestLoadMapInfo->reservePositionsList.back();
		
		if ( rDestReservePosition.nArtilleryLinkID != RMGC_INVALID_LINK_ID_VALUE )
		{
			rDestReservePosition.nArtilleryLinkID = usedIDs[it->nArtilleryLinkID];
		}
		if ( rDestReservePosition.nTruckLinkID != RMGC_INVALID_LINK_ID_VALUE )
		{
			rDestReservePosition.nTruckLinkID = usedIDs[it->nTruckLinkID];
		}
		rDestReservePosition.vPos.x += rDestPoint.x * SAIConsts::TILE_SIZE * 2;
		rDestReservePosition.vPos.y += rDestPoint.y * SAIConsts::TILE_SIZE * 2;
	}

	//��������� �����
	for ( TMapSoundInfoList::const_iterator soundIterator = rSourceLoadMapInfo.soundsList.begin(); soundIterator != rSourceLoadMapInfo.soundsList.end(); ++soundIterator )
	{
		pDestLoadMapInfo->soundsList.push_back( *soundIterator );
		CMapSoundInfo &rDestSoundInfo = pDestLoadMapInfo->soundsList.back();

		rDestSoundInfo.vPos.x += rDestPoint.x * fWorldCellSize;
		rDestSoundInfo.vPos.y += rDestPoint.y * fWorldCellSize;
	}
	
	//��������� AIGeneral
	for ( int nSourceSideIndex = 0; nSourceSideIndex < rSourceLoadMapInfo.aiGeneralMapInfo.sidesInfo.size(); ++nSourceSideIndex )
	{
		const SAIGeneralSideInfo &rSourceAIGeneralSideInfo = rSourceLoadMapInfo.aiGeneralMapInfo.sidesInfo[nSourceSideIndex];
		
		//���� ������� ��� �������� ����������� - ���������
		if ( nSourceSideIndex == pDestLoadMapInfo->aiGeneralMapInfo.sidesInfo.size() )
		{
			pDestLoadMapInfo->aiGeneralMapInfo.sidesInfo.push_back( SAIGeneralSideInfo() );
		}
		SAIGeneralSideInfo &rDestAIGeneralSideInfo = pDestLoadMapInfo->aiGeneralMapInfo.sidesInfo[nSourceSideIndex];

		//��������� ������������ ScriptID
		//�������� �����������
		for ( int nSourceScriptIDIndex = 0; nSourceScriptIDIndex < rSourceAIGeneralSideInfo.mobileScriptIDs.size(); ++nSourceScriptIDIndex )
		{
			const int &rSourceScriptID = rSourceAIGeneralSideInfo.mobileScriptIDs[nSourceScriptIDIndex];
			bool bScriptIDNotPresent = true;
			for ( int nDestScriptIDIndex = 0; nDestScriptIDIndex < rDestAIGeneralSideInfo.mobileScriptIDs.size(); ++nDestScriptIDIndex )
			{
				if ( rDestAIGeneralSideInfo.mobileScriptIDs[nDestScriptIDIndex] == rSourceScriptID )
				{
					bScriptIDNotPresent = false;
					break;
				}
			}
			if ( bScriptIDNotPresent )
			{
				rDestAIGeneralSideInfo.mobileScriptIDs.push_back( rSourceScriptID );
			}
		}

		//��������� �������
		for ( int nSourceParcelIndex = 0; nSourceParcelIndex < rSourceAIGeneralSideInfo.parcels.size(); ++nSourceParcelIndex )
		{
			const SAIGeneralParcelInfo &rSourceAIGeneralParcelInfo = rSourceAIGeneralSideInfo.parcels[nSourceParcelIndex];
			rDestAIGeneralSideInfo.parcels.push_back( rSourceAIGeneralParcelInfo );
			SAIGeneralParcelInfo &rDestAIGeneralParcelInfo = rDestAIGeneralSideInfo.parcels.back();
			
			rDestAIGeneralParcelInfo.vCenter.x += rDestPoint.x * SAIConsts::TILE_SIZE * 2;
			rDestAIGeneralParcelInfo.vCenter.y += rDestPoint.y * SAIConsts::TILE_SIZE * 2;
		}
	}
	
	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//��������� �������, ���������� ���������� �� ��������, � ������ �����,
// fDistanse > 0 ����� ������ ��������
// fDistanse == 0 ����� �� ������� ��������
// fDistanse < 0 ����� ������� �������� ( ���������� �� ���������� )
//����� ��������� ����� ������� ����� ��������� ����������� ���������
float GetInclusivePolygonSetDistance( const CVec2 &rPoint, const std::list<CVec2> &rInclusivePolygon, const std::list<std::list<CVec2> > &rExclusivePolygons )
{
	float fInclusiveDistance = -1;
	if ( ClassifyPolygon( rInclusivePolygon, rPoint ) != CP_OUTSIDE )
	{
		fInclusiveDistance = PolygonDistance( rInclusivePolygon, rPoint );
		if ( fInclusiveDistance >= 0 )
		{
			for ( std::list<std::list<CVec2> >::const_iterator exclusivePolygonIterator = rExclusivePolygons.begin(); exclusivePolygonIterator != rExclusivePolygons.end(); ++exclusivePolygonIterator )
			{
				float fDistance = PolygonDistance( ( *exclusivePolygonIterator ), rPoint );
				//������ 
				if ( fDistance > 0 )
				{
					fInclusiveDistance = -fDistance;
					break;
				}
				else
				{
					//�������
					fDistance = -fDistance;
					if ( fDistance < fInclusiveDistance )
					{
						fInclusiveDistance = fDistance;
					}
				}
			}
		}
	}
	return fInclusiveDistance;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//��������� �������, ���������� ����� ��������� � ������� ��� ���� �����
//���������� ���������� ��������� � pXPos
//���� ������������� �� ���� ����� ���, - ������ � ����� �������� �������� � �������
//����� �������� ���������� � �������, ��� ��������� �����
int GetPolygonLine( int nYPos, float fSide, const std::list<CVec2> &rPolygon, std::vector<int> *pXPos )
{
	NI_ASSERT_TF( pXPos != 0,
								NStr::Format( "GetPolygonLine(), invalid parameter: pXPos: %x", pXPos ),
								return false );
	pXPos->clear();

	if ( rPolygon.empty() )
	{
		return 0;
	}

	std::list<CVec2>::const_iterator currentPointIterator0 = rPolygon.begin();
	std::list<CVec2>::const_iterator currentPointIterator1 = rPolygon.begin();

	++currentPointIterator1;
	if ( currentPointIterator1 == rPolygon.end() )
	{
		if ( ( currentPointIterator0->y / fSide ) == nYPos )
		{
			const int nXPos = currentPointIterator0->x / fSide;
			pXPos->resize( 2, nXPos );
		}
		return pXPos->size();
	}
	
	const float fY = ( nYPos * fSide ) + ( fSide / 2.0f );

	while ( currentPointIterator0 != rPolygon.end() )
	{
		if ( ( ( currentPointIterator1->y <= fY ) && ( fY < currentPointIterator0->y ) ) ||
				 ( ( currentPointIterator0->y <= fY ) && ( fY < currentPointIterator1->y ) ) )
		{
			if ( currentPointIterator0->x == currentPointIterator1->x )
			{
				pXPos->push_back( currentPointIterator0->x / fSide );
			}
			else
			{
				const float fA = ( currentPointIterator0->y - currentPointIterator1->y ) / ( currentPointIterator0->x - currentPointIterator1->x );
				const float fB = currentPointIterator0->y - ( fA * currentPointIterator0->x );
				const float fX = ( fY - fB ) / fA;
				pXPos->push_back( fX / fSide );
			}
		}
		++currentPointIterator0;
		++currentPointIterator1;
		if ( currentPointIterator1 == rPolygon.end() )
		{
			currentPointIterator1 = rPolygon.begin();
		}
	}
	std::sort( pXPos->begin(), pXPos->end() );
	return pXPos->size();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMapInfo::FillTerrain( STerrainInfo *pTerrainInfo, const struct STilesetDesc &rTilesetDesc, int nTileIndex )
{
	NI_ASSERT_TF( pTerrainInfo != 0,
							  NStr::Format( "CMapInfo::FillTileSet(): Invalid parameter pTerrainInfo: %x (!= 0)\n", pTerrainInfo ),
							  return false );
	NI_ASSERT_TF( ( nTileIndex >= 0 ) && ( nTileIndex < rTilesetDesc.terrtypes.size() ),
							  NStr::Format( "CMapInfo::FillTileSet(): Invalid parameter nTileIndex: %d [0 %d)\n", nTileIndex, rTilesetDesc.terrtypes.size() ),
							  return false );
	for ( int nYIndex = 0; nYIndex < pTerrainInfo->tiles.GetSizeY(); ++nYIndex )
	{
		for ( int nXIndex = 0; nXIndex < pTerrainInfo->tiles.GetSizeX(); ++nXIndex )
		{
			pTerrainInfo->tiles[nYIndex][nXIndex].tile = rTilesetDesc.terrtypes[nTileIndex].GetMapsIndex();
		}
	}
	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMapInfo::FillTileSet( STerrainInfo *pTerrainInfo,
														const STilesetDesc &rTilesetDesc,
														const std::list<CVec2> &rInclusivePolygon,
														const std::list<std::list<CVec2> > &rExclusivePolygons,
														const CRMTileSet &rTileSet,
														std::unordered_map<LPARAM, float> *pDistances )
{
	if ( rTileSet.empty() )
	{
		return true;
	}

	NI_ASSERT_TF( pTerrainInfo != 0,
							  NStr::Format( "CMapInfo::FillTileSet(): Invalid parameter pTerrainInfo: %x (!= 0)\n", pTerrainInfo ),
							  return false );
	for ( CRMTileSet::const_iterator tileSetShellIterator = rTileSet.begin(); tileSetShellIterator != rTileSet.end(); ++tileSetShellIterator )
	{
		NI_ASSERT_T( tileSetShellIterator->fWidth >= 0,
								 NStr::Format( "CMapInfo::FillTileSet(): Invalid width: %g (0...INF)", tileSetShellIterator->fWidth ) );
	}

	CTRect<int> boundingRect( 0, 0, pTerrainInfo->tiles.GetSizeX(), pTerrainInfo->tiles.GetSizeY() );
	CTRect<float> boundingBox = GetPolygonBoundingBox( rInclusivePolygon );
	CTRect<int> indices( ( boundingBox.minx + ( fWorldCellSize / 2.0f ) ) / fWorldCellSize,
											 ( boundingBox.miny + ( fWorldCellSize / 2.0f ) ) / fWorldCellSize,
											 ( boundingBox.maxx + ( fWorldCellSize / 2.0f ) ) / fWorldCellSize,
											 ( boundingBox.maxy + ( fWorldCellSize / 2.0f ) ) / fWorldCellSize );
	//���������
	if ( ValidateIndices( boundingRect, &indices ) < 0 )
	{
		return false;
	}

	//��������� �� ������
	//
	//FOR_DEBUG_ONLY
	//_DEBUG_RECT_COUNT = indices.GetSizeX() * indices.GetSizeY();
	//_DEBUG_OUTSIDE_COUNT = 0;
	//_DEBUG_INSIDE_COUNT = 0;
	
	for ( int nYIndex = indices.miny; nYIndex < indices.maxy; ++nYIndex )
	{
		std::vector<int> xpos;
		int nCount = GetPolygonLine( nYIndex, fWorldCellSize, rInclusivePolygon, &xpos );
		if ( ( nCount > 0 ) && ( ( nCount & 0x1 ) == 0 ) )
		{
			std::vector<int>::const_iterator startXPosIterator = xpos.begin();
			std::vector<int>::const_iterator finishXPosIterator = xpos.begin();
			++finishXPosIterator;
			while ( true )
			{
				for ( int nXIndex = ( *startXPosIterator ); nXIndex <= ( *finishXPosIterator ); ++nXIndex )
				{
					//FOR_DEBUG_ONLY
					//++_DEBUG_OUTSIDE_COUNT;
					//
					float fInclusiveDistance = 0.0f;
					if ( pDistances )
					{
						const LPARAM lParam = MAKELPARAM( nXIndex, nYIndex );
						std::unordered_map<LPARAM, float>::const_iterator distanceIterator = pDistances->find( MAKELPARAM( nXIndex, nYIndex ) );
						if ( distanceIterator != pDistances->end() )
						{
							fInclusiveDistance = distanceIterator->second;
						}
						else
						{
							const CVec2	vTileCenter( ( nXIndex * fWorldCellSize ) + ( fWorldCellSize / 2.0f ),
																			 ( nYIndex * fWorldCellSize ) + ( fWorldCellSize / 2.0f ) );
							fInclusiveDistance = GetInclusivePolygonSetDistance( vTileCenter, rInclusivePolygon, rExclusivePolygons );
							( *pDistances )[lParam] = fInclusiveDistance;
						}
					}
					else
					{
						const CVec2	vTileCenter( ( nXIndex * fWorldCellSize ) + ( fWorldCellSize / 2.0f ),
																		 ( nYIndex * fWorldCellSize ) + ( fWorldCellSize / 2.0f ) );
						fInclusiveDistance = GetInclusivePolygonSetDistance( vTileCenter, rInclusivePolygon, rExclusivePolygons );
					}
					
					if ( fInclusiveDistance >= 0 )
					{
						//FOR_DEBUG_ONLY
						//++_DEBUG_INSIDE_COUNT;

						fInclusiveDistance /= fWorldCellSize;
						//��������� ����
						float fWidth = 0.0f;
						for ( CRMTileSet::const_iterator tileSetShellIterator = rTileSet.begin(); tileSetShellIterator != rTileSet.end(); ++tileSetShellIterator )
						{
							fWidth += tileSetShellIterator->fWidth;
							if ( fWidth > fInclusiveDistance )
							{
								if ( !tileSetShellIterator->tiles.empty() )
								{
									pTerrainInfo->tiles[pTerrainInfo->tiles.GetSizeY() - nYIndex - 1][nXIndex].tile = rTilesetDesc.terrtypes[tileSetShellIterator->tiles.GetRandom()].GetMapsIndex();
								}
								break;
							}
						}
					}
				}
				//
				startXPosIterator += 2;
				if ( startXPosIterator == xpos.end() )
				{
					break;
				}
				finishXPosIterator += 2;
				//
			}
		}
		else
		{
			NI_ASSERT_T( ( nCount > 0 ) && ( ( nCount & 0x1 ) == 0 ),
									 NStr::Format( "CMapInfo::FillTileSet(): Invalid GetPolygonLine() = %d\n", nCount ) );
		}
	}
	//FOR_DEBUG_ONLY
	//NStr::DebugTrace( "CMapInfo::FillTileSet()\n_DEBUG_RECT_COUNT = %d\n_DEBUG_ESTIMATE_INSIDE_COUNT = %d\n_DEBUG_INSIDE_COUNT = %d\n", _DEBUG_RECT_COUNT, _DEBUG_OUTSIDE_COUNT, _DEBUG_INSIDE_COUNT );
	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMapInfo::FillObjectSet( SLoadMapInfo *pLoadMapInfo,
															const std::list<CVec2> &rInclusivePolygon,
															const std::list<std::list<CVec2> > &rExclusivePolygons,
															const CRMObjectSet &rObjectSet,
															CArray2D<BYTE> *pTileMap )
{
	if ( rObjectSet.empty() )
	{
		return true;
	}

	NI_ASSERT_TF( pLoadMapInfo != 0,
								NStr::Format( "CMapInfo::FillObjectSet(): Invalid parameter pLoadMapInfo: %x (!= 0)\n", pLoadMapInfo ),
								return false );
	for ( CRMObjectSet::const_iterator objectSetShellIterator = rObjectSet.begin(); objectSetShellIterator != rObjectSet.end(); ++objectSetShellIterator )
	{
		NI_ASSERT_T( objectSetShellIterator->fWidth >= 0,
								 NStr::Format( "CMapInfo::FillObjectSet(): Invalid width: %g (0...INF)", objectSetShellIterator->fWidth ) );
	}

	if ( pTileMap )
	{
		NI_ASSERT_TF( ( pTileMap->GetSizeX() >= pLoadMapInfo->terrain.tiles.GetSizeX() * 2 ) &&
									( pTileMap->GetSizeY() >= pLoadMapInfo->terrain.tiles.GetSizeY() * 2 ),
									NStr::Format( "CMapInfo::FillObjectSet(): Invalid tile map size: [%d%d], ([%d%d]\n",
																pTileMap->GetSizeX(),	pTileMap->GetSizeY(),
																pLoadMapInfo->terrain.tiles.GetSizeX() * 2, pLoadMapInfo->terrain.tiles.GetSizeY() * 2 ),
									return false );
	}
	ModifyTilesFunctional<CArray2D<BYTE>, BYTE> tileMapModifyTiles( RMGC_LOCKED, pTileMap );
	CheckTilesFunctional<CArray2D<BYTE>, BYTE> tileMapCheckTiles( RMGC_LOCKED, pTileMap );
	CTRect<int> tileMapRect( 0, 0, pLoadMapInfo->terrain.tiles.GetSizeX() * 2, pLoadMapInfo->terrain.tiles.GetSizeY() * 2 );
	
	//��� ������� � ���������� � ��� ������
	//��-�� ����, ��� ������� ���������� ����� � ��� ������
	float fTileSide = fWorldCellSize / 2.0f;

	CTRect<float> boundingBox = GetPolygonBoundingBox( rInclusivePolygon );
	//���������� ��������� � �������� AI ���� � VIS �����������
	CTRect<int> indices( ( boundingBox.minx + ( fTileSide / 2.0f ) ) / fTileSide,
											 ( boundingBox.miny + ( fTileSide / 2.0f ) ) / fTileSide,
											 ( boundingBox.maxx + ( fTileSide / 2.0f ) ) / fTileSide,
											 ( boundingBox.maxy + ( fTileSide / 2.0f ) ) / fTileSide );
	//���������
	if ( ValidateIndices( tileMapRect, &indices ) < 0 )
	{
		return false;
	}

	//��������� �� ������
	//
	for ( int nYIndex = indices.miny; nYIndex < indices.maxy; ++nYIndex )
	{
		std::vector<int> xpos;
		int nCount = GetPolygonLine( nYIndex, fTileSide, rInclusivePolygon, &xpos );
		if ( ( nCount > 0 ) && ( ( nCount & 0x1 ) == 0 ) )
		{
			std::vector<int>::const_iterator startXPosIterator = xpos.begin();
			std::vector<int>::const_iterator finishXPosIterator = xpos.begin();
			++finishXPosIterator;
			while ( true )
			{
				for ( int nXIndex = ( *startXPosIterator ); nXIndex <= ( *finishXPosIterator ); ++nXIndex )
				{
					//
					CVec2	vObjectCenter( ( nXIndex * fTileSide ) + ( fTileSide / 2.0f ),
															 ( nYIndex * fTileSide ) + ( fTileSide / 2.0f ) );
					
					float fInclusiveDistance = GetInclusivePolygonSetDistance( vObjectCenter, rInclusivePolygon, rExclusivePolygons );
					if ( fInclusiveDistance >= 0 )
					{
						fInclusiveDistance /= fWorldCellSize;
						//��������� ����
						float fWidth = 0.0f;
						for ( CRMObjectSet::const_iterator objectSetShellIterator = rObjectSet.begin(); objectSetShellIterator != rObjectSet.end(); ++objectSetShellIterator )
						{
							fWidth += objectSetShellIterator->fWidth;
							if ( fWidth > fInclusiveDistance )
							{
								if ( ( objectSetShellIterator->nBetweenDistance > 0 ) && ( !objectSetShellIterator->objects.empty() ) )
								{
									const float fAdditionalRatio = 1.0f / fabs2( objectSetShellIterator->nBetweenDistance );
									if ( Random( 0.0f, 1.0f ) <= ( objectSetShellIterator->fRatio * fAdditionalRatio ) )
									{
										SMapObjectInfo mapObjectInfo;
										mapObjectInfo.szName = objectSetShellIterator->objects.GetRandom();
										mapObjectInfo.vPos = CVec3( vObjectCenter.x, vObjectCenter.y, 0.0f );
										mapObjectInfo.nDir = 0;
										mapObjectInfo.nPlayer = 0;
										mapObjectInfo.nScriptID = RMGC_INVALID_SCRIPT_ID_VALUE;
										mapObjectInfo.fHP = 1.0f;
										mapObjectInfo.nFrameIndex = RMGC_INVALID_FRAME_INDEX_VALUE;
										mapObjectInfo.link.nLinkID = RMGC_INVALID_LINK_ID_VALUE;
										mapObjectInfo.link.bIntention = true;
										mapObjectInfo.link.nLinkWith = RMGC_INVALID_LINK_ID_VALUE;

										const SStaticObjectRPGStats* pStaticObjectRPGStats = NGDB::GetRPGStats<SStaticObjectRPGStats>( mapObjectInfo.szName.c_str() );
										NI_ASSERT_T( pStaticObjectRPGStats != 0, NStr::Format( "CMapInfo::FillObjectSet(): Can't get RPG stats %s", mapObjectInfo.szName.c_str() ) );
										
										Vis2AI( &( mapObjectInfo.vPos ) );
										FitAIOrigin2AIGrid( &( mapObjectInfo.vPos ), pStaticObjectRPGStats->GetOrigin( mapObjectInfo.nFrameIndex ) );

										if ( pTileMap )
										{
											tileMapCheckTiles.isPresent = false;
											if ( ApplyTilesInObjectsPassability( tileMapRect, mapObjectInfo, tileMapCheckTiles ) )
											{
												if ( ( mapObjectInfo.vPos.x >= ( tileMapRect.minx * SAIConsts::TILE_SIZE ) ) && 
														 ( mapObjectInfo.vPos.y >= ( tileMapRect.miny * SAIConsts::TILE_SIZE ) ) &&
														 ( mapObjectInfo.vPos.x <  ( tileMapRect.maxx * SAIConsts::TILE_SIZE ) ) &&
														 ( mapObjectInfo.vPos.y <  ( tileMapRect.maxy * SAIConsts::TILE_SIZE ) ) )
												{
													//������ ����� �������
													ApplyTilesInObjectsPassability( tileMapRect, mapObjectInfo, tileMapModifyTiles );
													pLoadMapInfo->objects.push_back( mapObjectInfo );
												}
											}
										}
										else
										{
											pLoadMapInfo->objects.push_back( mapObjectInfo );
										}
									}
								}
								break;
							}
						}
					}
				}
				//
				startXPosIterator += 2;
				if ( startXPosIterator == xpos.end() )
				{
					break;
				}
				finishXPosIterator += 2;
				//
			}
		}
	}
	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMapInfo::FillProfilePattern( STerrainInfo *pTerrainInfo,
																	 const std::list<CVec2> &rInclusivePolygon,
																	 const std::list<std::list<CVec2> > &rExclusivePolygons,
																	 const SVAGradient &rGradient,
																	 const CTPoint<int> &rPatternSize,
																	 float fPositiveRatio, 
																	 std::unordered_map<LPARAM, float> *pDistances )
{
	NI_ASSERT_TF( pTerrainInfo != 0,
							  NStr::Format( "CMapInfo::FillProfilePattern(): Invalid parameter pTerrainInfo: %x (!= 0)\n", pTerrainInfo ),
							  return false );

	CTRect<int> boundingRect( 0, 0, pTerrainInfo->tiles.GetSizeX(), pTerrainInfo->tiles.GetSizeY() );
	CTRect<float> boundingBox = GetPolygonBoundingBox( rInclusivePolygon );
	CTRect<int> indices( ( boundingBox.minx + ( fWorldCellSize / 2.0f ) ) / fWorldCellSize,
											 ( boundingBox.miny + ( fWorldCellSize / 2.0f ) ) / fWorldCellSize,
											 ( boundingBox.maxx + ( fWorldCellSize / 2.0f ) ) / fWorldCellSize,
											 ( boundingBox.maxy + ( fWorldCellSize / 2.0f ) ) / fWorldCellSize );
	//���������
	if ( ValidateIndices( boundingRect, &indices ) < 0 )
	{
		return false;
	}

	std::vector<SVAPattern> patterns;
	for ( int nPatternSize = rPatternSize.a; nPatternSize <= rPatternSize.b; ++nPatternSize )
	{
		patterns.push_back( SVAPattern() );
		patterns.back().CreateFromGradient( rGradient, nPatternSize * 2 );
	}
	
	CTRect<int> mapAaltitudeRect( 0, 0, pTerrainInfo->altitudes.GetSizeX(), pTerrainInfo->altitudes.GetSizeY() );
	SVAAddPatternFunctional addPatternFunctional( &( pTerrainInfo->altitudes ) );
	SVASubstractPatternFunctional substractPatternFunctional( &( pTerrainInfo->altitudes ) );
	
	//��������� �� ������
	//
	for ( int nYIndex = indices.miny; nYIndex < indices.maxy; ++nYIndex )
	{
		std::vector<int> xpos;
		int nCount = GetPolygonLine( nYIndex, fWorldCellSize, rInclusivePolygon, &xpos );
		if ( ( nCount > 0 ) && ( ( nCount & 0x1 ) == 0 ) )
		{
			std::vector<int>::const_iterator startXPosIterator = xpos.begin();
			std::vector<int>::const_iterator finishXPosIterator = xpos.begin();
			++finishXPosIterator;
			while ( true )
			{
				for ( int nXIndex = ( *startXPosIterator ); nXIndex <= ( *finishXPosIterator ); ++nXIndex )
				{
					float fInclusiveDistance = 0.0f;
					if ( pDistances )
					{
						const LPARAM lParam = MAKELPARAM( nXIndex, nYIndex );
						std::unordered_map<LPARAM, float>::const_iterator distanceIterator = pDistances->find( MAKELPARAM( nXIndex, nYIndex ) );
						if ( distanceIterator != pDistances->end() )
						{
							fInclusiveDistance = distanceIterator->second;
						}
						else
						{
							const CVec2	vTileCenter( ( nXIndex * fWorldCellSize ) + ( fWorldCellSize / 2.0f ),
																			 ( nYIndex * fWorldCellSize ) + ( fWorldCellSize / 2.0f ) );
							fInclusiveDistance = GetInclusivePolygonSetDistance( vTileCenter, rInclusivePolygon, rExclusivePolygons );
							( *pDistances )[lParam] = fInclusiveDistance;
						}
					}
					else
					{
						const CVec2	vTileCenter( ( nXIndex * fWorldCellSize ) + ( fWorldCellSize / 2.0f ),
																		 ( nYIndex * fWorldCellSize ) + ( fWorldCellSize / 2.0f ) );
						fInclusiveDistance = GetInclusivePolygonSetDistance( vTileCenter, rInclusivePolygon, rExclusivePolygons );
					}
					//
					if ( fInclusiveDistance >= 0 )
					{
						fInclusiveDistance /= fWorldCellSize;

						int nPatternIndex = Random( patterns.size() );
						SVAPattern &rPattern = patterns[nPatternIndex];
						
						if ( fInclusiveDistance > ( rPattern.heights.GetSizeX() / 2 ) )
						{
							CTPoint<int> cornerTile( nXIndex - ( rPattern.heights.GetSizeX() / 2 - 1 ),
																			 ( pTerrainInfo->tiles.GetSizeY() - nYIndex - 1 ) - ( rPattern.heights.GetSizeY() / 2 - 1 ) );
							rPattern.pos = cornerTile;
							if ( Random( 0.0f, 1.0f ) < fPositiveRatio )
							{
								ApplyVAPattern( mapAaltitudeRect, rPattern, addPatternFunctional, true );
							}
							else
							{
								ApplyVAPattern( mapAaltitudeRect, rPattern, substractPatternFunctional, true );
							}
						}
					}
				}
				//
				startXPosIterator += 2;
				if ( startXPosIterator == xpos.end() )
				{
					break;
				}
				finishXPosIterator += 2;
				//
			}
		}
	}
	return true;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CMapInfo::CreateRandomMap( SMissionStats *pMissionStats, const std::string &rszContextFileName, int nLevel,  int nGraph, int nAngle, bool bSaveAsBZM, bool bSaveAsDDS, SRMUsedTemplateInfo *pRMUsedTemplateInfo, IProgressHook *pProgressHook )
{
	//TIME KEEPER
	STraceTimeKeeper timeKeeper;
	
	NI_ASSERT_TF( pMissionStats != 0,
								NStr::Format( "CreateRandomMap, invalid parameter pMissionStats %x", pMissionStats ), 
								return false );
	
	NStr::DebugTrace( "CreateRandomMap, Setting: %s\n"
										"CreateRandomMap, Context: %s\n"
										"CreateRandomMap, Template: %s\n"
										"CreateRandomMap, Name: %s\n"
										"CreateRandomMap, Level: %d\n"
										"CreateRandomMap, Graph: %d\n"
										"CreateRandomMap, Angle: %d\n"
										"CreateRandomMap, bSaveAsBZM: %d\n",
										pMissionStats->szSettingName.c_str(),
										rszContextFileName.c_str(),
										pMissionStats->szTemplateMap.c_str(),
										pMissionStats->szFinalMap.c_str(),
										nLevel,
										nGraph,
										nAngle,
										bSaveAsBZM );
	if ( pRMUsedTemplateInfo )
	{
		pRMUsedTemplateInfo->szTemplateName = pMissionStats->szTemplateMap;
		pRMUsedTemplateInfo->szContextName = rszContextFileName;
		pRMUsedTemplateInfo->szSettingName = pMissionStats->szSettingName;
	}

	IObjectsDB *pIDB = GetSingleton<IObjectsDB>();
	IDataStorage *pDataStorage = GetSingleton<IDataStorage>();
	IRandomGen *pRandomGen = GetSingleton<IRandomGen>();
	IImageProcessor *pImageProcessor = GetImageProcessor();
	NI_ASSERT_TF( ( pIDB != 0 ) && ( pDataStorage != 0 ) && ( pRandomGen != 0 ) && ( pImageProcessor != 0 ),
								NStr::Format( "CreateRandomMap, GetSingleton<IObjectsDB>(); = %x, GetSingleton<IDataStorage>() = %x, GetSingletone<IRandomGen>() = %x, etImageProcessor() = %x", pIDB, pDataStorage, pRandomGen, pImageProcessor ), 
								return false );
	
	//PROGRESS_HOOK
	if ( pProgressHook )
	{
		pProgressHook->Step();
	}

	//������ template
	SRMTemplate randomMapTemplate;
	bool bResult = LoadDataResource( pMissionStats->szTemplateMap, "", false, 0, RMGC_TEMPLATE_XML_NAME, randomMapTemplate );
	NI_ASSERT_TF( bResult,
								NStr::Format( "CreateRandomMap, Can't load SRMTemplate from %s", pMissionStats->szTemplateMap.c_str() ), 
								return false );
	
	//������ �������
	SRMSetting missionSetting;
	if ( LoadDataResource( pMissionStats->szSettingName, "", false, 0, RMGC_SETTING_NAME, missionSetting ) )
	{
		for ( int nAdditionalFieldIndex = 0; nAdditionalFieldIndex < missionSetting.fields.size(); ++nAdditionalFieldIndex )
		{
			std::string szToCompare0 = missionSetting.fields[nAdditionalFieldIndex];
			NStr::ToLower( szToCompare0 );

			bool bFieldNotPresent = true;
			for ( int nFieldIndex = 0;nFieldIndex < randomMapTemplate.fields.size(); ++nFieldIndex )
			{
				std::string szToCompare1 = randomMapTemplate.fields[nFieldIndex];
				NStr::ToLower( szToCompare1 );
				if ( szToCompare0 == szToCompare1 )
				{
					bFieldNotPresent = false;
					break;
				}
			}
			if ( bFieldNotPresent )
			{
				randomMapTemplate.fields.push_back( missionSetting.fields[nAdditionalFieldIndex],
																						missionSetting.fields.GetWeight( nAdditionalFieldIndex ) );
			}
		}
	}

	std::string szRandomMapName;
	if ( ( pMissionStats->szFinalMap.size() < 2 ) || ( pMissionStats->szFinalMap[1] != ':' ) )
	{
		szRandomMapName = pDataStorage->GetName() + std::string( "maps\\" ) + pMissionStats->szFinalMap;
	}
	else
	{
		szRandomMapName = pMissionStats->szFinalMap;
	}
	
	//��������� ������������ ���������� �������� �����!
	//����� � ������ ����������� � CMapInfo::Create()
	NI_ASSERT_TF( ( randomMapTemplate.nDefaultFieldIndex >= 0 ) && ( randomMapTemplate.nDefaultFieldIndex < randomMapTemplate.fields.size() ),
								NStr::Format( "CreateRandomMap,  invalid nDefaultFieldIndex %d [%d...%d]", randomMapTemplate.nDefaultFieldIndex, 0, randomMapTemplate.fields.size() ), 
								return false );

	//��������� random seed
	{
		CPtr<IRandomGenSeed> pRandomGenSeed = pRandomGen->GetSeed();
		NI_ASSERT_TF( pRandomGenSeed != 0,
									NStr::Format( "CreateRandomMap, Can't get Random Seed by pRandomGen->GetSeed()" ), 
									return false );
		CPtr<IDataStream> pRandomSeedStream = CreateFileStream( ( szRandomMapName + ".seed" ).c_str(), STREAM_ACCESS_WRITE );
		NI_ASSERT_TF( pRandomSeedStream != 0,
									NStr::Format( "CreateRandomMap, Can't create stream: %s", ( szRandomMapName + ".seed" ).c_str() ), 
									return false );
		pRandomGenSeed->Store( pRandomSeedStream );
	}	

	//������� �����
	CMapInfo mapInfo;
	//CRAP{����������� �������
	bResult = mapInfo.Create( randomMapTemplate.size, randomMapTemplate.nSeason, randomMapTemplate.szSeasonFolder, 0, randomMapTemplate.nType );
	//CRAP}����������� �������
	NI_ASSERT_TF( bResult != 0,
								NStr::Format( "CreateRandomMap, Can't get Random Seed by pRandomGen->GetSeed()" ), 
								return false );
	if ( !bResult )
	{
		return false;	
	}

	//TIME KEEPER
	timeKeeper.Trace("CreateRandomMap. Create.");
	
	//PROGRESS_HOOK
	if ( pProgressHook )
	{
		pProgressHook->Step();
	}

	//�������� ����������� ����������� ��� ���������� terrain
	STilesetDesc tilesetDesc;
	bResult = LoadDataResource( mapInfo.terrain.szTilesetDesc, "", false, 1, "tileset", tilesetDesc );
	NI_ASSERT_TF( bResult,
								NStr::Format( "CreateRandomMap, can't create file stream: %s", ( mapInfo.terrain.szTilesetDesc + ".xml" ).c_str() ), 
								return false );
	SCrossetDesc crossetDesc;
	bResult = LoadDataResource( mapInfo.terrain.szCrossetDesc, "", false, 1, "crosset", crossetDesc );
	NI_ASSERT_TF( bResult,
								NStr::Format( "CreateRandomMap, can't create file stream: %s", ( mapInfo.terrain.szCrossetDesc + ".xml" ).c_str() ), 
								return false );
	const int nSelectedSeason = mapInfo.GetSelectedSeason();
	/**
	SRoadsetDesc roadsetDesc;
	bResult = LoadDataResource( mapInfo.terrain.szRoadsetDesc, "", false, 1, "roadset", roadsetDesc );
	NI_ASSERT_TF( bResult != 0,
								NStr::Format( "CreateRandomMap, can't create file stream: %s", ( mapInfo.terrain.szRoadsetDesc + ".xml" ).c_str() ), 
								return false );
	/**/

	//���������� ��������� �� ���������
	mapInfo.szScriptFile = szRandomMapName;
	mapInfo.diplomacies = randomMapTemplate.diplomacies;
	mapInfo.playersCameraAnchors.clear();
	mapInfo.playersCameraAnchors.resize( mapInfo.diplomacies.size() - 1, VNULL3 );
	mapInfo.unitCreation = randomMapTemplate.unitCreation;
	mapInfo.szForestCircleSounds = randomMapTemplate.szForestCircleSounds;
	mapInfo.szForestAmbientSounds = randomMapTemplate.szForestAmbientSounds;

	mapInfo.szChapterName = randomMapTemplate.szChapterName;
	mapInfo.nMissionIndex = randomMapTemplate.nMissionIndex;
	
	mapInfo.szMODName = randomMapTemplate.szChapterName;
	mapInfo.szMODVersion = randomMapTemplate.szMODVersion;

	std::list<CVec2> mapVisPointsPolygon;
	mapVisPointsPolygon.push_back( CVec2( 0, 0 ) );
	mapVisPointsPolygon.push_back( CVec2( 0, mapInfo.terrain.tiles.GetSizeY() * fWorldCellSize ) );
	mapVisPointsPolygon.push_back( CVec2( mapInfo.terrain.tiles.GetSizeX() * fWorldCellSize, mapInfo.terrain.tiles.GetSizeY() * fWorldCellSize ) );
	mapVisPointsPolygon.push_back( CVec2( mapInfo.terrain.tiles.GetSizeX() * fWorldCellSize, 0 ) );
	std::list<CVec2> smallMapVisPointsPolygon;
	EnlargePolygonCore<std::list<CVec2>, CVec2>( mapVisPointsPolygon, mapVisPointsPolygon, ( ( -1.0f ) * fWorldCellSize / 2.0f ), &smallMapVisPointsPolygon );
	const CTRect<int> terrainPatchesRect( 0, 0,	mapInfo.terrain.patches.GetSizeX(), mapInfo.terrain.patches.GetSizeY() );
	const CTRect<int> terrainTilesRect( 0, 0,	mapInfo.terrain.tiles.GetSizeX(), mapInfo.terrain.tiles.GetSizeY() );
	const CTPoint<float> terrainTilesCenterPoint( ( mapInfo.terrain.tiles.GetSizeX() - 1 ) / 2.0f, ( mapInfo.terrain.tiles.GetSizeY() - 1 ) / 2.0f );

	CTRect<int> mapAaltitudeRect( 0, 0, mapInfo.terrain.altitudes.GetSizeX(), mapInfo.terrain.altitudes.GetSizeY() );
	SVASetMaxPatternFunctional vaSetMaxPatternFunctional( &( mapInfo.terrain.altitudes ) );

	SVAGradient vsoGradient;
	{
		SRMLevelVSOParameter levelParameter;
		bResult = LoadDataResource( mapInfo.szSeasonFolder + RMGC_ROAD_LEVEL_FILE_NAME, "", false, 0, RMGC_RM_LEVEL_VSO_PARAMETER_NAME, levelParameter );
		NI_ASSERT_T( bResult,
								 NStr::Format( "CreateRandomMap, can't open road level file: %s", ( mapInfo.szSeasonFolder + std::string( RMGC_ROAD_LEVEL_FILE_NAME ) + ".xml" ).c_str() ) );

		CPtr<IDataStream> pImageStream = GetSingleton<IDataStorage>()->OpenStream( ( std::string( levelParameter.szProfileFileName ) + ".tga" ).c_str(), STREAM_ACCESS_READ );
		if ( pImageStream != 0 )
		{
			if ( CPtr<IImage> pImage = pImageProcessor->LoadImage( pImageStream ) )
			{ 
				vsoGradient.CreateFromImage( pImage, CTPoint<float>( 0.0f, 1.0f ), CTPoint<float>( 0.0f, 1.0f ) );
			}
		}
	}

	//�������� fieldSet'�
	std::unordered_map<std::string, SRMFieldSet> fieldSetsHashMap;
	{
		SRMFieldSet fieldSet;
		bResult = LoadDataResource( randomMapTemplate.fields[randomMapTemplate.nDefaultFieldIndex], "", false, 1, RMGC_FIELDSET_XML_NAME, fieldSet );
		NI_ASSERT_TF( bResult,
									NStr::Format( "CreateRandomMap, can't create file stream: %s", ( randomMapTemplate.fields[randomMapTemplate.nDefaultFieldIndex] + ".xml" ).c_str() ), 
									return false );
		fieldSet.ValidateFieldSet( tilesetDesc, CMapInfo::MOST_COMMON_TILES[nSelectedSeason] );
		fieldSetsHashMap[ randomMapTemplate.fields[randomMapTemplate.nDefaultFieldIndex] ] = fieldSet;
	}

	//��������� ����� �������������� ��������
	{
		std::list<std::list<CVec2> > exclusivePolygons;
		const SRMFieldSet &rDefualtFieldSet = fieldSetsHashMap[ randomMapTemplate.fields[randomMapTemplate.nDefaultFieldIndex] ];
		CMapInfo::FillTileSet( &( mapInfo.terrain ), tilesetDesc, mapVisPointsPolygon, exclusivePolygons, rDefualtFieldSet.tilesShells, 0 );
	}	
	
	//TIME KEEPER
	timeKeeper.Trace( "CreateRandomMap. Fill default field." );

	//PROGRESS_HOOK
	if ( pProgressHook )
	{
		pProgressHook->Step();
	}

	//�������� ����
	std::string szSelectedGraphFileName;
	SRMGraph selectedGraph;
	if ( nGraph >= 0 )
	{
		NI_ASSERT_T( ( nGraph >= 0 )  && ( nGraph < randomMapTemplate.graphs.size() ), 
								 NStr::Format( "CreateRandomMap, Invalid Graph index: %d in [0...%d], press CONTINUE, to use random namber, SRMTemplate from %s",
															 nGraph,
															 ( randomMapTemplate.graphs.size() - 1 ),
															 pMissionStats->szTemplateMap.c_str() ) );
	}
	if ( ( nGraph >= 0 )  && ( nGraph < randomMapTemplate.graphs.size() ) )
	{
		szSelectedGraphFileName = randomMapTemplate.graphs[nGraph];
	}
	else
	{
		szSelectedGraphFileName = randomMapTemplate.graphs.GetRandom( true );
	}

	if ( pRMUsedTemplateInfo )
	{
		pRMUsedTemplateInfo->szGraphName = szSelectedGraphFileName;
	}

	bResult = LoadDataResource( szSelectedGraphFileName, "", false, 1, RMGC_GRAPH_XML_NAME, selectedGraph );
	NI_ASSERT_TF( bResult,
								NStr::Format( "CreateRandomMap, can't create file stream: %s", ( szSelectedGraphFileName + ".xml" ).c_str() ), 
									return false );

	//�������� ����
	const int nCosAngles[] = { 1, 0, -1, 0 };
	const int nSinAngles[] = { 0, -1, 0, 1 };
	int nSelectedAngle = 0;
	if ( nAngle >= 0 )
	{
		NI_ASSERT_T( ( nAngle >= 0 ) && ( nAngle < 4 ),
								 NStr::Format( "CreateRandomMap, Invalid Angle index: %d in [0...3], press CONTINUE, to use random namber, SRMTemplate from %s",
															 nAngle,
															 pMissionStats->szTemplateMap.c_str() ) );
	}
	if ( ( nAngle >= 0 ) && ( nAngle < 4 ) )
	{
		nSelectedAngle = nAngle;
	}
	else
	{
		nSelectedAngle = Random( 4 );
	}

	if ( pRMUsedTemplateInfo )
	{
		pRMUsedTemplateInfo->nGraphAngle = nSelectedAngle;
	}

	NI_ASSERT_TF( ( nSelectedAngle >= 0 ) && ( nSelectedAngle < 4 ),
								NStr::Format( "CreateRandomMap, invalid Angle: %d", nSelectedAngle ), 
								return false );
	const int fCosSelecteAngle = nCosAngles[nSelectedAngle];
	const int fSinSelecteAngle = nSinAngles[nSelectedAngle];

	//������������ ���������� � vAppearPoints
	{
		CVec3 vAICenter( ( ( mapInfo.terrain.tiles.GetSizeX() * SAIConsts::TILE_SIZE * 2.0f ) - 1.0f ) / 2.0f,
										 ( ( mapInfo.terrain.tiles.GetSizeY() * SAIConsts::TILE_SIZE * 2.0f ) - 1.0f ) / 2.0f,
										 0.0 );
		for ( std::vector<SUnitCreation>::iterator unitCreationIterator = mapInfo.unitCreation.units.begin(); unitCreationIterator != mapInfo.unitCreation.units.end(); ++unitCreationIterator )
		{
			for ( std::list<CVec3>::iterator appearPointIterator = unitCreationIterator->aviation.vAppearPoints.begin(); appearPointIterator != unitCreationIterator->aviation.vAppearPoints.end(); ++appearPointIterator )
			{
				CVec3 vPoint = ( *appearPointIterator ) - vAICenter;
				CVec3 vRotatedPoint( ( vPoint.x * fCosSelecteAngle ) - ( vPoint.y * fSinSelecteAngle ),
														 ( vPoint.x * fSinSelecteAngle ) + ( vPoint.y * fCosSelecteAngle ),
														 vPoint.z ); 
				( *appearPointIterator ) = vRotatedPoint + vAICenter;
			}
		}
	}	
	
	//��������� �����
	std::unordered_map<std::string, SRMContainer> containersHashMap;
	//��� �������� �����
	std::vector<SRMPlacedPatch> placedPatches;
	//��� ����������� �����
	CRMFieldGraph fieldGraph;
	for ( int nNodeIndex = 0; nNodeIndex < selectedGraph.nodes.size(); ++nNodeIndex )
	{
		const SRMGraphNode &rGraphNode = selectedGraph.nodes[nNodeIndex];
		//������ ���������
		if ( containersHashMap.find( rGraphNode.szContainerFileName ) == containersHashMap.end() )
		{
			SRMContainer container;
			bResult = LoadDataResource( rGraphNode.szContainerFileName, "", false, 1, RMGC_CONTAINER_XML_NAME, container );
			NI_ASSERT_TF( bResult,
										NStr::Format( "CreateRandomMap, can't create file stream: %s", ( rGraphNode.szContainerFileName + ".xml" ).c_str() ), 
										return false );
			containersHashMap[rGraphNode.szContainerFileName] = container;
		}
		const SRMContainer &rContainer = containersHashMap[rGraphNode.szContainerFileName];

		//�������� ����:
		std::vector<int> availiableIndices;
		rContainer.GetIndices( nSelectedAngle, pMissionStats->szSettingName, &availiableIndices );
		NI_ASSERT_TF( !availiableIndices.empty(),
									NStr::Format( "CreateRandomMap, can't find any patch in Container %s, Angle: %d, Place: %s",
																( rGraphNode.szContainerFileName + ".xml" ).c_str(),
																nSelectedAngle,
																pMissionStats->szSettingName.c_str() ), 
									return false );
		if ( availiableIndices.empty() )
		{
			return false;
		}
		placedPatches.push_back( rContainer.patches[ availiableIndices[Random( availiableIndices.size() )] ] );
		SRMPlacedPatch &rPlacedPatch = placedPatches[nNodeIndex];

		const CTRect<float> nodeRect( rGraphNode.rect.minx - terrainTilesCenterPoint.x,
																	rGraphNode.rect.miny - terrainTilesCenterPoint.y,
																	( rGraphNode.rect.maxx - 1 ) - terrainTilesCenterPoint.x,
																	( rGraphNode.rect.maxy - 1 ) - terrainTilesCenterPoint.y );
		CTRect<int> rotatedNodeRect( terrainTilesCenterPoint.x + ( nodeRect.minx * fCosSelecteAngle ) - ( nodeRect.miny * fSinSelecteAngle ),
																 terrainTilesCenterPoint.y + ( nodeRect.minx * fSinSelecteAngle ) + ( nodeRect.miny * fCosSelecteAngle ),
																 terrainTilesCenterPoint.x + ( nodeRect.maxx * fCosSelecteAngle ) - ( nodeRect.maxy * fSinSelecteAngle ),
																 terrainTilesCenterPoint.y + ( nodeRect.maxx * fSinSelecteAngle ) + ( nodeRect.maxy * fCosSelecteAngle ) );
		rotatedNodeRect.Normalize();
		rotatedNodeRect.maxx += 1;
		rotatedNodeRect.maxy += 1;
		NI_ASSERT_TF( ( rotatedNodeRect.Width() >= ( rPlacedPatch.size.x * STerrainPatchInfo::nSizeX ) ) &&
									( rotatedNodeRect.Height() >= ( rPlacedPatch.size.y * STerrainPatchInfo::nSizeY ) ),
									NStr::Format( "CreateRandomMap, invalid container rect: %s, angle %d, patch: %s, patch size [%dx%d], place (%d, %d, %d, %d) [%dx%d]",
																rGraphNode.szContainerFileName.c_str(),
																nSelectedAngle,
																rPlacedPatch.szFileName.c_str(),
																rPlacedPatch.size.x,
																rPlacedPatch.size.y,
																rotatedNodeRect.minx,
																rotatedNodeRect.miny,
																rotatedNodeRect.maxx,
																rotatedNodeRect.maxy,
																rotatedNodeRect.Width(),
																rotatedNodeRect.Height() ),
									return false );
		rPlacedPatch.minXYCorner.x = rotatedNodeRect.minx + Random( rotatedNodeRect.Width() - ( rPlacedPatch.size.x * STerrainPatchInfo::nSizeX ) + 1 );
		rPlacedPatch.minXYCorner.y = rotatedNodeRect.miny + Random( rotatedNodeRect.Height() - ( rPlacedPatch.size.y * STerrainPatchInfo::nSizeY ) + 1 );
		
		fieldGraph.AddPatch( CTRect<int>( rPlacedPatch.minXYCorner.x,
																			rPlacedPatch.minXYCorner.y,
																			rPlacedPatch.minXYCorner.x + rPlacedPatch.size.x * STerrainPatchInfo::nSizeX,
																			rPlacedPatch.minXYCorner.y + rPlacedPatch.size.y * STerrainPatchInfo::nSizeY ) );
		
		//��������� ���������� � �����
		CMapInfo patchMapInfo;
		bResult = LoadTypedSuperLatestDataResource( rPlacedPatch.szFileName, ".bzm", 1, patchMapInfo );
		NI_ASSERT_TF( bResult,
									NStr::Format( "CreateRandomMap, can't create file stream: %s", ( rPlacedPatch.szFileName + ".xml" ).c_str() ), 
									return false );
		//��������� ����
		patchMapInfo.UnpackFrameIndices();
		bResult = mapInfo.AddMapInfo( rPlacedPatch.minXYCorner, patchMapInfo );
		NI_ASSERT_TF( bResult,
									NStr::Format( "CreateRandomMap, can't add MapInfo: %s", ( rPlacedPatch.szFileName + ".xml" ).c_str() ), 
									return false );
		
		//����������� vCamera Anchor
		for ( int nPlayerIndex = 0; nPlayerIndex < mapInfo.playersCameraAnchors.size(); ++nPlayerIndex )
		{
			CVec3 &rPlayerCameraAnchore = mapInfo.playersCameraAnchors[nPlayerIndex];
			if ( rPlayerCameraAnchore == VNULL3 )
			{
				if ( ( nPlayerIndex < patchMapInfo.playersCameraAnchors.size() ) &&
						 ( patchMapInfo.playersCameraAnchors[nPlayerIndex] != VNULL3 ) )
				{
					rPlayerCameraAnchore = patchMapInfo.playersCameraAnchors[nPlayerIndex] + CVec3( fieldGraph.GetPatchMinXYVertex( nNodeIndex ), 0.0f );
				}
			}
		}
		if ( mapInfo.vCameraAnchor == VNULL3 )
		{
			if ( patchMapInfo.vCameraAnchor != VNULL3 )
			{
				mapInfo.vCameraAnchor = patchMapInfo.vCameraAnchor + CVec3( fieldGraph.GetPatchMinXYVertex( nNodeIndex ), 0.0f );
			}
		}

		//�������������� ������ �������� � ������ connectionPoints
		//��� ������ ������� � ������� ������� �� �� ������ ������� � ���������
		//����� ��������� ������������ ����� ��������� �� ���� �����
		std::list<CVec2> patchVisPointsPolygon;
		patchVisPointsPolygon.push_back( CVec2( 0, 0 ) + fieldGraph.GetPatchMinXYVertex( nNodeIndex ) );
		patchVisPointsPolygon.push_back( CVec2( 0, patchMapInfo.terrain.tiles.GetSizeY() * fWorldCellSize ) + fieldGraph.GetPatchMinXYVertex( nNodeIndex ) );
		patchVisPointsPolygon.push_back( CVec2( patchMapInfo.terrain.tiles.GetSizeX() * fWorldCellSize, patchMapInfo.terrain.tiles.GetSizeY() * fWorldCellSize ) + fieldGraph.GetPatchMinXYVertex( nNodeIndex ) );
		patchVisPointsPolygon.push_back( CVec2( patchMapInfo.terrain.tiles.GetSizeX() * fWorldCellSize, 0 ) + fieldGraph.GetPatchMinXYVertex( nNodeIndex ) );
		std::list<CVec2> smallPatchVisPointsPolygon;
		EnlargePolygonCore<std::list<CVec2>, CVec2>( mapVisPointsPolygon, patchVisPointsPolygon, ( ( -1.0f ) * fWorldCellSize / 2.0f ), &smallPatchVisPointsPolygon );

		//����� ������� �� ������� �������� �������� VIS ����� 
		//����
		for ( int nRiverIndex = 0; nRiverIndex < patchMapInfo.terrain.rivers.size(); ++nRiverIndex )
		{
			NI_ASSERT_T( !patchMapInfo.terrain.rivers[nRiverIndex].controlpoints.empty(),
									 NStr::Format( "CreateRandomMap, invalid river: %d", nRiverIndex ) );
			//begin
			{
				const CVec2 vPos = CVec2( patchMapInfo.terrain.rivers[nRiverIndex].controlpoints[0].x, patchMapInfo.terrain.rivers[nRiverIndex].controlpoints[0].y ) + fieldGraph.GetPatchMinXYVertex( nNodeIndex );
				const EClassifyPolygon classifyMapPolygon = ClassifyPolygon( smallMapVisPointsPolygon, vPos );
				const EClassifyPolygon classifyPatchPolygon = ClassifyPolygon( smallPatchVisPointsPolygon, vPos );
				if ( ( classifyMapPolygon != CP_OUTSIDE ) && ( classifyPatchPolygon != CP_INSIDE ) )
				{
					SRMPlacedPatch::SVSOPoint vsoPoint;
					vsoPoint.nID = mapInfo.terrain.rivers.size() - patchMapInfo.terrain.rivers.size() + nRiverIndex;
					vsoPoint.bBegin = true;
					vsoPoint.vPos = vPos;
					vsoPoint.szVSODescFileName = patchMapInfo.terrain.rivers[nRiverIndex].szDescName;
					NStr::ToLower( vsoPoint.szVSODescFileName );
					rPlacedPatch.riversPoints.push_back( vsoPoint );
				}
			}
			//end
			{
				const CVec2 vPos = CVec2( patchMapInfo.terrain.rivers[nRiverIndex].controlpoints[patchMapInfo.terrain.rivers[nRiverIndex].controlpoints.size() - 1].x, patchMapInfo.terrain.rivers[nRiverIndex].controlpoints[patchMapInfo.terrain.rivers[nRiverIndex].controlpoints.size() - 1].y ) + fieldGraph.GetPatchMinXYVertex( nNodeIndex );
				const EClassifyPolygon classifyMapPolygon = ClassifyPolygon( smallMapVisPointsPolygon, vPos );
				const EClassifyPolygon classifyPatchPolygon = ClassifyPolygon( smallPatchVisPointsPolygon, vPos );
				if ( ( classifyMapPolygon != CP_OUTSIDE ) && ( classifyPatchPolygon != CP_INSIDE ) )
				{
					SRMPlacedPatch::SVSOPoint vsoPoint;
					vsoPoint.nID = mapInfo.terrain.rivers.size() - patchMapInfo.terrain.rivers.size() + nRiverIndex;
					vsoPoint.bBegin = false;
					vsoPoint.vPos = vPos;
					vsoPoint.szVSODescFileName = patchMapInfo.terrain.rivers[nRiverIndex].szDescName;
					NStr::ToLower( vsoPoint.szVSODescFileName );
					rPlacedPatch.riversPoints.push_back( vsoPoint );
				}
			}
		}
		//������
		for ( int nRoad3DIndex = 0; nRoad3DIndex < patchMapInfo.terrain.roads3.size(); ++nRoad3DIndex )
		{
			NI_ASSERT_T( !patchMapInfo.terrain.roads3[nRoad3DIndex].controlpoints.empty(),
									 NStr::Format( "CreateRandomMap, invalid road3D: %d", nRoad3DIndex ) );
			//begin
			{
				const CVec2 vPos = CVec2( patchMapInfo.terrain.roads3[nRoad3DIndex].controlpoints[0].x, patchMapInfo.terrain.roads3[nRoad3DIndex].controlpoints[0].y ) + fieldGraph.GetPatchMinXYVertex( nNodeIndex );
				const EClassifyPolygon classifyMapPolygon = ClassifyPolygon( smallMapVisPointsPolygon, vPos );
				const EClassifyPolygon classifyPatchPolygon = ClassifyPolygon( smallPatchVisPointsPolygon, vPos );
				if ( ( classifyMapPolygon != CP_OUTSIDE ) && ( classifyPatchPolygon != CP_INSIDE ) )
				{
					SRMPlacedPatch::SVSOPoint vsoPoint;
					vsoPoint.nID = mapInfo.terrain.roads3.size()  - patchMapInfo.terrain.roads3.size() + nRoad3DIndex;
					vsoPoint.bBegin = true;
					vsoPoint.vPos = vPos;
					vsoPoint.szVSODescFileName = patchMapInfo.terrain.roads3[nRoad3DIndex].szDescName;
					NStr::ToLower( vsoPoint.szVSODescFileName );
					rPlacedPatch.roadsPoints.push_back( vsoPoint );
				}
			}
			//end
			{
				const CVec2 vPos = CVec2( patchMapInfo.terrain.roads3[nRoad3DIndex].controlpoints[patchMapInfo.terrain.roads3[nRoad3DIndex].controlpoints.size() - 1].x, patchMapInfo.terrain.roads3[nRoad3DIndex].controlpoints[patchMapInfo.terrain.roads3[nRoad3DIndex].controlpoints.size() - 1].y ) + fieldGraph.GetPatchMinXYVertex( nNodeIndex );
				const EClassifyPolygon classifyMapPolygon = ClassifyPolygon( smallMapVisPointsPolygon, vPos );
				const EClassifyPolygon classifyPatchPolygon = ClassifyPolygon( smallPatchVisPointsPolygon, vPos );
				if ( ( classifyMapPolygon != CP_OUTSIDE ) && ( classifyPatchPolygon != CP_INSIDE ) )
				{
					SRMPlacedPatch::SVSOPoint vsoPoint;
					vsoPoint.nID = mapInfo.terrain.roads3.size() - patchMapInfo.terrain.roads3.size() + nRoad3DIndex;
					vsoPoint.bBegin = false;
					vsoPoint.vPos = vPos;
					vsoPoint.szVSODescFileName = patchMapInfo.terrain.roads3[nRoad3DIndex].szDescName;
					NStr::ToLower( vsoPoint.szVSODescFileName );
					rPlacedPatch.roadsPoints.push_back( vsoPoint );
				}
			}
		}
	}

	if ( ( !mapInfo.playersCameraAnchors.empty() ) && ( mapInfo.playersCameraAnchors[0] == VNULL3 ) )
	{
		mapInfo.playersCameraAnchors[0] = mapInfo.vCameraAnchor;	
	}

	//TIME KEEPER
	timeKeeper.Trace( "CreateRandomMap. Add patches." );

	//PROGRESS_HOOK
	if ( pProgressHook )
	{
		pProgressHook->Step();
	}

	SRMContext context;
	if ( LoadDataResource( rszContextFileName, "", false, 0, RMGC_CONTEXT_NAME, context ) )
	{
		NI_ASSERT_TF( context.IsValid( SRMTemplateUnitsTable::DEFAULT_LEVELS_COUNT, mapInfo.diplomacies.size() - 1 ),
									NStr::Format( "CreateRandomMap, invalid chapter units table: %s",
																rszContextFileName.c_str() ),
									return false );
		
		//����� ������� ��� ��������������� ���������
		const SRMTemplateUnitsTable &rTemplateUnitsTable = context.levels[nLevel];
		
		//�������� UnitCreationInfo;
		for ( int nUnitCreationPlayerIndex = 0; nUnitCreationPlayerIndex < rTemplateUnitsTable.unitCreationInfo.units.size(); ++nUnitCreationPlayerIndex )
		{
			const SUnitCreation &rUnitCreation = rTemplateUnitsTable.unitCreationInfo.units[nUnitCreationPlayerIndex];
			if ( mapInfo.unitCreation.units.size() <= nUnitCreationPlayerIndex )
			{
				mapInfo.unitCreation.units.push_back( SUnitCreation() );
			}
			std::list<CVec3> vAppearPoints = mapInfo.unitCreation.units[nUnitCreationPlayerIndex].aviation.vAppearPoints;
			mapInfo.unitCreation.units[nUnitCreationPlayerIndex] = rUnitCreation;
			mapInfo.unitCreation.units[nUnitCreationPlayerIndex].aviation.vAppearPoints = vAppearPoints;
		}

		/**
		NI_ASSERT_TF( rTemplateUnitsTable.unitPlaceHolders.size() >= rTemplateUnitsTable.unitCreationInfo.units.size(),
									NStr::Format( "CreateRandomMap, Place Chapter Units <%s>, invalid unitCreationSize: %d [0...%d]",
																rszContextFileName.c_str(),							
																rTemplateUnitsTable.unitCreationInfo.units.size(),
																rTemplateUnitsTable.unitPlaceHolders.size() ),
									return false );
		/**/
		//����������� �� ��������
		CUsedLinkIDs usedlinkIDs;
		mapInfo.GetUsedLinkIDs( &usedlinkIDs );

		for ( std::vector<SMapObjectInfo>::iterator mapObjectIterator = mapInfo.objects.begin(); mapObjectIterator != mapInfo.objects.end(); )
		{
			//����� ���� ������
			int nUnitRPGType = SRMTemplateUnitsTable::INVALID_UNIT_RPG_TYPE;
			{
				CGDBPtr<SGDBObjectDesc> pDesc = pIDB->GetDesc( mapObjectIterator->szName.c_str() );
				NI_ASSERT_TF( pDesc != 0,
											NStr::Format("CreaterandomMap: Can't find DB object description for \"%s\"", mapObjectIterator->szName.c_str() ),
											return false );
				switch ( pDesc->eGameType )
				{
					case SGVOGT_UNIT:
					{
						const SUnitBaseRPGStats* pUnitBaseRPGStats = NGDB::GetRPGStats<SUnitBaseRPGStats>( pIDB, pDesc );
						if ( pUnitBaseRPGStats != 0 )
						{
							nUnitRPGType = pUnitBaseRPGStats->type;
						}
						break;
					}
					case SGVOGT_SQUAD:
					{
						const SSquadRPGStats* pSquadRPGStats = NGDB::GetRPGStats<SSquadRPGStats>( pIDB, pDesc );
						if ( pSquadRPGStats != 0 )
						{
							nUnitRPGType = pSquadRPGStats->type;
						}			
						break;
					}
				}				
			}
			
			if ( nUnitRPGType != SRMTemplateUnitsTable::INVALID_UNIT_RPG_TYPE )
			{
				//����� �����������
				NI_ASSERT_TF( mapObjectIterator->nPlayer < rTemplateUnitsTable.unitPlaceHolders.size(),
											NStr::Format( "CreateRandomMap, Place Chapter Units <%s>, invalid player number: %d [0...%d), unit: %s",
																		rszContextFileName.c_str(),
																		mapObjectIterator->nPlayer,
																		rTemplateUnitsTable.unitPlaceHolders.size(),
																		mapObjectIterator->szName.c_str() ),
											return false );
				
				//����� ������ ����� �� ������� ������������� � �����
				CRMUnitsPlaceHoldersHashMap::const_iterator unitsIterator = rTemplateUnitsTable.unitPlaceHolders[mapObjectIterator->nPlayer].find( nUnitRPGType );
				if ( unitsIterator != rTemplateUnitsTable.unitPlaceHolders[mapObjectIterator->nPlayer].end() )
				{
/**/
					NI_ASSERT_TF( unitsIterator != rTemplateUnitsTable.unitPlaceHolders[mapObjectIterator->nPlayer].end(),
												NStr::Format( "CreateRandomMap, Place Chapter Units <%s>, can't find unit RPG Type: %s ( %d ), player: %d, unit: %s",
																			rszContextFileName.c_str(),
																			SRMTemplateUnitsTable::GetUnitRPGMnemonic( nUnitRPGType ).c_str(),
																			nUnitRPGType,
																			mapObjectIterator->nPlayer,
																			mapObjectIterator->szName.c_str() ),
												return false );
					NI_ASSERT_TF( !unitsIterator->second.empty(),
												NStr::Format( "CreateRandomMap, Place Chapter Units <%s>, table entry is empty: %s ( %d ), player: %d, unit: %s",
																			rszContextFileName.c_str(),
																			SRMTemplateUnitsTable::GetUnitRPGMnemonic( nUnitRPGType ).c_str(),
																			nUnitRPGType,
																			mapObjectIterator->nPlayer,
																			mapObjectIterator->szName.c_str() ),
												return false );
/**/
					int nRandomIndex = unitsIterator->second.GetRandomIndex();
					if ( ( nRandomIndex < 0 ) || ( nRandomIndex >= unitsIterator->second.size() ) )
					{
						NStr::DebugTrace( "CreateRandomMap, Place Chapter Units, All table entries has zero weights: %s ( 0x%X ), player: %d, unit: %s\n",
															SRMTemplateUnitsTable::GetUnitRPGMnemonic( nUnitRPGType ).c_str(),
															nUnitRPGType,
															mapObjectIterator->nPlayer,
															mapObjectIterator->szName.c_str() );
						if ( ( mapObjectIterator->nScriptID != RMGC_INVALID_SCRIPT_ID_VALUE ) || 
								 ( usedlinkIDs.find( mapObjectIterator->link.nLinkID ) != usedlinkIDs.end() ) )
						{
							nRandomIndex = 0;
							NStr::DebugTrace( "CreateRandomMap, Place Chapter Units, Override unit to: %s, ( ScriptID: %d, LinkID: %d )\n",
																unitsIterator->second[nRandomIndex].c_str(),
																mapObjectIterator->nScriptID,
																mapObjectIterator->link.nLinkID );
						}
					}
					if ( ( nRandomIndex >= 0 ) && ( nRandomIndex < unitsIterator->second.size() ) )
					{
						std::string szNewName = unitsIterator->second[nRandomIndex];
						if ( nUnitRPGType > SRMTemplateUnitsTable::SQUAD_UNIT_RPG_TYPE )
						{
							CGDBPtr<SGDBObjectDesc> pDesc = pIDB->GetDesc( szNewName.c_str() );
							NI_ASSERT_TF( pDesc != 0,
														NStr::Format("CreaterandomMap: Can't find DB object description for \"%s\"", szNewName.c_str() ),
														return false );
							if ( pDesc->eGameType  == SGVOGT_SQUAD )
							{
								mapObjectIterator->nFrameIndex = 0;
							}
						}
						mapObjectIterator->szName = szNewName;
					}
					else
					{
						mapObjectIterator = mapInfo.objects.erase( mapObjectIterator );
						continue;
					}
				}
			}
			++mapObjectIterator;
		}
	}
	
	//TIME KEEPER
	timeKeeper.Trace( "CreateRandomMap. Place units." );
	
	//PROGRESS_HOOK
	if ( pProgressHook )
	{
		pProgressHook->Step();
	}
	
	//����������� ����� ��� � ����� �� ������� ���������� 
	for ( int nRiverIndex = 0; nRiverIndex < mapInfo.terrain.rivers.size(); ++nRiverIndex )
	{
		mapInfo.terrain.rivers[nRiverIndex].nID = nRiverIndex;
	}
	for ( int nRoad3DIndex = 0; nRoad3DIndex < mapInfo.terrain.roads3.size(); ++nRoad3DIndex )
	{
		mapInfo.terrain.roads3[nRoad3DIndex].nID = nRoad3DIndex;
	}
	
	//������������ ����� �� ������ ��������� ����������
	//������ ��� ��������� ��� ������, ������� ��� ����������� ( ����� �������� � ��������� �������� )
	//������� ������ �� ������� ����� �� ������, ��� ��� ����������
	std::vector<bool> bLinkPlaced;
	bLinkPlaced.resize( selectedGraph.links.size() );
	for ( int nLinkIndex = 0; nLinkIndex < selectedGraph.links.size(); ++nLinkIndex )
	{
		//CRAP{for validate invalid resources
		SRMGraphLink &rGraphLink = selectedGraph.links[nLinkIndex];
		if ( rGraphLink.nParts < 8 )
		{
			rGraphLink.nParts = 8;
		}
		//CRAP}for validate invalid resources
		bLinkPlaced[nLinkIndex] = false;
	}
	
	//����������� �����
	//������� ������ ������������ ����� ������
	//����� ����������� � ����� �������
	//����� �����
	//����� ������
	std::unordered_map<std::string, SVectorStripeObjectDesc> vsodescHashMap;
	for ( int nPSType = SRMPlacedPatch::PST_TWO; nPSType <= SRMPlacedPatch::PST_EMPTY; ++nPSType )
	{
		for ( int nLinkIndex = 0; nLinkIndex < selectedGraph.links.size(); ++nLinkIndex )
		{
			SRMGraphLink &rGraphLink = selectedGraph.links[nLinkIndex];
			if ( ( !bLinkPlaced[nLinkIndex] ) && ( ( nPSType == SRMPlacedPatch::PST_EMPTY ) || ( !rGraphLink.szDescFileName.empty() ) ) )
			{
				NI_ASSERT_T( ( rGraphLink.link.a >= 0 ) && ( rGraphLink.link.a < placedPatches.size() ) &&
										 ( rGraphLink.link.b >= 0 ) && ( rGraphLink.link.b < placedPatches.size() ),
										 NStr::Format( "CreateRandomMap, invalid link: %d, a: %d [0...%d], b: %d [0..%d]",
																	 nLinkIndex,
																	 rGraphLink.link.a,
																	 placedPatches.size(),
																	 rGraphLink.link.b,
																	 placedPatches.size() ) );
				SRMPlacedPatch &rBeginPatch = placedPatches[rGraphLink.link.a];
				SRMPlacedPatch &rEndPatch = placedPatches[rGraphLink.link.b];

				//������ ��� ��������� �����
				SRMPlacedPatch::SVSOPoint beginVSOPoint;
				SRMPlacedPatch::SVSOPoint endVSOPoint;
				NStr::ToLower( rGraphLink.szDescFileName );
				if ( SRMPlacedPatch::GetAndRemoveClosestVSOPoints( rGraphLink.nType, rBeginPatch, rEndPatch, rGraphLink.szDescFileName, nPSType, &beginVSOPoint, &endVSOPoint ) )
				{
					//������� �������
					SVectorStripeObject* pBeginVSO = const_cast<SVectorStripeObject*>( ( rGraphLink.nType != SRMGraphLink::TYPE_ROAD ) ? mapInfo.GetRiver( beginVSOPoint.nID ) : mapInfo.GetRoad3D( beginVSOPoint.nID ) );
					SVectorStripeObject* pEndVSO = const_cast<SVectorStripeObject*>( ( rGraphLink.nType != SRMGraphLink::TYPE_ROAD ) ? mapInfo.GetRiver( endVSOPoint.nID ) : mapInfo.GetRoad3D( endVSOPoint.nID ) );

					NI_ASSERT_TF( pBeginVSO != 0,
												NStr::Format( "CreateRandomMap, Can't get VSO with nID %d", beginVSOPoint.nID ), 
												return false );
					NI_ASSERT_TF( pEndVSO != 0,
												NStr::Format( "CreateRandomMap, Can't get VSO with nID %d", endVSOPoint.nID ), 
												return false );

					if ( nPSType == SRMPlacedPatch::PST_EMPTY )
					{
						rGraphLink.szDescFileName = CVSOBuilder::GetDescriptionName( pBeginVSO->szDescName, pEndVSO->szDescName );
					}
					//������ ����
					//
					// end       begin                      begin          end
					// <---------]                          [-------------->
					// *=========*----------///-------------*==============*
					// begin()   begin() + 1                rbegin() + 1   rBegin()
					//
					//
					CVec3 vBegin0 = VNULL3;
					CVec3 vEnd0 = VNULL3;
					CVec3 vBegin1 = VNULL3;
					CVec3 vEnd1 = VNULL3;

					float fBeginWidth = CVSOBuilder::DEFAULT_WIDTH;
					float fEndWidth = CVSOBuilder::DEFAULT_WIDTH;
					float fBeginHeight = CVSOBuilder::DEFAULT_HEIGHT;
					float fEndHeight = CVSOBuilder::DEFAULT_HEIGHT;
					
					//��������� ������ VSO � ������ � � �����:
					const float fBeginVSOHeight = CVSOBuilder::GetVSOEdgeHeght( mapInfo.terrain.altitudes, ( *pBeginVSO ), beginVSOPoint.bBegin, false );
					const float fEndVSOHeight = CVSOBuilder::GetVSOEdgeHeght( mapInfo.terrain.altitudes, ( *pEndVSO ), endVSOPoint.bBegin, false );

					if ( beginVSOPoint.bBegin )
					{
						vBegin1 = *( pBeginVSO->controlpoints.begin() + 1 );
						vEnd1 = *( pBeginVSO->controlpoints.begin() );
						fEndWidth = pBeginVSO->points.begin()->fWidth;
						fEndHeight = fBeginVSOHeight;
						fBeginHeight = fEndVSOHeight;

						if ( endVSOPoint.bBegin )
						{
							vBegin0 = *( pEndVSO->controlpoints.begin() + 1 );
							vEnd0 = *( pEndVSO->controlpoints.begin() );
							fBeginWidth = pEndVSO->points.begin()->fWidth;
						}
						else
						{
							vBegin0 = *( pEndVSO->controlpoints.rbegin() + 1 );
							vEnd0 = *( pEndVSO->controlpoints.rbegin() );
							fBeginWidth = pEndVSO->points.rbegin()->fWidth;
						}
					}
					else
					{
						vBegin0 = *( pBeginVSO->controlpoints.rbegin() + 1 );
						vEnd0 = *( pBeginVSO->controlpoints.rbegin() );
						fBeginWidth = pBeginVSO->points.rbegin()->fWidth;
						fEndHeight = fEndVSOHeight;
						fBeginHeight = fBeginVSOHeight;
						
						if ( endVSOPoint.bBegin )
						{
							vBegin1 = *( pEndVSO->controlpoints.begin() + 1 );
							vEnd1 = *( pEndVSO->controlpoints.begin() );
							fEndWidth = pEndVSO->points.begin()->fWidth;
						}
						else
						{
							vBegin1 = *( pEndVSO->controlpoints.rbegin() + 1 );
							vEnd1 = *( pEndVSO->controlpoints.rbegin() );
							fEndWidth = pEndVSO->points.rbegin()->fWidth;
						}
					}

					std::list<CVec2> pointsSequence;
					std::vector<std::vector<CVec2> > lockedPolygons;
					std::list<CVec2> usedPoints;

					bResult = CVSOBuilder::FindPath( CVec2( vBegin0.x, vBegin0.y ), CVec2( vEnd0.x, vEnd0.y ), false,
																					 CVec2( vBegin1.x, vBegin1.y ), CVec2( vEnd1.x, vEnd1.y ), false,
																					 rGraphLink.fRadius, rGraphLink.nParts, rGraphLink.fMinLength, rGraphLink.fDistance, rGraphLink.fDisturbance,
																					 &pointsSequence, lockedPolygons, &usedPoints, 0 );
					
					NI_ASSERT_T( bResult,
											 NStr::Format( "CreateRandomMap, Can't find path: link: %d", nLinkIndex ) );
					if ( bResult )
					{
						//�������� VSO
						if ( vsodescHashMap.find( rGraphLink.szDescFileName ) == vsodescHashMap.end() )
						{
							SVectorStripeObjectDesc vsoDesc;
							bResult = LoadDataResource( rGraphLink.szDescFileName, "", false, 0, "VSODescription", vsoDesc );
							NI_ASSERT_TF( bResult,
														NStr::Format( "CreateRandomMap, Can't load VSODesc %s", rGraphLink.szDescFileName.c_str() ), 
														return false );
							if ( !bResult )
							{
								return false;
							}
							vsodescHashMap[rGraphLink.szDescFileName] = vsoDesc;
						}

						const SVectorStripeObjectDesc &rVSODesc = vsodescHashMap[rGraphLink.szDescFileName];
						
						SVectorStripeObject newVSO;
						if ( !CVSOBuilder::CreateVSO( &newVSO, rGraphLink.szDescFileName, pointsSequence ) )
						{
							rGraphLink.szDescFileName = CVSOBuilder::GetDescriptionName( pBeginVSO->szDescName, pEndVSO->szDescName );
							CVSOBuilder::CreateVSO( &newVSO, rGraphLink.szDescFileName, pointsSequence );
						}
						CVSOBuilder::Update( &newVSO, false, CVSOBuilder::DEFAULT_STEP, CVSOBuilder::DEFAULT_WIDTH, CVSOBuilder::DEFAULT_OPACITY );

						//��������� ������, ����������� VSO
						const float fAdditionalWidth = fEndWidth - fBeginWidth;
						for ( int nPointIndex = 0; nPointIndex < newVSO.points.size(); ++nPointIndex )
						{
							newVSO.points[nPointIndex].fWidth = fBeginWidth + ( ( fAdditionalWidth * nPointIndex ) / ( ( newVSO.points.size() - 1 ) * 1.0f ) );
						}
						CVSOBuilder::Update( &newVSO, true, CVSOBuilder::DEFAULT_STEP, CVSOBuilder::DEFAULT_WIDTH, CVSOBuilder::DEFAULT_OPACITY );
							
						//�������� VSO ��� ����
						CVSOBuilder::MergeVSO( pBeginVSO, beginVSOPoint.bBegin, &newVSO, !( beginVSOPoint.bBegin ) );
						CVSOBuilder::MergeVSO( pEndVSO, endVSOPoint.bBegin, &newVSO, beginVSOPoint.bBegin );
						
						//� ������ ����� VSO ������������� ������ ������ �����������
						//�������������
						{
							const float fAdditionalHeight = fEndHeight - fBeginHeight;

							for ( int nPointIndex = 0; nPointIndex < newVSO.points.size(); ++nPointIndex )
							{
								const float fHeight = fBeginHeight + ( ( fAdditionalHeight * nPointIndex ) / ( ( newVSO.points.size() - 1 ) * 1.0f ) );

								int nVAPatternWidth = 2.0f + ( newVSO.points[nPointIndex].fWidth * rVSODesc.bottom.fRelWidth / fWorldCellSize );
								if ( nVAPatternWidth < 3 )
								{
									nVAPatternWidth = 3;
								}
								//const int nVAPatternWidth = ( ( rGraphLink.nType != SRMGraphLink::TYPE_ROAD ) ? ( 0.0f ) : ( 1.0f ) ) + ( ( newVSO.points[nPointIndex].fWidth * rVSODesc.bottom.fRelWidth / fWorldCellSize ) );
								CTPoint<int> terrainTile;
								mapInfo.GetTerrainTileIndices( newVSO.points[nPointIndex].vPos, &terrainTile );

								//������� ������� �� ������� ����� �����
								SVAPattern vaPattern;
								vaPattern.CreateFromGradient( vsoGradient, nVAPatternWidth * 2, 1.0f );
								vaPattern.pos.x = terrainTile.x - ( nVAPatternWidth - 1 );
								vaPattern.pos.y = terrainTile.y - ( nVAPatternWidth - 1 );
								vaPattern.fRatio = fHeight;
								ApplyVAPattern( mapAaltitudeRect, vaPattern, vaSetMaxPatternFunctional, false );
							}
						}

						//������� � �����
						if ( rGraphLink.nType != SRMGraphLink::TYPE_ROAD )
						{
							newVSO.nID = mapInfo.terrain.rivers.size();
							mapInfo.terrain.rivers.push_back( newVSO );
						}
						else
						{
							newVSO.nID = mapInfo.terrain.roads3.size();
							mapInfo.terrain.roads3.push_back( newVSO );
						}
						
						//������� � fieldGraph ��� ������� ������
						std::list<CVec2> pointsLeft;
						std::list<CVec2> pointsRight;

						for ( std::vector<SVectorStripeObjectPoint>::const_iterator pointIterator = newVSO.points.begin(); pointIterator != newVSO.points.end(); ++pointIterator )
						{
							if ( pointIterator->bKeyPoint )
							{
								CVec3 v0 = pointIterator->vPos - ( pointIterator->vNorm * pointIterator->fWidth * rVSODesc.bottom.fRelWidth );
								CVec3 v1 = pointIterator->vPos + ( pointIterator->vNorm * pointIterator->fWidth * rVSODesc.bottom.fRelWidth );
								pointsLeft.push_back( CVec2( v0.x, v0.y ) );
								pointsRight.push_back( CVec2( v1.x, v1.y ) );
							}
						}
						
						fieldGraph.AddLine( pointsLeft, newVSO.nID );
						fieldGraph.AddLine( pointsRight, newVSO.nID );
						
						int nLineIndexLeft = fieldGraph.GetLinesCount() - 2;
						int nLineIndexRight = fieldGraph.GetLinesCount() - 1;

						//����������� ������ ������ ������ ��������� �� � ������ ������� � � ������ �����
						bResult = fieldGraph.ConnectLineToPatch( rGraphLink.link.a, nLineIndexLeft, !( beginVSOPoint.bBegin ) ) &&
											fieldGraph.ConnectLineToPatch( rGraphLink.link.a, nLineIndexRight, !( beginVSOPoint.bBegin ) ) &&
											fieldGraph.ConnectLineToPatch( rGraphLink.link.b, nLineIndexLeft, beginVSOPoint.bBegin ) &&
											fieldGraph.ConnectLineToPatch( rGraphLink.link.b, nLineIndexRight, beginVSOPoint.bBegin );
						NI_ASSERT_TF( bResult,
													NStr::Format( "CreateRandomMap, Can't add line to fieldGraph, link index: %d", nLinkIndex ), 
													return false );
						if ( !bResult )
						{
							return false;
						}
						
						//���� ���������
						bLinkPlaced[nLinkIndex] = true;
					}
				}
			}
		}	
	}	
	
	//TIME KEEPER
	timeKeeper.Trace( "CreateRandomMap. Add links." );

	bResult = fieldGraph.FindPolygons( terrainTilesRect );
	NI_ASSERT_TF( bResult,
								NStr::Format( "CreateRandomMap(), FindPolygons() return false" ), 
								return false );
	if ( !bResult )
	{
		return false;
	}

	//TIME KEEPER
	timeKeeper.Trace( "CreateRandomMap. Find Polygons." );

	//PROGRESS_HOOK
	if ( pProgressHook )
	{
		pProgressHook->Step();
	}

	//��������� ������ ������������
	std::unordered_map<std::string, SVAGradient> gradientsHashMap;

	if ( !fieldGraph.inclusivePolygons.empty() )
	{
		//������ � ����������� �������
		CArray2D<BYTE> tileMap( mapInfo.terrain.tiles.GetSizeX() * 2, mapInfo.terrain.tiles.GetSizeY() * 2 );
		tileMap.Set( RMGC_UNLOCKED );
		CTRect<int> tileMapRect( 0, 0, tileMap.GetSizeX(), tileMap.GetSizeY() );
		ModifyTilesFunctional<CArray2D<BYTE>, BYTE> tileMapModifyTiles( RMGC_LOCKED, &tileMap );
		
		// ��������� ������ �o������ ������ (���� ������ ������ ������ ��� ���������� ���������)
		if ( !mapInfo.objects.empty() )
		{
			ApplyTilesInObjectsPassability( tileMapRect, &( mapInfo.objects[0] ), mapInfo.objects.size(), tileMapModifyTiles, true );
		}

		for ( std::list<std::list<CVec2> >::const_iterator inclusivePolygonIterator = fieldGraph.inclusivePolygons.begin(); inclusivePolygonIterator != fieldGraph.inclusivePolygons.end(); ++inclusivePolygonIterator )
		{
			std::list<std::list<CVec2> > exclusivePolygons;
			for ( std::list<std::list<CVec2> >::const_iterator exclusivePolygonIterator = fieldGraph.exclusivePolygons.begin(); exclusivePolygonIterator != fieldGraph.exclusivePolygons.end(); ++exclusivePolygonIterator )
			{
				if ( ClassifyPolygon( ( *inclusivePolygonIterator ), exclusivePolygonIterator->front() ) == CP_INSIDE )
				{
					std::list<CVec2> exclusivePolygon;
					RandomizeEdges<std::list<CVec2>, CVec2>( ( *exclusivePolygonIterator ), 100, 0.3f, CTPoint<float>( 0.2f, 0.4f ), &exclusivePolygon, 4.0f * fWorldCellSize, 12.0f * fWorldCellSize, true );
					CutByPolygonCore<std::list<CVec2>, CVec2>( exclusivePolygon, mapVisPointsPolygon, &exclusivePolygon );
					//exclusivePolygon = ( *exclusivePolygonIterator );
					exclusivePolygons.push_back( exclusivePolygon );
				}
			}
			
			std::list<CVec2> inclusivePolygon;
			RandomizeEdges<std::list<CVec2>, CVec2>( ( *inclusivePolygonIterator ), 100, 0.3f, CTPoint<float>( 0.2f, 0.4f ), &inclusivePolygon, 4.0f * fWorldCellSize, 12.0f * fWorldCellSize, true );
			CutByPolygonCore<std::list<CVec2>, CVec2>( inclusivePolygon, mapVisPointsPolygon, &inclusivePolygon );
			//inclusivePolygon = ( *inclusivePolygonIterator );
			if ( !inclusivePolygon.empty() )
			{
				const std::string szKey = randomMapTemplate.fields.GetRandom();
				if ( fieldSetsHashMap.find( szKey ) == fieldSetsHashMap.end() )
				{
					SRMFieldSet fieldSet;
					bResult = LoadDataResource( szKey, "", false, 1, RMGC_FIELDSET_XML_NAME, fieldSet );
					NI_ASSERT_TF( bResult,
												NStr::Format( "CreateRandomMap(), can't create file stream: %s", ( szKey + ".xml" ).c_str() ), 
												return false );
					fieldSet.ValidateFieldSet( tilesetDesc, CMapInfo::MOST_COMMON_TILES[nSelectedSeason] );
					fieldSetsHashMap[szKey] = fieldSet;
				}
				const SRMFieldSet &rFieldSet = fieldSetsHashMap[szKey];
			
				std::unordered_map<LPARAM, float> distances;
				CMapInfo::FillTileSet( &( mapInfo.terrain ), tilesetDesc, inclusivePolygon, exclusivePolygons, rFieldSet.tilesShells, &distances );
				CMapInfo::FillObjectSet( &mapInfo, inclusivePolygon, exclusivePolygons, rFieldSet.objectsShells, &tileMap );

				if ( gradientsHashMap.find( szKey ) == gradientsHashMap.end() )
				{
					SVAGradient gradient;

					if ( rFieldSet.fHeight > 0 )
					{
						CPtr<IDataStream> pImageStream = GetSingleton<IDataStorage>()->OpenStream( ( rFieldSet.szProfileFileName + ".tga" ).c_str(), STREAM_ACCESS_READ );
						if ( pImageStream != 0 )
						{
							if ( CPtr<IImage> pImage = pImageProcessor->LoadImage( pImageStream ) )
							{ 
								gradient.CreateFromImage( pImage, CTPoint<float>( 0.0f, 1.0f ), CTPoint<float>( 0.0f, rFieldSet.fHeight ) );
							}
						}
					}
					gradientsHashMap[szKey] = gradient;
				}

				const SVAGradient &rGradient = gradientsHashMap[szKey];
				if ( !rGradient.heights.empty() )
				{
					CMapInfo::FillProfilePattern( &( mapInfo.terrain ), inclusivePolygon, exclusivePolygons, rGradient, rFieldSet.patternSize, rFieldSet.fPositiveRatio, &distances );
				}
			}
		}
	}
	//TIME KEEPER
	timeKeeper.Trace( "CreateRandomMap. Fill polygons." );

	//PROGRESS_HOOK
	if ( pProgressHook )
	{
		pProgressHook->Step();
	}
	
	//�������� �����
	mapInfo.UpdateTerrain( terrainPatchesRect );
	mapInfo.UpdateObjects( terrainTilesRect );
	
	//TIME KEEPER
	timeKeeper.Trace( "CreateRandomMap. Update terrain." );

	//PROGRESS_HOOK
	if ( pProgressHook )
	{
		pProgressHook->Step();
	}

	//����������� ���������� objectives
	bool bOnlyOneObject = false;
	for ( std::vector<SMissionStats::SObjective>::iterator objectiveIterator = pMissionStats->objectives.begin(); objectiveIterator != pMissionStats->objectives.end(); ++objectiveIterator )
	{
		objectiveIterator->vPosOnMap = VNULL2;
		if ( ( objectiveIterator->nAnchorScriptID != RMGC_INVALID_SCRIPT_ID_VALUE ) && ( objectiveIterator->nAnchorScriptID != RMGC_DEFAULT_SCRIPT_ID_VALUE ) )
		{
			int nObjectsCount = 0;
			for ( int nObjectIndex = 0; nObjectIndex < mapInfo.objects.size(); ++nObjectIndex )
			{	
				if ( mapInfo.objects[nObjectIndex].nScriptID == objectiveIterator->nAnchorScriptID )
				{
					objectiveIterator->vPosOnMap.x += mapInfo.objects[nObjectIndex].vPos.x;
					objectiveIterator->vPosOnMap.y += mapInfo.objects[nObjectIndex].vPos.y;
					++nObjectsCount;
					if ( bOnlyOneObject )
					{
						break;
					}
				}
			}
			if ( ( !bOnlyOneObject ) || ( nObjectsCount < 1 ) )
			{
				for ( int nObjectIndex = 0; nObjectIndex < mapInfo.scenarioObjects.size(); ++nObjectIndex )
				{	
					if ( mapInfo.scenarioObjects[nObjectIndex].nScriptID == objectiveIterator->nAnchorScriptID )
					{
						objectiveIterator->vPosOnMap.x += mapInfo.objects[nObjectIndex].vPos.x;
						objectiveIterator->vPosOnMap.y += mapInfo.objects[nObjectIndex].vPos.y;
						++nObjectsCount;
						if ( bOnlyOneObject )
						{
							break;
						}
					}
				}
			}
			if ( nObjectsCount > 0 )
			{
				objectiveIterator->vPosOnMap.x *= 512.0f / ( nObjectsCount * mapInfo.terrain.tiles.GetSizeX() * SAIConsts::TILE_SIZE * 2.0f );
				objectiveIterator->vPosOnMap.y *= 512.0f / ( nObjectsCount * mapInfo.terrain.tiles.GetSizeY() * SAIConsts::TILE_SIZE * 2.0f );
				objectiveIterator->vPosOnMap.y = 512.0f - objectiveIterator->vPosOnMap.y - 1.0f;
			}
		}
	}
	
	//TIME KEEPER
	timeKeeper.Trace( "CreateRandomMap. Place objectives." );

	//��������� minimap images
	CRMImageCreateParameterList imageCreateParameterList;
	imageCreateParameterList.push_back( SRMImageCreateParameter( pMissionStats->szMapImage, CTPoint<int>( 0x200, 0x200 ), bSaveAsDDS, false, SRMImageCreateParameter::INTERMISSION_IMAGE_BRIGHTNESS, SRMImageCreateParameter::INTERMISSION_IMAGE_CONSTRAST, SRMImageCreateParameter::INTERMISSION_IMAGE_GAMMA ) ); 
	imageCreateParameterList.push_back( SRMImageCreateParameter( szRandomMapName, CTPoint<int>( 0x100, 0x100 ), bSaveAsDDS ) ); 
	bResult = mapInfo.CreateMiniMapImage( imageCreateParameterList, pProgressHook );
	NI_ASSERT_TF( bResult,
								NStr::Format( "CreateRandomMap, Can't create minimap image" ), 
								return false );
	//TIME KEEPER
	timeKeeper.Trace( "CreateRandomMap. Create minimap image." );

	//��������� �����
	mapInfo.PackFrameIndices();
	SQuickLoadMapInfo quickLoadMapInfo;
	quickLoadMapInfo.FillFromMapInfo( mapInfo );

	if ( bSaveAsBZM ) 
	{
		CPtr<IDataStream> pStreamBinary = CreateFileStream( ( szRandomMapName + ".bzm" ).c_str(), STREAM_ACCESS_WRITE );
		if ( pStreamBinary != 0 )
		{
			CPtr<IStructureSaver> pSaver = CreateStructureSaver( pStreamBinary, IStructureSaver::WRITE );
			CSaverAccessor saver = pSaver;
			saver.Add( 1, &mapInfo );
			saver.Add( RMGC_QUICK_LOAD_MAP_INFO_CHUNK_NUMBER, &quickLoadMapInfo );
		}
		else
		{
			bResult = false;
		}
	}
	else
	{
		CPtr<IDataStream> pStreamXML = CreateFileStream( ( szRandomMapName + ".xml" ).c_str(), STREAM_ACCESS_WRITE );
		if ( pStreamXML != 0 )
		{
			CPtr<IDataTree> pSaver = CreateDataTreeSaver( pStreamXML, IDataTree::WRITE );
			CTreeAccessor saver = pSaver;
			saver.AddTypedSuper( &mapInfo );
			saver.Add( RMGC_QUICK_LOAD_MAP_INFO_NAME, &quickLoadMapInfo );
		}
		else
		{
			bResult = false;
		}
	}
	NI_ASSERT_TF( bResult,
								NStr::Format( "CreateRandomMap, Can't save CMapInfo to %s", ( szRandomMapName ).c_str() ), 
								return false );
	if ( !bResult )
	{
		return false;
	}

	//PROGRESS_HOOK
	if ( pProgressHook )
	{
		pProgressHook->Step();
	}

	//��������� script
	{
		CPtr<IDataStream> pSourceStream = pDataStorage->OpenStream( ( randomMapTemplate.szScriptFile  + ".lua" ).c_str(), STREAM_ACCESS_READ );
		CPtr<IDataStream> pDestinationStream = CreateFileStream( ( szRandomMapName + ".lua" ).c_str(), STREAM_ACCESS_WRITE );
		NI_ASSERT_TF( pSourceStream != 0,
									NStr::Format( "CreateRandomMap, Can't open IDataStream: %s", ( randomMapTemplate.szScriptFile  + ".lua" ).c_str() ), 
									return false );
		NI_ASSERT_TF( pDestinationStream != 0,
									NStr::Format( "CreateRandomMap, Can't create IDataStream: %s", ( szRandomMapName + ".lua" ).c_str() ), 
									return false );
		if ( ( pSourceStream == 0 ) || ( pDestinationStream == 0 ) )
		{
			return false;
		}
		int nBytesCopied = pSourceStream->CopyTo( pDestinationStream, pSourceStream->GetSize() );
		NI_ASSERT_TF( nBytesCopied == pSourceStream->GetSize(),
									NStr::Format( "CreateRandomMap, Bytes copied: %d from %d, %s to %s", nBytesCopied, pSourceStream->GetSize(), ( randomMapTemplate.szScriptFile  + ".lua" ).c_str(), ( szRandomMapName + ".lua" ).c_str() ), 
									return false );
		if ( nBytesCopied != pSourceStream->GetSize() )
		{
			return false;
		}
	}

	//TIME KEEPER
	timeKeeper.Trace( "CreateRandomMap. Save map." );

	return bResult;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// basement storage  
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
	//������ ����� ���� ���� ��� �������
	SRMContext context;
	{
		context.levels.push_back( SRMTemplateUnitsTable() );
		{
			context.levels.back().unitPlaceHolders.push_back( CRMUnitsPlaceHoldersHashMap() );
			CRMUnitsPlaceHoldersHashMap &rUnitsPlaceHolders = context.levels.back().unitPlaceHolders.back();

			//squads
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[0]].push_back( "ussr_rifle_39", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[0]].push_back( "USSR_rifle_41", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[1]].push_back( "ussr_rifle_43", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[1]].push_back( "USSR_rifle_42", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[2]].push_back( "ussr_rpd_43", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[3]].push_back( "ussr_ptrs_43", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[4]].push_back( "ussr_sniper", 1 );

			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[5]].push_back( "ZIS_5_Cargo", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[5]].push_back( "Studebaker_Cargo", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[6]].push_back( "ZIS_5v_Engineering", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[6]].push_back( "Studebaker_Engineering", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[7]].push_back( "ZIS_5v_Medicine", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[8]].push_back( "Voroshilovets", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[8]].push_back( "Komintern", 1 ); 
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[9]].push_back( "Willys_MB_Lend_Lease", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[10]].push_back( "GAZ_61", 1 );
			// artillery
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[11]].push_back( "57-mm ZIS-2", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[11]].push_back( "76-mm ZIS-3", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[12]].push_back( "122-mm_M-30", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[12]].push_back( "152_mm_D1", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[13]].push_back( "122-mm_A-19", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[13]].push_back( "152-mm_ML-20", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[14]].push_back( "37_mm_61_K", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[15]].push_back( "BM_13", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[15]].push_back( "BM_8-48", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[16]].push_back( "BM_31_12", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[17]].push_back( "82-mm '37", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[17]].push_back( "120_mm_38", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[18]].push_back( "12_7_mm_DShK", 1 );
			// SPG
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[19]].push_back( "SU_152", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[19]].push_back( "SU_122", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[20]].push_back( "SU_100", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[20]].push_back( "SU_85", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[21]].push_back( "JSU_152", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[22]].push_back( "85-mm_52-K", 1 );
			// armor
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[23]].push_back( "T-70", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[23]].push_back( "T-26", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[24]].push_back( "T_34_76", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[24]].push_back( "BT-7", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[25]].push_back( "JS_3", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[26]].push_back( "KV_1C", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[26]].push_back( "KV-2", 1 );
		}
		{
			context.levels.back().unitPlaceHolders.push_back( CRMUnitsPlaceHoldersHashMap() );
			CRMUnitsPlaceHoldersHashMap &rUnitsPlaceHolders = context.levels.back().unitPlaceHolders.back();

			//squads
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[0]].push_back( "ussr_rifle_39", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[0]].push_back( "USSR_rifle_41", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[1]].push_back( "ussr_rifle_43", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[1]].push_back( "USSR_rifle_42", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[2]].push_back( "ussr_rpd_43", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[3]].push_back( "ussr_ptrs_43", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[4]].push_back( "ussr_sniper", 1 );

			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[5]].push_back( "ZIS_5_Cargo", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[5]].push_back( "Studebaker_Cargo", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[6]].push_back( "ZIS_5v_Engineering", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[6]].push_back( "Studebaker_Engineering", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[7]].push_back( "ZIS_5v_Medicine", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[8]].push_back( "Voroshilovets", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[8]].push_back( "Komintern", 1 ); 
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[9]].push_back( "Willys_MB_Lend_Lease", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[10]].push_back( "GAZ_61", 1 );
			// artillery
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[11]].push_back( "57-mm ZIS-2", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[11]].push_back( "76-mm ZIS-3", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[12]].push_back( "122-mm_M-30", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[12]].push_back( "152_mm_D1", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[13]].push_back( "122-mm_A-19", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[13]].push_back( "152-mm_ML-20", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[14]].push_back( "37_mm_61_K", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[15]].push_back( "BM_13", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[15]].push_back( "BM_8-48", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[16]].push_back( "BM_31_12", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[17]].push_back( "82-mm '37", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[17]].push_back( "120_mm_38", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[18]].push_back( "12_7_mm_DShK", 1 );
			// SPG
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[19]].push_back( "SU_152", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[19]].push_back( "SU_122", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[20]].push_back( "SU_100", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[20]].push_back( "SU_85", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[21]].push_back( "JSU_152", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[22]].push_back( "85-mm_52-K", 1 );
			// armor
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[23]].push_back( "T-70", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[23]].push_back( "T-26", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[24]].push_back( "T_34_76", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[24]].push_back( "BT-7", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[25]].push_back( "JS_3", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[26]].push_back( "KV_1C", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[26]].push_back( "KV-2", 1 );
		}
		context.levels.push_back( SRMTemplateUnitsTable() );
		{
			context.levels.back().unitPlaceHolders.push_back( CRMUnitsPlaceHoldersHashMap() );
			CRMUnitsPlaceHoldersHashMap &rUnitsPlaceHolders = context.levels.back().unitPlaceHolders.back();

			//squads
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[0]].push_back( "ussr_rifle_39", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[0]].push_back( "USSR_rifle_41", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[1]].push_back( "ussr_rifle_43", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[1]].push_back( "USSR_rifle_42", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[2]].push_back( "ussr_rpd_43", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[3]].push_back( "ussr_ptrs_43", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[4]].push_back( "ussr_sniper", 1 );

			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[5]].push_back( "ZIS_5_Cargo", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[5]].push_back( "Studebaker_Cargo", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[6]].push_back( "ZIS_5v_Engineering", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[6]].push_back( "Studebaker_Engineering", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[7]].push_back( "ZIS_5v_Medicine", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[8]].push_back( "Voroshilovets", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[8]].push_back( "Komintern", 1 ); 
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[9]].push_back( "Willys_MB_Lend_Lease", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[10]].push_back( "GAZ_61", 1 );
			// artillery
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[11]].push_back( "57-mm ZIS-2", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[11]].push_back( "76-mm ZIS-3", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[12]].push_back( "122-mm_M-30", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[12]].push_back( "152_mm_D1", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[13]].push_back( "122-mm_A-19", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[13]].push_back( "152-mm_ML-20", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[14]].push_back( "37_mm_61_K", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[15]].push_back( "BM_13", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[15]].push_back( "BM_8-48", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[16]].push_back( "BM_31_12", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[17]].push_back( "82-mm '37", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[17]].push_back( "120_mm_38", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[18]].push_back( "12_7_mm_DShK", 1 );
			// SPG
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[19]].push_back( "SU_152", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[19]].push_back( "SU_122", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[20]].push_back( "SU_100", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[20]].push_back( "SU_85", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[21]].push_back( "JSU_152", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[22]].push_back( "85-mm_52-K", 1 );
			// armor
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[23]].push_back( "T-70", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[23]].push_back( "T-26", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[24]].push_back( "T_34_76", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[24]].push_back( "BT-7", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[25]].push_back( "JS_3", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[26]].push_back( "KV_1C", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[26]].push_back( "KV-2", 1 );
		}
		{
			context.levels.back().unitPlaceHolders.push_back( CRMUnitsPlaceHoldersHashMap() );
			CRMUnitsPlaceHoldersHashMap &rUnitsPlaceHolders = context.levels.back().unitPlaceHolders.back();

			//squads
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[0]].push_back( "ussr_rifle_39", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[0]].push_back( "USSR_rifle_41", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[1]].push_back( "ussr_rifle_43", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[1]].push_back( "USSR_rifle_42", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[2]].push_back( "ussr_rpd_43", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[3]].push_back( "ussr_ptrs_43", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[4]].push_back( "ussr_sniper", 1 );

			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[5]].push_back( "ZIS_5_Cargo", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[5]].push_back( "Studebaker_Cargo", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[6]].push_back( "ZIS_5v_Engineering", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[6]].push_back( "Studebaker_Engineering", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[7]].push_back( "ZIS_5v_Medicine", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[8]].push_back( "Voroshilovets", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[8]].push_back( "Komintern", 1 ); 
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[9]].push_back( "Willys_MB_Lend_Lease", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[10]].push_back( "GAZ_61", 1 );
			// artillery
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[11]].push_back( "57-mm ZIS-2", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[11]].push_back( "76-mm ZIS-3", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[12]].push_back( "122-mm_M-30", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[12]].push_back( "152_mm_D1", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[13]].push_back( "122-mm_A-19", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[13]].push_back( "152-mm_ML-20", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[14]].push_back( "37_mm_61_K", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[15]].push_back( "BM_13", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[15]].push_back( "BM_8-48", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[16]].push_back( "BM_31_12", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[17]].push_back( "82-mm '37", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[17]].push_back( "120_mm_38", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[18]].push_back( "12_7_mm_DShK", 1 );
			// SPG
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[19]].push_back( "SU_152", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[19]].push_back( "SU_122", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[20]].push_back( "SU_100", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[20]].push_back( "SU_85", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[21]].push_back( "JSU_152", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[22]].push_back( "85-mm_52-K", 1 );
			// armor
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[23]].push_back( "T-70", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[23]].push_back( "T-26", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[24]].push_back( "T_34_76", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[24]].push_back( "BT-7", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[25]].push_back( "JS_3", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[26]].push_back( "KV_1C", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[26]].push_back( "KV-2", 1 );
		}
	}
	{
		context.levels.push_back( SRMTemplateUnitsTable() );
		{
			context.levels.back().unitPlaceHolders.push_back( CRMUnitsPlaceHoldersHashMap() );
			CRMUnitsPlaceHoldersHashMap &rUnitsPlaceHolders = context.levels.back().unitPlaceHolders.back();

			//squads
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[0]].push_back( "ussr_rifle_39", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[0]].push_back( "USSR_rifle_41", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[1]].push_back( "ussr_rifle_43", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[1]].push_back( "USSR_rifle_42", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[2]].push_back( "ussr_rpd_43", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[3]].push_back( "ussr_ptrs_43", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[4]].push_back( "ussr_sniper", 1 );

			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[5]].push_back( "ZIS_5_Cargo", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[5]].push_back( "Studebaker_Cargo", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[6]].push_back( "ZIS_5v_Engineering", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[6]].push_back( "Studebaker_Engineering", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[7]].push_back( "ZIS_5v_Medicine", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[8]].push_back( "Voroshilovets", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[8]].push_back( "Komintern", 1 ); 
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[9]].push_back( "Willys_MB_Lend_Lease", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[10]].push_back( "GAZ_61", 1 );
			// artillery
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[11]].push_back( "57-mm ZIS-2", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[11]].push_back( "76-mm ZIS-3", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[12]].push_back( "122-mm_M-30", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[12]].push_back( "152_mm_D1", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[13]].push_back( "122-mm_A-19", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[13]].push_back( "152-mm_ML-20", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[14]].push_back( "37_mm_61_K", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[15]].push_back( "BM_13", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[15]].push_back( "BM_8-48", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[16]].push_back( "BM_31_12", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[17]].push_back( "82-mm '37", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[17]].push_back( "120_mm_38", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[18]].push_back( "12_7_mm_DShK", 1 );
			// SPG
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[19]].push_back( "SU_152", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[19]].push_back( "SU_122", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[20]].push_back( "SU_100", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[20]].push_back( "SU_85", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[21]].push_back( "JSU_152", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[22]].push_back( "85-mm_52-K", 1 );
			// armor
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[23]].push_back( "T-70", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[23]].push_back( "T-26", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[24]].push_back( "T_34_76", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[24]].push_back( "BT-7", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[25]].push_back( "JS_3", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[26]].push_back( "KV_1C", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[26]].push_back( "KV-2", 1 );
		}
		{
			context.levels.back().unitPlaceHolders.push_back( CRMUnitsPlaceHoldersHashMap() );
			CRMUnitsPlaceHoldersHashMap &rUnitsPlaceHolders = context.levels.back().unitPlaceHolders.back();

			//squads
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[0]].push_back( "ussr_rifle_39", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[0]].push_back( "USSR_rifle_41", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[1]].push_back( "ussr_rifle_43", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[1]].push_back( "USSR_rifle_42", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[2]].push_back( "ussr_rpd_43", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[3]].push_back( "ussr_ptrs_43", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[4]].push_back( "ussr_sniper", 1 );

			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[5]].push_back( "ZIS_5_Cargo", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[5]].push_back( "Studebaker_Cargo", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[6]].push_back( "ZIS_5v_Engineering", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[6]].push_back( "Studebaker_Engineering", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[7]].push_back( "ZIS_5v_Medicine", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[8]].push_back( "Voroshilovets", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[8]].push_back( "Komintern", 1 ); 
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[9]].push_back( "Willys_MB_Lend_Lease", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[10]].push_back( "GAZ_61", 1 );
			// artillery
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[11]].push_back( "57-mm ZIS-2", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[11]].push_back( "76-mm ZIS-3", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[12]].push_back( "122-mm_M-30", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[12]].push_back( "152_mm_D1", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[13]].push_back( "122-mm_A-19", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[13]].push_back( "152-mm_ML-20", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[14]].push_back( "37_mm_61_K", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[15]].push_back( "BM_13", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[15]].push_back( "BM_8-48", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[16]].push_back( "BM_31_12", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[17]].push_back( "82-mm '37", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[17]].push_back( "120_mm_38", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[18]].push_back( "12_7_mm_DShK", 1 );
			// SPG
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[19]].push_back( "SU_152", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[19]].push_back( "SU_122", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[20]].push_back( "SU_100", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[20]].push_back( "SU_85", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[21]].push_back( "JSU_152", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[22]].push_back( "85-mm_52-K", 1 );
			// armor
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[23]].push_back( "T-70", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[23]].push_back( "T-26", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[24]].push_back( "T_34_76", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[24]].push_back( "BT-7", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[25]].push_back( "JS_3", 1 );
			rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[26]].push_back( "KV_1C", 1 ); rUnitsPlaceHolders[SRMTemplateUnitsTable::UNIT_RPG_TYPES[26]].push_back( "KV-2", 1 );
		}
	}
	SaveDataResource( "ChapterUnits_Ussr43", "", false, 0, RMGC_CONTEXT_NAME, context );
/**/