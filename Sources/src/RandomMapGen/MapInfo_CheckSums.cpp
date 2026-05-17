#include "stdafx.h"

#include <float.h>

#include "MapInfo_Types.h"
#include "..\Formats\FmtMap.h"
#include "..\Formats\FmtTerrain.h"
#include "..\zlib\zlib.h"
#include "..\Misc\CheckSums.h"
#include "Resource_Types.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
std::unordered_set<const IGDBObject*, SDefaultPtrHash> CMapInfo::gdbObjects;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
using namespace NCheckSums;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMapInfo::GetTerrainCheckSum( uLong *pResourcesCheckSum, uLong *pMapCheckSum )
{
	SCheckSumBufferStorage bufRes;
	SCheckSumBufferStorage bufMap;

	CopyToBuf( &bufMap, terrain.tiles.GetSizeX() );
	CopyToBuf( &bufMap, terrain.tiles.GetSizeY() );

	for ( int i = 0; i < terrain.altitudes.GetSizeY(); ++i )
	{
		for ( int j = 0; j < terrain.altitudes.GetSizeX(); ++j )
			CopyToBuf( &bufMap, terrain.altitudes[i][j].fHeight );
	}

	// terrain params
	STilesetDesc tilesetDesc;
	LoadDataResource( terrain.szTilesetDesc, "", false, 0, "tileset", tilesetDesc ); 
	//CPtr<IDataStream> pStream = GetSingleton<IDataStorage>()->OpenStream( NStr::Format("%s.xml", terrain.szTilesetDesc.c_str()), STREAM_ACCESS_READ );
	//CTreeAccessor tree = CreateDataTreeSaver( pStream, IDataTree::READ );
	//tree.Add( "tileset", &tilesetDesc );

	for ( int i = 0; i < tilesetDesc.terrtypes.size(); ++i )
	{
		CopyToBuf( &bufRes, tilesetDesc.terrtypes[i].tiles.size() );
		for ( int j = 0; j < tilesetDesc.terrtypes[i].tiles.size(); ++j )
			CopyToBuf( &bufRes, tilesetDesc.terrtypes[i].tiles[j].nIndex );

		CopyToBuf( &bufRes, tilesetDesc.terrtypes[i].fPassability );
		CopyToBuf( &bufRes, tilesetDesc.terrtypes[i].dwAIClasses );
		CopyToBuf( &bufRes, tilesetDesc.terrtypes[i].cSoilParams );
		CopyToBuf( &bufRes, tilesetDesc.terrtypes[i].bCanEntrench );
	}

	CopyToBuf( &bufMap, terrain.tiles.GetSizeX() );
	CopyToBuf( &bufMap, terrain.tiles.GetSizeY() );

	for ( int y = 0; y < terrain.tiles.GetSizeY(); ++y )
	{
		for ( int x = 0; x < terrain.tiles.GetSizeX(); ++x )
			CopyToBuf( &bufMap, terrain.tiles[y][x].tile );
	}

	// 3d roads
	for ( int i = 0; i < terrain.roads3.size(); ++i )
	{
		CopyToBuf( &bufRes, terrain.roads3[i].fPassability );
		CopyToBuf( &bufRes, terrain.roads3[i].dwAIClasses );
		CopyToBuf( &bufRes, terrain.roads3[i].cSoilParams );

		CopyToBuf( &bufMap, terrain.roads3[i].points.size() );
		for ( int j = 0; j < terrain.roads3[i].points.size(); ++j )
		{
			CopyToBuf( &bufMap, terrain.roads3[i].points[j].vPos );
			CopyToBuf( &bufMap, terrain.roads3[i].points[j].vNorm );
			CopyToBuf( &bufMap, terrain.roads3[i].points[j].fWidth );
		}
	}

	// rivers
	for ( int i = 0; i < terrain.rivers.size(); ++i )
	{
		CopyToBuf( &bufMap, terrain.rivers[i].points.size() );
		for ( int j = 0; j < terrain.rivers[i].points.size(); ++j )
		{
			CopyToBuf( &bufMap, terrain.rivers[i].points[j].vPos );
			CopyToBuf( &bufMap, terrain.rivers[i].points[j].vNorm );
			CopyToBuf( &bufMap, terrain.rivers[i].points[j].fWidth );
		}
	}

	*pResourcesCheckSum = crc32( 0L, &(bufRes.buf[0]), bufRes.nCnt );
	*pMapCheckSum = crc32( 0L, &(bufMap.buf[0]), bufMap.nCnt );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMapInfo::ProcessObjectCheckSum( const char *pszObjectName, SCheckSumBufferStorage *pBufRes, SCheckSumBufferStorage *pBufMap )
{
	const IGDBObject *pObject = NGDB::GetRPGStats<IGDBObject>( pszObjectName );
	uLong objectCheckSum = pObject->GetCheckSum();
	if ( gdbObjects.find( pObject ) == gdbObjects.end() )
	{
		CopyToBuf( pBufRes, objectCheckSum );
		gdbObjects.insert( pObject );
	}

//	CopyToBuf( pBufMap, objectCheckSum );

	const int nNameLen = NStr::GetStrLen( pszObjectName );
	const int nRequiredSize = pBufMap->nCnt + nNameLen;
	if ( nRequiredSize >= pBufMap->buf.size() )
		pBufMap->buf.resize( nRequiredSize * 1.5 );

	memcpy( &(pBufMap->buf[0]) + pBufMap->nCnt, pszObjectName, nNameLen );
	pBufMap->nCnt += nNameLen;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMapInfo::GetObjectsCheckSum( uLong *pResourcesCheckSum, uLong *pMapCheckSum )
{
	SCheckSumBufferStorage bufRes;
	SCheckSumBufferStorage bufMap;

	for ( int i = 0; i < objects.size(); ++i )
	{
		ProcessObjectCheckSum( objects[i].szName.c_str(), &bufRes, &bufMap );

		CopyToBuf( &bufMap, objects[i].fHP );
		CopyToBuf( &bufMap, objects[i].link.bIntention );
		CopyToBuf( &bufMap, objects[i].link.nLinkID );
		CopyToBuf( &bufMap, objects[i].link.nLinkWith );

		CopyToBuf( &bufMap, objects[i].nDir );
		CopyToBuf( &bufMap, objects[i].nFrameIndex );
		CopyToBuf( &bufMap, objects[i].nPlayer );
		CopyToBuf( &bufMap, objects[i].vPos );
		CopyToBuf( &bufMap, objects[i].nScriptID );
	}

	*pResourcesCheckSum = crc32( 0L, &(bufRes.buf[0]), bufRes.nCnt );
	*pMapCheckSum = crc32( 0L, &(bufMap.buf[0]), bufMap.nCnt );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMapInfo::GetEntrenchmentsCheckSum( uLong *pResourcesCheckSum, uLong *pMapCheckSum )
{
	SCheckSumBufferStorage bufMap;

	for ( int i = 0; i < entrenchments.size(); ++i )
	{
		for ( int j = 0; j < entrenchments[i].sections.size(); ++j )
		{
			for ( int k = 0; k < entrenchments[i].sections[j].size(); ++k )
				CopyToBuf( &bufMap, entrenchments[i].sections[j][k] );
		}
	}

	*pResourcesCheckSum = 0L;
	*pMapCheckSum = crc32( 0L, &(bufMap.buf[0]), bufMap.nCnt );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMapInfo::GetBridgesCheckSum( uLong *pResourcesCheckSum, uLong *pMapCheckSum )
{
	SCheckSumBufferStorage bufMap;
	
	for ( int i = 0; i < bridges.size(); ++i )
	{
		for ( int j = 0; j < bridges[i].size(); ++j )
			CopyToBuf( &bufMap, bridges[i][j] );
	}

	*pResourcesCheckSum = 0L;
	*pMapCheckSum = crc32( 0L, &(bufMap.buf[0]), bufMap.nCnt );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMapInfo::GetReinforcementsCheckSum( uLong *pResourcesCheckSum, uLong *pMapCheckSum )
{
	SCheckSumBufferStorage bufMap;

	for ( std::unordered_map< int, SReinforcementGroupInfo::SGroupsVector >::iterator iter = reinforcements.groups.begin(); iter != reinforcements.groups.end(); ++iter )
	{
		CopyToBuf( &bufMap, iter->first );
		for ( int i = 0; i < iter->second.ids.size(); ++i )
			CopyToBuf( &bufMap, iter->second.ids[i] );
	}

	*pResourcesCheckSum = 0L;
	*pMapCheckSum = crc32( 0L, &(bufMap.buf[0]), bufMap.nCnt );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMapInfo::GetScriptFileCheckSum( uLong *pResourcesCheckSum, uLong *pMapCheckSum )
{
	*pResourcesCheckSum = 0L;
	*pMapCheckSum = 0L;

	if ( szScriptFile != "" )
	{
		CPtr<IDataStream> pStream = GetSingleton<IDataStorage>()->OpenStream( (szScriptFile + ".lua").c_str(), STREAM_ACCESS_READ );
		if ( pStream )
		{
			int cntMap = pStream->GetSize();
			std::vector<BYTE> bufMap( cntMap );
			pStream->Read( &(bufMap[0]), cntMap );

			*pMapCheckSum = crc32( 0L, &(bufMap[0]), cntMap );
		}
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMapInfo::GetScriptAreasCheckSum( uLong *pResourcesCheckSum, uLong *pMapCheckSum )
{
	SCheckSumBufferStorage bufMap;

	for ( int i = 0; i < scriptAreas.size(); ++i )
	{
		CopyToBuf( &bufMap, scriptAreas[i].eType );
		CopyToBuf( &bufMap, scriptAreas[i].center );
		CopyToBuf( &bufMap, scriptAreas[i].vAABBHalfSize );
		CopyToBuf( &bufMap, scriptAreas[i].fR );
	}

	*pResourcesCheckSum = 0L;
	*pMapCheckSum = crc32( 0L, &(bufMap.buf[0]), bufMap.nCnt );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMapInfo::GetDiplomaciesCheckSum( uLong *pResourcesCheckSum, uLong *pMapCheckSum )
{
	SCheckSumBufferStorage bufMap;

	for ( int i = 0; i < diplomacies.size(); ++i )
		CopyToBuf( &bufMap, diplomacies[i] );

	*pResourcesCheckSum = 0L;
	*pMapCheckSum = crc32( 0L, &(bufMap.buf[0]), bufMap.nCnt );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMapInfo::GetUnitCreationCheckSum( uLong *pResourcesCheckSum, uLong *pMapCheckSum )
{
	SCheckSumBufferStorage bufRes;
	SCheckSumBufferStorage bufMap;

	for ( int i = 0; i < unitCreation.units.size(); ++i )
	{
		// aviation
		SUCAviation &aviation = unitCreation.units[i].aviation;
		for ( int j = 0; j < aviation.aircrafts.size(); ++j )
		{
			ProcessObjectCheckSum( aviation.aircrafts[j].szName.c_str(), &bufRes, &bufMap );
			CopyToBuf( &bufMap, aviation.aircrafts[j].nFormationSize );
			CopyToBuf( &bufMap, aviation.aircrafts[j].nPlanes );
		}

		ProcessObjectCheckSum( aviation.szParadropSquadName.c_str(), &bufRes, &bufMap );
		CopyToBuf( &bufMap, aviation.nParadropSquadCount );
		CopyToBuf( &bufMap, aviation.nRelaxTime );
		
		for ( std::list<CVec3>::iterator iter = aviation.vAppearPoints.begin(); iter != aviation.vAppearPoints.end(); ++iter )
			CopyToBuf( &bufMap, *iter );

		//
		if ( !unitCreation.units[i].szPartyName.empty() )
		{
			const int nSize = unitCreation.units[i].szPartyName.size() * sizeof( unitCreation.units[i].szPartyName[0] );
			if ( bufMap.buf.size() <= bufMap.nCnt + nSize )
				bufMap.buf.resize( bufMap.nCnt + nSize );

			memcpy( &(bufMap.buf[0]) + bufMap.nCnt, unitCreation.units[i].szPartyName.c_str(), nSize );
			bufMap.nCnt += nSize;
		}
	}

	*pResourcesCheckSum = crc32( 0L, &(bufRes.buf[0]), bufRes.nCnt );
	*pMapCheckSum = crc32( 0L, &(bufMap.buf[0]), bufMap.nCnt );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMapInfo::GetStartCommandsListCheckSum( uLong *pResourcesCheckSum, uLong *pMapCheckSum )
{
	SCheckSumBufferStorage bufMap;
	
	for ( TStartCommandsList::iterator iter = startCommandsList.begin(); iter != startCommandsList.end(); ++iter )
	{
		CopyToBuf( &bufMap, iter->cmdType );
		CopyToBuf( &bufMap, iter->fNumber );
		CopyToBuf( &bufMap, iter->fromExplosion );
		CopyToBuf( &bufMap, iter->linkID );
		CopyToBuf( &bufMap, iter->vPos );

		for ( int i = 0; i < iter->unitLinkIDs.size(); ++i )
			CopyToBuf( &bufMap, iter->unitLinkIDs[i] );
	}

	*pResourcesCheckSum = 0L;
	*pMapCheckSum = crc32( 0L, &(bufMap.buf[0]), bufMap.nCnt );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMapInfo::GetReservePositionsListCheckSum( uLong *pResourcesCheckSum, uLong *pMapCheckSum )
{
	SCheckSumBufferStorage bufMap;

	for ( TReservePositionsList::iterator iter = reservePositionsList.begin(); iter != reservePositionsList.end(); ++iter )
	{
		CopyToBuf( &bufMap, iter->nArtilleryLinkID );
		CopyToBuf( &bufMap, iter->nTruckLinkID );
		CopyToBuf( &bufMap, iter->vPos );
	}

	*pResourcesCheckSum = 0L;
	*pMapCheckSum = crc32( 0L, &(bufMap.buf[0]), bufMap.nCnt );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMapInfo::GetAIConstsCheckSum( uLong *pResourcesCheckSum, uLong *pMapCheckSum )
{
	SCheckSumBufferStorage bufRes;

	CTableAccessor constsTbl = NDB::OpenDataTable( "consts.xml" );
	std::vector<std::string> entryNames;
	constsTbl.GetEntryNames( "AI", entryNames );
	for ( std::vector<std::string>::iterator iter = entryNames.begin(); iter != entryNames.end(); ++iter )
		CopyToBuf( &bufRes, constsTbl.GetDouble( "AI", iter->c_str(), -1.0 ) );

	*pResourcesCheckSum = crc32( 0L, &(bufRes.buf[0]), bufRes.nCnt );
	*pMapCheckSum = 0L;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMapInfo::UpdateCheckSum( TGetCheckSumFunc pCheckSumFunc, uLong *pResourcesCheckSum, uLong *pMapCheckSum )
{
	uLong resourcesCheckSum, mapCheckSum;
	(this->*pCheckSumFunc)( &resourcesCheckSum, &mapCheckSum );

	*pResourcesCheckSum = GetCRC( resourcesCheckSum, *pResourcesCheckSum );
	*pMapCheckSum = GetCRC( mapCheckSum, *pMapCheckSum );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CMapInfo::GetCheckSums( uLong *pResourcesCheckSum, uLong *pMapCheckSum )
{
	_control87( _EM_INVALID | _EM_ZERODIVIDE | _EM_OVERFLOW | _EM_UNDERFLOW | _EM_INEXACT | _EM_DENORMAL | _PC_24, 0xfffff );

	gdbObjects.clear();
	*pResourcesCheckSum = *pMapCheckSum = 0L;
	
	UpdateCheckSum( &CMapInfo::GetTerrainCheckSum, pResourcesCheckSum, pMapCheckSum );
	UpdateCheckSum( &CMapInfo::GetObjectsCheckSum, pResourcesCheckSum, pMapCheckSum );
	UpdateCheckSum( &CMapInfo::GetEntrenchmentsCheckSum, pResourcesCheckSum, pMapCheckSum );
	UpdateCheckSum( &CMapInfo::GetBridgesCheckSum, pResourcesCheckSum, pMapCheckSum );
	UpdateCheckSum( &CMapInfo::GetReinforcementsCheckSum, pResourcesCheckSum, pMapCheckSum );
	UpdateCheckSum( &CMapInfo::GetScriptFileCheckSum, pResourcesCheckSum, pMapCheckSum );
	UpdateCheckSum( &CMapInfo::GetScriptAreasCheckSum, pResourcesCheckSum, pMapCheckSum );
	UpdateCheckSum( &CMapInfo::GetDiplomaciesCheckSum, pResourcesCheckSum, pMapCheckSum );
	UpdateCheckSum( &CMapInfo::GetUnitCreationCheckSum, pResourcesCheckSum, pMapCheckSum );
	UpdateCheckSum( &CMapInfo::GetStartCommandsListCheckSum, pResourcesCheckSum, pMapCheckSum );
	UpdateCheckSum( &CMapInfo::GetReservePositionsListCheckSum, pResourcesCheckSum, pMapCheckSum );
	UpdateCheckSum( &CMapInfo::GetAIConstsCheckSum, pResourcesCheckSum, pMapCheckSum );

	*pMapCheckSum = GetCRC( nType, *pMapCheckSum );
	*pMapCheckSum = GetCRC( nAttackingSide, *pMapCheckSum );
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
